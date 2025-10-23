#pragma once

#include <common/net/message/SimpleInt64Msg.h>
#include <common/Common.h>

#define GETCLIENTSTATSMSG_FLAG_PERUSERSTATS      1 /* query per-user (instead per-client) stats */


/**
 * Request per-client or per-user operation statictics.
 */
class GetClientStatsMsg : public SimpleInt64Msg
{
   public:
      /**
       * @param cookie  - Not all clients fit into the last stats message. So we use this value
       *                  as a cookie to know where to continue (cookie + 1). cookie may be
       *                  a binary IP address in host-order or a UID. -1 is used as the "start"
       *                  cookie.
       *
       *  This constructor is called on the side sending the mesage.
       */
      // XXX cookieIP needs to be uint128_t. How to do this with backward compatibility?
      GetClientStatsMsg(int64_t cookie) : SimpleInt64Msg(NETMSGTYPE_GetClientStats, cookie)
      {
         //std::cout << "cookieIP: " << StringTk::uint64ToHexStr(cookie) << std::endl;
      }

      /**
       * For deserialization only.
       */
      GetClientStatsMsg() : SimpleInt64Msg(NETMSGTYPE_GetClientStats)
      {
      }


   protected:
      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return GETCLIENTSTATSMSG_FLAG_PERUSERSTATS;
      }


   public:
      uint64_t getCookieIP()
      {
         return getValue();
      }
};


