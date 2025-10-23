#include <common/toolkit/StringTk.h>
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
   for (ChunkBalancerFileSyncSlaveVecIter iter = ChunkSlaveVec.begin(); iter != ChunkSlaveVec.end();
      iter++)
   {
      ChunkBalancerFileSyncSlave* slave = *iter;
      SAFE_DELETE(slave);
   }
}

void ChunkBalancerJob::run()
{
   const char* logContext = "ChunkBalanceJob running";
   int64_t time_checks = 0; 
   // make sure only one job at a time can run!
   {
      std::lock_guard<Mutex> mutexLock(statusMutex);
      
      if (status == ChunkBalancerJobState_RUNNING)
      {
         LogContext(__func__).logErr("Refusing to run same ChunkBalancerJob twice!");
         return;
      }
      else
      {
         status = ChunkBalancerJobState_RUNNING;
         startTime = time(NULL);
         endTime = 0;
         ChunkSlaveVec.push_back(createSyncSlave(targetID,  &syncCandidates, 0, &createSlaveRes)); //create first slave
                     
         if (!createSlaveRes)
         {
            LogContext(logContext).log(LogTopic_CHUNKBALANCING,  Log_WARNING,"Unable to start first chunk balance slave."); 
            status = ChunkBalancerJobState_FAILURE;                
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
      if (syncCandidates.getNumFiles() >=  CHUNKBALANCERJOB_MAX_FILE_PER_SLAVE_LIMIT) // check if queue per slave is full, if yes spawn more slave threads
      {
         time_checks = 0;
         
         if (ChunkSlaveVec.size() >= CHUNKBALANCERJOB_MAX_SLAVE_LIMIT)  // check if number of slaves is max
         {
            LogContext(logContext).log(LogTopic_CHUNKBALANCING,  Log_NOTICE,"Unable to start chunk balance slave; Number of slaves is equal to MAX_SLAVE_LIMIT."); 
         }
         else
         {
            //start another chunk balance slave if queue is full and number of slaves is less then max
            ChunkSlaveVec.push_back(createSyncSlave(targetID,  &syncCandidates, ChunkSlaveVec.size(), &createSlaveRes)); 
            
            if (!createSlaveRes)
            {
               //if failed, log a message 
               LogContext(logContext).log(LogTopic_CHUNKBALANCING,  Log_WARNING,"Unable to start additional chunk balance slave."); 
            }
         }
      }
      else if (syncCandidates.getNumFiles()==0)  //check if queue is empty and increment timing counter
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
         //iterate backwards to avoid index shifting when erasing elements
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
            ChunkSlaveVec.push_back(createSyncSlave(targetID, &syncCandidates, ChunkSlaveVec.size(), &createSlaveRes)); //create slave if there is not one running        
            if (!createSlaveRes)
            {
               LogContext(logContext).log(LogTopic_CHUNKBALANCING,  Log_WARNING,"Unable to start chunk balance slave."); 
               setStatus(ChunkBalancerJobState_FAILURE);                
               goto cleanup;
            }   
         }
      }
      sleep(CHUNKBALANCERJOB_SLEEP_TIME);
   }

cleanup:

   // wait for sync slaves to stop and save if any errors occured
   for (ChunkBalancerFileSyncSlaveVecIter iter = ChunkSlaveVec.begin();
      iter != ChunkSlaveVec.end(); iter++)
   {
      ChunkBalancerFileSyncSlave* slave = *iter;
      if (slave)
      {
         {
            std::lock_guard<Mutex> safeLock(slave->statusMutex);
            while (slave->isRunning)  
               slave->isRunningChangeCond.wait(&(slave->statusMutex));
         }

         if (slave->getErrorCount() != 0)
         {
            setStatus(ChunkBalancerJobState_ERRORS);
         }
      }
   }

   endTime = time(NULL);
   selfShutdown(); //call the shutdown method from app
}

ChunkBalancerFileSyncSlave*  ChunkBalancerJob::createSyncSlave(uint16_t targetID, ChunkSyncCandidateStore* syncCandidates, uint8_t slaveID, bool* createSlaveResPtr)
{    
      ChunkBalancerFileSyncSlave* slave = new ChunkBalancerFileSyncSlave(targetID,syncCandidates, slaveID); 
      try
      {
         slave->resetSelfTerminate();
         slave->start();
         slave->setIsRunning(true);
         *createSlaveResPtr = true;
      }
      catch (PThreadCreateException& e)
      {
         LogContext(__func__).logErr(std::string("Unable to start thread: ") + e.what());

         // stop already started sync slave
         slave->selfTerminate();
         *createSlaveResPtr = false;
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
   Config* config = app->getConfig();
   if (syncCandidates.getNumFiles() >=  config->getTuneChunkBalanceQueueLimit()) // check if total queue is full
   {
      LogContext(__func__).log(Log_WARNING, "Chunk Balancing queue is full, cannot add more chunks for balancing.");
      return FhgfsOpsErr_AGAIN;
   }
   syncCandidates.add(*candidate, this); 
   return FhgfsOpsErr_SUCCESS;
}


void ChunkBalancerJob::shutdown()
{
   for (size_t i = 0; i < ChunkSlaveVec.size(); i++)
   {
      ChunkSlaveVec[i]->selfTerminate(); // terminate all slave threads 
   }
   joinSyncSlaves(); //wait for all slave threads to finish
   endTime = time(NULL);
   this->selfTerminate();
}