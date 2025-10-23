#include <components/chunkbalancer/ChunkBalancerMetaSlave.h>
#include <program/Program.h>


#include "ChunkBalancerJob.h"

ChunkBalancerJob::ChunkBalancerJob(std::function<void()> shutdown) :
   PThread("ChunkBalancerJob"),
   status(ChunkBalancerJobState_NOTSTARTED), 
   startTime(0), endTime(0),
   selfShutdownApp(shutdown)
   {}

ChunkBalancerJob::~ChunkBalancerJob()
{
   for (ChunkBalancerMetaSlaveVecIter iter = ChunkSlaveVec.begin(); iter != ChunkSlaveVec.end();
      iter++)
   {
      ChunkBalancerMetaSlave* slave = *iter;
      SAFE_DELETE(slave);
   }
}

void ChunkBalancerJob::run()
{
   const char* logContext = "ChunkBalanceJob running";
   int64_t time_checks = 0; 

   App* app = Program::getApp();
   Config* cfg = app->getConfig();
   unsigned CHUNKBALANCERJOB_MAX_LOCK_TIME_LIMIT = cfg->getTuneChunkBalanceLockingTimeLimit(); // maximum time in seconds of files locked before ChunkBalancerJob releases lock (configurable)
   unsigned CHUNKBALANCERJOB_MAX_TIME_LIMIT = 1.6 * CHUNKBALANCERJOB_MAX_LOCK_TIME_LIMIT; // maximum time in  seconds of empty queues before ChunkBalancerJob shuts itself and slaves down

   MetaStore* metaStore = app->getMetaStore();
   inodeLockStore = metaStore->getInodeLockStore();  

   // make sure only one job at a time can run!
   {
      std::lock_guard<Mutex> mutexLock(statusMutex);
      
      if (status == ChunkBalancerJobState_RUNNING)  //check if there is already a chunkbalancer job running
      {
         LogContext(__func__).logErr("Refusing to run same ChunkBalancerJob twice!");
         return;
      }
      else
      {
         status = ChunkBalancerJobState_RUNNING;
         ChunkSlaveVec.push_back(createSyncSlave( &copyCandidates, 0, &createSlaveRes)); //create first chunk balance slave
                     
         if (!createSlaveRes)
         {
            LogContext(logContext).log(LogTopic_CHUNKBALANCING,  Log_WARNING,"Unable to start first chunk balance slave."); 
            setStatus(ChunkBalancerJobState_FAILURE);                
            goto cleanup;
         }
      }
   }

   while (true) 
   {
      if (getSelfTerminate())
      {
         setStatus(ChunkBalancerJobState_INTERRUPTED);
         break;
      }
      if (copyCandidates.getNumFiles() >=  CHUNKBALANCERJOB_MAX_FILE_PER_SLAVE_LIMIT ) // check if queue is full
      {
         time_checks = 0;
         
         if (ChunkSlaveVec.size() >= CHUNKBALANCERJOB_MAX_SLAVE_LIMIT) 
         {
            LogContext(logContext).log(LogTopic_CHUNKBALANCING, Log_NOTICE,"Unable to start chunk balance slave; Number of slaves is equal to MAX_SLAVE_LIMIT."); 
         }
         else
         {
            //start another chunk balance slave if queue is full and number of slaves is less then max
            ChunkSlaveVec.push_back(createSyncSlave(&copyCandidates, ChunkSlaveVec.size(), &createSlaveRes)); 
            
            if (!createSlaveRes)
            {
               LogContext(logContext).log(LogTopic_CHUNKBALANCING,  Log_WARNING,"Unable to start additional chunk balance slave."); 
            }
         }
      }
      else if(copyCandidates.getNumFiles()==0 )  //check if queue is empty and if yes increment timing counter
      {
         time_checks ++;  

         if (time_checks > CHUNKBALANCERJOB_MAX_TIME_LIMIT / CHUNKBALANCERJOB_SLEEP_TIME)  //if queue is empty for sufficient time, shut down
         {
            for (size_t i = 0; i < ChunkSlaveVec.size(); i++)
            {
               ChunkSlaveVec[i]->selfTerminate();  
            }
            setStatus(ChunkBalancerJobState_SUCCESS);   
            LogContext(logContext).log(LogTopic_CHUNKBALANCING,  Log_NOTICE,"Shutting down ChunkBalanceJob and Slave components after timeout. "); 
            break;
         }
      }
      else   // normal operation when there are items in queue, reset timer
      {
         time_checks = 0;
         // Iterate backwards to avoid index shifting when erasing elements
         for (int i = static_cast<int>(ChunkSlaveVec.size()) - 1; i >= 0; i--)
         {   
            if (!(ChunkSlaveVec[i]->getIsRunning())) // check if slaves are running, if not, terminate them
            {
               LogContext(logContext).log(LogTopic_CHUNKBALANCING, Log_NOTICE,"Chunk Balance slave (ID:"+ std::to_string(i)+ ") is not running and will be terminated");
               ChunkSlaveVec[i]->selfTerminate(); 
               SAFE_DELETE(ChunkSlaveVec[i]);  //delete slave from vector
               ChunkSlaveVec.erase(ChunkSlaveVec.begin() + i);

            }   
         }
      
         if (ChunkSlaveVec.empty())
         {
            ChunkSlaveVec.push_back(createSyncSlave(&copyCandidates, ChunkSlaveVec.size(), &createSlaveRes)); //create slave if there is not one running        
            if (!createSlaveRes)
            {
               LogContext(logContext).log(LogTopic_CHUNKBALANCING,  Log_WARNING,"Unable to start chunk balance slave."); 
               setStatus(ChunkBalancerJobState_FAILURE);                
               goto cleanup;
            }   
         }
      }
      inodeLockStore->updateInodeLockTimesteps(CHUNKBALANCERJOB_MAX_LOCK_TIME_LIMIT / CHUNKBALANCERJOB_SLEEP_TIME);
      sleep(CHUNKBALANCERJOB_SLEEP_TIME);
   }

cleanup:

   // wait for sync slaves to stop and save if any errors occured
   for (ChunkBalancerMetaSlaveVecIter iter = ChunkSlaveVec.begin();
      iter != ChunkSlaveVec.end(); iter++)
   {
      ChunkBalancerMetaSlave* slave = *iter;
      if (slave)
      {
         {
            std::lock_guard<Mutex> safeLock(slave->stateMutex);
            while (slave->isRunning) 
               slave->isRunningChangeCond.wait(&(slave->stateMutex));
         }

         if (slave->getErrorCount() != 0)
         {
            setStatus(ChunkBalancerJobState_ERRORS);
         }
      }
   }
   inodeLockStore->clearLockStore(); //clear the inode locking store
   endTime = time(NULL);
   selfShutdown(); //call the shutdown method from app
}

