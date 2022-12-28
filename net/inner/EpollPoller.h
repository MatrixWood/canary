#pragma once

#include <map>
#include <memory>

#include "EventLoop.h"
#include "NonCopyable.h"
#include "Poller.h"

using EventList = std::vector<struct epoll_event>;

namespace canary {

class Channel;

class EpollPoller : public Poller {
 public:
  explicit EpollPoller(EventLoop *loop);
  virtual ~EpollPoller();
  virtual void poll(int timeoutMs, ChannelList *activeChannels) override;
  virtual void updateChannel(Channel *channel) override;
  virtual void removeChannel(Channel *channel) override;

 private:
  static const int kInitEventListSize = 16;
  int epollfd_;
  EventList events_;
  void update(int operation, Channel *channel);
  using ChannelMap = std::map<int, Channel *>;
  ChannelMap channels_;
  void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
};

}  // namespace canary
