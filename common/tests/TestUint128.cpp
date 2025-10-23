#include <unordered_map>
#include <sstream>
#include <common/toolkit/UInt128.h>
#include <common/toolkit/StringTk.h>
#include <common/toolkit/serialization/Byteswap.h>

#include <gtest/gtest.h>

typedef std::map<uint128_t, int> map128;
typedef std::unordered_map<uint128_t, int, uint128::Hash> unmap128;
typedef std::unordered_map<int, uint128_t> unmapint;

struct Uint128TestResult
{
      uint64_t msw;
      uint64_t lsw;
      const char* str;
      const char* rev_order;
};

static struct Uint128TestResult const __TESTUINT128_RESULTS []  =
{
   { 0x00, 0x00, "00000000000000000000000000000000", "00000000000000000000000000000000" },
   { (uint64_t)-1, (uint64_t)-1, "ffffffffffffffffffffffffffffffff", "ffffffffffffffffffffffffffffffff" },
   { (uint64_t)-1, 0, "ffffffffffffffff0000000000000000", "0000000000000000ffffffffffffffff" },
   { 0, (uint64_t)-1, "0000000000000000ffffffffffffffff", "ffffffffffffffff0000000000000000" },
   { 0x0123456789abcdefLLU, 0xdeadbeef00bee6f5LLU, "0123456789abcdefdeadbeef00bee6f5", "f5e6be00efbeaddeefcdab8967452301" },
   { 0 }
};


class TestUint128 : public ::testing::Test
{
};

TEST_F(TestUint128, testMswLsw)
{
   uint128_t a;
   for (auto r = &__TESTUINT128_RESULTS[0]; r->str != NULL; ++r)
   {
      a = uint128::make(r->msw, r->lsw);
      EXPECT_EQ(r->msw, uint128::upper64(a));
      EXPECT_EQ(r->lsw, uint128::lower64(a));
   }
}

TEST_F(TestUint128, testToHexStr)
{
   uint128_t a;
   for (auto r = &__TESTUINT128_RESULTS[0]; r->str != NULL; ++r)
   {
      a = uint128::make(r->msw, r->lsw);
      EXPECT_EQ(r->str, uint128::toHexStr(a));
   }
}
   
TEST_F(TestUint128, testToHtonx)
{
   uint128_t a;
   for (auto r = &__TESTUINT128_RESULTS[0]; r->str != NULL; ++r)
   {
      a = uint128::make(r->msw, r->lsw);
      EXPECT_EQ(a, LE_TO_HOST_128(HOST_TO_LE_128(a)));
#if BYTE_ORDER == BIG_ENDIAN
      EXPECT_EQ(r->rev_order, uint128::toHexStr(HOST_TO_LE_128(a)));
      EXPECT_EQ(r->str, uint128::toHexStr(HOST_TO_BE_128(a)));
#elif BYTE_ORDER == LITTLE_ENDIAN
      EXPECT_EQ(r->rev_order, uint128::toHexStr(HOST_TO_BE_128(a)));      
      EXPECT_EQ(r->str, uint128::toHexStr(HOST_TO_LE_128(a)));
#endif
   }
}

TEST_F(TestUint128, testUnorderedMap)
{
   unmap128 mapBy128;
   unmapint mapByInt;
   uint128_t a = 0;

   for (int i = 0; i < 128; ++i)
   {
      mapBy128[a] = i;
      mapByInt[i] = a;
      if (a == 0)
         a = 1;
      else
         a *= 2;
   }

   for (auto iter = mapBy128.begin(); iter != mapBy128.end(); ++iter)
   {
      uint128_t o = mapByInt[iter->second];
      EXPECT_EQ(iter->first, o);
      mapByInt.erase(iter->second);
   }

   EXPECT_EQ(true, mapByInt.empty());
}

TEST_F(TestUint128, testMap)
{
   map128 mapBy128;
   unmapint mapByInt;
   uint128_t a = 0;

   for (int i = 0; i < 128; ++i)
   {
      mapBy128[a] = i;
      mapByInt[i] = a;
      if (a == 0)
         a = 1;
      else
         a *= 2;
   }

   for (auto iter = mapBy128.begin(); iter != mapBy128.end(); ++iter)
   {
      uint128_t o = mapByInt[iter->second];
      EXPECT_EQ(iter->first, o);
      mapByInt.erase(iter->second);
   }

   EXPECT_EQ(true, mapByInt.empty());
}

TEST_F(TestUint128, testConcat)
{
   uint128_t a;
   for (auto r = &__TESTUINT128_RESULTS[0]; r->str != NULL; ++r)
   {
      a = uint128::make(r->msw, r->lsw);
      std::ostringstream out;
      out << a;
      EXPECT_EQ(r->str, out.str());
   }
}
