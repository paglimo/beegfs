#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/storage/chunkbalancing/CpChunkPathsRespMsg.h>
#include <toolkit/StorageTkEx.h>
#include <program/Program.h>
#include <components/chunkbalancer/ChunkBalancerFileSyncSlave.h>

#include "CpChunkPathsMsgEx.h"

bool CpChunkPathsMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "CpChunkPathsMsg incoming"; 
   
   uint16_t targetID = getTargetID();
   uint16_t destinationID = getDestinationID();
   std::string& relativePath = getRelativePath();
   EntryInfo* entryInfo = getEntryInfo();
   FileEvent* fileEvent = getFileEvent();
   StorageTarget* target;
   FhgfsOpsErr cpMsgRes;

   App* app = Program::getApp();
   bool isBuddyMirrorChunk = isMsgHeaderFeatureFlagSet(CPCHUNKPATHSMSG_FLAG_BUDDYMIRROR);
   
   if (isBuddyMirrorChunk)
   { // given source targetID refers to a buddy mirror group ID, turn it into primary targetID
      MirrorBuddyGroupMapper* mirrorBuddies = app->getMirrorBuddyGroupMapper();
      
      targetID = mirrorBuddies->getPrimaryTargetID(targetID);
      if (unlikely(!targetID) )
      { // unknown target
         LogContext(logContext).logErr("Invalid mirror buddy group ID: " +
            std::to_string(getTargetID() ) );
         cpMsgRes = FhgfsOpsErr_UNKNOWNTARGET;
         goto send_response;
      }

      // given destinationID targetID refers to a buddy mirror group ID, turn it into primary targetID
      destinationID = mirrorBuddies->getPrimaryTargetID(destinationID);
      if (unlikely(!destinationID) )
      { // unknown target
         LogContext(logContext).logErr("Invalid mirror buddy group ID: " +
            std::to_string(getDestinationID() ) );
         cpMsgRes = FhgfsOpsErr_UNKNOWNTARGET;
         goto send_response;
      }
   }
   target = app->getStorageTargets()->getTarget(targetID);

   if (!target)
   { // unknown targetID
      if (isBuddyMirrorChunk)
      { /* buddy mirrored file => fail with GenericResp to make the caller retry.
           mgmt will mark this target as (p)offline in a few moments. */
         ctx.sendResponse(
               GenericResponseMsg(GenericRespMsgCode_INDIRECTCOMMERR, "Unknown target ID"));
         return true;
      }

      LogContext(logContext).logErr("Unknown targetID: " + std::to_string(targetID));
      cpMsgRes = FhgfsOpsErr_UNKNOWNTARGET;
      goto send_response;
   }

   { // valid targetID

      // try to start a ChunkBalance  job; if it already exists, we get that job
      ChunkBalancerJob* chunkBalanceJob = CpChunkPathsMsgEx::addChunkBalanceJob();
      ChunkSyncCandidateFile candidate((relativePath).c_str(), targetID, destinationID, entryInfo, isBuddyMirrorChunk, fileEvent);
      cpMsgRes = chunkBalanceJob->addChunkSyncCandidate(&candidate);
   }

send_response:
   ctx.sendResponse(CpChunkPathsRespMsg(cpMsgRes));
   return true;
}

ChunkBalancerJob* CpChunkPathsMsgEx::addChunkBalanceJob()
{
   App* app = Program::getApp();
   std::lock_guard<Mutex> mutexLock(app->ChunkBalanceJobMutex);
      
   ChunkBalancerJob* chunkBalanceJob = app->getChunkBalancerJob();
   if (chunkBalanceJob == nullptr)
   {
      chunkBalanceJob = new ChunkBalancerJob(std::bind(&App::cleanupChunkBalancerJob, app));
      chunkBalanceJob->start();
      LogContext(__func__).log(Log_NOTICE, "Starting ChunkBalanceJob on storage.");
      app->setChunkBalancerJob(chunkBalanceJob);
   }
   return chunkBalanceJob; 
}

