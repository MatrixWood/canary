#pragma once

#include "Date.h"
#include "LockFreeQueue.h"
#include "NonCopyable.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace canary {

class Poller;
class TimerQueue;
class Channel;
using ChannelList = std::vector<Channel *>;
using Func = std::function<void()>;
using TimerId = uint64_t;
enum { InvalidTimerId = 0 };

class EventLoop : NonCopyable {
 public:
  EventLoop();

  ~EventLoop();

  void loop();

  void quit();

  void assertInLoopThread() {
    if (!isInLoopThread()) {
      abortNotInLoopThread();
    }
  };

  void resetTimerQueue();

  void resetAfterFork();

  bool isInLoopThread() const {
    return threadId_ == std::this_thread::get_id();
  };

  static EventLoop *getEventLoopOfCurrentThread();

  template <typename Functor>
  inline void runInLoop(Functor &&f) {
    if (isInLoopThread()) {
      f();
    } else {
      queueInLoop(std::forward<Functor>(f));
    }
  }

  void queueInLoop(const Func &f);

  void queueInLoop(Func &&f);

  TimerId runAt(const Date &time, const Func &cb);

  TimerId runAt(const Date &time, Func &&cb);

  TimerId runAfter(double delay, const Func &cb);

  TimerId runAfter(double delay, Func &&cb);

  TimerId runAfter(const std::chrono::duration<double> &delay, const Func &cb) {
    return runAfter(delay.count(), cb);
  }

  TimerId runAfter(const std::chrono::duration<double> &delay, Func &&cb) {
    return runAfter(delay.count(), std::move(cb));
  }

  TimerId runEvery(double interval, const Func &cb);

  TimerId runEvery(double interval, Func &&cb);

  TimerId runEvery(const std::chrono::duration<double> &interval,
                   const Func &cb) {
    return runEvery(interval.count(), cb);
  }

  TimerId runEvery(const std::chrono::duration<double> &interval, Func &&cb) {
    return runEvery(interval.count(), std::move(cb));
  }

  void invalidateTimer(TimerId id);

  void moveToCurrentThread();

  void updateChannel(Channel *chl);

  void removeChannel(Channel *chl);

  size_t index() { return index_; }

  void setIndex(size_t index) { index_ = index; }

  bool isRunning() {
    return looping_.load(std::memory_order_acquire) &&
           (!quit_.load(std::memory_order_acquire));
  }

  bool isCallingFunctions() { return callingFuncs_; }

  void runOnQuit(Func &&cb);

  void runOnQuit(const Func &cb);

 private:
  void abortNotInLoopThread();

  void wakeup();

  void wakeupRead();

  void doRunInLoopFuncs();

  std::atomic<bool> looping_;
  std::thread::id threadId_;
  std::atomic<bool> quit_;
  std::unique_ptr<Poller> poller_;

  ChannelList activeChannels_;
  Channel *currentActiveChannel_;

  bool eventHandling_;
  MpscQueue<Func> funcs_;
  std::unique_ptr<TimerQueue> timerQueue_;
  MpscQueue<Func> funcsOnQuit_;
  bool callingFuncs_{false};
  int wakeupFd_;
  std::unique_ptr<Channel> wakeupChannelPtr_;
  size_t index_{std::numeric_limits<size_t>::max()};

  EventLoop **threadLocalLoopPtr_;
};

}  // namespace canary
