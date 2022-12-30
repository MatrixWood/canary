#pragma once

#include <signal.h>

#include <memory>
#include <set>
#include <string>

#include "Callback.h"
#include "EventLoopThreadPool.h"
#include "InetAddress.h"
#include "NonCopyable.h"
#include "TcpConnection.h"
#include "TimingWheel.h"

namespace canary {
class Acceptor;
class SSLContext;

class TcpServer : NonCopyable {
 public:
  TcpServer(EventLoop *loop, const InetAddress &address,
            const std::string &name, bool reUseAddr = true,
            bool reUsePort = true);
  ~TcpServer();

  void start();

  void stop();

  void setIoLoopNum(size_t num) {
    assert(!started_);
    loopPoolPtr_ = std::make_shared<EventLoopThreadPool>(num);
    loopPoolPtr_->start();
  }

  void setIoLoopThreadPool(const std::shared_ptr<EventLoopThreadPool> &pool) {
    assert(pool->size() > 0);
    assert(!started_);
    loopPoolPtr_ = pool;
    loopPoolPtr_->start();
  }

  void setRecvMessageCallback(const RecvMessageCallback &cb) {
    recvMessageCallback_ = cb;
  }

  void setRecvMessageCallback(RecvMessageCallback &&cb) {
    recvMessageCallback_ = std::move(cb);
  }

  void setConnectionCallback(const ConnectionCallback &cb) {
    connectionCallback_ = cb;
  }

  void setConnectionCallback(ConnectionCallback &&cb) {
    connectionCallback_ = std::move(cb);
  }

  void setWriteCompleteCallback(const WriteCompleteCallback &cb) {
    writeCompleteCallback_ = cb;
  }

  void setWriteCompleteCallback(WriteCompleteCallback &&cb) {
    writeCompleteCallback_ = std::move(cb);
  }

  const std::string &name() const { return serverName_; }

  const std::string ipPort() const;

  const InetAddress &address() const;

  EventLoop *getLoop() const { return loop_; }

  std::vector<EventLoop *> getIoLoops() const {
    return loopPoolPtr_->getLoops();
  }

  void kickoffIdleConnections(size_t timeout) {
    loop_->runInLoop([this, timeout]() {
      assert(!started_);
      idleTimeout_ = timeout;
    });
  }

  void enableSSL(
      const std::string &certPath, const std::string &keyPath,
      bool useOldTLS = false,
      const std::vector<std::pair<std::string, std::string>> &sslConfCmds = {},
      const std::string &caPath = "");

 private:
  void handleCloseInLoop(const TcpConnectionPtr &connectionPtr);
  
  void newConnection(int fd, const InetAddress &peer);

  void connectionClosed(const TcpConnectionPtr &connectionPtr);

  EventLoop *loop_;
  std::unique_ptr<Acceptor> acceptorPtr_;
  std::string serverName_;
  std::set<TcpConnectionPtr> connSet_;

  RecvMessageCallback recvMessageCallback_;
  ConnectionCallback connectionCallback_;
  WriteCompleteCallback writeCompleteCallback_;

  size_t idleTimeout_{0};
  std::map<EventLoop *, std::shared_ptr<TimingWheel>> timingWheelMap_;
  std::shared_ptr<EventLoopThreadPool> loopPoolPtr_;

  class IgnoreSigPipe {
   public:
    IgnoreSigPipe() {
      ::signal(SIGPIPE, SIG_IGN);
      // LOG_TRACE << "Ignore SIGPIPE";
    }
  };

  IgnoreSigPipe initObj;

  bool started_{false};

  std::shared_ptr<SSLContext> sslCtxPtr_;
};

}  // namespace canary
