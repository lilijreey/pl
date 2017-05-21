#pragma once

/**
 * @file     ListenSocket.hpp
 *  
 *  wauper a listen socket          
 *
 */


#include <string>
#include <functional>


namespace pl { namespace net {


//TODO IPv4, IPv6
//TODO rename TcpListenSocket
class ListenSocket {
 public:
  /// 触发事件类型
  struct Event {
    Event(short code, ListenSocket *socket)
        :_code(code), _socket(socket)
    {}

    short _code;
    ListenSocket * _socket;

    enum {
      SOCKET_ERROR, //socket() error
      BIND_ERROR, //bind() error
      LISTEN_ERROR, //listen()  error
      ATTACH_EPOLL_ERROR, //listenfd 添加到epoll出错

      LISTEN_OK,
      CLOSE_ERROR,
      LISTEN_CLOSED, // after close(_socketfd)
    };

  };


  //Evethandle 可以返回的值
  enum class EventHandleRet {
    OK = 0,
    CLOST_LISTEN //返回后会关闭listen
  };

  static const EventHandleRet ok           = EventHandleRet::OK;
  static const EventHandleRet close_listen = EventHandleRet::CLOST_LISTEN;

  //事件处理函数类型
  using EventHandler  = std::function<EventHandleRet(const Event &)>;
  
  
 public:
  ///创建一个Tcp Server 对象
  ListenSocket(short port) : ListenSocket("", port)
  {}
  ListenSocket(const std::string &ip, short port);

  //所有权的迁移
  ListenSocket(ListenSocket &&other);



  ~ListenSocket();

  ListenSocket(const ListenSocket&) = delete;
  ListenSocket& operator=(const ListenSocket&) = delete;

  //int eventHandler(Event ev)  处理所有可能出发的事件，如果返回-1
  //则会关闭ListenSocket  listen

  ///开始监听
  int listen(EventHandler eventHandler) {
    return listen(5, eventHandler);
  }

  //TODO 支持传入一个对象数据驱动形式
  //listen(EventHandler)
  
  //TODO reference
  int listen(int backlog, EventHandler eventHandler);

  void setEventHanndl();


  //TODO
  //void unattchEpoll();
  
  bool isListening() const {return _fd != -1;}
  short getListenPort() const {return _port;}
  int getFd() const { return _fd;}

  //void setListenBacklog(int backlog) {_listenBacklog = backlog ;}
  int getListenBacklog() const { return _listenBacklog; }

  //TOOD set connect socketOpt
  //setListenSocketOpt


  void debugInfo() const; 

  void close();
  

 private:
  EventHandleRet postEvent(int code);


 private:
  enum ServerState { };

  short _port{};
  int _listenBacklog = 5;
  int _fd = -1;
  std::string _ip;

  EventHandler _eventHandle;


 private:
  //TODO serverInfo


};

}} // namespace pl::net 
