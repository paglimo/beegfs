#pragma once

#include <components/buddyresyncer/SyncCandidate.h>
#include <components/chunkbalancer/ChunkBalancerFileSyncSlave.h>

enum ChunkBalancerJobState
{
   ChunkBalancerJobState_NOTSTARTED = 0,
   ChunkBalancerJobState_RUNNING,
   ChunkBalancerJobState_SUCCESS,
   ChunkBalancerJobState_INTERRUPTED,
   ChunkBalancerJobState_FAILURE,
   ChunkBalancerJobState_ERRORS
};

/**
 * A storage chunk balancing job component. Starts, stops and controls the storage chunk balance slaves. 
 * Only one instance can run at a given time. 
 */

class ChunkBalancerJob : public PThread
{
   public:
      ChunkBalancerJob(std::function<void()> shutdown); 
      virtual ~ChunkBalancerJob();

      virtual void run();

      void abort();

   private:
      //set maximum parameters of slaves and queues
      static constexpr unsigned CHUNKBALANCERJOB_MAX_SLAVE_LIMIT = 4;
      static constexpr unsigned CHUNKBALANCERJOB_MAX_FILE_PER_SLAVE_LIMIT = 5000; // number of max items per slave thread
   
      static constexpr unsigned CHUNKBALANCERJOB_SLEEP_TIME = 5; //seconds of idle time before chunkbalancerJob checks status of queues again
      static constexpr unsigned CHUNKBALANCERJOB_MAX_TIME_LIMIT = 600;  //maximum time in seconds of empty queue before ChunkBalancerJob shuts itself and slaves down
   
      uint16_t targetID;
      Mutex statusMutex;
      ChunkBalancerJobState status;

      int64_t startTime;
      int64_t endTime;

      ChunkSyncCandidateStore syncCandidates;

      ChunkBalancerFileSyncSlaveVec ChunkSlaveVec;

      AtomicInt16 shallAbort; // quasi-boolean
      AtomicInt16 targetWasOffline;

      bool createSlaveRes;
      ChunkBalancerFileSyncSlave* createSyncSlave(uint16_t targetID, ChunkSyncCandidateStore* syncCandidates, uint8_t slaveID, bool* createSlaveResPtr);

      void joinSyncSlaves();
      std::function<void()> selfShutdownApp;

   public:
      uint16_t getTargetID() const
      {
         return targetID;
      }

      ChunkBalancerJobState getStatus()
      {
         std::lock_guard<Mutex> mutexLock(statusMutex);
         return status;
      }

      bool isRunning()
      {
         std::lock_guard<Mutex> mutexLock(statusMutex);
         return status == ChunkBalancerJobState_RUNNING;
      }

      void setTargetOffline()
      {
         targetWasOffline.set(1);
      }

      FhgfsOpsErr addChunkSyncCandidate(ChunkSyncCandidateFile* candidate);

      void shutdown();

   private:
      void setStatus(ChunkBalancerJobState status)
      {
         std::lock_guard<Mutex> mutexLock(statusMutex);
         this->status = status;
      }
      
      void selfShutdown()
      {   
         this->selfTerminate();
         selfShutdownApp(); //call the method from app
      }    
};

