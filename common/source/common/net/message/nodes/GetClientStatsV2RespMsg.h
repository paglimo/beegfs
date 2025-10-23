#pragma once


#include <common/net/message/NetMessage.h>
#include <common/toolkit/HighResolutionStats.h>
#include <common/Common.h>


#define GETCLIENTSTATSRESP_MAX_PAYLOAD_LEN   (56*1024) // actual max is 64KiB minus header and
                                                       // encoding overhead, so we have a safe
                                                       // amount (8KiB) of room here left for that.


/**
 * This message sends the stats for multiple client IPs encoded in a single vector.
 */
class GetClientStatsV2RespMsg : public NetMessageSerdes<GetClientStatsV2RespMsg>
{
   public:

      /**
       * @param statsVec   - The list has: IP, data1, data2, ..., dataN, IP, data1, ..., dataN
       *                     NOTE: Just a reference, DO NOT free it as long as you use this
       *                           object!
       */
      GetClientStatsV2RespMsg(Uint128Vector* statsVec) :
         BaseType(NETMSGTYPE_GetClientStatsV2Resp)
      {
         this->statsVec = statsVec;
      }

      GetClientStatsV2RespMsg() : BaseType(NETMSGTYPE_GetClientStatsV2Resp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % serdes::backedPtr(obj->statsVec, obj->parsed.statsVec);
      }

   private:
      // for serialization
      Uint128Vector* statsVec; // not owned by this object!

      // for deserialization
      struct {
         Uint128Vector statsVec;
      } parsed;

   public:
      Uint128Vector& getStatsVector()
      {
         return *statsVec;
      }
};

