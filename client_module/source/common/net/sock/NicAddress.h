#ifndef NICADDRESS_H_
#define NICADDRESS_H_

#include <common/Common.h>
#include <linux/if.h>


enum NicAddrType;
typedef enum NicAddrType NicAddrType_t;

struct NicAddress;
typedef struct NicAddress NicAddress;

struct NicListCapabilities;
typedef struct NicListCapabilities NicListCapabilities;

struct ib_device;

// inliners
static inline bool NicAddress_equals(NicAddress* this, NicAddress* other);


enum NicAddrType
{
   NICADDRTYPE_STANDARD = 0,
   // removed: NICADDRTYPE_SDP = 1,
   NICADDRTYPE_RDMA = 2
};

struct NicAddress
{
   struct in6_addr ipAddr;
   NicAddrType_t     nicType;
   char              name[IFNAMSIZ];
#ifdef BEEGFS_RDMA
   struct ib_device *ibdev;
#endif
};

struct NicListCapabilities
{
   bool supportsRDMA;
};


bool NicAddress_equals(NicAddress* this, NicAddress* other)
{
   return ipv6_addr_equal(&this->ipAddr, &other->ipAddr) &&
      (this->nicType == other->nicType) &&
      !strncmp(this->name, other->name, IFNAMSIZ);
}

#endif /*NICADDRESS_H_*/
