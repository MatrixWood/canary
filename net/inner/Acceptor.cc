#include "Acceptor.h"

using namespace canary;

Acceptor::Acceptor(EventLoop *loop, const InetAddress &addr, bool reUseAddr,
                   bool reUsePort)

    : idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC)),
      sock_(
          Socket::createNonblockingSocketOrDie(addr.getSockAddr()->sa_family)),
      addr_(addr),
      loop_(loop),
      acceptChannel_(loop, sock_.fd()) {
  sock_.setReuseAddr(reUseAddr);
  sock_.setReusePort(reUsePort);
  sock_.bindAddress(addr_);
  acceptChannel_.setReadCallback(std::bind(&Acceptor::readCallback, this));
  if (addr_.toPort() == 0) {
    addr_ = InetAddress{Socket::getLocalAddr(sock_.fd())};
  }
}

Acceptor::~Acceptor() {
  acceptChannel_.disableAll();
  acceptChannel_.remove();
  ::close(idleFd_);
}

void Acceptor::listen() {
  loop_->assertInLoopThread();
  sock_.listen();
  acceptChannel_.enableReading();
}

void Acceptor::readCallback() {
  InetAddress peer;
  int newsock = sock_.accept(&peer);
  if (newsock >= 0) {
    if (newConnectionCallback_) {
      newConnectionCallback_(newsock, peer);
    } else {
      ::close(newsock);
    }
  } else {
    // LOG_SYSERR << "Accpetor::readCallback";

    if (errno == EMFILE) {
      ::close(idleFd_);
      idleFd_ = sock_.accept(&peer);
      ::close(idleFd_);
      idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    }
  }
}
