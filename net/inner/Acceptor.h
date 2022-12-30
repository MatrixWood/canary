#pragma once

#include <functional>

#include "Channel.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "NonCopyable.h"
#include "Socket.h"

namespace canary {

using NewConnectionCallback = std::function<void(int fd, const InetAddress &)>;

class Acceptor : NonCopyable {
 public:
  Acceptor(EventLoop *loop, const InetAddress &addr, bool reUseAddr = true,
           bool reUsePort = true);

  ~Acceptor();

  const InetAddress &addr() const { return addr_; }

  void setNewConnectionCallback(const NewConnectionCallback &cb) {
    newConnectionCallback_ = cb;
  };

  void listen();

 protected:
  void readCallback();

  int idleFd_;
  Socket sock_;
  InetAddress addr_;
  EventLoop *loop_;
  NewConnectionCallback newConnectionCallback_;
  Channel acceptChannel_;
};

}  // namespace canary
