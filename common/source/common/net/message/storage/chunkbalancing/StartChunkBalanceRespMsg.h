#pragma once

#include <common/net/message/SimpleIntMsg.h>

class StartChunkBalanceRespMsg : public SimpleIntMsg
{
   public:
      StartChunkBalanceRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_StartChunkBalanceResp, result) {}

      StartChunkBalanceRespMsg() : SimpleIntMsg(NETMSGTYPE_StartChunkBalanceResp) {}
};


