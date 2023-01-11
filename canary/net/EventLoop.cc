#include "EventLoop.h"

#include <assert.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <algorithm>
#include <functional>
#include <iostream>
#include <thread>

#include "Channel.h"
#include "inner/Poller.h"
#include "inner/TimerQueue.h"

namespace canary {
int createEventfd() {
  int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (evtfd < 0) {
    std::cout << "Failed in eventfd" << std::endl;
    abort();
  }
  return evtfd;
}

const int kPollTimeMs = 10000;

thread_local EventLoop *t_loopInThisThread = nullptr;

EventLoop::EventLoop()
    : looping_(false),
      threadId_(std::this_thread::get_id()),
      quit_(false),
      poller_(Poller::newPoller(this)),
      currentActiveChannel_(nullptr),
      eventHandling_(false),
      timerQueue_(new TimerQueue(this)),
      wakeupFd_(createEventfd()),
      wakeupChannelPtr_(new Channel(this, wakeupFd_)),
      threadLocalLoopPtr_(&t_loopInThisThread) {
  if (t_loopInThisThread) {
    // LOG_FATAL << "There is already an EventLoop in this thread";
    exit(-1);
  }
  t_loopInThisThread = this;
  wakeupChannelPtr_->setReadCallback(std::bind(&EventLoop::wakeupRead, this));
  wakeupChannelPtr_->enableReading();
}

void EventLoop::resetTimerQueue() {
  assertInLoopThread();
  assert(!looping_.load(std::memory_order_acquire));
  timerQueue_->reset();
}

void EventLoop::resetAfterFork() { poller_->resetAfterFork(); }

EventLoop::~EventLoop() {
  struct timespec delay = {0, 1000000}; /* 1 msec */

  quit();

  while (looping_.load(std::memory_order_acquire)) {
    nanosleep(&delay, nullptr);
  }

  t_loopInThisThread = nullptr;

  close(wakeupFd_);
}

EventLoop *EventLoop::getEventLoopOfCurrentThread() {
  return t_loopInThisThread;
}

void EventLoop::updateChannel(Channel *channel) {
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel) {
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  poller_->removeChannel(channel);
}

void EventLoop::quit() {
  quit_.store(true, std::memory_order_release);

  if (!isInLoopThread()) {
    wakeup();
  }
}

namespace {
template <typename F>
struct ScopeExit {
  ScopeExit(F &&f) : f_(std::forward<F>(f)) {}
  ~ScopeExit() { f_(); }
  F f_;
};

template <typename F>
ScopeExit<F> makeScopeExit(F &&f) {
  return ScopeExit<F>(std::forward<F>(f));
};
}  // namespace

void EventLoop::loop() {
  assert(!looping_);
  assertInLoopThread();
  looping_.store(true, std::memory_order_release);
  quit_.store(false, std::memory_order_release);

  std::exception_ptr loopException;
  try {

    auto loopFlagCleaner = makeScopeExit(
        [this]() { looping_.store(false, std::memory_order_release); });
    while (!quit_.load(std::memory_order_acquire)) {
      activeChannels_.clear();
      poller_->poll(kPollTimeMs, &activeChannels_);

      eventHandling_ = true;
      for (auto it = activeChannels_.begin(); it != activeChannels_.end();
           ++it) {
        currentActiveChannel_ = *it;
        currentActiveChannel_->handleEvent();
      }
      currentActiveChannel_ = nullptr;
      eventHandling_ = false;
      // std::cout << "looping" << endl;
      doRunInLoopFuncs();
    }
    // loopFlagCleaner clears the loop flag here
  } catch (std::exception &e) {
    //LOG_WARN << "Exception thrown from event loop, rethrowing after "
    //            "running functions on quit";
    loopException = std::current_exception();
  }

  Func f;
  while (funcsOnQuit_.dequeue(f)) {
    f();
  }

  // Throw the exception from the end
  if (loopException) {
    //LOG_WARN << "Rethrowing exception from event loop";
    std::rethrow_exception(loopException);
  }
}

void EventLoop::abortNotInLoopThread() {
  //LOG_FATAL << "It is forbidden to run loop on threads other than event-loop "
  //             "thread";
  exit(1);
}

void EventLoop::queueInLoop(const Func &cb) {
  funcs_.enqueue(cb);
  if (!isInLoopThread() || !looping_.load(std::memory_order_acquire)) {
    wakeup();
  }
}

void EventLoop::queueInLoop(Func &&cb) {
  funcs_.enqueue(std::move(cb));
  if (!isInLoopThread() || !looping_.load(std::memory_order_acquire)) {
    wakeup();
  }
}

TimerId EventLoop::runAt(const Date &time, const Func &cb) {
  auto microSeconds =
      time.microSecondsSinceEpoch() - Date::now().microSecondsSinceEpoch();
  std::chrono::steady_clock::time_point tp =
      std::chrono::steady_clock::now() +
      std::chrono::microseconds(microSeconds);
  return timerQueue_->addTimer(cb, tp, std::chrono::microseconds(0));
}

TimerId EventLoop::runAt(const Date &time, Func &&cb) {
  auto microSeconds =
      time.microSecondsSinceEpoch() - Date::now().microSecondsSinceEpoch();
  std::chrono::steady_clock::time_point tp =
      std::chrono::steady_clock::now() +
      std::chrono::microseconds(microSeconds);
  return timerQueue_->addTimer(std::move(cb), tp, std::chrono::microseconds(0));
}

TimerId EventLoop::runAfter(double delay, const Func &cb) {
  return runAt(Date::date().after(delay), cb);
}

TimerId EventLoop::runAfter(double delay, Func &&cb) {
  return runAt(Date::date().after(delay), std::move(cb));
}

TimerId EventLoop::runEvery(double interval, const Func &cb) {
  std::chrono::microseconds dur(
      static_cast<std::chrono::microseconds::rep>(interval * 1000000));
  auto tp = std::chrono::steady_clock::now() + dur;
  return timerQueue_->addTimer(cb, tp, dur);
}

TimerId EventLoop::runEvery(double interval, Func &&cb) {
  std::chrono::microseconds dur(
      static_cast<std::chrono::microseconds::rep>(interval * 1000000));
  auto tp = std::chrono::steady_clock::now() + dur;
  return timerQueue_->addTimer(std::move(cb), tp, dur);
}

void EventLoop::invalidateTimer(TimerId id) {
  if (isRunning() && timerQueue_) timerQueue_->invalidateTimer(id);
}

void EventLoop::doRunInLoopFuncs() {
  callingFuncs_ = true;
  {
    auto callingFlagCleaner =
        makeScopeExit([this]() { callingFuncs_ = false; });
    while (!funcs_.empty()) {
      Func func;
      while (funcs_.dequeue(func)) {
        func();
      }
    }
  }
}

void EventLoop::wakeup() {
  uint64_t tmp = 1;
  int ret = write(wakeupFd_, &tmp, sizeof(tmp));
  (void)ret;
}

void EventLoop::wakeupRead() {
  ssize_t ret = 0;
  uint64_t tmp;
  ret = read(wakeupFd_, &tmp, sizeof(tmp));

  //if (ret < 0) LOG_SYSERR << "wakeup read error";
}

void EventLoop::moveToCurrentThread() {
  if (isRunning()) {
    //LOG_FATAL << "EventLoop cannot be moved when running";
    exit(-1);
  }
  if (isInLoopThread()) {
    //LOG_WARN << "This EventLoop is already in the current thread";
    return;
  }
  if (t_loopInThisThread) {
    //LOG_FATAL << "There is already an EventLoop in this thread, you cannot "
    //             "move another in";
    exit(-1);
  }
  *threadLocalLoopPtr_ = nullptr;
  t_loopInThisThread = this;
  threadLocalLoopPtr_ = &t_loopInThisThread;
  threadId_ = std::this_thread::get_id();
}

void EventLoop::runOnQuit(Func &&cb) { funcsOnQuit_.enqueue(std::move(cb)); }

void EventLoop::runOnQuit(const Func &cb) { funcsOnQuit_.enqueue(cb); }

}  // namespace canary
