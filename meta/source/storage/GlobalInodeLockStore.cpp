#include <common/threading/UniqueRWLock.h>
#include <common/threading/RWLockGuard.h>
#include <program/Program.h>
#include "GlobalInodeLockStore.h"

/**
 * 
 * Note: This does not increase the inode reference count, only adds the inode to the lock store. 
 * * @return false if inode creation failed, true if file was inserted or if file is already in store
 */
bool GlobalInodeLockStore::insertFileInode(EntryInfo* entryInfo, FileEvent* fileEvent = nullptr, bool increaseRefCount = true)
{
   RWLockGuard lock(rwlock, SafeRWLock_WRITE);
   std::string entryID = entryInfo->getEntryID();
   GlobalInodeLockMapIter iter =  this->inodes.find(entryID);
   App* app = Program::getApp();
   FileEventLogger* eventLogger = app->getFileEventLogger();
   bool insertRes = true;

   if(iter == this->inodes.end())
   { // not in map yet => try to insert it in map
      LOG_DBG(CHUNKBALANCING, SPAM, "Insert file inode in GlobalInodeLockStore.", ("EntryID", entryID));
      FileInode* inode = FileInode::createFromEntryInfo(entryInfo);
      if(!inode)
         return false;
      auto insertRes = this->inodes.insert(GlobalInodeLockMap::value_type(entryID, new GlobalInodeLockReferencer(inode)));
      this->inodeTimes.insert(GlobalInodeTimestepMap::value_type(entryID, 0.0));
      iter = insertRes.first; //update iter to newly inserted element

      if (eventLogger && fileEvent)
      {
         EventContext eventCtx = makeEventContext(
            entryInfo,
            entryInfo->getParentEntryID(),
            0,  //using root user ID 
            "",
            inode ? inode->getNumHardlinks() : 0,
            false  // GlobalInodeLockStore only used on primary
         );
         logEvent(eventLogger , *fileEvent, eventCtx); //notify event listener of inode being locked
      }
   }
   
   if (increaseRefCount)
   {
      increaseInodeRefCountUnlocked(iter); //increase reference count on inode
   }

   return insertRes;
}

/**
 *  Take the write lock and decrease the inode reference count. 
 *  If we are the only one with a reference, release the inode from the lock store
 */
void GlobalInodeLockStore::releaseFileInode(const std::string& entryID)
{
   RWLockGuard lock(rwlock, SafeRWLock_WRITE);
   releaseFileInodeUnlocked(entryID);
}

/**
 * Note: SafeRWLock_WRITE lock should be held here
 * * @return Iterator to timestep map:
*        - If inode not found: returns find() result (may be end())
*        - If inode found and deleted: returns next iterator after erase
*        - If inode found but not deleted: returns find() result
*/
GlobalInodeTimestepMapIter GlobalInodeLockStore::releaseFileInodeUnlocked(const std::string& entryID)
{
   GlobalInodeLockMapIter iter =  this->inodes.find(entryID);

   if(iter == this->inodes.end() )
   { 
      LogContext(__func__).log(LogTopic_CHUNKBALANCING,  Log_WARNING,"Failed to release lock on inode. Inode not found in lock store. entryID:" + entryID); 
      return this->inodeTimes.find(entryID);
   }
   GlobalInodeLockReferencer* inodeRefer = iter->second;
   unsigned refCount = inodeRefer->getRefCount();

   // inode is in the map, decrease the reference counter

   if (refCount > 0) // possible that refCount=0 in case of crash, do not decrease counter in that case
   {
      refCount = decreaseInodeRefCountUnlocked(iter);
   }

   if(!refCount)
   { // dropped last reference => unload Inode
      SAFE_DELETE(inodeRefer);
      this->inodes.erase(iter);
      //only release the inode from time store if refCount=0
      return this->releaseInodeTimeUnlocked(entryID);
   }

   return this->inodeTimes.find(entryID);
}


/**
 * Note: SafeRWLock_WRITE lock should be held here
 * 
 * @return next iter if we found the file in the time store, original iter if file was not in time store at all
 */
GlobalInodeTimestepMapIter GlobalInodeLockStore::releaseInodeTimeUnlocked(const std::string& entryID)
{
   GlobalInodeTimestepMap::iterator iterTime = this->inodeTimes.find(entryID);
   if(iterTime == this->inodeTimes.end() )
   {
      LogContext(__func__).log(LogTopic_CHUNKBALANCING,  Log_WARNING,"Issue while releasing  lock on inode. Inode removed from lock store but not in time store. entryID:" + entryID); 
      return iterTime;
   }
   return this->inodeTimes.erase(iterTime);
}

/**
 * @return true if file is in the store, false if it is not
 */
bool GlobalInodeLockStore::lookupFileInode(EntryInfo* entryInfo) 
{
   RWLockGuard lock(rwlock, SafeRWLock_READ);
   auto iter = this->inodes.find(entryInfo->getEntryID());
   return (iter != this->inodes.end());
}

/**
 * Note: Remember to call releaseInode() after
 * Get inode object and increase reference counter
 * 
 * @return Inode object
 */
FileInode* GlobalInodeLockStore::getFileInode(EntryInfo* entryInfo)
{
   std::string entryID = entryInfo->getEntryID();
   RWLockGuard lock(rwlock, SafeRWLock_WRITE);
   GlobalInodeLockMapIter iter =  this->inodes.find(entryID);
   
   if(iter != this->inodes.end() )
   { 
      FileInode* inode = increaseInodeRefCountUnlocked(iter);
      // reset timer since we take a new reference on the inode
      resetTimeCounterUnlocked(entryID);
      return inode;
   }
   return nullptr;
}

