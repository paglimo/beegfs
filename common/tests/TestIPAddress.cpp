
#include <unordered_map>
#include <common/net/sock/IPAddress.h>
#include <gtest/gtest.h>


typedef std::unordered_map<IPAddress, std::size_t, std::hash<IPAddress>> AddrMap;
typedef std::unordered_map<std::size_t, IPAddress> IdMap;

struct IPAddrTestResult
{
      const char* name;
      const char* tostring;
      bool shouldParse;
      bool isZero;
      bool isIPv4;
      bool isIPv6;
      bool isLoopback;
      bool isLinkLocal;      
};

static struct IPAddrTestResult const __TESTIPADDRESS_RESULTS []  =
{
   {"127.0.0.1", NULL, true, false, true, false, true, false},
   {"0.0.0.0", NULL, true, false, true, false, false, false},
   {"::", NULL, true, true, false, true, false, false},
   {"::0", "::", true, true, false, true, false, false},
   {"0:0:0:0:0::0", "::", true, true, false, true, false, false},
   {"::1", NULL, true, false, false, true, true, false},
   {"10.11.12.13", NULL, true, false, true, false, false, false},
   {"::ffff:10.11.12.13", "10.11.12.13", true, false, true, false, false, false},
   {"fe80::63f:7203:fc:2200", NULL, true, false, false, true, false, true},
   {"fda7:cdff:34cd:f002:1234:6789:bee6:f506", NULL, true, false, false, true, false, false},
   {"invalid*&_'", NULL, false, false, false, false, false, false},
   {NULL}
};

class TestIPAddress : public ::testing::Test
{
};

TEST_F(TestIPAddress, testIPs)
{
   for (const struct IPAddrTestResult* res = __TESTIPADDRESS_RESULTS; res->name != NULL; ++res)
   {
      IPAddress a;
      if (res->shouldParse) {
         EXPECT_NO_THROW(
            a = IPAddress::resolve(res->name);
         );
      } else {
         EXPECT_ANY_THROW(
            a = IPAddress::resolve(res->name);
         );
      }
         
      if (res->shouldParse)
      {
         EXPECT_EQ(res->isIPv4, a.isIPv4()) << res->name;
         EXPECT_EQ(res->isIPv6, a.isIPv6()) << res->name;
         EXPECT_EQ(res->isLoopback, a.isLoopback()) << res->name;
         EXPECT_EQ(res->isLinkLocal, a.isLinkLocal()) << res->name;
         EXPECT_EQ(true, a.equals(a)) << res->name;
         const char* expstr = res->tostring ? res->tostring : res->name;
         EXPECT_EQ(true, a.toString() == std::string(expstr)) << a.toString() << "/" << expstr;
         IPAddress b = a;
         EXPECT_EQ(true, b.equals(a)) << res->name;
         EXPECT_EQ(res->isZero, a.equals(IPAddress())) << res->name;
      }
   }
}

TEST_F(TestIPAddress, testDefaultCtor)
{
   IPAddress a;

   EXPECT_EQ(false, a.isIPv4());
   EXPECT_EQ(true, a.isIPv6());
   EXPECT_EQ(false, a.isLoopback());
   EXPECT_EQ(false, a.isLinkLocal());
}

TEST_F(TestIPAddress, testLoopBacksIPv4IPv6NotEqual)
{
   auto a = IPAddress::resolve("127.0.0.1");
   auto b = IPAddress::resolve("::1");

   EXPECT_EQ(false, a.equals(b));
}

TEST_F(TestIPAddress, testIPv4LoopbackNotEqualAll)
{
   auto a = IPAddress::resolve("127.0.0.1");
   auto b = IPAddress::resolve("0.0.0.0");

   EXPECT_EQ(false, a.equals(b));
}

TEST_F(TestIPAddress, testIPv6LoopbackNotEqualAll)
{
   auto a = IPAddress::resolve("::1");
   auto b = IPAddress::resolve("::");

   EXPECT_EQ(false, a.equals(b));
}

TEST_F(TestIPAddress, testMap)
{
   AddrMap addrMap;
   IdMap idMap;

   for (const struct IPAddrTestResult* res = __TESTIPADDRESS_RESULTS; res->name != NULL; ++res)
   {
      if(!res->shouldParse)
         continue;

      // :: and ::0 are equal in binary
      if (!strcmp(res->name, "::"))
      {
         auto a = IPAddress::resolve(res->name);
         std::size_t id = idMap.size();
         addrMap[a] = id;
         idMap[id] = a;
      }
   }

   for (auto iter = addrMap.begin(); iter != addrMap.end(); iter++)
   {
      const IPAddress& a = iter->first;
      std::size_t id = iter->second;
      EXPECT_EQ(a, idMap[id]);
      idMap.erase(id);
   }

   EXPECT_EQ(true, idMap.empty());
}

TEST_F(TestIPAddress, testMappedIPv4AddressEquality)
{
   std::string addr = "192.168.122.10";

   auto v4 = IPAddress::resolve(addr);
   EXPECT_EQ(true, v4.isIPv4());
   EXPECT_EQ(false, v4.isIPv6());

   auto v4w = IPAddress::resolve("::ffff:" + addr);
   EXPECT_EQ(true, v4w.isIPv4());
   EXPECT_EQ(false, v4w.isIPv6());

   EXPECT_EQ(true, v4.equals(v4w));
   EXPECT_EQ(true, v4w.data() == v4.data());
}

