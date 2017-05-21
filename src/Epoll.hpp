#pragma once

/** 
 * Linux epoll wapper
 */


#include <errno.h>
#include <sys/epoll.h>
#include <cassert>
#include <unordered_map>
#include "ListenSocket.hpp"

namespace pl { namespace os {


class Epoll {
 public:
  enum  //Event Codes
  {
    CREATE_ERROR          , //epoll_create1() error
    ADDFD_ERROR           ,
    EPOLL_CTL_ADD_ERROR   ,
    ADD_LISTEN_SOCKET     ,
  };


 public:
  /// 触发事件类型
  struct Event {
    Event(short code, int fd=-1) 
        : _code(code)
        , _fd(fd)
        , _errno(errno)
    { }

    const char * name() const { 
      static const char * names[] ={
        "CREATE_ERROR",
        "ADDFD_ERROR",     
        "EPOLL_CTL_ADD_ERROR",
        "ADD_LISTEN_SOCKET" ,
      };


      return names[_code]; 
    }


    short _code;
    int _fd; //关联的fd;
    int _errno; //错误
    const char * _name = nullptr;
    //const char * const _msg;
  };



 public:
  //same as epoll_create1(flags)
  Epoll(int flags=0, bool isETMode=false);
  ~Epoll();

  Epoll(const Epoll&) = delete;
  Epoll& operator(const Epoll&) = delete;


  void isOpening() const {return _epfd != -1;}
  void setETMode() {_isETMode = true;}

  //添加一个listen Socket 添加后listenSocket的所有全被转移到Epoll中管理
  //调用后实参不可以使用
  int addSocket(pl::net::ListenSocket &listenSock);

  //关闭所有监听的fd, 释放所有资源
  void close(); 

  //int removefd(int fd);

  //TODO 资源管理 ???
  //ctl()
  //
  
  //epoll_wait()
  

 private:
  void postEvent(const Event &);

 private:
  int _flags =0;
  int _epfd = -1;
  bool _isETMode = false;

  //std::unordered_map<int[> fd <], struct epoll_event> _fds;
  std::unordered_map<int/* listenfd */, 
                     std::pair<struct epoll_event, pl::net::ListenSocket*>
                    >  _listenSockts;

};

}} // namespace pl::os
