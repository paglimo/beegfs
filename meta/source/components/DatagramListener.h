#pragma once

#include "common/net/sock/IPAddress.h"
#include <common/components/AbstractDatagramListener.h>

class DatagramListener : public AbstractDatagramListener
{
   public:
      DatagramListener(NetFilter* netFilter, NicAddressList& localNicList,
         AcknowledgmentStore* ackStore, unsigned short udpPort,
         bool restrictOutboundInterfaces);

   protected:
      virtual void handleIncomingMsg(struct sockaddr* fromAddr, NetMessage* msg);

   private:

};

