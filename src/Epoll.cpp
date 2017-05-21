/**
 * @file     Epoll.cpp
 *           
 *
 */


#include <cassert>
#include "Epoll.hpp"

namespace pl { namespace os {


Epoll::Epoll(int flags, bool isETMode ) 
    :_flags(flags)
    ,_isETMode(isETMode)
{

  _epfd = epoll_create1(_flags);
  if (_epfd == -1) 
    postEvent(CREATE_ERROR);
}

Epoll::~Epoll() {
  if (isOpening()) 
    close();
}


//int Epoll::addfd(int fd) {
//}

int Epoll::addSocket(pl::net::ListenSocket &listenSock) {
  pl::net::ListenSocket *socket = new pl::net::ListenSocket(std::move(listenSock));
  printf("move new socket\n");
  socket->debugInfo();
  printf("old socket\n");
  listenSock.debugInfo();

  //TODO development {
  assert(_listenSockts.count(socket->getFd()) == 0 && "重复加入");
  //}
  
  epoll_event epEv;

  epEv.events = EPOLLIN | (_isETMode ? EPOLLET : 0);
  epEv.data.ptr = socket;

  if (-1 == epoll_ctl(_epfd, EPOLL_CTL_ADD, socket->getFd(), &epEv)) {
    postEvent(EPOLL_CTL_ADD_ERROR);
    return -1;
  }

  _listenSockts.insert({socket->getFd(), {epEv, socket}});
  postEvent(ADD_LISTEN_SOCKET);


  return 0;
}



void Epoll::postEvent(const Event &) {

}

void close() {
  //TODO
}





}} // namespace pl::os
