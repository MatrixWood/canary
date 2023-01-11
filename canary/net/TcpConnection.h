#pragma once

#include <functional>
#include <memory>
#include <string>

#include "Callback.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "MsgBuffer.h"
#include "NonCopyable.h"

namespace canary {

class SSLContext;

std::shared_ptr<SSLContext> newSSLServerContext(
    const std::string &certPath, const std::string &keyPath,
    bool useOldTLS = false,
    const std::vector<std::pair<std::string, std::string>> &sslConfCmds = {},
    const std::string &caPath = "");

class TcpConnection {
 public:
  TcpConnection() = default;
  virtual ~TcpConnection(){};

  virtual void send(const char *msg, size_t len) = 0;
  virtual void send(const void *msg, size_t len) = 0;
  virtual void send(const std::string &msg) = 0;
  virtual void send(std::string &&msg) = 0;
  virtual void send(const MsgBuffer &buffer) = 0;
  virtual void send(MsgBuffer &&buffer) = 0;
  virtual void send(const std::shared_ptr<std::string> &msgPtr) = 0;
  virtual void send(const std::shared_ptr<MsgBuffer> &msgPtr) = 0;

  virtual void sendFile(const char *fileName, size_t offset = 0,
                        size_t length = 0) = 0;

  virtual void sendFile(const wchar_t *fileName, size_t offset = 0,
                        size_t length = 0) = 0;

  virtual void sendStream(
      std::function<std::size_t(char *, std::size_t)> callback) = 0;

  virtual const InetAddress &localAddr() const = 0;

  virtual const InetAddress &peerAddr() const = 0;

  virtual bool connected() const = 0;

  virtual bool disconnected() const = 0;

  virtual MsgBuffer *getRecvBuffer() = 0;

  virtual void setHighWaterMarkCallback(const HighWaterMarkCallback &cb,
                                        size_t markLen) = 0;

  virtual void setTcpNoDelay(bool on) = 0;

  virtual void shutdown() = 0;

  virtual void forceClose() = 0;

  virtual EventLoop *getLoop() = 0;

  void setContext(const std::shared_ptr<void> &context) {
    contextPtr_ = context;
  }

  void setContext(std::shared_ptr<void> &&context) {
    contextPtr_ = std::move(context);
  }

  template <typename T>
  std::shared_ptr<T> getContext() const {
    return std::static_pointer_cast<T>(contextPtr_);
  }

  bool hasContext() const { return (bool)contextPtr_; }

  void clearContext() { contextPtr_.reset(); }

  virtual void keepAlive() = 0;

  virtual bool isKeepAlive() = 0;

  virtual size_t bytesSent() const = 0;

  virtual size_t bytesReceived() const = 0;

  virtual bool isSSLConnection() const = 0;

  virtual void startClientEncryption(
      std::function<void()> callback, bool useOldTLS = false,
      bool validateCert = true, std::string hostname = "",
      const std::vector<std::pair<std::string, std::string>> &sslConfCmds =
          {}) = 0;

  virtual void startServerEncryption(const std::shared_ptr<SSLContext> &ctx,
                                     std::function<void()> callback) = 0;

 protected:
  bool validateCert_ = false;

 private:
  std::shared_ptr<void> contextPtr_;
};

}  // namespace canary