ChunkBalancerMetaSlave*  ChunkBalancerJob::createSyncSlave(ChunkSyncCandidateStore* copyCandidates, uint8_t slaveID, bool* createSlaveRes)
{   
      ChunkBalancerMetaSlave* slave = new ChunkBalancerMetaSlave(*this, copyCandidates, slaveID); 
      try
      {
         slave->resetSelfTerminate();
         slave->start();
         slave->setIsRunning(true);
         *createSlaveRes = true;
      }
      catch (PThreadCreateException& e)
      {
         LogContext(__func__).logErr(std::string("Unable to start thread: ") + e.what());

         // stop already started sync slave
         slave->selfTerminate();
         *createSlaveRes = false;
      }
      return slave;
}

void ChunkBalancerJob::joinSyncSlaves()
{
   for (size_t i = 0; i < ChunkSlaveVec.size(); i++)
      ChunkSlaveVec[i]->join();
}


FhgfsOpsErr ChunkBalancerJob::addChunkSyncCandidate(ChunkSyncCandidateFile* candidate) 
{
   App* app = Program::getApp();
   Config* cfg = app->getConfig();
   if (this->copyCandidates.getNumFiles() >= cfg->getTuneChunkBalanceQueueLimit())
   {
      LogContext(__func__).log(Log_WARNING, "Chunk Balancing queue is full, cannot add more chunks for balancing.");
      return FhgfsOpsErr_AGAIN;
   }
   copyCandidates.add(*candidate, this); 
   return FhgfsOpsErr_SUCCESS;
}


void ChunkBalancerJob::shutdown()
{  
   for (size_t i = 0; i < ChunkSlaveVec.size(); i++)
   {
      ChunkSlaveVec[i]->selfTerminate(); // terminate all slave threads 
   }
   joinSyncSlaves(); //wait for all slave threads to finish
   inodeLockStore->clearLockStore(); //clear the inode locking store
   endTime = time(NULL);
   this->selfTerminate();
}