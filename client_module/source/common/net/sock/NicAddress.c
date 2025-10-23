#include <common/net/sock/NicAddress.h>
#include <common/toolkit/Serialization.h>

/**
 * Compares the preference of NICs
 * returns: -1 if lhs goes first, 1 if rhs goes first, 0 if equal
 */
int NicAddress_preferenceComp(const NicAddress* lhs, const NicAddress* rhs)
{
   // prefer RDMA NICs
   {
      int a = (lhs->nicType == NICADDRTYPE_RDMA);
      int b = (rhs->nicType == NICADDRTYPE_RDMA);
      if (a != b)
         return b - a;
   }

   // prefer IPv4 over IPv6
   {
      int a = ipv6_addr_v4mapped(&lhs->ipAddr);
      int b = ipv6_addr_v4mapped(&rhs->ipAddr);
      if (a != b)
         return b - a;
   }

   // byte-by-byte comparison.
   return memcmp(&lhs->ipAddr, &rhs->ipAddr, sizeof lhs->ipAddr);
}
