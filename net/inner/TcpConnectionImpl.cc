#include "TcpConnectionImpl.h"

#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "Channel.h"
#include "Socket.h"
#include "Utility.h"

using namespace canary;

namespace canary {

std::shared_ptr<SSLContext> newSSLServerContext(
    const std::string &, const std::string &, bool,
    const std::vector<std::pair<std::string, std::string>> &,
    const std::string &) {
  // LOG_FATAL << "OpenSSL is not found in your system!";
  throw std::runtime_error("OpenSSL is not found in your system!");
}
}  // namespace canary

TcpConnectionImpl::TcpConnectionImpl(EventLoop *loop, int socketfd,
                                     const InetAddress &localAddr,
                                     const InetAddress &peerAddr)
    : loop_(loop),
      ioChannelPtr_(new Channel(loop, socketfd)),
      socketPtr_(new Socket(socketfd)),
      localAddr_(localAddr),
      peerAddr_(peerAddr) {
  // LOG_TRACE << "new connection:" << peerAddr.toIpPort() << "->"
  //           << localAddr.toIpPort();
  ioChannelPtr_->setReadCallback(
      std::bind(&TcpConnectionImpl::readCallback, this));
  ioChannelPtr_->setWriteCallback(
      std::bind(&TcpConnectionImpl::writeCallback, this));
  ioChannelPtr_->setCloseCallback(
      std::bind(&TcpConnectionImpl::handleClose, this));
  ioChannelPtr_->setErrorCallback(
      std::bind(&TcpConnectionImpl::handleError, this));
  socketPtr_->setKeepAlive(true);
  name_ = localAddr.toIpPort() + "--" + peerAddr.toIpPort();
}

TcpConnectionImpl::~TcpConnectionImpl() {}

void TcpConnectionImpl::startServerEncryption(
    const std::shared_ptr<SSLContext> &ctx, std::function<void()> callback) {
  (void)ctx;
  (void)callback;

  // LOG_FATAL << "OpenSSL is not found in your system!";
  throw std::runtime_error("OpenSSL is not found in your system!");
}

void TcpConnectionImpl::startClientEncryption(
    std::function<void()> callback, bool useOldTLS, bool validateCert,
    std::string hostname,
    const std::vector<std::pair<std::string, std::string>> &sslConfCmds) {
  // When not using OpenSSL, using `void` here will
  // work around the unused parameter warnings without overhead.
  (void)callback;
  (void)useOldTLS;
  (void)validateCert;
  (void)hostname;
  (void)sslConfCmds;

  // LOG_FATAL << "OpenSSL is not found in your system!";
  throw std::runtime_error("OpenSSL is not found in your system!");
}

void TcpConnectionImpl::readCallback() {
  // LOG_TRACE<<"read Callback";

  loop_->assertInLoopThread();
  int ret = 0;

  ssize_t n = readBuffer_.readFd(socketPtr_->fd(), &ret);
  // LOG_TRACE<<"read "<<n<<" bytes from socket";
  if (n == 0) {
    // socket closed by peer
    handleClose();
  } else if (n < 0) {
    if (errno == EPIPE || errno == ECONNRESET) {
      // LOG_TRACE << "EPIPE or ECONNRESET, errno=" << errno
      //           << " fd=" << socketPtr_->fd();
      return;
    }
    if (errno == EAGAIN)  // TODO: any others?
    {
      // LOG_TRACE << "EAGAIN, errno=" << errno << " fd=" << socketPtr_->fd();
      return;
    }
    // LOG_SYSERR << "read socket error";
    handleClose();
    return;
  }
  extendLife();
  if (n > 0) {
    bytesReceived_ += n;
    if (recvMsgCallback_) {
      recvMsgCallback_(shared_from_this(), &readBuffer_);
    }
  }
}

