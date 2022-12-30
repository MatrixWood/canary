#include <EventLoopThread.h>
#include <TcpServer.h>

#include <iostream>
#include <string>

using namespace canary;

#define USE_IPV6 0

int main() {
  EventLoopThread loopThread;
  loopThread.run();
  InetAddress addr(8888);
  TcpServer server(loopThread.getLoop(), addr, "test");
  server.setRecvMessageCallback(
      [](const TcpConnectionPtr &connectionPtr, MsgBuffer *buffer) {
        std::cout << std::string(buffer->peek(), buffer->readableBytes());
        connectionPtr->send(buffer->peek(), buffer->readableBytes());
        buffer->retrieveAll();
        // connectionPtr->forceClose();
      });
  server.setConnectionCallback([](const TcpConnectionPtr &connPtr) {
    if (connPtr->connected()) {
      std::cout << "New connection" << std::endl;
    } else if (connPtr->disconnected()) {
      std::cout << "connection disconnected" << std::endl;
    }
  });
  server.setIoLoopNum(3);
  server.start();
  loopThread.wait();
}
