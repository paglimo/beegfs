#pragma once

#include <common/Common.h>
#include <common/net/message/SimpleIntMsg.h>

class SetTargetConsistencyStatesRespMsg : public SimpleIntMsg
{
   public:
      SetTargetConsistencyStatesRespMsg(int result)
         : SimpleIntMsg(NETMSGTYPE_SetTargetConsistencyStatesResp, result)
      {
      }

      SetTargetConsistencyStatesRespMsg() : SimpleIntMsg(NETMSGTYPE_SetTargetConsistencyStatesResp)
      {
      }

      FhgfsOpsErr getResult()
      {
         return (FhgfsOpsErr)getValue();
      }
};