void TcpConnectionImpl::extendLife() {
  if (idleTimeout_ > 0) {
    auto now = Date::date();
    if (now < lastTimingWheelUpdateTime_.after(1.0)) return;
    lastTimingWheelUpdateTime_ = now;
    auto entry = kickoffEntry_.lock();
    if (entry) {
      auto timingWheelPtr = timingWheelWeakPtr_.lock();
      if (timingWheelPtr) timingWheelPtr->insertEntry(idleTimeout_, entry);
    }
  }
}

void TcpConnectionImpl::writeCallback() {
  loop_->assertInLoopThread();
  extendLife();
  if (ioChannelPtr_->isWriting()) {
    assert(!writeBufferList_.empty());
    auto writeBuffer_ = writeBufferList_.front();
    if (!writeBuffer_->isFile()) {
      // not a file
      if (writeBuffer_->msgBuffer_->readableBytes() <= 0) {
        // finished sending
        writeBufferList_.pop_front();
        if (writeBufferList_.empty()) {
          // stop writing
          ioChannelPtr_->disableWriting();
          if (writeCompleteCallback_)
            writeCompleteCallback_(shared_from_this());
          if (status_ == ConnStatus::Disconnecting) {
            socketPtr_->closeWrite();
          }
        } else {
          // send next
          // what if the next is not a file???
          auto fileNode = writeBufferList_.front();
          assert(fileNode->isFile());
          sendFileInLoop(fileNode);
        }
      } else {
        // continue sending
        auto n = writeInLoop(writeBuffer_->msgBuffer_->peek(),
                             writeBuffer_->msgBuffer_->readableBytes());
        if (n >= 0) {
          writeBuffer_->msgBuffer_->retrieve(n);
        } else {
          if (errno != EWOULDBLOCK) {
            // TODO: any others?
            if (errno == EPIPE || errno == ECONNRESET) {
              // LOG_TRACE << "EPIPE or ECONNRESET, errno=" << errno;
              return;
            }
            // LOG_SYSERR << "Unexpected error(" << errno << ")";
            return;
          }
        }
      }
    } else {
      // is a file
      if (writeBuffer_->fileBytesToSend_ <= 0) {
        // finished sending
        writeBufferList_.pop_front();
        if (writeBufferList_.empty()) {
          // stop writing
          ioChannelPtr_->disableWriting();
          if (writeCompleteCallback_)
            writeCompleteCallback_(shared_from_this());
          if (status_ == ConnStatus::Disconnecting) {
            socketPtr_->closeWrite();
          }
        } else {
          // next is not a file
          if (!writeBufferList_.front()->isFile()) {
            // There is data to be sent in the buffer.
            auto n = writeInLoop(
                writeBufferList_.front()->msgBuffer_->peek(),
                writeBufferList_.front()->msgBuffer_->readableBytes());
            if (n >= 0) {
              writeBufferList_.front()->msgBuffer_->retrieve(n);
            } else {
              if (errno != EWOULDBLOCK) {
                // TODO: any others?
                if (errno == EPIPE || errno == ECONNRESET) {
                  // LOG_TRACE << "EPIPE or "
                  //              "ECONNRESET, erron="
                  //           << errno;
                  return;
                }
                // LOG_SYSERR << "Unexpected error(" << errno << ")";
                return;
              }
            }
          } else {
            // next is a file
            sendFileInLoop(writeBufferList_.front());
          }
        }
      } else {
        sendFileInLoop(writeBuffer_);
      }
    }
  } else {
    // LOG_SYSERR << "no writing but write callback called";
  }
}

void TcpConnectionImpl::connectEstablished() {
  // loop_->assertInLoopThread();

  auto thisPtr = shared_from_this();
  loop_->runInLoop([thisPtr]() {
    // LOG_TRACE << "connectEstablished";
    assert(thisPtr->status_ == ConnStatus::Connecting);
    thisPtr->ioChannelPtr_->tie(thisPtr);
    thisPtr->ioChannelPtr_->enableReading();
    thisPtr->status_ = ConnStatus::Connected;
    if (thisPtr->connectionCallback_) thisPtr->connectionCallback_(thisPtr);
  });
}