/**
 * Get Inode object without increasing reference counter.
 * Used to obtain the inode if refCount is already increased.
 * For first access to the inode, use getFIleInode() which increases the refCount. 
 * @return Inode object
 */
FileInode* GlobalInodeLockStore::getFileInodeUnreferenced(EntryInfo* entryInfo)
{
   std::string entryID = entryInfo->getEntryID();
   RWLockGuard lock(rwlock, SafeRWLock_READ);
   GlobalInodeLockMapIter iter =  this->inodes.find(entryID);
   
   if(iter != this->inodes.end() )
   { 
      FileInode* inode = iter->second->getReferencedObject();
      return inode;
   }
   return nullptr;
}

/**
 * The parent thread that locks an inode (ChunkBalancerJob or other) should periodically call this method to track how long an inode has been locked by a thread. 
 * The time is tracked by an incremented counter. 
 * If an inode is locked longer than a time limit, it is forcefully released from the GlobalInodeLockStore (case of crash or unexpected error).
 * The input parameter maxTimeLimitCounter represents the maximum number of iterations this method is called by the parent thread before the inode is forcefully released due to timeout. 
 */
void GlobalInodeLockStore::updateInodeLockTimesteps(unsigned int maxTimeLimitCounter)
{
   RWLockGuard lock(rwlock, SafeRWLock_WRITE);
   for(GlobalInodeTimestepMap::iterator iter = this->inodeTimes.begin(); iter !=  this->inodeTimes.end();)
   {
      iter->second ++; //update the timing counter
      if (iter->second > maxTimeLimitCounter)  //check if timeing counter is higher than the maxTimeLimitCounter value, if yes release lock
      {
         std::string entryID = iter->first;
         LogContext(__func__).log(LogTopic_CHUNKBALANCING,  Log_WARNING,"Releasing lock on inode after timeout. entryID:" 
            + entryID + "; There may be orphaned chunks in the system, "
            "you may want to perform a filesystem check to ensure all additional chunks are deleted." ); 
         iter = this->releaseFileInodeUnlocked(entryID); 
      }
      else 
      {
         ++iter;
      }
   }
}

void GlobalInodeLockStore::clearLockStore()
{
   
   RWLockGuard lock(rwlock, SafeRWLock_WRITE);
   LOG_DBG(CHUNKBALANCING, DEBUG, "Releasing all locks on inodes in GlobalInodeLockStore.", ( "# of loaded entries to be cleared:", std::to_string(inodes.size())));
         
   if (!inodes.empty())
   {
      LogContext(__func__).log(LogTopic_CHUNKBALANCING,  Log_WARNING, "Releasing locks on all inodes in GlobalInodeLockStore. The store is not empty."
         "There may be orphaned chunks in the system, you may want to perform a filesystem check to ensure all additional chunks are deleted." );
   }
   for(GlobalInodeLockMapIter iter = this->inodes.begin(); iter != this->inodes.end(); iter++)
   {
      SAFE_DELETE(iter->second);       //delete inode pointer
   }
   this->inodes.clear();
   //clear Timestep store
   this->clearTimeStoreUnlocked();
}

// Note: SafeRWLock_WRITE lock should be held here
void GlobalInodeLockStore::clearTimeStoreUnlocked()
{
   for(GlobalInodeTimestepMap::iterator iter = this->inodeTimes.begin(); iter != this->inodeTimes.end();)
   {
      iter = this->inodeTimes.erase(iter); // return next iterator after erase and assign to `iter` variable
   }
}


/**
 * Decrease the inode reference counter using the given iter.
 *
 * Note: GlobalInodeLockStore needs to be write-locked.
 *
 * @return number of inode references after release()
 */
unsigned GlobalInodeLockStore::decreaseInodeRefCountUnlocked(GlobalInodeLockMapIter& iter)
{

   unsigned refCount = (unsigned) iter->second->release();

   LogContext(__func__).log(LogTopic_CHUNKBALANCING, Log_DEBUG, "Release file inode. EntryID:" + iter->first + "; Refcount:" +  std::to_string(iter->second->getRefCount()));

   return refCount;
}

/**
 * Increase the inode reference counter using the given iter.
 *
 * Note: GlobalInodeLockStore needs to be write-locked.
 *
 * @return FileInode object
 */
FileInode* GlobalInodeLockStore::increaseInodeRefCountUnlocked(GlobalInodeLockMapIter& iter)
{
   if (unlikely(iter == this->inodes.end() ) )
      return nullptr;

   GlobalInodeLockReferencer* inodeRefer = iter->second;
   FileInode* inode = inodeRefer->reference();

   LogContext(__func__).log(LogTopic_CHUNKBALANCING, Log_DEBUG, "Reference file inode. EntryID:" + iter->first + "; Refcount:" +  std::to_string(inodeRefer->getRefCount()));

   return inode;
}

/**
 * Reset timer that tracks time that Inode has been locked in time store
 *
 * Note: GlobalInodeLockStore needs to be write-locked.
 *
 */
bool GlobalInodeLockStore::resetTimeCounterUnlocked(const std::string& entryID)
{     
   GlobalInodeTimestepMap::iterator iterTime = this->inodeTimes.find(entryID);
   if (unlikely(iterTime == this->inodeTimes.end() ) )
   {
      LOG_DBG(CHUNKBALANCING, DEBUG, "Unable to reset time counter in GlobalInodeLockStore time store."
         "Locked file may be released earlier than expected.");

      return false;
   }
   iterTime->second = 0;
   return true;
}
