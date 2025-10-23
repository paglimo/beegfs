#include <app/App.h>
#include <common/net/message/storage/creating/RmChunkPathsMsg.h>
#include <common/net/message/storage/creating/RmChunkPathsRespMsg.h>
#include <toolkit/StorageTkEx.h>
#include <program/Program.h>

#include "BuddyResyncerFileSyncSlave.h"

#include <boost/lexical_cast.hpp>

#define PROCESS_AT_ONCE 1
#define SYNC_BLOCK_SIZE (1024*1024) // 1M

BuddyResyncerFileSyncSlave::BuddyResyncerFileSyncSlave(uint16_t targetID,
   ChunkSyncCandidateStore* syncCandidates, uint8_t slaveID) : ChunkFileResyncer(targetID, slaveID)
  

{
   this->chunkFileResyncerMode = CHUNKFILERESYNCER_FLAG_BUDDYMIRROR; // chunk should be copied to buddymir dir 
   this->syncCandidates = syncCandidates;
   this->targetID = targetID;
}

BuddyResyncerFileSyncSlave::~BuddyResyncerFileSyncSlave()
{
}


int BuddyResyncerFileSyncSlave::getFD(const std::unique_ptr<StorageTarget> & target)
{
   
   return *target->getMirrorFD();
}

void BuddyResyncerFileSyncSlave::syncLoop()
{
   App* app = Program::getApp();
   MirrorBuddyGroupMapper* buddyGroupMapper = app->getMirrorBuddyGroupMapper();

   while (! getSelfTerminateNotIdle())
   {
      if((syncCandidates->isFilesEmpty()) && (getSelfTerminate()))
         break;

      ChunkSyncCandidateFile candidate;

      syncCandidates->fetch(candidate, this);

      if (unlikely(candidate.getTargetID() == 0)) // ignore targetID 0
         continue;

      std::string relativePath = candidate.getRelativePath();
      uint16_t localTargetID = candidate.getTargetID();

      // get buddy targetID
      uint16_t buddyTargetID = buddyGroupMapper->getBuddyTargetID(localTargetID);
      // perform sync
      FhgfsOpsErr resyncRes = ChunkFileResyncer::doResync(relativePath, localTargetID, buddyTargetID, chunkFileResyncerMode);
      if (resyncRes == FhgfsOpsErr_SUCCESS)
         numChunksSynced.increase();
      else
      if (resyncRes != FhgfsOpsErr_INTERRUPTED)
         errorCount.increase();
   }
}