void TcpConnectionImpl::handleClose() {
  // LOG_TRACE << "connection closed, fd=" << socketPtr_->fd();
  loop_->assertInLoopThread();
  status_ = ConnStatus::Disconnected;
  ioChannelPtr_->disableAll();
  //  ioChannelPtr_->remove();
  auto guardThis = shared_from_this();
  if (connectionCallback_) connectionCallback_(guardThis);
  if (closeCallback_) {
    // LOG_TRACE << "to call close callback";
    closeCallback_(guardThis);
  }
}

void TcpConnectionImpl::handleError() {
  int err = socketPtr_->getSocketError();
  if (err == 0) return;
  if (err == EPIPE || err == EBADMSG ||  // ??? 104=EBADMSG
      err == ECONNRESET) {
    // LOG_TRACE << "[" << name_ << "] - SO_ERROR = " << err << " "
    //           << strerror_tl(err);
  } else {
    // LOG_ERROR << "[" << name_ << "] - SO_ERROR = " << err << " "
    //           << strerror_tl(err);
  }
}

void TcpConnectionImpl::setTcpNoDelay(bool on) {
  socketPtr_->setTcpNoDelay(on);
}

void TcpConnectionImpl::connectDestroyed() {
  loop_->assertInLoopThread();
  if (status_ == ConnStatus::Connected) {
    status_ = ConnStatus::Disconnected;
    ioChannelPtr_->disableAll();

    connectionCallback_(shared_from_this());
  }
  ioChannelPtr_->remove();
}

void TcpConnectionImpl::shutdown() {
  auto thisPtr = shared_from_this();
  loop_->runInLoop([thisPtr]() {
    if (thisPtr->status_ == ConnStatus::Connected) {
      thisPtr->status_ = ConnStatus::Disconnecting;
      if (!thisPtr->ioChannelPtr_->isWriting()) {
        thisPtr->socketPtr_->closeWrite();
      }
    }
  });
}

void TcpConnectionImpl::forceClose() {
  auto thisPtr = shared_from_this();
  loop_->runInLoop([thisPtr]() {
    if (thisPtr->status_ == ConnStatus::Connected ||
        thisPtr->status_ == ConnStatus::Disconnecting) {
      thisPtr->status_ = ConnStatus::Disconnecting;
      thisPtr->handleClose();
    }
  });
}

void TcpConnectionImpl::sendInLoop(const void *buffer, size_t length) {
  loop_->assertInLoopThread();
  if (status_ != ConnStatus::Connected) {
    // LOG_WARN << "Connection is not connected,give up sending";
    return;
  }
  extendLife();
  size_t remainLen = length;
  ssize_t sendLen = 0;
  if (!ioChannelPtr_->isWriting() && writeBufferList_.empty()) {
    // send directly
    sendLen = writeInLoop(buffer, length);
    if (sendLen < 0) {
      // error
      if (errno != EWOULDBLOCK) {
        if (errno == EPIPE || errno == ECONNRESET)  // TODO: any others?
        {
          // LOG_TRACE << "EPIPE or ECONNRESET, errno=" << errno;
          return;
        }
        // LOG_SYSERR << "Unexpected error(" << errno << ")";
        return;
      }
      sendLen = 0;
    }
    remainLen -= sendLen;
  }
  if (remainLen > 0 && status_ == ConnStatus::Connected) {
    if (writeBufferList_.empty()) {
      BufferNodePtr node = std::make_shared<BufferNode>();
      node->msgBuffer_ = std::make_shared<MsgBuffer>();
      writeBufferList_.push_back(std::move(node));
    } else if (writeBufferList_.back()->isFile()) {
      BufferNodePtr node = std::make_shared<BufferNode>();
      node->msgBuffer_ = std::make_shared<MsgBuffer>();
      writeBufferList_.push_back(std::move(node));
    }
    writeBufferList_.back()->msgBuffer_->append(
        static_cast<const char *>(buffer) + sendLen, remainLen);
    if (!ioChannelPtr_->isWriting()) ioChannelPtr_->enableWriting();
    if (highWaterMarkCallback_ &&
        writeBufferList_.back()->msgBuffer_->readableBytes() >
            highWaterMarkLen_) {
      highWaterMarkCallback_(
          shared_from_this(),
          writeBufferList_.back()->msgBuffer_->readableBytes());
    }
  }
}

