#pragma once

#include <signal.h>

#include <atomic>
#include <functional>
#include <thread>

#include "EventLoop.h"
#include "InetAddress.h"
#include "TcpConnection.h"

namespace canary {
  
class Connector;
using ConnectorPtr = std::shared_ptr<Connector>;
class SSLContext;

class TcpClient : NonCopyable, public std::enable_shared_from_this<TcpClient> {
 public:
  TcpClient(EventLoop *loop, const InetAddress &serverAddr,
            const std::string &nameArg);
  ~TcpClient();

  void connect();

  void disconnect();

  void stop();

  TcpConnectionPtr connection() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connection_;
  }

  EventLoop *getLoop() const { return loop_; }

  bool retry() const { return retry_; }

  void enableRetry() { retry_ = true; }

  const std::string &name() const { return name_; }

  void setConnectionCallback(const ConnectionCallback &cb) {
    connectionCallback_ = cb;
  }

  void setConnectionCallback(ConnectionCallback &&cb) {
    connectionCallback_ = std::move(cb);
  }

  void setConnectionErrorCallback(const ConnectionErrorCallback &cb) {
    connectionErrorCallback_ = cb;
  }

  void setMessageCallback(const RecvMessageCallback &cb) {
    messageCallback_ = cb;
  }
  void setMessageCallback(RecvMessageCallback &&cb) {
    messageCallback_ = std::move(cb);
  }

  void setWriteCompleteCallback(const WriteCompleteCallback &cb) {
    writeCompleteCallback_ = cb;
  }

  void setWriteCompleteCallback(WriteCompleteCallback &&cb) {
    writeCompleteCallback_ = std::move(cb);
  }

  void setSSLErrorCallback(const SSLErrorCallback &cb) {
    sslErrorCallback_ = cb;
  }

  void setSSLErrorCallback(SSLErrorCallback &&cb) {
    sslErrorCallback_ = std::move(cb);
  }

  void enableSSL(
      bool useOldTLS = false, bool validateCert = true,
      std::string hostname = "",
      const std::vector<std::pair<std::string, std::string>> &sslConfCmds = {},
      const std::string &certPath = "", const std::string &keyPath = "",
      const std::string &caPath = "");

 private:
  void newConnection(int sockfd);

  void removeConnection(const TcpConnectionPtr &conn);

  EventLoop *loop_;
  ConnectorPtr connector_;  // avoid revealing Connector
  const std::string name_;
  ConnectionCallback connectionCallback_;
  ConnectionErrorCallback connectionErrorCallback_;
  RecvMessageCallback messageCallback_;
  WriteCompleteCallback writeCompleteCallback_;
  SSLErrorCallback sslErrorCallback_;
  std::atomic_bool retry_;    // atomic
  std::atomic_bool connect_;  // atomic
  // always in loop thread
  mutable std::mutex mutex_;
  TcpConnectionPtr connection_;  // @GuardedBy mutex_
  std::shared_ptr<SSLContext> sslCtxPtr_;
  std::string SSLHostName_;
  bool validateCert_{false};

  class IgnoreSigPipe {
   public:
    IgnoreSigPipe() { ::signal(SIGPIPE, SIG_IGN); }
  };

  static IgnoreSigPipe initObj;
};

}  // namespace canary
