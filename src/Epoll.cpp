/**
 * @file     Epoll.cpp
 *           
 *
 */


#include <cassert>
#include "Epoll.hpp"

namespace pl { namespace os {


Epoll::Epoll(int flags, bool isETMode ) 
    : _isETMode(isETMode)
    , _flags(flags)
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




void Epoll::postEvent(const Event &) {
  //TODO

}

void Epoll::close() {
  //TODO
}


void Epoll::wait_loop(int size) {
  _eventBuf = (struct epoll_event*)realloc(_eventBuf, size * sizeof(struct epoll_event));
  assert(_eventBuf);
  _eventsBufSize = size;
  //TODO 动态负载

  _isLoop = true;
  int nfds;
  while (_isLoop) {
    nfds= epoll_wait(_epfd, _eventBuf, _eventsBufSize, _ticks);
    if (nfds == -1 ) { //TODO likely
      if (errno == EINTR)
        continue;

      stopLoop();
      //TODO post Error
      return ;
    }

    for (int i=0; i < nfds; ++i) {
      HandlEpollable *ev = (HandlEpollable*)_eventBuf[i].data.ptr;
      //TODO retrun
      ev->handleEpollEv(&_eventBuf[i]);
    }
  }
}



}} // namespace pl::os