// The order of data sending should be same as the order of calls of send()
void TcpConnectionImpl::send(const std::shared_ptr<std::string> &msgPtr) {
  if (loop_->isInLoopThread()) {
    std::lock_guard<std::mutex> guard(sendNumMutex_);
    if (sendNum_ == 0) {
      sendInLoop(msgPtr->data(), msgPtr->length());
    } else {
      ++sendNum_;
      auto thisPtr = shared_from_this();
      loop_->queueInLoop([thisPtr, msgPtr]() {
        thisPtr->sendInLoop(msgPtr->data(), msgPtr->length());
        std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
        --thisPtr->sendNum_;
      });
    }
  } else {
    auto thisPtr = shared_from_this();
    std::lock_guard<std::mutex> guard(sendNumMutex_);
    ++sendNum_;
    loop_->queueInLoop([thisPtr, msgPtr]() {
      thisPtr->sendInLoop(msgPtr->data(), msgPtr->length());
      std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
      --thisPtr->sendNum_;
    });
  }
}

// The order of data sending should be same as the order of calls of send()
void TcpConnectionImpl::send(const std::shared_ptr<MsgBuffer> &msgPtr) {
  if (loop_->isInLoopThread()) {
    std::lock_guard<std::mutex> guard(sendNumMutex_);
    if (sendNum_ == 0) {
      sendInLoop(msgPtr->peek(), msgPtr->readableBytes());
    } else {
      ++sendNum_;
      auto thisPtr = shared_from_this();
      loop_->queueInLoop([thisPtr, msgPtr]() {
        thisPtr->sendInLoop(msgPtr->peek(), msgPtr->readableBytes());
        std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
        --thisPtr->sendNum_;
      });
    }
  } else {
    auto thisPtr = shared_from_this();
    std::lock_guard<std::mutex> guard(sendNumMutex_);
    ++sendNum_;
    loop_->queueInLoop([thisPtr, msgPtr]() {
      thisPtr->sendInLoop(msgPtr->peek(), msgPtr->readableBytes());
      std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
      --thisPtr->sendNum_;
    });
  }
}

void TcpConnectionImpl::send(const char *msg, size_t len) {
  if (loop_->isInLoopThread()) {
    std::lock_guard<std::mutex> guard(sendNumMutex_);
    if (sendNum_ == 0) {
      sendInLoop(msg, len);
    } else {
      ++sendNum_;
      auto buffer = std::make_shared<std::string>(msg, len);
      auto thisPtr = shared_from_this();
      loop_->queueInLoop([thisPtr, buffer]() {
        thisPtr->sendInLoop(buffer->data(), buffer->length());
        std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
        --thisPtr->sendNum_;
      });
    }
  } else {
    auto buffer = std::make_shared<std::string>(msg, len);
    auto thisPtr = shared_from_this();
    std::lock_guard<std::mutex> guard(sendNumMutex_);
    ++sendNum_;
    loop_->queueInLoop([thisPtr, buffer]() {
      thisPtr->sendInLoop(buffer->data(), buffer->length());
      std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
      --thisPtr->sendNum_;
    });
  }
}

