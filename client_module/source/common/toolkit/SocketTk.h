#ifndef OPEN_SOCKETTK_H_
#define OPEN_SOCKETTK_H_

#include <common/Common.h>
#include <common/net/sock/Socket.h>
#include <common/toolkit/Time.h>
#include <linux/poll.h>

// forward declarations
struct PollState;
typedef struct PollState PollState;


bool SocketTk_initOnce(void);
void SocketTk_uninitOnce(void);

int SocketTk_poll(PollState* state, int timeoutMS);

bool SocketTk_getHostByAddrStr(const char* hostAddr, struct in6_addr *outIPAddr);
struct in6_addr SocketTk_in_aton(const char* hostAddr);

struct PollState
{
   struct list_head list;
};

static inline void PollState_init(PollState* state)
{
   INIT_LIST_HEAD(&state->list);
}

static inline void PollState_addSocket(PollState* state, Socket* socket, short events)
{
   list_add_tail(&socket->poll._list, &state->list);
   socket->poll._events = events;
   socket->poll.revents = 0;
}

// if possible give buffer of length SOCKET_IPADDRSTR_LEN
int SocketTk_ipaddrToStr(char *buf, size_t size, struct in6_addr ipaddress);
int SocketTk_endpointToStr(char *buf, size_t size, struct in6_addr ipaddress, unsigned short port);

static inline const char *Socket_formatAddrOrPeername(struct sockaddr_in6 *addr, Socket *sock)
{
   // Helper function for a few code locations that want to print either the
   // source address, or the peername if there's no explicit source address
   // (probably there was a message received via recv() instead of recvfrom(),
   // so the source address is what the socket is connected to).
   //
   // We use the temp buffer in the socket to provide a simple API. No
   // concurrency possible!
   //
   if (addr)
      SocketTk_ipaddrToStr(sock->temp_format_buffer, sizeof sock->temp_format_buffer, addr->sin6_addr);
   else
      scnprintf(sock->temp_format_buffer, sizeof sock->temp_format_buffer, "%s", sock->peername);
   return sock->temp_format_buffer;
}

#endif /*OPEN_SOCKETTK_H_*/
