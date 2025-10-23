#pragma once

#include <common/components/worker/Work.h>
#include <common/Common.h>
#include <common/storage/StorageErrors.h>  
#include <common/toolkit/SynchronizedCounter.h>
#include <common/storage/FileEvent.h>

 
class CopyChunkFileWork : public Work
{
   public:
   
      CopyChunkFileWork(uint16_t sourceTargetID, uint16_t destinationTargetID,  
         std::string relPath, EntryInfo* entryInfo, bool mirroredChunk,   FhgfsOpsErr* outResult, SynchronizedCounter* counter, FileEvent* fileEvent) :
         sourceTargetID(sourceTargetID), destinationTargetID(destinationTargetID), relPath(relPath), entryInfo(*entryInfo), 
         mirroredChunk(mirroredChunk), outResult(outResult), counter(counter), fileEvent(*fileEvent)
      {
         // all assignments done in initializer list
      }

      virtual ~CopyChunkFileWork() {}

      virtual void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen);

   private:

      uint16_t sourceTargetID;
      uint16_t destinationTargetID;
      std::string relPath;
      EntryInfo entryInfo; 
      bool mirroredChunk;

      FhgfsOpsErr* outResult;
      FhgfsOpsErr communicate();
      bool isTargetIDMirrored(UInt16List& buddyGroupIDs, uint16_t& targetID);
      SynchronizedCounter* counter;
      FileEvent fileEvent;
};