void TcpConnectionImpl::send(const void *msg, size_t len) {
  if (loop_->isInLoopThread()) {
    std::lock_guard<std::mutex> guard(sendNumMutex_);
    if (sendNum_ == 0) {
      sendInLoop(msg, len);
    } else {
      ++sendNum_;
      auto buffer =
          std::make_shared<std::string>(static_cast<const char *>(msg), len);
      auto thisPtr = shared_from_this();
      loop_->queueInLoop([thisPtr, buffer]() {
        thisPtr->sendInLoop(buffer->data(), buffer->length());
        std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
        --thisPtr->sendNum_;
      });
    }
  } else {
    auto buffer =
        std::make_shared<std::string>(static_cast<const char *>(msg), len);
    auto thisPtr = shared_from_this();
    std::lock_guard<std::mutex> guard(sendNumMutex_);
    ++sendNum_;
    loop_->queueInLoop([thisPtr, buffer]() {
      thisPtr->sendInLoop(buffer->data(), buffer->length());
      std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
      --thisPtr->sendNum_;
    });
  }
}

void TcpConnectionImpl::send(const std::string &msg) {
  if (loop_->isInLoopThread()) {
    std::lock_guard<std::mutex> guard(sendNumMutex_);
    if (sendNum_ == 0) {
      sendInLoop(msg.data(), msg.length());
    } else {
      ++sendNum_;
      auto thisPtr = shared_from_this();
      loop_->queueInLoop([thisPtr, msg]() {
        thisPtr->sendInLoop(msg.data(), msg.length());
        std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
        --thisPtr->sendNum_;
      });
    }
  } else {
    auto thisPtr = shared_from_this();
    std::lock_guard<std::mutex> guard(sendNumMutex_);
    ++sendNum_;
    loop_->queueInLoop([thisPtr, msg]() {
      thisPtr->sendInLoop(msg.data(), msg.length());
      std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
      --thisPtr->sendNum_;
    });
  }
}

void TcpConnectionImpl::send(std::string &&msg) {
  if (loop_->isInLoopThread()) {
    std::lock_guard<std::mutex> guard(sendNumMutex_);
    if (sendNum_ == 0) {
      sendInLoop(msg.data(), msg.length());
    } else {
      auto thisPtr = shared_from_this();
      ++sendNum_;
      loop_->queueInLoop([thisPtr, msg = std::move(msg)]() {
        thisPtr->sendInLoop(msg.data(), msg.length());
        std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
        --thisPtr->sendNum_;
      });
    }
  } else {
    auto thisPtr = shared_from_this();
    std::lock_guard<std::mutex> guard(sendNumMutex_);
    ++sendNum_;
    loop_->queueInLoop([thisPtr, msg = std::move(msg)]() {
      thisPtr->sendInLoop(msg.data(), msg.length());
      std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
      --thisPtr->sendNum_;
    });
  }
}

void TcpConnectionImpl::send(const MsgBuffer &buffer) {
  if (loop_->isInLoopThread()) {
    std::lock_guard<std::mutex> guard(sendNumMutex_);
    if (sendNum_ == 0) {
      sendInLoop(buffer.peek(), buffer.readableBytes());
    } else {
      ++sendNum_;
      auto thisPtr = shared_from_this();
      loop_->queueInLoop([thisPtr, buffer]() {
        thisPtr->sendInLoop(buffer.peek(), buffer.readableBytes());
        std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
        --thisPtr->sendNum_;
      });
    }
  } else {
    auto thisPtr = shared_from_this();
    std::lock_guard<std::mutex> guard(sendNumMutex_);
    ++sendNum_;
    loop_->queueInLoop([thisPtr, buffer]() {
      thisPtr->sendInLoop(buffer.peek(), buffer.readableBytes());
      std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
      --thisPtr->sendNum_;
    });
  }
}

