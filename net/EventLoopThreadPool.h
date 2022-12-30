#pragma once

#include <memory>
#include <vector>

#include "EventLoopThread.h"

namespace canary {

class EventLoopThreadPool : NonCopyable {
 public:
  EventLoopThreadPool() = delete;

  EventLoopThreadPool(size_t threadNum,
                      const std::string &name = "EventLoopThreadPool");

  void start();

  void wait();

  size_t size() { return loopThreadVector_.size(); }

  EventLoop *getNextLoop();

  EventLoop *getLoop(size_t id);

  std::vector<EventLoop *> getLoops() const;

 private:
  std::vector<std::shared_ptr<EventLoopThread>> loopThreadVector_;
  size_t loopIndex_;
};

}  // namespace canary
