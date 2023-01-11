#pragma once

#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <thread>

#include "EventLoop.h"
#include "NonCopyable.h"

namespace canary {

class EventLoopThread : NonCopyable {
 public:
  explicit EventLoopThread(const std::string &threadName = "EventLoopThread");
  ~EventLoopThread();

  void wait();

  EventLoop *getLoop() const { return loop_.get(); }

  void run();

 private:
  void loopFuncs();

  std::shared_ptr<EventLoop> loop_;
  std::mutex loopMutex_;
  std::string loopThreadName_;
  std::promise<std::shared_ptr<EventLoop>> promiseForLoopPointer_;
  std::promise<int> promiseForRun_;
  std::promise<int> promiseForLoop_;
  std::once_flag once_;
  std::thread thread_;
};

}  // namespace canary
