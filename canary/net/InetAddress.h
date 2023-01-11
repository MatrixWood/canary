#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <mutex>
#include <string>
#include <unordered_map>

#include "Date.h"

namespace canary {

class InetAddress {
 public:
  InetAddress(uint16_t port = 0, bool loopbackOnly = false, bool ipv6 = false);

  InetAddress(const std::string &ip, uint16_t port, bool ipv6 = false);

  explicit InetAddress(const struct sockaddr_in &addr)
      : addr_(addr), isUnspecified_(false) {}

  explicit InetAddress(const struct sockaddr_in6 &addr)
      : addr6_(addr), isIpV6_(true), isUnspecified_(false) {}

  sa_family_t family() const { return addr_.sin_family; }

  std::string toIp() const;

  std::string toIpPort() const;

  uint16_t toPort() const;

  bool isIpV6() const { return isIpV6_; }

  bool isIntranetIp() const;

  bool isLoopbackIp() const;

  const struct sockaddr *getSockAddr() const {
    return static_cast<const struct sockaddr *>((void *)(&addr6_));
  }

  void setSockAddrInet6(const struct sockaddr_in6 &addr6) {
    addr6_ = addr6;
    isIpV6_ = (addr6_.sin6_family == AF_INET6);
    isUnspecified_ = false;
  }

  uint32_t ipNetEndian() const;

  const uint32_t *ip6NetEndian() const;

  uint16_t portNetEndian() const { return addr_.sin_port; }

  void setPortNetEndian(uint16_t port) { addr_.sin_port = port; }

  inline bool isUnspecified() const { return isUnspecified_; }

 private:
  union {
    struct sockaddr_in addr_;
    struct sockaddr_in6 addr6_;
  };
  bool isIpV6_{false};
  bool isUnspecified_{true};
};

}  // namespace canary
