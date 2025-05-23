#pragma once

#ifdef BEEGFS_NVFS
#include <common/net/message/SimpleInt64Msg.h>

class WriteLocalFileRDMARespMsg : public SimpleInt64Msg
{
   public:
      WriteLocalFileRDMARespMsg(int64_t result) :
         SimpleInt64Msg(NETMSGTYPE_WriteLocalFileRDMAResp, result)
      {
      }

      WriteLocalFileRDMARespMsg() :
         SimpleInt64Msg(NETMSGTYPE_WriteLocalFileRDMAResp)
      {
      }
};
#endif /* BEEGFS_NVFS */

