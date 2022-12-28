#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "NonCopyable.h"

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

  bool isInLoopThread() const {};

  static EventLoop *getEventLoopOfCurrentThread();

  template <typename Functor>
  inline void runInLoop(Functor &&f) {}

  void moveToCurrentThread();

 private:
  void abortNotInLoopThread();
  void wakeup();
  void wakeupRead();

  void doRunInLoopFuncs();

  EventLoop **threadLocalLoopPtr_;
};

}  // namespace canary