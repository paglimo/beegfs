#pragma once

#include <common/storage/mirroring/SyncCandidateStore.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/FileEvent.h>


#include <string>


enum IdType:  uint8_t
{
   idType_INVALID = 0,  // invalid id value
   idType_TARGET  = 1,  // targetIDs given
   idType_GROUP   = 2,  // groupIDs given
   idType_POOL    = 3,  // poolIDs given
};

/**
 * A metadata chunk balancing sync candidate. Has a target ID, a path, a destination id and EntryInfo.
 */
class ChunkSyncCandidateDir  
{
   public:
      ChunkSyncCandidateDir(IdType  idType, const std::string& relativePath,  uint16_t targetID, uint16_t destinationID,  EntryInfo* entryInfo, FileEvent* fileEvent)
         : idType(static_cast<IdType>(idType)), relativePath(relativePath), targetID(targetID), destinationID(destinationID), entryInfo(*entryInfo), fileEvent(*fileEvent)

      { }

      ChunkSyncCandidateDir() = default;

   private:
      IdType  idType;
      std::string relativePath;
      uint16_t targetID;
      uint16_t destinationID;
      EntryInfo entryInfo;
      FileEvent fileEvent; 

   public:
      const std::string& getRelativePath() const { return relativePath; }
      IdType getIdType() const                   { return idType; }
      uint16_t getTargetID() const               { return targetID; }
      uint16_t getDestinationID() const          { return destinationID; }
      EntryInfo* getEntryInfo()                  { return &entryInfo; }
      FileEvent* getFileEvent() 
      { 
         return &fileEvent; // returns nullptr if no file event is set
      }

};

class ChunkSyncCandidateFile : public ChunkSyncCandidateDir
{
   public:

      ChunkSyncCandidateFile(IdType  idType, const std::string& relativePath, uint16_t targetID, uint16_t destinationID, EntryInfo* entryInfo, FileEvent* fileEvent)
         : ChunkSyncCandidateDir(idType, relativePath, targetID, destinationID, entryInfo, fileEvent
      )
      { }

      ChunkSyncCandidateFile() = default;
};

typedef SyncCandidateStore<ChunkSyncCandidateDir, ChunkSyncCandidateFile> ChunkSyncCandidateStore;