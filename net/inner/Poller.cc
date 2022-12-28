#include "Poller.h"

#ifdef __linux__
#include "EpollPoller.h"
#endif

using namespace canary;

Poller *Poller::newPoller(EventLoop *loop) {
#if defined __linux__
  return new EpollPoller(loop);
#endif
}