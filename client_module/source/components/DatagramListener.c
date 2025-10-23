#include "DatagramListener.h"
#include <common/toolkit/SocketTk.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/NetFilter.h>
#include <common/toolkit/Serialization.h>
#include <net/message/NetMessageFactory.h>
#include <linux/in.h>


void __DatagramListener_run(Thread* this)
{
   DatagramListener* thisCast = (DatagramListener*)this;

   Logger* log = App_getLogger(thisCast->app);
   const char* logContext = "DatagramListener (run)";

   __DatagramListener_initBuffers(thisCast);

   __DatagramListener_listenLoop(thisCast);

   Logger_log(log, 4, logContext, "Component stopped.");
}

void __DatagramListener_listenLoop(DatagramListener* this)
{
   Logger* log = App_getLogger(this->app);
   const char* logContext = "DatagramListener (listen loop)";

   Thread* thisThread = (Thread*)this;

   struct sockaddr_in6 fromAddr;
   const int recvTimeoutMS = 2000;

   while(!Thread_getSelfTerminate(thisThread) )
   {
      NetMessage* msg;
      ssize_t recvRes;

      struct iov_iter *iter = STACK_ALLOC_BEEGFS_ITER_KVEC(this->recvBuf, DGRAMMGR_RECVBUF_SIZE, READ);
      recvRes = StandardSocket_recvfromT(this->udpSock, iter, 0, &fromAddr, recvTimeoutMS);

      if(recvRes == -ETIMEDOUT)
      { // timeout: nothing to worry about, just idle
         continue;
      }
      else
      if(recvRes == 0)
      {
         char fromIp[SOCKET_IPADDRSTR_LEN];
         SocketTk_ipaddrToStr(fromIp, sizeof fromIp, fromAddr.sin6_addr);
         Logger_logFormatted(log, Log_NOTICE, logContext,
            "Received an empty datagram. IP: %s; port: %d",
            fromIp, beegfs_get_port(&fromAddr));
         continue;
      }
      else
      if(unlikely(recvRes < 0) )
      { // error

         Logger_logErrFormatted(log, logContext,
            "Encountered an unrecoverable socket error. ErrCode: %ld", recvRes);

         break;
      }

      if(__DatagramListener_isDGramFromLocalhost(this, &fromAddr) )
      {
         //log.log(5, "Discarding DGram from localhost");
         continue;
      }

      msg = NetMessageFactory_createFromBuf(this->app, this->recvBuf, recvRes);

      if (msg->msgHeader.msgType == NETMSGTYPE_Invalid
            || msg->msgHeader.msgLength != recvRes
            || msg->msgHeader.msgSequence != 0
            || msg->msgHeader.msgSequenceDone != 0)
      {
         char ipStr[SOCKET_IPADDRSTR_LEN];
         SocketTk_ipaddrToStr(ipStr, sizeof ipStr, fromAddr.sin6_addr);
         Logger_logFormatted(this->app->logger, Log_NOTICE, logContext,
               "Received invalid message from peer %s", ipStr);
      }
      else
      {
         _DatagramListener_handleIncomingMsg(this, &fromAddr, msg);
      }

      NETMESSAGE_FREE(msg);

   } // end of while loop
}

void _DatagramListener_handleIncomingMsg(DatagramListener* this,
   struct sockaddr_in6* fromAddr, NetMessage* msg)
{
   Logger* log = App_getLogger(this->app);
   const char* logContext = "DatagramListener (incoming msg)";

   switch(NetMessage_getMsgType(msg) )
   {
      // An ack has historically been considered a valid message in this context, but the client
      // doesn't actually do anything with them. Ack messages are handled as a SimpleStringMsg which
      // uses the default NetMessage_processIncoming() handler which always returns false causing a
      // confusing "problem encountered" error to be logged if `processIncoming()` is called below.
      // Probably historically this wasn't an issue because clients didn't usually see acks, but at
      // least with the 8.0 mgmtd this can happen when the client and mgmtd are on the same node.
      case NETMSGTYPE_Ack:
      {
         Logger_log(log, 4, logContext, "Ignoring incoming ack message");
      } break;
      // valid messages within this context
      case NETMSGTYPE_HeartbeatRequest:
      case NETMSGTYPE_Heartbeat:
      case NETMSGTYPE_MapTargets:
      case NETMSGTYPE_RemoveNode:
      case NETMSGTYPE_LockGranted:
      case NETMSGTYPE_RefreshTargetStates:
      case NETMSGTYPE_SetMirrorBuddyGroup:
      {
         if(!msg->ops->processIncoming(msg, this->app, fromAddr, (Socket*)this->udpSock,
            this->sendBuf, DGRAMMGR_SENDBUF_SIZE) )
         {
            Logger_logFormatted(log, 2, logContext,
               "Problem encountered during handling of incoming message of type %d",
               NetMessage_getMsgType(msg));
         }
      } break;

      default:
      { // valid fhgfs message, but not allowed within this context
         char ipStr[SOCKET_IPADDRSTR_LEN];
         SocketTk_ipaddrToStr(ipStr, sizeof ipStr, fromAddr->sin6_addr);
         Logger_logErrFormatted(log, logContext, "Received a message of type %d "
            "that is invalid within the current context from: %s",
            NetMessage_getMsgType(msg), ipStr);
      } break;
   };
}

