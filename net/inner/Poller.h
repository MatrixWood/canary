#pragma once
#include <map>
#include <memory>

#include "EventLoop.h"
#include "NonCopyable.h"

namespace canary {

class Channel;

class Poller : NonCopyable {
 public:
  explicit Poller(EventLoop *loop) : ownerLoop_(loop){};
  virtual ~Poller() {}
  void assertInLoopThread() { ownerLoop_->assertInLoopThread(); }
  virtual void updateChannel(Channel *channel) = 0;
  virtual void removeChannel(Channel *channel) = 0;
  virtual void resetAfterFork() {}
  static Poller *newPoller(EventLoop *loop);

 private:
  EventLoop *ownerLoop_;
};

}  // namespace canary