void TcpConnectionImpl::send(MsgBuffer &&buffer) {
  if (loop_->isInLoopThread()) {
    std::lock_guard<std::mutex> guard(sendNumMutex_);
    if (sendNum_ == 0) {
      sendInLoop(buffer.peek(), buffer.readableBytes());
    } else {
      ++sendNum_;
      auto thisPtr = shared_from_this();
      loop_->queueInLoop([thisPtr, buffer = std::move(buffer)]() {
        thisPtr->sendInLoop(buffer.peek(), buffer.readableBytes());
        std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
        --thisPtr->sendNum_;
      });
    }
  } else {
    auto thisPtr = shared_from_this();
    std::lock_guard<std::mutex> guard(sendNumMutex_);
    ++sendNum_;
    loop_->queueInLoop([thisPtr, buffer = std::move(buffer)]() {
      thisPtr->sendInLoop(buffer.peek(), buffer.readableBytes());
      std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
      --thisPtr->sendNum_;
    });
  }
}

void TcpConnectionImpl::sendFile(const char *fileName, size_t offset,
                                 size_t length) {
  assert(fileName);
  int fd = open(fileName, O_RDONLY);

  if (fd < 0) {
    // LOG_SYSERR << fileName << " open error";
    return;
  }

  if (length == 0) {
    struct stat filestat;
    if (stat(fileName, &filestat) < 0) {
      // LOG_SYSERR << fileName << " stat error";
      close(fd);
      return;
    }
    length = filestat.st_size;
  }

  sendFile(fd, offset, length);
}

void TcpConnectionImpl::sendFile(const wchar_t *fileName, size_t offset,
                                 size_t length) {
  assert(fileName);
  sendFile(utils::toNativePath(fileName).c_str(), offset, length);
}

void TcpConnectionImpl::sendFile(int sfd, size_t offset, size_t length) {
  assert(length > 0);
  assert(sfd >= 0);
  BufferNodePtr node = std::make_shared<BufferNode>();
  node->sendFd_ = sfd;
  node->offset_ = static_cast<off_t>(offset);
  node->fileBytesToSend_ = length;
  if (loop_->isInLoopThread()) {
    std::lock_guard<std::mutex> guard(sendNumMutex_);
    if (sendNum_ == 0) {
      writeBufferList_.push_back(node);
      if (writeBufferList_.size() == 1) {
        sendFileInLoop(writeBufferList_.front());
        return;
      }
    } else {
      ++sendNum_;
      auto thisPtr = shared_from_this();
      loop_->queueInLoop([thisPtr, node]() {
        thisPtr->writeBufferList_.push_back(node);
        {
          std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
          --thisPtr->sendNum_;
        }

        if (thisPtr->writeBufferList_.size() == 1) {
          thisPtr->sendFileInLoop(thisPtr->writeBufferList_.front());
        }
      });
    }
  } else {
    auto thisPtr = shared_from_this();
    std::lock_guard<std::mutex> guard(sendNumMutex_);
    ++sendNum_;
    loop_->queueInLoop([thisPtr, node]() {
      // LOG_TRACE << "Push sendfile to list";
      thisPtr->writeBufferList_.push_back(node);
      {
        std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
        --thisPtr->sendNum_;
      }

      if (thisPtr->writeBufferList_.size() == 1) {
        thisPtr->sendFileInLoop(thisPtr->writeBufferList_.front());
      }
    });
  }
}

