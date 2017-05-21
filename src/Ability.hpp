/**
 * @file     Ability.hpp
 *           
 *
 * 定义各种能力接口
 */


#pragma once

namespace pl::net {

//具有加入epoll的能力, 需要epoll event
//继承的此类需要实现 handleEpollEv(const struct epoll_event *) 接口
//TODO 使用模板做到能够检查是否定义对应函数
struct Epollable {
  ///void handleEpollEv(const struct epoll_event *);//
  ///int getFD()
};

struct DebugInfoable {
  ///void debugInfo

}
