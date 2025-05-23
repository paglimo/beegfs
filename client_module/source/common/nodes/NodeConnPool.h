#ifndef NODECONNPOOL_H_
#define NODECONNPOOL_H_

#include <app/log/Logger.h>
#include <app/App.h>
#include <common/net/sock/NetworkInterfaceCard.h>
#include <common/net/sock/StandardSocket.h>
#include <common/net/sock/NicAddressStatsList.h>
#include <common/threading/Mutex.h>
#include <common/threading/Condition.h>
#include <common/toolkit/ListTk.h>
#include <common/toolkit/SocketTk.h>
#include <common/Common.h>
#include "ConnectionList.h"
#include "ConnectionListIter.h"
#include "DevicePriorityContext.h"


// forward declaration
struct App;
struct Node;

struct NodeConnPoolStats;
typedef struct NodeConnPoolStats NodeConnPoolStats;
struct NodeConnPoolErrorState;
typedef struct NodeConnPoolErrorState NodeConnPoolErrorState;

struct NodeConnPool;
typedef struct NodeConnPool NodeConnPool;


extern void NodeConnPool_init(NodeConnPool* this, struct App* app, struct Node* parentNode,
   unsigned short streamPort, NicAddressList* nicList, NicAddressList* localRdmaNicList);
extern NodeConnPool* NodeConnPool_construct(struct App* app, struct Node* parentNode,
   unsigned short streamPort, NicAddressList* nicList, NicAddressList* localRdmaNicList);
extern void NodeConnPool_uninit(NodeConnPool* this);
extern void NodeConnPool_destruct(NodeConnPool* this);

extern Socket* NodeConnPool_acquireStreamSocket(NodeConnPool* this);
extern Socket* NodeConnPool_acquireStreamSocketEx(NodeConnPool* this, bool allowWaiting,
   DevicePriorityContext* devPrioCtx);
extern void NodeConnPool_releaseStreamSocket(NodeConnPool* this, Socket* sock);
extern void NodeConnPool_invalidateStreamSocket(NodeConnPool* this, Socket* sock);
extern unsigned NodeConnPool_disconnectAvailableStreams(NodeConnPool* this);
extern unsigned NodeConnPool_disconnectAndResetIdleStreams(NodeConnPool* this);
extern bool NodeConnPool_updateInterfaces(NodeConnPool* this, unsigned short streamPort,
   NicAddressList* nicList);

extern void __NodeConnPool_invalidateSpecificStreamSocket(NodeConnPool* this, Socket* sock);
extern unsigned __NodeConnPool_invalidateAvailableStreams(NodeConnPool* this,
   bool idleStreamsOnly, bool closeOnRelease);
extern void __NodeConnPool_resetStreamsIdleFlag(NodeConnPool* this);
extern bool __NodeConnPool_applySocketOptionsPreConnect(NodeConnPool* this, Socket* sock);
extern bool __NodeConnPool_applySocketOptionsConnected(NodeConnPool* this, Socket* sock);

extern void __NodeConnPool_statsAddNic(NodeConnPool* this, NicAddrType_t nicType);
extern void __NodeConnPool_statsRemoveNic(NodeConnPool* this, NicAddrType_t nicType);

extern void __NodeConnPool_setCompleteFail(NodeConnPool* this);
extern void __NodeConnPool_setConnSuccess(NodeConnPool* this, struct in_addr lastSuccessPeerIP,
   NicAddrType_t lastSuccessNicType);
extern bool __NodeConnPool_equalsLastSuccess(NodeConnPool* this, struct in_addr lastSuccessPeerIP,
   NicAddrType_t lastSuccessNicType);
extern bool __NodeConnPool_isLastSuccessInitialized(NodeConnPool* this);
extern bool __NodeConnPool_shouldPrintConnectedLogMsg(NodeConnPool* this,
   struct in_addr currentPeerIP, NicAddrType_t currentNicType);
extern bool __NodeConnPool_shouldPrintConnectFailedLogMsg(NodeConnPool* this,
   struct in_addr currentPeerIP, NicAddrType_t currentNicType);


extern void NodeConnPool_getStats(NodeConnPool* this, NodeConnPoolStats* outStats);
extern unsigned NodeConnPool_getFirstPeerName(NodeConnPool* this, NicAddrType_t nicType,
   ssize_t outBufLen, char* outBuf, bool* outIsNonPrimary);

/**
 * @param localNicList copied
 * @param localNicCaps copied
 * @param localRdmaNicList copied
 */
extern void NodeConnPool_updateLocalInterfaces(NodeConnPool* this, NicAddressList* localNicList,
   NicListCapabilities* localNicCaps, NicAddressList* localRdmaNicList);

