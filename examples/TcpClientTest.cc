#include <EventLoopThread.h>
#include <TcpClient.h>

#include <atomic>
#include <iostream>
#include <string>

using namespace canary;

#define USE_IPV6 0

int main() {
  EventLoop loop;

  InetAddress serverAddr("127.0.0.1", 8888);

  std::shared_ptr<TcpClient> client[10];
  std::atomic_int connCount;
  connCount = 10;
  for (int i = 0; i < 10; ++i) {
    client[i] = std::make_shared<TcpClient>(&loop, serverAddr, "tcpclienttest");
    client[i]->setConnectionCallback(
        [i, &loop, &connCount](const TcpConnectionPtr &conn) {
          if (conn->connected()) {
            std::cout << i << " connected!" << std::endl;
            char tmp[20];
            sprintf(tmp, "%d client!!", i);
            conn->send(tmp);
          } else {
            std::cout << i << " disconnected" << std::endl;
            --connCount;
            if (connCount == 0) loop.quit();
          }
        });
    client[i]->setMessageCallback([](const TcpConnectionPtr &conn,
                                     MsgBuffer *buf) {
      std::cout << std::string(buf->peek(), buf->readableBytes()) << std::endl;
      buf->retrieveAll();
      conn->shutdown();
    });
    client[i]->connect();
  }
  loop.loop();
}
