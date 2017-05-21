
#pragma once
/**
 * @file     Util.hpp
 *           
 *
 * @author   lili <lilijreey@gmail.com>
 * @date     05/21/2017 05:46:28 AM
 *
 */

#include <unistd.h>
#include <fcntl.h>
#include <cassert>

namespace pl { namespace os { 

inline static 
int setnonblock(int socket)
{
    int flags;

    flags = fcntl(socket, F_GETFL, 0);
    assert(flags != -1);
    flags |= O_NONBLOCK;
    flags |= O_NDELAY;
    return ::fcntl(socket, F_SETFL, flags);
}


}} // namespace pl::os
