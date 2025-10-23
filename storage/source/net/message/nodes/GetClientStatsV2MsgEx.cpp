#include "GetClientStatsV2MsgEx.h"

#include <program/Program.h>
#include <common/net/message/storage/GetHighResStatsRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/nodes/OpCounter.h>
#include <nodes/StorageNodeOpStats.h>
#include "common/toolkit/UInt128.h"
#include "common/net/message/nodes/GetClientStatsV2RespMsg.h"


/**
 * Server side, called when the server gets a GetClientStatsMsgEx request
 */
bool GetClientStatsV2MsgEx::processIncoming(ResponseContext& ctx)
{
   // get stats
   StorageNodeOpStats* clientOpStats = Program::getApp()->getNodeOpStats();

   bool wantPerUserStats = isMsgHeaderFeatureFlagSet(GETCLIENTSTATSMSG_FLAG_PERUSERSTATS);
   Uint128Vector opStatsVec;

   clientOpStats->mapToUInt128Vec(
      getCookieIP(), GETCLIENTSTATSRESP_MAX_PAYLOAD_LEN, wantPerUserStats, &opStatsVec);

   ctx.sendResponse(GetClientStatsV2RespMsg(&opStatsVec) );

   return true;
}