bool __DatagramListener_initSock(DatagramListener* this, unsigned short udpPort)
{
   App *app = this->app;
   Config* cfg = App_getConfig(this->app);
   Logger* log = App_getLogger(this->app);
   const char* logContext = "DatagramListener (init sock)";

   bool broadcastRes;
   bool bindRes;
   Socket* udpSockBase;
   int bufsize;

   this->udpPort = udpPort;
   this->udpSock = StandardSocket_construct(app->sockDomain, SOCK_DGRAM, 0);

   if(!this->udpSock)
   {
      Logger_logErr(log, logContext, "Initialization of UDP socket failed");
      return false;
   }

   udpSockBase = &this->udpSock->pooledSocket.socket;

   // set some socket options

   broadcastRes = StandardSocket_setSoBroadcast(this->udpSock, true);
   if(!broadcastRes)
   {
      Logger_logErr(log, logContext, "Enabling broadcast for UDP socket failed.");
      goto err_valid;
   }

   bufsize = Config_getConnUDPRcvBufSize(cfg);
   if (bufsize > 0)
      StandardSocket_setSoRcvBuf(this->udpSock, bufsize);

   // bind the socket

   bindRes = Socket_bind(udpSockBase, udpPort);
   if(!bindRes)
   {
      Logger_logErrFormatted(log, logContext, "Binding UDP socket to port %d failed.", udpPort);
      goto err_valid;
   }

   Logger_logFormatted(log, 3, logContext, "Listening for UDP datagrams: Port %d", udpPort);

   return true;

err_valid:
   Socket_virtualDestruct(udpSockBase);
   return false;
}

/**
 * Note: Delayed init of buffers (better for NUMA).
 */
void __DatagramListener_initBuffers(DatagramListener* this)
{
   this->recvBuf = (char*)vmalloc(DGRAMMGR_RECVBUF_SIZE);
   this->sendBuf = (char*)vmalloc(DGRAMMGR_SENDBUF_SIZE);
}


/**
 * Sends the buffer to all available node interfaces.
 */
static void DatagramListener_sendBufToNode_kernel(DatagramListener* this, Node* node,
   char* buf, size_t bufLen)
{
   NodeConnPool* connPool = Node_getConnPool(node);
   NicAddressList* nicList;
   unsigned short port = Node_getPortUDP(node);
   NicAddressListIter iter;

   NodeConnPool_lock(connPool);
   nicList = NodeConnPool_getNicListLocked(connPool);
   NicAddressListIter_init(&iter, nicList);

   for( ; !NicAddressListIter_end(&iter); NicAddressListIter_next(&iter) )
   {
      NicAddress* nicAddr = NicAddressListIter_value(&iter);

      if(nicAddr->nicType != NICADDRTYPE_STANDARD)
         continue;

      if(!NetFilter_isAllowed(this->netFilter, nicAddr->ipAddr) )
         continue;

      DatagramListener_sendtoIP_kernel(this, buf, bufLen, 0, nicAddr->ipAddr, port);
   }
   NodeConnPool_unlock(connPool);
}

/**
 * Sends the message to all available node interfaces.
 */
void DatagramListener_sendMsgToNode(DatagramListener* this, Node* node, NetMessage* msg)
{
   char* msgBuf = MessagingTk_createMsgBuf(msg);
   unsigned msgLen = NetMessage_getMsgLength(msg);

   DatagramListener_sendBufToNode_kernel(this, node, msgBuf, msgLen);

   kfree(msgBuf);
}

bool __DatagramListener_isDGramFromLocalhost(DatagramListener* this,
   struct sockaddr_in6* fromAddr)
{
   NodeConnPool* connPool;
   NicAddressList* nicList;
   NicAddressListIter iter;
   struct in6_addr fromIp;
   int nicListSize;
   int i;
   bool result = false;

   if (beegfs_get_port(fromAddr) != this->udpPort)
      return false;

   fromIp = fromAddr->sin6_addr;

   if (ipv6_addr_v4mapped(&fromIp) && fromIp.s6_addr32[3] == htonl(INADDR_LOOPBACK))
      return true;
   if (ipv6_addr_equal(&fromIp, &in6addr_loopback))
      return true;

   connPool = Node_getConnPool(this->localNode);
   NodeConnPool_lock(connPool);
   nicList = NodeConnPool_getNicListLocked(connPool);

   NicAddressListIter_init(&iter, nicList);
   nicListSize = NicAddressList_length(nicList);

   for(i = 0; i < nicListSize; i++, NicAddressListIter_next(&iter) )
   {
      NicAddress* nicAddr = NicAddressListIter_value(&iter);

      if (ipv6_addr_equal(&nicAddr->ipAddr, &fromIp))
      {
         result = true;
         break;
      }
   }

   NodeConnPool_unlock(connPool);
   return result;
}
