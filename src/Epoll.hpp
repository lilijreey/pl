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
  Epoll& operator=(const Epoll&) = delete;


  bool isOpening() const {return _epfd != -1;}
  void setETMode() {_isETMode = true;}

  //添加一个listen Socket 添加后listenSocket的所有全被转移到Epoll中管理
  //调用后实参不可以使用
  

  template <typename T> //cache is Epollable
  int addEpollable(T &);
 




  //关闭所有监听的fd, 释放所有资源
  void close(); 

  //TODO set ticks

  //int removefd(int fd);

  //TODO 资源管理 ???
  //ctl()
  //
  
  //ms 
  void wait_loop(int size);
  //epoll_wait()
  

  void stopLoop() { _isLoop = false;}
  


 private:
  void postEvent(const Event &);
  int addEpollable(int fd, int epollCtlMod, int type, void *obj);

 private:
  bool _isETMode = false;
  bool _isLoop = false;
  int _flags =0;
  int _epfd = -1;
  int _ticks = -1; //每次wait的超时ms

  int _eventsBufSize=0;
  struct epoll_event *_eventBuf=nullptr;

  //std::unordered_map<int[> fd <], struct epoll_event> _fds;
  std::unordered_map<int/* listenfd */, 
                     std::pair<int/* type */ void /* thing*/>
                    > _things; 

};


template <typename T>
int Epoll::addSocket(T &epollable) {
  T *thing= new T(std::move(epollable));
  printf("move new epollable\n");
  thing->debugInfo();
  printf("old epollable\n");
  thing.debugInfo();

  //TODO development {
  assert(_things.count(socket->getFd()) == 0 && "重复加入");
  //}
  

  return addEpollable(thing->getFd(), EPOLLIN, thing);

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

}} // namespace pl::os
