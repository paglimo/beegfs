#pragma once

#include <cerrno>
#include <common/app/log/LogContext.h>
#include <common/threading/Atomics.h>
#include <common/threading/PThread.h>
#include <common/net/sock/StandardSocket.h>
#include <common/net/message/AcknowledgeableMsg.h>
#include <common/net/message/NetMessage.h>
#include <common/nodes/AbstractNodeStore.h>
#include <common/toolkit/AcknowledgmentStore.h>
#include <common/Common.h>
#include "ComponentInitException.h"
#include "common/net/sock/IPAddress.h"

#include <mutex>

#define DGRAMMGR_RECVBUF_SIZE    65536
#define DGRAMMGR_SENDBUF_SIZE    DGRAMMGR_RECVBUF_SIZE

typedef std::unordered_map<IPAddress, std::shared_ptr<StandardSocket>, std::hash<IPAddress>> StandardSocketMap;

class AbstractDatagramListener : public PThread
{
   // for efficient individual multi-notifications (needs access to mutex)
   friend class LockEntryNotificationWork;
   friend class LockRangeNotificationWork;


   public:
      virtual ~AbstractDatagramListener();

      void sendToNodesUDP(const AbstractNodeStore* nodes, NetMessage* msg, int numRetries,
         int timeoutMS=0);
      void sendToNodesUDP(const std::vector<NodeHandle>& nodes, NetMessage* msg, int numRetries,
         int timeoutMS=0);
      bool sendToNodesUDPwithAck(const AbstractNodeStore* nodes, AcknowledgeableMsg* msg,
         int ackWaitSleepMS = 1000, int numRetries=2);
      bool sendToNodesUDPwithAck(const std::vector<NodeHandle>& nodes, AcknowledgeableMsg* msg,
         int ackWaitSleepMS = 1000, int numRetries=2);
      bool sendToNodeUDPwithAck(const NodeHandle& node, AcknowledgeableMsg* msg,
         int ackWaitSleepMS = 1000, int numRetries=2);
      bool sendBufToNode(Node& node, const char* buf, size_t bufLen);
      bool sendMsgToNode(Node& node, NetMessage* msg);

      void sendDummyToSelfUDP();

   private:
      std::shared_ptr<StandardSocket> findSenderSockUnlocked(const IPAddress& addr);

   protected:
      AbstractDatagramListener(const std::string& threadName, NetFilter* netFilter,
         NicAddressList& localNicList, AcknowledgmentStore* ackStore, uint16_t udpPort,
         bool restrictOutboundInterfaces);

      LogContext log;

      NetFilter* netFilter;
      AcknowledgmentStore* ackStore;
      bool restrictOutboundInterfaces;

      uint16_t udpPort;
      uint16_t udpPortNetByteOrder;

      char* sendBuf;

      virtual void handleIncomingMsg(struct sockaddr* fromAddr, NetMessage* msg) = 0;

      std::shared_ptr<StandardSocket> findSenderSock(const IPAddress& fromAddr);

      /**
       * Returns the mutex related to seralization of sends.
       */
      Mutex* getSendMutex()
      {
         return &mutex;
      }

   private:
      std::shared_ptr<StandardSocketGroup> udpSock;
      StandardSocketMap interfaceSocks;
      IpSourceMap ipSrcMap;
      char* recvBuf;
      int recvTimeoutMS;
      RoutingTable routingTable;
      /**
       * For now, use a single mutex for all of the members that are subject to
       * thread contention. Using multiple mutexes makes the locking more difficult
       * and can lead to deadlocks. The state dependencies are all intertwined,
       * anyway.
       */
      Mutex mutex;

      AtomicUInt32 ackCounter; // used to generate ackIDs

      NicAddressList localNicList;

      bool initSocks();
      void initBuffers();
      void configSocket(StandardSocket* sock, NicAddress* nicAddr, int bufsize);

      void run();
      void listenLoop();

      bool isDGramFromSelf(const IPAddress& fromAddr, uint16_t port);
      unsigned incAckCounter();

   public:
      // inliners

      /**
       * Returns ENETUNREACH if no local NIC found to reach @to.
       */
      ssize_t sendto(const void* buf, size_t len, int flags,
         sockaddr* to)
      {
         const std::lock_guard<Mutex> lock(mutex);
         SocketAddress addr(to);
         std::shared_ptr<StandardSocket> s = findSenderSockUnlocked(addr.addr);
         if (s == nullptr)
            return ENETUNREACH;
         return s->sendto(buf, len, flags, &addr);
      }

      /**
       * Returns ENETUNREACH if no local NIC found to reach @ipAddr.
       */
      ssize_t sendto(const void *buf, size_t len, int flags, const SocketAddress& ipAddr)
      {
         const std::lock_guard<Mutex> lock(mutex);
         std::shared_ptr<StandardSocket> s = findSenderSockUnlocked(ipAddr.addr);
         if (s == nullptr)
            return ENETUNREACH;
         if (s->getFamily() == AF_INET && !ipAddr.addr.isIPv4())
            // Skip this address if it's ipv6 and the socket is in ipv4 fallback mode
            return EAFNOSUPPORT;
         return s->sendto(buf, len, flags, &ipAddr);
      }

      // getters & setters
      void setRecvTimeoutMS(int recvTimeoutMS)
      {
         this->recvTimeoutMS = recvTimeoutMS;
      }

      uint16_t getUDPPort()
      {
         return udpPort;
      }

      void setLocalNicList(NicAddressList& nicList);

};

