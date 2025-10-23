#pragma once

#include <common/storage/mirroring/SyncCandidateStore.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/FileEvent.h>

#include <string>

/**
 * A storage sync candidate. Has a target ID and a path.
 */
class ChunkSyncCandidateDir
{
   public:
      ChunkSyncCandidateDir(const std::string& relativePath, const uint16_t targetID)
         : relativePath(relativePath), targetID(targetID)
      { }

      ChunkSyncCandidateDir(const std::string& relativePath, const uint16_t targetID, const uint16_t destinationID, EntryInfo* entryInfo, bool isBuddyMirrorChunk, FileEvent* fileEvent)
         : relativePath(relativePath), targetID(targetID), destinationID(destinationID), entryInfo(*entryInfo), isBuddyMirrorChunk(isBuddyMirrorChunk), fileEvent(*fileEvent)
      { }

      ChunkSyncCandidateDir()
         : targetID(0)
      { }

   private:
      std::string relativePath;
      uint16_t targetID;
      uint16_t destinationID;
      EntryInfo entryInfo;
      bool isBuddyMirrorChunk;
      FileEvent fileEvent;


   public:
      const std::string& getRelativePath() const { return relativePath; }
      uint16_t getTargetID() const               { return targetID; }
      uint16_t getDestinationID() const          { return destinationID; }
      EntryInfo* getEntryInfo()                  { return &entryInfo; }
      bool isChunkBuddyMirrored() const          { return isBuddyMirrorChunk; }
      FileEvent* getFileEvent()                  { return &fileEvent; }
};

/**
 * A storage sync candidate that also has an onlyAttribs flag.
 */
class ChunkSyncCandidateFile : public ChunkSyncCandidateDir
{
   public:
      ChunkSyncCandidateFile(const std::string& relativePath, uint16_t targetID)
         : ChunkSyncCandidateDir(relativePath, targetID)
      { }

      ChunkSyncCandidateFile(const std::string& relativePath, uint16_t targetID, uint16_t destinationID,  EntryInfo* entryInfo, bool isBuddyMirrorChunk, FileEvent* fileEvent)   
         : ChunkSyncCandidateDir(relativePath, targetID, destinationID, entryInfo, isBuddyMirrorChunk, fileEvent)
      { }

      ChunkSyncCandidateFile() = default;
};

typedef SyncCandidateStore<ChunkSyncCandidateDir, ChunkSyncCandidateFile> ChunkSyncCandidateStore;

