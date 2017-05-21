#pragma once

/**
 * @file     ListenSocket.hpp
 *  
 *  wauper a listen socket          
 *
 */


#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string>
#include <functional>


namespace pl { namespace net {


//TODO IPv4, IPv6
//TODO rename TcpListenSocket
class ListenSocket : {
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


      NEW_CONNECTION, //aceept ok
      ACCEPT_ERROR, //aceept() error
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

  //TODO maxConnections 最大连接数

  //TODO 支持传入一个对象数据驱动形式
  //listen(EventHandler)
  
  //TODO reference
  int listen(int backlog, EventHandler eventHandler);


  void setEventHanndl();


  //TODO
  //void unattchEpoll();
  
  bool isListening() const {return _fd != -1;}
  bool isMoved() const {return _port == -1; }
  short getListenPort() const {return _port;}
  int getFd() const { return _fd;}

  //void setListenBacklog(int backlog) {_listenBacklog = backlog ;}
  int getListenBacklog() const { return _listenBacklog; }

  //TOOD set connect socketOpt
  //setListenSocketOpt



  void debugInfo() const; 

  void close();

  
  //same as accept(2)
  void accept();

  //设置监听socket为非阻塞模式
  ListenSocket& setnonblock();

  //设置accept返回的socket为非阻塞模式
  ListenSocket& setclientnonblock();
  //void isnonblock() const 
  
  int getNewConnectFd() const {return _newConnectFd;}
  struct sockaddr_in getNewConnectAddr() const {return _newConnectAddr; }
  socklen_t getNewConnectAddrLen() const {return _newConnectAddrLen;}

  uint32_t getHisConnects() const {return _hisConnects;}

 private:
  EventHandleRet postEvent(int code);


 private:
  enum ServerState { };

  bool _isClientNonblock = false;
  short _port{};
  int _listenBacklog = 5;
  int _fd = -1;
  std::string _ip;

  int _newConnectFd = -1;
  struct sockaddr_in _newConnectAddr;
  socklen_t _newConnectAddrLen;

  EventHandler _eventHandle;


 private:
  //TODO 历史链接数量
  uint32_t _hisConnects= 0;
  //TODO 当前的链接数量
  //错误数量
  //TODO serverInfo


};

}} // namespace pl::net 
