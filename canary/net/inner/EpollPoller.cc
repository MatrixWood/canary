#include "EpollPoller.h"

#include <cstring>

#include "Channel.h"
#ifdef __linux__
#include <assert.h>
#include <poll.h>
#include <strings.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <iostream>
#endif

namespace canary {

#if defined __linux__
static_assert(EPOLLIN == POLLIN, "EPOLLIN != POLLIN");
static_assert(EPOLLPRI == POLLPRI, "EPOLLPRI != POLLPRI");
static_assert(EPOLLOUT == POLLOUT, "EPOLLOUT != POLLOUT");
static_assert(EPOLLRDHUP == POLLRDHUP, "EPOLLRDHUP != POLLRDHUP");
static_assert(EPOLLERR == POLLERR, "EPOLLERR != POLLERR");
static_assert(EPOLLHUP == POLLHUP, "EPOLLHUP != POLLHUP");
#endif

namespace {
const int kNew = -1;
const int kAdded = 1;
const int kDeleted = 2;
}  // namespace

EpollPoller::EpollPoller(EventLoop *loop)
    : Poller(loop),
      epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize) {}

EpollPoller::~EpollPoller() { close(epollfd_); }

void EpollPoller::poll(int timeoutMs, ChannelList *activeChannels) {
  int numEvents = ::epoll_wait(epollfd_, &*events_.begin(),
                               static_cast<int>(events_.size()), timeoutMs);
  int savedErrno = errno;
  // Timestamp now(Timestamp::now());
  if (numEvents > 0) {
    // LOG_TRACE << numEvents << " events happended";
    fillActiveChannels(numEvents, activeChannels);
    if (static_cast<size_t>(numEvents) == events_.size()) {
      events_.resize(events_.size() * 2);
    }
  } else if (numEvents == 0) {
    // std::cout << "nothing happended" << std::endl;
  } else {
    // error happens, log uncommon ones
    if (savedErrno != EINTR) {
      errno = savedErrno;
      // LOG_SYSERR << "EPollEpollPoller::poll()";
    }
  }
  return;
}

void EpollPoller::fillActiveChannels(int numEvents,
                                     ChannelList *activeChannels) const {
  assert(static_cast<size_t>(numEvents) <= events_.size());
  for (int i = 0; i < numEvents; ++i) {
    Channel *channel = static_cast<Channel *>(events_[i].data.ptr);
    int fd = channel->fd();
    ChannelMap::const_iterator it = channels_.find(fd);
    assert(it != channels_.end());
    assert(it->second == channel);
    channel->setRevents(events_[i].events);
    activeChannels->push_back(channel);
  }
  // LOG_TRACE<<"active Channels num:"<<activeChannels->size();
}

void EpollPoller::updateChannel(Channel *channel) {
  assertInLoopThread();
  assert(channel->fd() >= 0);

  const int index = channel->index();
  // LOG_TRACE << "fd = " << channel->fd()
  //  << " events = " << channel->events() << " index = " << index;
  if (index == kNew || index == kDeleted) {
    int fd = channel->fd();
    if (index == kNew) {
      assert(channels_.find(fd) == channels_.end());
      channels_[fd] = channel;
    } else {  // index == kDeleted
      assert(channels_.find(fd) != channels_.end());
      assert(channels_[fd] == channel);
    }
    channel->setIndex(kAdded);
    update(EPOLL_CTL_ADD, channel);
  } else {
    int fd = channel->fd();
    (void)fd;
    assert(channels_.find(fd) != channels_.end());
    assert(channels_[fd] == channel);
    assert(index == kAdded);
    if (channel->isNoneEvent()) {
      update(EPOLL_CTL_DEL, channel);
      channel->setIndex(kDeleted);
    } else {
      update(EPOLL_CTL_MOD, channel);
    }
  }
}

void EpollPoller::removeChannel(Channel *channel) {
  EpollPoller::assertInLoopThread();
  int fd = channel->fd();
  assert(channels_.find(fd) != channels_.end());
  assert(channels_[fd] == channel);
  size_t n = channels_.erase(fd);
  (void)n;
  assert(n == 1);
  assert(channel->isNoneEvent());
  int index = channel->index();
  assert(index == kAdded || index == kDeleted);
  if (index == kAdded) {
    update(EPOLL_CTL_DEL, channel);
  }
  channel->setIndex(kNew);
}

void EpollPoller::update(int operation, Channel *channel) {
  struct epoll_event event;
  memset(&event, 0, sizeof(event));
  event.events = channel->events();
  event.data.ptr = channel;
  int fd = channel->fd();
  if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) {
    if (operation == EPOLL_CTL_DEL) {
      // LOG_SYSERR << "epoll_ctl op =" << operationToString(operation) <<
      // " fd =" << fd;
    } else {
      //  LOG_SYSFATAL << "epoll_ctl op =" << operationToString(operation)
      //  << " fd =" << fd;
    }
  }
}

}  // namespace canary
