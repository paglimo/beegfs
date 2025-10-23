#pragma once

#include "common/net/sock/IPAddress.h"
#include <common/components/AbstractDatagramListener.h>

class DatagramListener : public AbstractDatagramListener
{
   public:
      DatagramListener(NetFilter* netFilter, NicAddressList& localNicList,
         AcknowledgmentStore* ackStore, uint16_t udpPort,
         bool restrictOutboundInterfaces);
      virtual ~DatagramListener();

   
   protected:
      virtual void handleIncomingMsg(struct sockaddr* fromAddr, NetMessage* msg);
   
   private:
   
};

