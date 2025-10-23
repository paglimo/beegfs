#ifndef NETFILTER_H_
#define NETFILTER_H_

#include <common/Common.h>
#include <linux/in.h>
#include <linux/inet.h>

typedef struct NetFilterEntry NetFilterEntry;
struct NetFilterEntry
{
   struct in6_addr mask;
   struct in6_addr compare;  // pre-masked
};

typedef struct NetFilter NetFilter;
struct NetFilter
{
   NetFilterEntry* filterArray;
   size_t filterArrayLen;
};

static inline size_t NetFilter_getNumFilterEntries(NetFilter* this)
{
   return this->filterArrayLen;
}

__must_check bool NetFilter_init(NetFilter* this, const char* filename);
NetFilter* NetFilter_construct(const char* filename);
void NetFilter_uninit(NetFilter* this);
void NetFilter_destruct(NetFilter* this);
bool NetFilter_isAllowed(NetFilter* this, struct in6_addr ipAddr);
bool NetFilter_isContained(NetFilter* this, struct in6_addr addr);

#endif /* NETFILTER_H_ */
