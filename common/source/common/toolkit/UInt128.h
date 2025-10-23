#pragma once

#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

typedef unsigned __int128 uint128_t;
typedef std::vector<uint128_t> Uint128Vector;

namespace uint128 {

// Suitable for hashing std::map<uint128_t,...>
// There is a std::hash<unsigned __int128> but it ignores the MSW.
struct Hash
{
   size_t operator()(uint128_t v) const
   {
      return ((size_t)(v >> 64)) ^ ((size_t) v);
   }
};

static inline uint128_t make(uint64_t msw, uint64_t lsw) {
   return ((uint128_t)msw << 64) | (uint128_t)lsw;
}

static inline uint64_t lower64(uint128_t n) {
   return (uint64_t)n;
}

static inline uint64_t upper64(uint128_t n) {
   return (uint64_t)(n >> 64);
}

static inline std::string toHexStr(uint128_t a)
{
   char aStr[33] = "";
   snprintf(aStr, sizeof aStr, "%016" PRIx64 "%016" PRIx64, upper64(a), lower64(a));
   return aStr;
}

}

static inline std::ostream& operator<<(std::ostream& out, uint128_t v)
{
   out << uint128::toHexStr(v);
   return out;
}

