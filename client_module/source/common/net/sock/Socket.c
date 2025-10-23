#include <common/toolkit/SocketTk.h>
#include <common/net/sock/Socket.h>
#include <common/threading/Thread.h>
#include <linux/in.h>

void _Socket_init(Socket* this)
{
   memset(this, 0, sizeof(*this) );

   this->sockType = NICADDRTYPE_STANDARD;
   this->boundPort = -1;
}

void _Socket_uninit(Socket* this)
{
}


bool Socket_bind(Socket* this, unsigned short port)
{
   // binding to "in6addr_any" should also bind to IPv4 mapped addresses
   struct in6_addr ipAddr = in6addr_any;
   return this->ops->bindToAddr(this, ipAddr, port);
}

bool Socket_bindToAddr(Socket* this, struct in6_addr ipAddr, unsigned short port)
{
   return this->ops->bindToAddr(this, ipAddr, port);
}
