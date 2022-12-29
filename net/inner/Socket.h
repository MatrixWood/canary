#pragma once

#include <fcntl.h>
#include <unistd.h>

#include <string>

#include "InetAddress.h"
#include "NonCopyable.h"

namespace canary {

class Socket : NonCopyable {
 public:
  explicit Socket(int sockfd) : sockFd_(sockfd) {}

  ~Socket();

  static int createNonblockingSocketOrDie(int family) {
    int sock = ::socket(family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
                        IPPROTO_TCP);
    if (sock < 0) {
      // LOG_SYSERR << "sockets::createNonblockingOrDie";
      exit(1);
    }
    // LOG_TRACE << "sock=" << sock;
    return sock;
  }

  static int getSocketError(int sockfd) {
    int optval;
    socklen_t optlen = static_cast<socklen_t>(sizeof optval);
    if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
      return errno;
    } else {
      return optval;
    }
  }

  static int connect(int sockfd, const InetAddress &addr) {
    if (addr.isIpV6())
      return ::connect(sockfd, addr.getSockAddr(),
                       static_cast<socklen_t>(sizeof(struct sockaddr_in6)));
    else
      return ::connect(sockfd, addr.getSockAddr(),
                       static_cast<socklen_t>(sizeof(struct sockaddr_in)));
  }

  static bool isSelfConnect(int sockfd);

  void bindAddress(const InetAddress &localaddr);

  void listen();

  int accept(InetAddress *peeraddr);

  void closeWrite();

  int read(char *buffer, uint64_t len);

  int fd() { return sockFd_; }

  static struct sockaddr_in6 getLocalAddr(int sockfd);

  static struct sockaddr_in6 getPeerAddr(int sockfd);

  void setTcpNoDelay(bool on);

  void setReuseAddr(bool on);

  void setReusePort(bool on);

  void setKeepAlive(bool on);

  int getSocketError();

 protected:
  int sockFd_;

 public:
  static void setNonBlockAndCloseOnExec(int sockfd) {
    int flags = ::fcntl(sockfd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    int ret = ::fcntl(sockfd, F_SETFL, flags);

    flags = ::fcntl(sockfd, F_GETFD, 0);
    flags |= FD_CLOEXEC;
    ret = ::fcntl(sockfd, F_SETFD, flags);
    (void)ret;
  }
};

}  // namespace canary
