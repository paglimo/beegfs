#pragma once

#include <common/storage/mirroring/SyncCandidateStore.h>
#include <common/threading/PThread.h>
#include <components/buddyresyncer/SyncSlaveBase.h>
#include <components/chunkbalancer/SyncCandidate.h>
#include <components/worker/CopyChunkFileWork.h>

#include <mutex>

/**
 * A metadata chunk balancing slave component. Started by the chunk balance job component.  
 * Processes the items in the queues and either updates the striping pattern of an already copied chunk, or forwards chunk information to storage for copying. 
 */

class ChunkBalancerMetaSlave :  public SyncSlaveBase
{

   friend class ChunkBalancerJob; // (to grant access to internal mutex)
   public:
       ChunkBalancerMetaSlave(ChunkBalancerJob& parentJob, ChunkSyncCandidateStore* copyCandidates, uint8_t slaveID);

      virtual ~ChunkBalancerMetaSlave();

   
      uint64_t getNumChunksSynced()
      {
         return numChunksSynced.read();
      }

      uint64_t getErrorCount()
      {
         return errorCount.read();
      }

   private:
   
      AtomicUInt64 numChunksSynced;
      AtomicUInt64 errorCount;
      uint16_t targetID;
     
      ChunkSyncCandidateStore* copyCandidates;
      GlobalInodeLockStore* inodeLockStore;
      
      virtual void syncLoop();
 };     

typedef std::vector<ChunkBalancerMetaSlave*> ChunkBalancerMetaSlaveVec;
typedef ChunkBalancerMetaSlaveVec::iterator ChunkBalancerMetaSlaveVecIter;