void TcpConnectionImpl::sendStream(
    std::function<std::size_t(char *, std::size_t)> callback) {
  BufferNodePtr node = std::make_shared<BufferNode>();
  node->offset_ = 0;  // not used, the offset should be handled by the callback
  node->fileBytesToSend_ = 1;  // force to > 0 until stream sent
  node->streamCallback_ = std::move(callback);
  if (loop_->isInLoopThread()) {
    std::lock_guard<std::mutex> guard(sendNumMutex_);
    if (sendNum_ == 0) {
      writeBufferList_.push_back(node);
      if (writeBufferList_.size() == 1) {
        sendFileInLoop(writeBufferList_.front());
        return;
      }
    } else {
      ++sendNum_;
      auto thisPtr = shared_from_this();
      loop_->queueInLoop([thisPtr, node]() {
        thisPtr->writeBufferList_.push_back(node);
        {
          std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
          --thisPtr->sendNum_;
        }

        if (thisPtr->writeBufferList_.size() == 1) {
          thisPtr->sendFileInLoop(thisPtr->writeBufferList_.front());
        }
      });
    }
  } else {
    auto thisPtr = shared_from_this();
    std::lock_guard<std::mutex> guard(sendNumMutex_);
    ++sendNum_;
    loop_->queueInLoop([thisPtr, node]() {
      // LOG_TRACE << "Push sendstream to list";
      thisPtr->writeBufferList_.push_back(node);

      {
        std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
        --thisPtr->sendNum_;
      }

      if (thisPtr->writeBufferList_.size() == 1) {
        thisPtr->sendFileInLoop(thisPtr->writeBufferList_.front());
      }
    });
  }
}