TEST_F(TestIPAddress, testIPv4ToBinaryIPv4Broadcast)
{
   auto v4 = IPAddress::resolve("255.255.255.255");
   EXPECT_EQ(true, v4.isIPv4());
   EXPECT_EQ(0xfffffffful, v4.toIPv4InAddrT());
}

TEST_F(TestIPAddress, testIPv6ToBinaryIPv4Broadcast)
{
   auto v6 = IPAddress::resolve("::ffff:255.255.255.255");
   EXPECT_EQ(false, v6.isIPv6());
   EXPECT_EQ(0xfffffffful, v6.toIPv4InAddrT());
}

TEST_F(TestIPAddress, testIPv4ToBinaryIPv4Addr)
{
   auto v4 = IPAddress::resolve("192.168.10.2");
   EXPECT_EQ(true, v4.isIPv4());
   in_addr_t b = v4.toIPv4InAddrT();
   EXPECT_EQ(0x020aa8c0ul, b);
}

TEST_F(TestIPAddress, testIPv6ToBinaryIPv4Addr)
{
   auto v4 = IPAddress::resolve("::ffff:192.168.10.2");
   EXPECT_EQ(true, v4.isIPv4());
   in_addr_t b = v4.toIPv4InAddrT();
   EXPECT_EQ(0x20aa8c0ul, b);
}

TEST_F(TestIPAddress, testHash)
{
   auto v4 = IPAddress::resolve("192.168.10.2");
   EXPECT_EQ(v4.hash(), (size_t) 0x0 ^ 0x0000ffffc0a80a02);
   auto v4_2 = IPAddress::resolve("::ffff:192.168.10.2");
   EXPECT_EQ(v4_2.hash(), (size_t) 0x0 ^ 0x0000ffffc0a80a02);
   auto v6 = IPAddress::resolve("fd00:beee:beee::1234");
   EXPECT_EQ(v6.hash(), (size_t) 0xfd00beeebeee0000 ^ 0x0000000000001234);
}

TEST_F(TestIPAddress, testIPNetwork)
{
   auto n1 = IPNetwork::fromCidr("192.168.0.0/16");
   ASSERT_TRUE(n1.containsAddress(IPAddress::resolve("192.168.0.1")));
   ASSERT_FALSE(n1.containsAddress(IPAddress::resolve("192.150.0.1")));

   auto n2 = IPNetwork::fromCidr("fd00:beee:beee::0/48");
   ASSERT_TRUE(n2.containsAddress(IPAddress::resolve("fd00:beee:beee:1234::2")));
   ASSERT_FALSE(n2.containsAddress(IPAddress::resolve("fd00:beee:beef::2")));

   auto n3 = IPNetwork::fromCidr("0.0.0.0/0");
   ASSERT_TRUE(n3.containsAddress(IPAddress::resolve("192.168.0.1")));
   ASSERT_TRUE(n3.containsAddress(IPAddress::resolve("192.150.0.1")));
   ASSERT_FALSE(n3.containsAddress(IPAddress::resolve("fd00:beee:beef::2")));

   auto n4 = IPNetwork::fromCidr("192.168.0.1/32");
   ASSERT_TRUE(n4.containsAddress(IPAddress::resolve("192.168.0.1")));
   ASSERT_FALSE(n4.containsAddress(IPAddress::resolve("192.150.0.1")));
   ASSERT_FALSE(n4.containsAddress(IPAddress::resolve("192.168.0.2")));

   auto n5 = IPNetwork::fromCidr("fd00:beee:beee::0/47");
   ASSERT_TRUE(n5.containsAddress(IPAddress::resolve("fd00:beee:beef::0")));
   ASSERT_FALSE(n5.containsAddress(IPAddress::resolve("fd00:beee:befe::0")));

   auto n6 = IPNetwork::fromCidr("fd00:beee:beee::0/17");
   ASSERT_TRUE(n6.containsAddress(IPAddress::resolve("fd00:80ab:cdef::123")));
   ASSERT_FALSE(n6.containsAddress(IPAddress::resolve("fd00:70ab:cdef::123")));

   auto n7 = IPNetwork::fromCidr("::/0");
   ASSERT_TRUE(n7.containsAddress(IPAddress::resolve("fd00:beee:beef::0")));
   ASSERT_TRUE(n7.containsAddress(IPAddress::resolve("fd00:80ab:cdef::123")));
   ASSERT_TRUE(n7.containsAddress(IPAddress::resolve("1234:5678:cdef::abcd:1234")));

   auto n8 = IPNetwork::fromCidr("::/128");
   ASSERT_FALSE(n8.containsAddress(IPAddress::resolve("fd00:beee:beef::0")));
   ASSERT_FALSE(n8.containsAddress(IPAddress::resolve("fd00:80ab:cdef::123")));
   ASSERT_FALSE(n8.containsAddress(IPAddress::resolve("1234:5678:cdef::abcd:1234")));
}