// getters & setters
/**
 * Called to lock the internal mute when accessing resources that may be
 * modified by other threads. Case in point: NodeConnPool_getNicListLocked.
 */
static inline void NodeConnPool_lock(NodeConnPool* this);
/**
 * Release the lock acquired by NodeConnPool_lock.
 */
static inline void NodeConnPool_unlock(NodeConnPool* this);
static inline void NodeConnPool_cloneNicList(NodeConnPool* this, NicAddressList* nicList);
static inline NicAddressList* NodeConnPool_getNicListLocked(NodeConnPool* this);
static inline void NodeConnPool_setLocalNicCaps(NodeConnPool* this,
   NicListCapabilities* localNicCaps);
static inline unsigned short NodeConnPool_getStreamPort(NodeConnPool* this);
static inline void NodeConnPool_setLogConnErrors(NodeConnPool* this, bool logConnErrors);
static inline bool __NodeConnPool_getWasLastTimeCompleteFail(NodeConnPool* this);


/**
 * Holds statistics about currently established connections.
 */
struct NodeConnPoolStats
{
   unsigned numEstablishedStd;
   unsigned numEstablishedRDMA;
};

/**
 * Holds state of failed connects to avoid log spamming with conn errors.
 */
struct NodeConnPoolErrorState
{
   struct in_addr lastSuccessPeerIP; // the last IP that we connected to successfully
   NicAddrType_t lastSuccessNicType; // the last nic type that we connected to successfully
   bool wasLastTimeCompleteFail; // true if last attempt failed on all routes
};


/**
 * This class represents a pool of stream connections to a certain node.
 */
struct NodeConnPool
{
   struct App* app;

   NicAddressList nicList;

   NicAddressList localRdmaNicList;
   NicAddressStatsList rdmaNicStatsList;

   ConnectionList connList;

   struct Node* parentNode; // backlink to the node object which to which this conn pool belongs
   unsigned short streamPort;
   NicListCapabilities localNicCaps;

   unsigned availableConns; // available established conns
   unsigned establishedConns; // not equal to connList.size!!
   unsigned maxConns;
   unsigned fallbackExpirationSecs; // expiration time for conns to fallback interfaces
   unsigned maxConcurrentAttempts;
   int      rdmaNicCount;

   NodeConnPoolStats stats;
   NodeConnPoolErrorState errState;

   bool logConnErrors; // false to disable logging during acquireStream()
   bool enableTCPFallback;

   Mutex mutex;
   Condition changeCond;
   struct semaphore connSemaphore;
};

void NodeConnPool_lock(NodeConnPool* this)
{
   Mutex_lock(&this->mutex); // L O C K
}

void NodeConnPool_unlock(NodeConnPool* this)
{
   Mutex_unlock(&this->mutex); // U N L O C K
}

/**
 * Mutex lock must be held while using the returned pointer.
 */
NicAddressList* NodeConnPool_getNicListLocked(NodeConnPool* this)
{
   return &this->nicList;
}

/**
 * Retrieve NICs for the connection pool.
 *
 * @param nicList an uninitialized NicAddressList. Caller is responsible for
 *        memory management.
 */
void NodeConnPool_cloneNicList(NodeConnPool* this, NicAddressList* nicList)
{
   Mutex_lock(&this->mutex); // L O C K
   ListTk_cloneNicAddressList(&this->nicList, nicList, true);
   Mutex_unlock(&this->mutex); // U N L O C K
}

/**
 * @param localNicCaps will be copied
 */
void NodeConnPool_setLocalNicCaps(NodeConnPool* this, NicListCapabilities* localNicCaps)
{
   Mutex_lock(&this->mutex); // L O C K
   this->localNicCaps = *localNicCaps;
   Mutex_unlock(&this->mutex); // U N L O C K
}

unsigned short NodeConnPool_getStreamPort(NodeConnPool* this)
{
   unsigned short retVal;

   Mutex_lock(&this->mutex); // L O C K

   retVal = this->streamPort;

   Mutex_unlock(&this->mutex); // U N L O C K

   return retVal;
}

void NodeConnPool_setLogConnErrors(NodeConnPool* this, bool logConnErrors)
{
   this->logConnErrors = logConnErrors;
}

/**
 * @return true if connection on all available routes failed last time we tried.
 */
bool __NodeConnPool_getWasLastTimeCompleteFail(NodeConnPool* this)
{
   return this->errState.wasLastTimeCompleteFail;
}


#endif /*NODECONNPOOL_H_*/
