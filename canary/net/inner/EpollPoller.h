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
  void update(int operation, Channel *channel);
  void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;

  static const int kInitEventListSize = 16;
  int epollfd_;
  EventList events_;
  using ChannelMap = std::map<int, Channel *>;
  ChannelMap channels_;
};

}  // namespace canary
