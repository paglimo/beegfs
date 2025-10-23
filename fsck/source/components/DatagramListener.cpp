#include "DatagramListener.h"
#include "common/net/sock/IPAddress.h"

#include <common/net/message/NetMessageLogHelper.h>

DatagramListener::DatagramListener(NetFilter* netFilter, NicAddressList& localNicList,
   AcknowledgmentStore* ackStore, uint16_t udpPort, bool restrictOutboundInterfaces) :
   AbstractDatagramListener("DGramLis", netFilter, localNicList, ackStore, udpPort,
      restrictOutboundInterfaces)
{
}

DatagramListener::~DatagramListener()
{
}

void DatagramListener::handleIncomingMsg(struct sockaddr* fromAddr, NetMessage* msg)
{
   HighResolutionStats stats; // currently ignored
   IPAddress fromIP(fromAddr);
   std::shared_ptr<StandardSocket> sock = findSenderSock(fromIP);
   if (sock == nullptr)
   {
      log.log(Log_WARNING, "Could not handle incoming message: no socket");
      return;
   }

   NetMessage::ResponseContext rctx(fromAddr, sock.get(), sendBuf, DGRAMMGR_SENDBUF_SIZE, &stats);

   const auto messageType = netMessageTypeToStr(msg->getMsgType());

   switch(msg->getMsgType() )
   {
      // valid messages within this context
      case NETMSGTYPE_Heartbeat:
      case NETMSGTYPE_FsckModificationEvent:
      {
         if(!msg->processIncoming(rctx) )
         {
            LOG(GENERAL, WARNING,
                  "Problem encountered during handling of incoming message.", messageType);
         }
      } break;

      default:
      { // valid, but not within this context
         log.logErr(
            "Received a message that is invalid within the current context "
            "from: " + fromIP.toString() + "; "
            "type: " + messageType );
      } break;
   };
}


