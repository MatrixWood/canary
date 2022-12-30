#pragma once

#include <unistd.h>

#include <array>
#include <list>
#include <mutex>
#include <thread>

#include "TcpConnection.h"
#include "TimingWheel.h"

namespace canary {

class Channel;
class Socket;
class TcpServer;

void removeConnection(EventLoop *loop, const TcpConnectionPtr &conn);

class TcpConnectionImpl
    : public TcpConnection,
      public NonCopyable,
      public std::enable_shared_from_this<TcpConnectionImpl> {
  friend class TcpServer;
  friend class TcpClient;
  friend void removeConnection(EventLoop *loop, const TcpConnectionPtr &conn);

 public:
  class KickoffEntry {
   public:
    explicit KickoffEntry(const std::weak_ptr<TcpConnection> &conn)
        : conn_(conn) {}
    void reset() { conn_.reset(); }
    ~KickoffEntry() {
      auto conn = conn_.lock();
      if (conn) {
        conn->forceClose();
      }
    }

   private:
    std::weak_ptr<TcpConnection> conn_;
  };

  TcpConnectionImpl(EventLoop *loop, int socketfd, const InetAddress &localAddr,
                    const InetAddress &peerAddr);

  virtual ~TcpConnectionImpl();

  virtual void send(const char *msg, size_t len) override;
  virtual void send(const void *msg, size_t len) override;
  virtual void send(const std::string &msg) override;
  virtual void send(std::string &&msg) override;
  virtual void send(const MsgBuffer &buffer) override;
  virtual void send(MsgBuffer &&buffer) override;
  virtual void send(const std::shared_ptr<std::string> &msgPtr) override;
  virtual void send(const std::shared_ptr<MsgBuffer> &msgPtr) override;
  virtual void sendFile(const char *fileName, size_t offset = 0,
                        size_t length = 0) override;
  virtual void sendFile(const wchar_t *fileName, size_t offset = 0,
                        size_t length = 0) override;
  virtual void sendStream(
      std::function<std::size_t(char *, std::size_t)> callback) override;

  virtual const InetAddress &localAddr() const override { return localAddr_; }
  virtual const InetAddress &peerAddr() const override { return peerAddr_; }

  virtual bool connected() const override {
    return status_ == ConnStatus::Connected;
  }
  virtual bool disconnected() const override {
    return status_ == ConnStatus::Disconnected;
  }

  // virtual MsgBuffer* getSendBuffer() override{ return  &writeBuffer_;}
  virtual MsgBuffer *getRecvBuffer() override { return &readBuffer_; }

  virtual void setHighWaterMarkCallback(const HighWaterMarkCallback &cb,
                                        size_t markLen) override {
    highWaterMarkCallback_ = cb;
    highWaterMarkLen_ = markLen;
  }

  virtual void keepAlive() override {
    idleTimeout_ = 0;
    auto entry = kickoffEntry_.lock();
    if (entry) {
      entry->reset();
    }
  }

  virtual bool isKeepAlive() override { return idleTimeout_ == 0; }

  virtual void setTcpNoDelay(bool on) override;

  virtual void shutdown() override;

  virtual void forceClose() override;

  virtual EventLoop *getLoop() override { return loop_; }

  virtual size_t bytesSent() const override { return bytesSent_; }

  virtual size_t bytesReceived() const override { return bytesReceived_; }

  virtual void startClientEncryption(
      std::function<void()> callback, bool useOldTLS = false,
      bool validateCert = true, std::string hostname = "",
      const std::vector<std::pair<std::string, std::string>> &sslConfCmds = {})
      override;

  virtual void startServerEncryption(const std::shared_ptr<SSLContext> &ctx,
                                     std::function<void()> callback) override;

  virtual bool isSSLConnection() const override { return isEncrypted_; }

 private:
  std::weak_ptr<KickoffEntry> kickoffEntry_;
  std::weak_ptr<TimingWheel> timingWheelWeakPtr_;
  size_t idleTimeout_{0};
  Date lastTimingWheelUpdateTime_;

  void enableKickingOff(size_t timeout,
                        const std::shared_ptr<TimingWheel> &timingWheel) {
    assert(timingWheel);
    assert(timingWheel->getLoop() == loop_);
    assert(timeout > 0);
    auto entry = std::make_shared<KickoffEntry>(shared_from_this());
    kickoffEntry_ = entry;
    timingWheelWeakPtr_ = timingWheel;
    idleTimeout_ = timeout;
    timingWheel->insertEntry(timeout, entry);
  }
  void extendLife();

  void sendFile(int sfd, size_t offset = 0, size_t length = 0);

  void setRecvMsgCallback(const RecvMessageCallback &cb) {
    recvMsgCallback_ = cb;
  }
  void setRecvMsgCallback(RecvMessageCallback &&cb) {
    recvMsgCallback_ = std::move(cb);
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
  void setCloseCallback(const CloseCallback &cb) { closeCallback_ = cb; }
  void setCloseCallback(CloseCallback &&cb) { closeCallback_ = std::move(cb); }
  void setSSLErrorCallback(const SSLErrorCallback &cb) {
    sslErrorCallback_ = cb;
  }
  void setSSLErrorCallback(SSLErrorCallback &&cb) {
    sslErrorCallback_ = std::move(cb);
  }
  void connectDestroyed();
  virtual void connectEstablished();

 protected:
  struct BufferNode {
    int sendFd_{-1};
    off_t offset_{0};

    ssize_t fileBytesToSend_{0};
    std::function<std::size_t(char *, std::size_t)> streamCallback_;
    std::size_t nDataWritten_{0};
    std::shared_ptr<MsgBuffer> msgBuffer_;
    bool isFile() const {
      if (streamCallback_) return true;
      if (sendFd_ >= 0) return true;
      return false;
    }
    ~BufferNode() {
      if (sendFd_ >= 0) close(sendFd_);
      if (streamCallback_) streamCallback_(nullptr, 0);
    }
  };

  using BufferNodePtr = std::shared_ptr<BufferNode>;

  void readCallback();
  void writeCallback();
  void handleClose();
  void handleError();

  void sendFileInLoop(const BufferNodePtr &file);
  void sendInLoop(const void *buffer, size_t length);
  ssize_t writeInLoop(const void *buffer, size_t length);

  enum class ConnStatus { Disconnected, Connecting, Connected, Disconnecting };

  bool isEncrypted_{false};
  EventLoop *loop_;
  std::unique_ptr<Channel> ioChannelPtr_;
  std::unique_ptr<Socket> socketPtr_;
  MsgBuffer readBuffer_;
  std::list<BufferNodePtr> writeBufferList_;
  InetAddress localAddr_, peerAddr_;
  ConnStatus status_{ConnStatus::Connecting};

  RecvMessageCallback recvMsgCallback_;
  ConnectionCallback connectionCallback_;
  CloseCallback closeCallback_;
  WriteCompleteCallback writeCompleteCallback_;
  HighWaterMarkCallback highWaterMarkCallback_;
  SSLErrorCallback sslErrorCallback_;

  size_t highWaterMarkLen_;
  std::string name_;

  uint64_t sendNum_{0};
  std::mutex sendNumMutex_;

  size_t bytesSent_{0};
  size_t bytesReceived_{0};

  std::unique_ptr<std::vector<char>> fileBufferPtr_;
};

using TcpConnectionImplPtr = std::shared_ptr<TcpConnectionImpl>;

}  // namespace canary
