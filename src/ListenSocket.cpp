/**
 * @file     ListenSocket.cpp
 *           
 *
 */

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <strings.h>
#include <cassert>

#include "ListenSocket.hpp"
#include "Epoll.hpp"

namespace pl { namespace net {

ListenSocket::ListenSocket(const std::string &ip, short port)
    : _port(port)
    , _ip(ip)
  {}


ListenSocket::ListenSocket(ListenSocket &&other) 
    :_port(other._port)
    , _listenBacklog(other._listenBacklog)
    , _fd(other._fd)
    , _ip(std::move(other._ip))
    , _eventHandle(other._eventHandle)
{
  other._port = -1;
  other._ip = "";
  other._fd = -1;
}


ListenSocket::~ListenSocket() {
  if (isListening()) 
    this->close();
}

void ListenSocket::debugInfo() const {
  printf("ListenSocket ip=[%s] port=[%d] fd=[%d] isListening=[%s]\n",
         _ip.c_str(), _port, _fd, (isListening())?"true" :"false");
}

int ListenSocket::listen(int backlog, ListenSocket::EventHandler eventHandleFn) {
  _listenBacklog = std::max(1, backlog);
  _eventHandle = eventHandleFn;

  if (isListening()) {
    //TODO develpment{
    //std::assert(true && "已经开始监听了");
    //}
    
    return 0;
  }


  _fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (_fd == -1) {
    postEvent(Event::SOCKET_ERROR);
    return -1;
  }


  struct sockaddr_in servAddr;
  ::bzero(&servAddr, sizeof(servAddr));
  servAddr.sin_family = AF_INET;
  servAddr.sin_addr.s_addr = ::htonl(INADDR_ANY); //TODO _ip
  servAddr.sin_port = ::htons(_port);

  if (::bind(_fd, (struct sockaddr*)&servAddr, sizeof(servAddr))) {
    postEvent(Event::BIND_ERROR);
    return -1;
  }


  if (::listen(_fd, _listenBacklog) == -1) {
    postEvent(Event::LISTEN_ERROR);
    return -1;
  }

  postEvent(Event::LISTEN_OK);


  return 0;
}

ListenSocket::EventHandleRet ListenSocket::postEvent(int code) {
  Event ev(code, this);
  return _eventHandle(ev);
}

void ListenSocket::close() {
  if (not isListening()) {
    //develop
    assert(false && "重复关闭listen socket");

    return;
  }

again:
  if (-1 == ::close(_fd)) {
    if (errno == EINTR)
      goto again;

    postEvent(Event::CLOSE_ERROR);
    return;
  }

  _fd = -1;
  postEvent(Event::LISTEN_CLOSED);
}


}} // namespace pl::net 
