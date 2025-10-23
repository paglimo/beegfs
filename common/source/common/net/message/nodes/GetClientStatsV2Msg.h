#pragma once

#include "common/net/message/NetMessage.h"
#include <common/net/message/SimpleInt64Msg.h>
#include <common/Common.h>

#define GETCLIENTSTATSMSG_FLAG_PERUSERSTATS      1 /* query per-user (instead per-client) stats */

class GetClientStatsV2Msg : public NetMessageSerdes<GetClientStatsV2Msg>
{
   public:
      GetClientStatsV2Msg(uint128_t cookie) : BaseType(NETMSGTYPE_GetClientStatsV2), cookie(cookie)
      {
      }

      /**
       * For deserialization only.
       */
      GetClientStatsV2Msg() : BaseType(NETMSGTYPE_GetClientStatsV2)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % obj->cookie;
      }


   protected:
      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return GETCLIENTSTATSMSG_FLAG_PERUSERSTATS;
      }

   public:
      uint128_t getCookieIP()
      {
         return cookie;
      }

   private:
      uint128_t cookie;
};


