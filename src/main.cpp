/**
 * @file     main.cpp
 *           
 */
#include <stdio.h>
#include "ListenSocket.hpp"
#include "Epoll.hpp"

using namespace pl::net;
using namespace pl::os;

int main() {
  ListenSocket l(3333);
  l.listen([](const ListenSocket::Event &ev) { 
    switch (ev._code) { //TODO ev.code()
    case ListenSocket::Event::SOCKET_ERROR:
      perror("socket() failed");
      return ListenSocket::close_listen;

    case ListenSocket::Event::BIND_ERROR:
      perror("bind() failed");
      return ListenSocket::close_listen;

    case ListenSocket::Event::LISTEN_ERROR:
      perror("listen() failed");
      return ListenSocket::close_listen;

    case ListenSocket::Event::LISTEN_OK:
       printf("server listen %d listening...\n", ev._socket->getListenPort());
       break;

    case ListenSocket::Event::CLOSE_ERROR:
       perror("close() failed");
       break;

    case ListenSocket::Event::LISTEN_CLOSED:
       printf("server listen %d close listening\n", ev._socket->getListenPort());
       break;


    default:
       printf("一个忽略的事件 %d\n", ev._code);
       break;
    };

    return ListenSocket::ok;
    });



  Epoll epoll;
  epoll.addSocket(l);
  printf("addSocket\n");
  //epoll.loop([epoll]() {

  //});



  //eng.regEvent()

  //eng.wait()




}

