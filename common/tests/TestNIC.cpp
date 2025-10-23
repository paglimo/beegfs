#include "common/net/sock/IPAddress.h"
#include <common/net/sock/NetworkInterfaceCard.h>

#include <gtest/gtest.h>

class TestNIC : public ::testing::Test
{
};

TEST_F(TestNIC, testFindNICs)
{
   NicAddressList nics;
   StringList allowedInterfaces;
   size_t count = 0;

   ASSERT_TRUE(NetworkInterfaceCard::findAll(&allowedInterfaces, true, false, &nics));
   for (auto iter = nics.begin(); iter != nics.end(); ++iter)
   {
      count++;
      ASSERT_TRUE(strlen(iter->name) > 0);
      ASSERT_FALSE(iter->ipAddr.isLoopback());
      ASSERT_TRUE(iter->nicType == NICADDRTYPE_STANDARD || iter->nicType == NICADDRTYPE_RDMA);
      ASSERT_TRUE(iter->protocol == 4 || iter->protocol == 6);
   }

   ASSERT_TRUE(count > 0);
}

TEST_F(TestNIC, testFindNicPosition)
{
   auto addrv4 = IPAddress::resolve("192.168.0.1");
   auto addrv6 = IPAddress::resolve("fd00:beee:beee::1");

   auto naddr_v4_std_eth0 =
      NicAddress{addrv4, NICADDRTYPE_STANDARD, "eth0"};
   auto naddr_v6_std_eth0 =
      NicAddress{addrv6, NICADDRTYPE_STANDARD, "eth0"};
   auto naddr_v4_rdma_eth0 =
      NicAddress{addrv4, NICADDRTYPE_RDMA, "eth0"};
   auto naddr_v6_rdma_eth0 =
      NicAddress{addrv6, NICADDRTYPE_RDMA, "eth0"};
   auto naddr_v6_std_eth1 =
      NicAddress{addrv6, NICADDRTYPE_STANDARD, "eth1"};

   {
      // Default
      StringList p {"*"};
      ASSERT_EQ(findNicPosition(p, naddr_v4_std_eth0), 0);
      ASSERT_EQ(findNicPosition(p, naddr_v6_std_eth0), 0);
      ASSERT_EQ(findNicPosition(p, naddr_v4_rdma_eth0), 0);
      ASSERT_EQ(findNicPosition(p, naddr_v6_rdma_eth0), 0);
      ASSERT_EQ(findNicPosition(p, naddr_v6_std_eth1), 0);
   }

   {
      // Prefer ipv6
      StringList p {"* * 6", "* * 4"};
      ASSERT_EQ(findNicPosition(p, naddr_v4_std_eth0), 1);
      ASSERT_EQ(findNicPosition(p, naddr_v6_std_eth0), 0);
      ASSERT_EQ(findNicPosition(p, naddr_v4_rdma_eth0), 1);
      ASSERT_EQ(findNicPosition(p, naddr_v6_rdma_eth0), 0);
      ASSERT_EQ(findNicPosition(p, naddr_v6_std_eth1), 0);
   }

   {
      // Prefer eth1, then any rdma, deny anything else
      StringList p {"eth1" , "* * * rdma"};
      ASSERT_EQ(findNicPosition(p, naddr_v4_std_eth0), -1);
      ASSERT_EQ(findNicPosition(p, naddr_v6_std_eth0), -1);
      ASSERT_EQ(findNicPosition(p, naddr_v4_rdma_eth0), 1);
      ASSERT_EQ(findNicPosition(p, naddr_v6_rdma_eth0), 1);
      ASSERT_EQ(findNicPosition(p, naddr_v6_std_eth1), 0);
   }

   {
      // Prefer 192.168.0.1 + rdma, same + standard
      StringList p {"* 192.168.0.1 * rdma", "* 192.168.0.1 * tcp", "*"};
      ASSERT_EQ(findNicPosition(p, naddr_v4_std_eth0), 1);
      ASSERT_EQ(findNicPosition(p, naddr_v6_std_eth0), 2);
      ASSERT_EQ(findNicPosition(p, naddr_v4_rdma_eth0), 0);
      ASSERT_EQ(findNicPosition(p, naddr_v6_rdma_eth0), 2);
      ASSERT_EQ(findNicPosition(p, naddr_v6_std_eth1), 2);
   }

   {
      // Deny rdma
      StringList p {"! * * * rdma", "*"};
      ASSERT_EQ(findNicPosition(p, naddr_v4_std_eth0), 1);
      ASSERT_EQ(findNicPosition(p, naddr_v6_std_eth0), 1);
      ASSERT_EQ(findNicPosition(p, naddr_v4_rdma_eth0), -1);
      ASSERT_EQ(findNicPosition(p, naddr_v6_rdma_eth0), -1);
      ASSERT_EQ(findNicPosition(p, naddr_v6_std_eth1), 1);
   }

   {
      // Deny eth2, deny ipv6
      StringList p {"! eth2", "! * * 6", "*"};
      ASSERT_EQ(findNicPosition(p, naddr_v4_std_eth0), 2);
      ASSERT_EQ(findNicPosition(p, naddr_v6_std_eth0), -1);
      ASSERT_EQ(findNicPosition(p, naddr_v4_rdma_eth0), 2);
      ASSERT_EQ(findNicPosition(p, naddr_v6_rdma_eth0), -1);
      ASSERT_EQ(findNicPosition(p, naddr_v6_std_eth1), -1);
   }
}
