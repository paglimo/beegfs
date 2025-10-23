#pragma once

#include <common/storage/StorageErrors.h>
#include <common/net/message/nodes/GetClientStatsV2Msg.h>

class GetClientStatsV2MsgEx : public GetClientStatsV2Msg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

