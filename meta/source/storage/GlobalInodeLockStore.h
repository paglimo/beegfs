#pragma once

#include <common/Common.h>
#include <storage/FileInode.h>
#include <components/FileEventLogger.h>


typedef ObjectReferencer<FileInode*> GlobalInodeLockReferencer;
typedef std::map<std::string, GlobalInodeLockReferencer*> GlobalInodeLockMap;
typedef GlobalInodeLockMap::iterator GlobalInodeLockMapIter;

typedef std::map<std::string, float> GlobalInodeTimestepMap;
typedef GlobalInodeTimestepMap::iterator GlobalInodeTimestepMapIter;

/**
 * Global store for file inodes which are locked for referencing.
 * Used for chunk balancing operations.
 * This object initalizes the GlobalInodeLockMap
 * It is used for taking and releasing locks on the inodes of file chunks.
 * * "locking" here means we successfully insert an element into a map.
 */
class GlobalInodeLockStore
{
   public:
      ~GlobalInodeLockStore()
      {
         this->clearLockStore();
      }

      bool insertFileInode(EntryInfo* entryInfo, FileEvent* fileEvent, bool increaseRefCount);
      void releaseFileInode(const std::string& entryID); 
      bool lookupFileInode(EntryInfo* entryInfo); 
      FileInode* getFileInode(EntryInfo* entryInfo);
      FileInode* getFileInodeUnreferenced(EntryInfo* entryInfo);
      void clearLockStore(); 
      void updateInodeLockTimesteps(unsigned int);

   private:
      GlobalInodeLockMap inodes;
      GlobalInodeTimestepMap inodeTimes;
      GlobalInodeTimestepMapIter releaseInodeTimeUnlocked(const std::string& entryID);
      GlobalInodeTimestepMapIter releaseFileInodeUnlocked(const std::string& entryID);
      unsigned decreaseInodeRefCountUnlocked(GlobalInodeLockMapIter& iter);
      FileInode* increaseInodeRefCountUnlocked(GlobalInodeLockMapIter& iter);
      bool resetTimeCounterUnlocked(const std::string& entryID);
   

      RWLock rwlock;
      void clearTimeStoreUnlocked(); 
};

