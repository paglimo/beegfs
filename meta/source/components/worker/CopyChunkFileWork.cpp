#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>  
#include <common/threading/PThread.h>
#include <common/net/message/storage/chunkbalancing/CpChunkPathsMsg.h>
#include <common/net/message/storage/chunkbalancing/CpChunkPathsRespMsg.h>
#include <common/net/message/NetMessage.h>
#include <program/Program.h>
#include "CopyChunkFileWork.h"

#include <boost/lexical_cast.hpp>

void CopyChunkFileWork::process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
{
   FhgfsOpsErr commRes = communicate(); 
   *outResult = commRes;
   counter->incCount();
}

FhgfsOpsErr CopyChunkFileWork::communicate()
{
   const char* logContext = "Copy chunk file work for chunk balancing";

   App* app = Program::getApp();
   NodeStoreServers* storageNodes = app->getStorageNodes();
   TargetMapper* targetMapper = app->getTargetMapper();
   MirrorBuddyGroupMapper* buddyMapper = app->getStorageBuddyGroupMapper();

   UInt16List buddyGroupIDs;
   UInt16List buddyPrimaryTargetIDs;
   UInt16List buddySecondaryTargetIDs;

   // prepare request message
   CpChunkPathsMsg CpChunkMsg(sourceTargetID, destinationTargetID, &entryInfo,  &relPath, &fileEvent);
   CpChunkMsg.addMsgHeaderFeatureFlag(CPCHUNKPATHSMSG_FLAG_HAS_EVENT);

   if (mirroredChunk)
   {
      buddyMapper->getMappingAsLists(buddyGroupIDs, buddyPrimaryTargetIDs,  buddySecondaryTargetIDs);
      //check if both targetIDs are valid buddyGroupIDs
      if (isTargetIDMirrored(buddyGroupIDs, sourceTargetID)
         && isTargetIDMirrored(buddyGroupIDs, destinationTargetID) )
      {
         CpChunkMsg.addMsgHeaderFeatureFlag(CPCHUNKPATHSMSG_FLAG_BUDDYMIRROR);
      }
      else
      {
         LogContext(logContext).log(Log_WARNING,
        "Copying chunk file from target failed. The file is mirrored but one of the targets is not a valid mirror group ID." 
        "TargetID:  " + std::to_string(sourceTargetID)  + " ; "
        "Relative path: " + relPath);  
        return FhgfsOpsErr_INTERRUPTED;
      }
   }
   RequestResponseTarget storageTarget(sourceTargetID, targetMapper, storageNodes);
   storageTarget.setTargetStates(app->getTargetStateStore() );
   
   if (mirroredChunk)
      storageTarget.setMirrorInfo(buddyMapper, false);

   RequestResponseArgs rrArgs(NULL, &CpChunkMsg, NETMSGTYPE_CpChunkPathsResp);

   FhgfsOpsErr requestRes  = MessagingTk::requestResponseTarget(&storageTarget, &rrArgs);
   if (unlikely(requestRes != FhgfsOpsErr_SUCCESS) )
   {
      LogContext(logContext).log(Log_WARNING,
         "Communication with storage target failed. " +
          std::string( (mirroredChunk) ? "Mirror " : "") +
         "TargetID: " + StringTk::uintToStr(sourceTargetID) + "; ");   
      return FhgfsOpsErr_COMMUNICATION;
   }
   
   // correct response type received
   CpChunkPathsRespMsg* CpChunkPathsRespMsgCast = (CpChunkPathsRespMsg*)rrArgs.outRespMsg.get();
   FhgfsOpsErr cpChunkRes = CpChunkPathsRespMsgCast->getResult();
   if (cpChunkRes != FhgfsOpsErr_SUCCESS)
   { // error: chunk file copy operation not started  
     LogContext(logContext).log(Log_WARNING,
        "Copying chunk file from target failed." 
        "TargetID:  " + std::to_string(sourceTargetID)  + " ; "
        "Relative path: " + relPath);     
   }
   return cpChunkRes;
}

bool CopyChunkFileWork::isTargetIDMirrored(UInt16List& buddyGroupIDs, uint16_t& targetID)
{
   return (std::find(buddyGroupIDs.begin(), buddyGroupIDs.end(), targetID) != buddyGroupIDs.end());
}
