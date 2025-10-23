#pragma once

#include <common/storage/mirroring/SyncCandidateStore.h>
#include <common/nodes/Node.h>
#include <common/storage/StorageErrors.h>
#include <common/threading/PThread.h>
#include <components/buddyresyncer/ChunkFileResyncer.h>

#include <mutex>

class BuddyResyncerFileSyncSlave : public ChunkFileResyncer
{
   friend class BuddyResyncer; // (to grant access to internal mutex)
   friend class BuddyResyncJob; // (to grant access to internal mutex)

   public:
      BuddyResyncerFileSyncSlave(uint16_t targetID, ChunkSyncCandidateStore* syncCandidates,
         uint8_t slaveID);
      virtual ~BuddyResyncerFileSyncSlave();
      
   private:

      ChunkSyncCandidateStore* syncCandidates;
      
      virtual void syncLoop();
      virtual int getFD(const std::unique_ptr<StorageTarget> & target);

};
  
typedef std::list<BuddyResyncerFileSyncSlave*> BuddyResyncerFileSyncSlaveList;
typedef BuddyResyncerFileSyncSlaveList::iterator BuddyResyncerFileSyncSlaveListIter;

typedef std::vector<BuddyResyncerFileSyncSlave*> BuddyResyncerFileSyncSlaveVec;
typedef BuddyResyncerFileSyncSlaveVec::iterator BuddyResyncerFileSyncSlaveVecIter;

