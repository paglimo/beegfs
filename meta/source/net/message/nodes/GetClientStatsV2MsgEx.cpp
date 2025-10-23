#include "GetClientStatsV2MsgEx.h"

#include <program/Program.h>
#include <common/net/message/storage/GetHighResStatsRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/nodes/OpCounter.h>
#include <nodes/MetaNodeOpStats.h>
#include <common/net/message/nodes/GetClientStatsV2RespMsg.h>


/**
 * Server side gets a GetClientStatsMsgEx request
 */
bool GetClientStatsV2MsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("GetClientStatsV2MsgEx incoming");

   // get stats
   MetaNodeOpStats* opStats = Program::getApp()->getNodeOpStats();

   bool wantPerUserStats = isMsgHeaderFeatureFlagSet(GETCLIENTSTATSMSG_FLAG_PERUSERSTATS);
   Uint128Vector opStatsVec;

   opStats->mapToUInt128Vec(
      getCookieIP(), GETCLIENTSTATSRESP_MAX_PAYLOAD_LEN, wantPerUserStats, &opStatsVec);

   ctx.sendResponse(GetClientStatsV2RespMsg(&opStatsVec) );

   return true;
}