void TcpConnectionImpl::sendFileInLoop(const BufferNodePtr &filePtr) {
  loop_->assertInLoopThread();
  assert(filePtr->isFile());
  if (!isEncrypted_ && !filePtr->streamCallback_) {
    // LOG_TRACE << "send file in loop using linux kernel sendfile()";
    auto bytesSent = sendfile(socketPtr_->fd(), filePtr->sendFd_,
                              &filePtr->offset_, filePtr->fileBytesToSend_);
    if (bytesSent < 0) {
      if (errno != EAGAIN) {
        // LOG_SYSERR << "TcpConnectionImpl::sendFileInLoop";
        if (ioChannelPtr_->isWriting()) ioChannelPtr_->disableWriting();
      }
      return;
    }
    if (bytesSent < filePtr->fileBytesToSend_) {
      if (bytesSent == 0) {
        // LOG_SYSERR << "TcpConnectionImpl::sendFileInLoop";
        return;
      }
    }
    // LOG_TRACE << "sendfile() " << bytesSent << " bytes sent";
    filePtr->fileBytesToSend_ -= bytesSent;
    if (!ioChannelPtr_->isWriting()) {
      ioChannelPtr_->enableWriting();
    }
    return;
  }
  // Send stream
  if (filePtr->streamCallback_) {
    // LOG_TRACE << "send stream in loop";
    if (!fileBufferPtr_) {
      fileBufferPtr_ = std::make_unique<std::vector<char>>();
      fileBufferPtr_->reserve(16 * 1024);
    }
    while ((filePtr->fileBytesToSend_ > 0) || !fileBufferPtr_->empty()) {
      // get next chunk
      if (fileBufferPtr_->empty()) {
        //                LOG_TRACE << "send stream in loop: fetch data
        //                on buffer empty";
        fileBufferPtr_->resize(16 * 1024);
        std::size_t nData;
        nData = filePtr->streamCallback_(fileBufferPtr_->data(),
                                         fileBufferPtr_->size());
        fileBufferPtr_->resize(nData);
        if (nData == 0)  // no more data!
        {
          // LOG_TRACE << "send stream in loop: no more data";
          filePtr->fileBytesToSend_ = 0;
        }
      }
      if (fileBufferPtr_->empty()) {
        // LOG_TRACE << "send stream in loop: break on buffer empty";
        break;
      }
      auto nToWrite = fileBufferPtr_->size();
      auto nWritten = writeInLoop(fileBufferPtr_->data(), nToWrite);
      if (nWritten >= 0) {
        filePtr->nDataWritten_ += nWritten;
        // LOG_TRACE << "send stream in loop: bytes written: " << nWritten
        //           << " / total bytes written: " << filePtr->nDataWritten_;
        if (static_cast<std::size_t>(nWritten) < nToWrite) {
          // Partial write - return and wait for next call to continue
          fileBufferPtr_->erase(fileBufferPtr_->begin(),
                                fileBufferPtr_->begin() + nWritten);
          if (!ioChannelPtr_->isWriting()) ioChannelPtr_->enableWriting();
          // LOG_TRACE << "send stream in loop: return on partial write "
          //              "(socket buffer full?)";
          return;
        }
        //                LOG_TRACE << "send stream in loop: continue on
        //                data written";
        fileBufferPtr_->resize(0);
        continue;
      }
      // nWritten < 0

      if (errno != EWOULDBLOCK) {
        if (errno == EPIPE || errno == ECONNRESET) {
          // LOG_TRACE << "EPIPE or ECONNRESET, errno=" << errno;
          //  abort
          // LOG_TRACE << "send stream in loop: return on connection closed";
          filePtr->fileBytesToSend_ = 0;
          return;
        }
        // TODO: any others?
        // LOG_SYSERR << "send stream in loop: return on unexpected error("
        //           << errno << ")";
        filePtr->fileBytesToSend_ = 0;
        return;
      }
      // Socket buffer full - return and wait for next call
      // LOG_TRACE << "send stream in loop: break on socket buffer full (?)";
      break;
    }
    if (!ioChannelPtr_->isWriting()) ioChannelPtr_->enableWriting();
    // LOG_TRACE << "send stream in loop: return on loop exit";
    return;
  }
  // Send file
  // LOG_TRACE << "send file in loop";
  if (!fileBufferPtr_) {
    fileBufferPtr_ = std::make_unique<std::vector<char>>(16 * 1024);
  }
  lseek(filePtr->sendFd_, filePtr->offset_, SEEK_SET);
  while (filePtr->fileBytesToSend_ > 0) {
    auto n = read(filePtr->sendFd_, &(*fileBufferPtr_)[0],
                  std::min(fileBufferPtr_->size(),
                           static_cast<decltype(fileBufferPtr_->size())>(
                               filePtr->fileBytesToSend_)));
    if (n > 0) {
      auto nSend = writeInLoop(&(*fileBufferPtr_)[0], n);
      if (nSend >= 0) {
        filePtr->fileBytesToSend_ -= nSend;
        filePtr->offset_ += static_cast<off_t>(nSend);
        if (static_cast<size_t>(nSend) < static_cast<size_t>(n)) {
          if (!ioChannelPtr_->isWriting()) {
            ioChannelPtr_->enableWriting();
          }
          // LOG_TRACE << "send file in loop: return on partial write "
          //              "(socket buffer full?)";
          return;
        } else if (nSend == n) {
          //                    LOG_TRACE << "send file in loop:
          //                    continue on data written";
          continue;
        }
      }
      if (nSend < 0) {
        if (errno != EWOULDBLOCK) {
          // TODO: any others?
          if (errno == EPIPE || errno == ECONNRESET) {
            // LOG_TRACE << "EPIPE or ECONNRESET, errno=" << errno;
            // LOG_TRACE << "send file in loop: return on connection closed";
            return;
          }
          // LOG_SYSERR << "send file in loop: return on unexpected error("
          //<< errno << ")";
          return;
        }
        // LOG_TRACE << "send file in loop: break on socket buffer full (?)";
        break;
      }
    }
    if (n < 0) {
      // LOG_SYSERR << "send file in loop: return on read error";
      if (ioChannelPtr_->isWriting()) ioChannelPtr_->disableWriting();
      return;
    }
    if (n == 0) {
      // LOG_SYSERR << "send file in loop: return on read 0 (file truncated)";
      return;
    }
  }
  // LOG_TRACE << "send file in loop: return on loop exit";
  if (!ioChannelPtr_->isWriting()) {
    ioChannelPtr_->enableWriting();
  }
}

ssize_t TcpConnectionImpl::writeInLoop(const void *buffer, size_t length) {
  int nWritten = write(socketPtr_->fd(), buffer, length);
  if (nWritten > 0) bytesSent_ += nWritten;
  return nWritten;
}
