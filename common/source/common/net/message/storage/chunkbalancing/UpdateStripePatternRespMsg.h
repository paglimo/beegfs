#pragma once

#include <common/net/message/SimpleIntMsg.h>

class UpdateStripePatternRespMsg : public SimpleIntMsg
{
   public:
      UpdateStripePatternRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_UpdateStripePatternResp, result) {}

     UpdateStripePatternRespMsg() : SimpleIntMsg(NETMSGTYPE_UpdateStripePatternResp) {}


   public:
      // inliners
      FhgfsOpsErr getResult()
      {
         return static_cast<FhgfsOpsErr>(getValue());
      }
};


