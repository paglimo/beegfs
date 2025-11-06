#pragma once

#include <common/storage/striping/StripePattern.h>
#include <common/storage/RemoteStorageTarget.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/StorageErrors.h>
#include <common/storage/PathInfo.h>
#include <common/storage/StatData.h>
#include <common/storage/striping/ChunkFileInfo.h>
#include <common/threading/UniqueRWLock.h>
#include <common/threading/SafeRWLock.h>
#include <common/threading/Condition.h>
#include <common/threading/PThread.h>
#include <common/toolkit/TimeAbs.h>
#include <common/app/log/LogContext.h>
#include <common/Common.h>
#include <session/LockingNotifier.h>
#include "Locking.h"
#include "MetadataEx.h"
#include "DiskMetaData.h"
#include "DentryStoreData.h"
#include "FileInodeStoreData.h"


typedef std::vector<DynamicFileAttribs> DynamicFileAttribsVec;
typedef DynamicFileAttribsVec::iterator DynamicFileAttribsVecIter;
typedef DynamicFileAttribsVec::const_iterator DynamicFileAttribsVecCIter;

typedef std::vector<ChunkFileInfo> ChunkFileInfoVec;
typedef ChunkFileInfoVec::iterator ChunkFileInfoVecIter;
typedef ChunkFileInfoVec::const_iterator ChunkFileInfoVecCIter;


/**
 * Data are not really belonging to the inode, but we just need it to write inodes in the
 * dentry-format. We need to read those from disk first or obtain it from the dentry.
 */
struct DentryCompatData
{
      DentryCompatData()
         : entryType(DirEntryType_INVALID), featureFlags(0)
      {
      }

      DirEntryType entryType;
      uint32_t featureFlags;

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::as<int32_t>(obj->entryType)
            % obj->featureFlags;
      }

      bool operator==(const DentryCompatData& other) const
      {
         return entryType == other.entryType
            && featureFlags == other.featureFlags;
      }

      bool operator!=(const DentryCompatData& other) const { return !(*this == other); }
};


/**
 * Our inode object, but for files only (so all file types except of directories).
 * Directories are in class DirInode.
 */
class FileInode
{
   friend class MetaStore;
   friend class InodeFileStore;
   friend class SessionFile; /* (needed for entry/range locking) */


   public:

      /**
       * Should be rarely used and if required, only with an immediate following
       * inode->deserialize(buf)
       */
      FileInode();

      /**
       * The preferred inode initializer.
       */
      FileInode(std::string entryID, FileInodeStoreData* inodeDiskData,
         DirEntryType entryType, unsigned dentryFeatureFlags);


      ~FileInode()
      {
         LOG_DEBUG("Delete FileInode", Log_SPAM, std::string("Deleting inode: ") + getEntryID());
      }

      FhgfsOpsErr setRemoteStorageTarget(EntryInfo* entryInfo, const RemoteStorageTarget& rst);
      FhgfsOpsErr clearRemoteStorageTarget(EntryInfo* entryInfo);

      void decNumSessionsAndStore(EntryInfo* entryInfo, unsigned accessFlags);
      static FileInode* createFromEntryInfo(EntryInfo* entryInfo);

      void serializeMetaData(Serializer& ser);

      std::pair<bool, LockEntryNotifyList> flockAppend(EntryLockDetails& lockDetails);
      void flockAppendCancelAllWaiters();
      LockEntryNotifyList flockAppendCancelByClientID(NumNodeID clientID);
      std::string flockAppendGetAllAsStr();
      std::pair<bool, LockEntryNotifyList> flockEntry(EntryLockDetails& lockDetails);
      void flockEntryCancelAllWaiters();
      LockEntryNotifyList flockEntryCancelByClientID(NumNodeID clientID);
      std::string flockEntryGetAllAsStr();
      std::pair<bool, LockRangeNotifyList> flockRange(RangeLockDetails& lockDetails);
      void flockRangeCancelAllWaiters();
      LockRangeNotifyList flockRangeCancelByClientID(NumNodeID clientNumID);
      bool flockRangeGetConflictor(RangeLockDetails& lockDetails, RangeLockDetails* outConflictor);
      std::string flockRangeGetAllAsStr();

      void deserializeMetaData(Deserializer& des);

      std::pair<FhgfsOpsErr, StringVector> listXAttr();
      std::tuple<FhgfsOpsErr, std::vector<char>, ssize_t> getXAttr(const std::string& xAttrName,
         size_t maxSize);
      FhgfsOpsErr removeXAttr(EntryInfo* entryInfo, const std::string& xAttrName);
      FhgfsOpsErr setXAttr(EntryInfo* entryInfo, const std::string& xAttrName,
         const CharVector& xAttrValue, int flags);

      bool operator==(const FileInode& other) const;

      bool operator!=(const FileInode& other) const { return !(*this == other); }

      /**
       * only for unit tests
       */
      void initLocksRandomForSerializationTests();

      FhgfsOpsErr checkAccessAndOpen(unsigned openAccessFlags, bool bypassAccessCheck);

   public:
      template<typename InodeT>
      class LockState
      {
         friend class FileInode;

         public:
            template<typename This, typename Ctx>
            static void serialize(This obj, Ctx& ctx)
            {
               ctx
                  % obj->source->exclAppendLock
                  % obj->source->exclFLock
                  % obj->source->sharedFLocks
                  % obj->source->exclRangeFLocks
                  % obj->source->sharedRangeFLocks;
            }

            friend Deserializer& operator%(Deserializer& des, const LockState& state)
            {
               serialize(&state, des);
               return des;
            }

         private:
            LockState(InodeT* source) : source(source) {}

            InodeT* source;
      };
      template<typename>
      friend class LockState;

      LockState<FileInode>       lockState()       { return {this}; }
      LockState<const FileInode> lockState() const { return {this}; }

   private:
      FileInodeStoreData inodeDiskData;
      RemoteStorageTarget rstInfo;

      ChunkFileInfoVec fileInfoVec; // stores individual data for each node (e.g. file length),
         // esp. the DYNAMIC ATTRIBS, that live only in memory and are not stored to disk (compared
         // to static attribs)

      pthread_t exclusiveTID; // ID of the thread, which has exclusiveTID access to the inode

      uint32_t numSessionsRead; // open read-only
      uint32_t numSessionsWrite; // open for writing or read-write


      bool isInlined; // boolean if the inode inlined into the Dentry or a separate file

      // append lock queues (exclusive only, entire file locking)
      EntryLockDetails exclAppendLock; // current exclusiveTID lock
      EntryLockDetailsList waitersExclAppendLock; // queue (append new to end, pop from top)
      StringSet waitersLockIDsAppendLock; // currently enqueued lockIDs (for fast duplicate check)

      // flock() queues (entire file locking, i.e. no ranges)
      EntryLockDetails exclFLock; // current exclusiveTID lock
      EntryLockDetailsSet sharedFLocks; // current shared locks (key is lock, value is dummy)
      EntryLockDetailsList waitersExclFLock; // queue (append new to end, pop from top)
      EntryLockDetailsList waitersSharedFLock; // queue (append new to end, pop from top)
      StringSet waitersLockIDsFLock; // currently enqueued lockIDs (for fast duplicate check)

      // fcntl() flock queues (range-based)
      RangeLockExclSet exclRangeFLocks; // current exclusiveTID locks
      RangeLockSharedSet sharedRangeFLocks;  // current shared locks (key is lock, value is dummy)
      RangeLockDetailsList waitersExclRangeFLock; // queue (append new to end, pop from top)
      RangeLockDetailsList waitersSharedRangeFLock; // queue (append new to end, pop from top)
      StringSet waitersLockIDsRangeFLock; // currently enqueued lockIDs (for fast duplicate check)

      RWLock rwlock; // default inode lock

      DentryCompatData dentryCompatData;

      RWLock setGetReferenceParentLock; // just to set/get the referenceParentID
      std::string referenceParentID;
      AtomicSSizeT numParentRefs; /* counter how often parentDir is referenced, 0 if we
                                   * are part of MetaStores FileStore */


      static FileInode* createFromInlinedInode(EntryInfo* entryInfo);
      static FileInode* createFromInodeFile(EntryInfo* entryInfo);

      void initFileInfoVec();
      void updateDynamicAttribs(void);

      bool setAttrData(EntryInfo* entryInfo, int validAttribs, SettableFileAttribs* attribs);
      bool incDecNumHardLinks(EntryInfo * entryInfo, int value);

      bool storeUpdatedMetaDataBuf(char* buf, unsigned bufLen);
      bool storeUpdatedMetaDataBufAsXAttr(char* buf, unsigned bufLen, std::string metaFilename);
      bool storeUpdatedMetaDataBufAsContents(char* buf, unsigned bufLen, std::string metaFilename);
      bool storeUpdatedMetaDataBufAsContentsInPlace(char* buf, unsigned bufLen,
         std::string metaFilename);
      bool storeUpdatedInodeUnlocked(EntryInfo* entryInfo,
         StripePattern* updatedStripePattern = NULL);
      FhgfsOpsErr storeUpdatedInlinedInodeUnlocked(EntryInfo* entryInfo,
         StripePattern* updatedStripePattern = NULL);

      std::string getMetaFilePath(EntryInfo* entryInfo);
      bool storeRemoteStorageTargetUnlocked(EntryInfo* entryInfo);
      bool storeRemoteStorageTargetBufAsXAttr(char* buf, unsigned bufLen, const std::string& metafilename);

      static bool removeStoredMetaData(const std::string& id, bool isBuddyMirrored);

      bool loadFromInodeFile(EntryInfo* entryInfo);
      bool loadFromFileXAttr(const std::string& id, bool isBuddyMirrored);
      bool loadFromFileContents(const std::string& id, bool isBuddyMirrored);

      bool loadRstFromInodeFile(EntryInfo* entryInfo);
      bool loadRstFromFileXAttr(EntryInfo* entryInfo);

      std::pair<bool, LockEntryNotifyList> flockEntryUnlocked(EntryLockDetails& lockDetails,
         EntryLockQueuesContainer* lockQs);
      bool flockEntryCancelByHandle(EntryLockDetails& lockDetails,
         EntryLockQueuesContainer* lockQs);
      void flockEntryGenericCancelAllWaiters(EntryLockQueuesContainer* lockQs);
      LockEntryNotifyList flockEntryGenericCancelByClientID(NumNodeID clientNumID,
         EntryLockQueuesContainer* lockQs);
      std::string flockEntryGenericGetAllAsStr(EntryLockQueuesContainer* lockQs);

      bool flockEntryCheckConflicts(EntryLockDetails& lockDetails, EntryLockQueuesContainer* lockQs,
         EntryLockDetails* outConflictor);
      bool flockEntryIsGranted(EntryLockDetails& lockDetails, EntryLockQueuesContainer* lockQs);
      bool flockEntryUnlock(EntryLockDetails& lockDetails, EntryLockQueuesContainer* lockQs);
      void flockEntryShared(EntryLockDetails& lockDetails, EntryLockQueuesContainer* lockQs);
      void flockEntryExclusive(EntryLockDetails& lockDetails, EntryLockQueuesContainer* lockQs);
      LockEntryNotifyList flockEntryTryNextWaiters(EntryLockQueuesContainer* lockQs);

      std::pair<bool, LockRangeNotifyList> flockRangeUnlocked(RangeLockDetails& lockDetails);
      bool flockRangeCancelByHandle(RangeLockDetails& lockDetails);

      bool flockRangeCheckConflicts(RangeLockDetails& lockDetails, RangeLockDetails* outConflictor);
      bool flockRangeCheckConflictsEx(RangeLockDetails& lockDetails, int maxExclWaitersCheckNum,
         RangeLockDetails* outConflictor);
      bool flockRangeIsGranted(RangeLockDetails& lockDetails);
      bool flockRangeUnlock(RangeLockDetails& lockDetails);
      void flockRangeShared(RangeLockDetails& lockDetails);
      void flockRangeExclusive(RangeLockDetails& lockDetails);
      LockRangeNotifyList flockRangeTryNextWaiters();


   public:
      // inliners

      void setRemoteStorageTargetUnpersistent(const RemoteStorageTarget& rst)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K
         this->rstInfo.set(rst);
         safeLock.unlock();
      }

      /**
       * Save meta-data to disk
       */
      bool updateInodeOnDisk(EntryInfo* entryInfo, StripePattern* updatedStripePattern = NULL)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

         bool retVal = storeUpdatedInodeUnlocked(entryInfo, updatedStripePattern);

         safeLock.unlock(); // U N L O C K

         return retVal;
      }

      /**
       * Save meta-data to disk and increment meta version
       */
      bool updateInodeOnDiskIncrementVersion(EntryInfo* entryInfo, StripePattern* updatedStripePattern = NULL)
      {
         RWLockGuard lock(this->rwlock, SafeRWLock_WRITE); // L O C K
         inodeDiskData.setMetaVersion(inodeDiskData.getMetaVersion() + 1);         
         return storeUpdatedInodeUnlocked(entryInfo, updatedStripePattern);
      }

      bool updateInodeChangeTime(EntryInfo* entryInfo)
      {
         UniqueRWLock lock(rwlock, SafeRWLock_WRITE);

         inodeDiskData.inodeStatData.setAttribChangeTimeSecs(TimeAbs().getTimeval()->tv_sec);

         return storeUpdatedInodeUnlocked(entryInfo, nullptr);
      }

      /**
       * Unlink the stored file-inode file on disk.
       */
      static bool unlinkStoredInodeUnlocked(const std::string& id, bool isBuddyMirrored)
      {
         return removeStoredMetaData(id, isBuddyMirrored);
      }


      // getters & setters
      std::string getEntryID()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         std::string entryID = getEntryIDUnlocked();

         safeLock.unlock();

         return entryID;
      }

      std::string getEntryIDUnlocked()
      {
         return this->inodeDiskData.getEntryID();
      }


      /**
       * Note: Does not (immediately) update the persistent metadata.
       * Note: Be careful with this!
       */
      void setIDUnpersistent(std::string newID)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

         this->inodeDiskData.setEntryID(newID);

         safeLock.unlock();
      }

      RemoteStorageTarget* getRemoteStorageTargetInfo()
      {
         return &this->rstInfo;
      }

      StripePattern* getStripePatternUnlocked()
      {
         return this->inodeDiskData.getStripePattern();
      }

      StripePattern* getStripePattern()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         StripePattern* pattern = this->getStripePatternUnlocked();

         safeLock.unlock();

         return pattern;
      }

      /**
       * Return the stripe pattern and set our value to NULL.
       *
       * Note: Usually this inode should not be further used anymore.
       */
      StripePattern* getStripePatternAndSetToNull()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

         StripePattern* pattern = this->inodeDiskData.getStripePatternAndSetToNull();

         safeLock.unlock();

         return pattern;
      }

      bool modifyStripePattern(uint16_t localTargetID, uint16_t destinationID)
      {
         UniqueRWLock lock(rwlock, SafeRWLock_WRITE);

         StripePattern* pattern = this->getStripePatternUnlocked();
         UInt16Vector* stripeTargetIDs = pattern->getStripeTargetIDsModifyable();
         if (stripeTargetIDs->empty() ||  (std::find(stripeTargetIDs->begin(), stripeTargetIDs->end(), localTargetID) == stripeTargetIDs->end()) )
         {
            return false;
         }
         //change chunk striping pattern from localID to destinationID
         std::replace(stripeTargetIDs->begin(), stripeTargetIDs->end(), localTargetID, destinationID);

         return true;
      }

      /**
       * Used to check if storage target in pattern actually holds a chunk 
       * Used for cases when target is in pattern but chunk is not present (e.g. small files, special files)
       *
       * Note: Takes fileSize and targetID we want to check
       * returns: true if target does not hold a chunk, false if chunk is on target
       */
      bool checkTargetIsActiveInPattern(int64_t fileSize, uint16_t targetID)
      {
         UniqueRWLock lock(rwlock, SafeRWLock_READ);

         StripePattern* pattern = this->getStripePatternUnlocked();
         const UInt16Vector* stripeTargetIDs = pattern->getStripeTargetIDs();
         size_t numTargets      = stripeTargetIDs->size();
         unsigned chunkSize     = pattern->getChunkSize();

         int64_t numChunks = (fileSize + chunkSize - 1) / chunkSize;
         size_t activeCount = std::min<int64_t>(numChunks, numTargets);
         std::set<uint16_t> activeTargets;

         // determine which targets actually hold chunks
         for (size_t i = 0; i < activeCount; ++i)
         {
            activeTargets.insert((*stripeTargetIDs)[i]);
         }

         return (fileSize < static_cast<int64_t>(chunkSize*numTargets) && activeTargets.find(targetID) == activeTargets.end()); 
      }

      void setFeatureFlags(unsigned flags)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

         this->inodeDiskData.setInodeFeatureFlags(flags);

         safeLock.unlock();
      }

      void addFeatureFlag(unsigned flag)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

         addFeatureFlagUnlocked(flag);

         safeLock.unlock();
      }

      unsigned getFeatureFlags()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         unsigned flags = this->inodeDiskData.getInodeFeatureFlags();

         safeLock.unlock();

         return flags;
      }

      unsigned getUserID()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         unsigned retVal = this->inodeDiskData.getInodeStatData()->getUserID();

         safeLock.unlock();

         return retVal;
      }

      unsigned getGroupID()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         unsigned retVal = this->inodeDiskData.getInodeStatData()->getGroupID();

         safeLock.unlock();

         return retVal;
      }

      int getMode()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         int retVal = this->inodeDiskData.getInodeStatData()->getMode();

         safeLock.unlock();

         return retVal;
      }

     /* uint16_t getMirrorNodeID()
      {
         return this->inodeDiskData.getMirrorNodeID();
      } */


      /**
       * Note: We do not set our class values here yet, because this method is called frequently and
       *       we do not need our class values here yet. So class values are only set on demand.
       *       this->updateDynamicAttribs() will set/update our class values·
       *
       * @param erroneousVec if errors occurred during vector retrieval from storage nodes
       *       (this parameter was used to set dynAttribsUptodate to false, but we do not have
       *        this value anymore since the switch to (post-)DB metadata format)
       * @return false if error during save occurred (but we don't save dynAttribs currently)
       *
       */
      bool setDynAttribs(DynamicFileAttribsVec& newAttribsVec)
      {
         /* note: we cannot afford to make a disk write each time this is updated, so
            we only update metadata on close currrently */

         bool retVal = true;
         bool anyAttribUpdated = false;

         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

         if(unlikely(newAttribsVec.size() != fileInfoVec.size() ) )
         { // should never happen
            LOG(GENERAL, ERR, "Apply dynamic file attribs - Vector sizes do not match.");

            retVal = false;
            goto unlock_and_exit;
         }


         for(size_t i=0; i < newAttribsVec.size(); i++)
         {
            if(fileInfoVec[i].updateDynAttribs(newAttribsVec[i] ) )
               anyAttribUpdated = true;
         }

         IGNORE_UNUSED_VARIABLE(anyAttribUpdated);

      unlock_and_exit:
         safeLock.unlock();

         return retVal;
      }

      /**
       * Note: Use this only if the file is not loaded already (because otherwise the dyn attribs
       * will be outdated)
       */
      static FhgfsOpsErr getStatData(EntryInfo* entryInfo, StatData& outStatData)
      {
         FileInode* inode = createFromEntryInfo(entryInfo);

         if(!inode)
            return FhgfsOpsErr_PATHNOTEXISTS;

         FhgfsOpsErr retVal = inode->getStatData(outStatData);

         delete inode;

         return retVal;
      }

      /**
       * Unlocked method to get statdata
       *
       * Note: Needs write-lock
       */
      FhgfsOpsErr getStatDataUnlocked(StatData& outStatData)
      {
         FhgfsOpsErr statRes = FhgfsOpsErr_SUCCESS;

         this->updateDynamicAttribs();

         outStatData = *(this->inodeDiskData.getInodeStatData() );

         /* note: we don't check numSessionsRead below (for dyn attribs outated statRes), because
            that would be too much overhead; side-effect is that we don't update atime for read-only
            files while they are open. */

         if(numSessionsWrite)
            statRes = FhgfsOpsErr_DYNAMICATTRIBSOUTDATED;

         return statRes;
      }


      /**
       * Return statData
       *
       * Note: Also updates dynamic attribs, so needs a write lock
       */
      FhgfsOpsErr getStatData(StatData& outStatData)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

         FhgfsOpsErr statRes = getStatDataUnlocked(outStatData);

         safeLock.unlock();

         return statRes;
      }

      void setStatData(StatData& statData)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

         this->inodeDiskData.setInodeStatData(statData);

         safeLock.unlock();
      }

      /**
       * Note: Does not take a lock and is a very special use case. For example on moving a file
       *       to a remote server, when an inode object is created from a buffer and we want to
       *       have values from this inode object.
       */
      StatData* getNotUpdatedStatDataUseCarefully()
      {
         return this->inodeDiskData.getInodeStatData();
      }

      void setNumHardlinksUnpersistent(unsigned numHardlinks)
      {
         /* note: this is the version for the quick on-close unlink check, so file is referenced by
          *       client sessions and we don't need to update persistent metadata
          */

         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

         this->inodeDiskData.setNumHardlinks(numHardlinks);

         safeLock.unlock();
      }

      unsigned getNumHardlinks()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         unsigned retVal = this->inodeDiskData.getNumHardlinks();

         safeLock.unlock();

         return retVal;
      }

      /**
       * Get the thread ID of the thread allow to work with this inode, so the ID, which has
       * set the exclusiveTID value.
       */
      pthread_t getExclusiveThreadID()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         pthread_t retVal = this->exclusiveTID;

         safeLock.unlock();

         return retVal;
      }

      void setExclusiveTID(pthread_t posixTID)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

         this->exclusiveTID = posixTID;

         safeLock.unlock();
      }

      /**
       * @return num read-sessions + num write-sessions
       */
      unsigned getNumSessionsAll()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         unsigned num = this->numSessionsRead + this->numSessionsWrite;

         safeLock.unlock();

         return num;
      }

      unsigned getNumSessionsRead()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         unsigned num = this->numSessionsRead;

         safeLock.unlock();

         return num;
      }

      unsigned getNumSessionsWrite()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         unsigned num = this->numSessionsWrite;

         safeLock.unlock();

         return num;
      }

      /**
       * Increase number of sessions for read or write (=> file open).
       *
       * @param accessFlags OPENFILE_ACCESS_... flags
       */
      void incNumSessions(unsigned accessFlags)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);
         incNumSessionsUnlocked(accessFlags);
         safeLock.unlock();
      }

      void incNumSessionsUnlocked(unsigned accessFlags)
      {
         if(accessFlags & OPENFILE_ACCESS_READ)
            this->numSessionsRead++;
         else
            this->numSessionsWrite++; // (includes read+write)
      }

      FileInode* clone()
      {
         Serializer ser;
         serializeMetaData(ser);

         boost::scoped_array<char> buf(new (std::nothrow) char[ser.size()]);
         if (!buf)
            return NULL;

         ser = Serializer(buf.get(), ser.size());
         serializeMetaData(ser);

         FileInode* clone = new FileInode();
         Deserializer des(buf.get(), ser.size());
         clone->deserializeMetaData(des);

         if (unlikely(!des.good()) )
         {
            delete clone;
            clone = NULL;
         }
         else
         {
            /* Ihe inode serializer might will not set origParentEntryID and origParentUID if the file
             * was not moved or the inode was not de-inlined, but the cloned inode needs these
             * values */
            clone->inodeDiskData.origParentEntryID = this->inodeDiskData.origParentEntryID;
            clone->inodeDiskData.origParentUID           = this->inodeDiskData.origParentUID;
         }

         return clone;
      }

      void incParentRef(const std::string& referenceParentID)
      {
         this->numParentRefs.increase();

         SafeRWLock rwLock(&this->setGetReferenceParentLock, SafeRWLock_WRITE);
         this->referenceParentID = referenceParentID;
         rwLock.unlock();
      }

      void decParentRef()
      {
         this->numParentRefs.decrease();
      }

      ssize_t getNumParentRefs()
      {
         return this->numParentRefs.read();
      }

      std::string getReferenceParentID()
      {
         SafeRWLock rwLock(&this->setGetReferenceParentLock, SafeRWLock_READ);

         std::string id = this->referenceParentID;

         rwLock.unlock();

         return id;
      }

      /**
       * Update inline information.
       *
       * @param value   If set to false pathInfo flags and dentryCompat data will be updated as
       *                well.
       */
      void setIsInlined(bool value)
      {
         SafeRWLock safeLock(&this->rwlock, SafeRWLock_WRITE);

         setIsInlinedUnlocked(value);

         safeLock.unlock();
      }

      /**
       * See setIsInlined().
       */
      void setIsInlinedUnlocked(bool value)
      {
         this->isInlined = value;

         if (value == false)
         {
            if (this->inodeDiskData.getOrigFeature() == FileInodeOrigFeature_TRUE)
            {
               /* If the inode gets de-inlined origParentEntryID cannot be set from parentDir
                * anymore. */
               addFeatureFlagUnlocked(FILEINODE_FEATURE_HAS_ORIG_PARENTID);
            }

            // dentryCompatData also need the update
            this->dentryCompatData.featureFlags &= ~(DENTRY_FEATURE_INODE_INLINE);


         }
         else
            this->dentryCompatData.featureFlags |= DENTRY_FEATURE_INODE_INLINE;
      }

      bool getIsInlined()
      {
         SafeRWLock rwLock(&this->rwlock, SafeRWLock_READ);

         bool retVal = this->isInlined;

         rwLock.unlock();

         return retVal;
      }

      void getPathInfo(PathInfo* outPathInfo)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         this->inodeDiskData.getPathInfo(outPathInfo);

         safeLock.unlock();
      }

      void setIsBuddyMirrored(bool mirrored = true)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

         this->inodeDiskData.setBuddyMirrorFeatureFlag(mirrored);

         safeLock.unlock();
      }

      bool getIsBuddyMirrored()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         bool retVal = getIsBuddyMirroredUnlocked();

         safeLock.unlock();

         return retVal;
      }

      bool getIsRstAvailable()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);
         bool retVal = this->getIsRstAvailableUnlocked();
         safeLock.unlock();
         return retVal;
      }

      /**
       * Sets the file state using a raw state value and persists it to disk.
       * Only updates file state if the access flags transition is allowed based
       * on active read/write sessions.
       *
       * @param entryInfo EntryInfo of the file to update
       * @param value Raw byte value representing the new file state
       * @return FhgfsOpsErr_SUCCESS if state was successfully updated to disk
       *         FhgfsOpsErr_INUSE if file has active client sessions
       *         FhgfsOpsErr_INTERNAL if state update failed to persist to disk
       * @note This method is called by the FileState-based overload below.
       */
      FhgfsOpsErr setFileState(EntryInfo* entryInfo, uint8_t value)
      {
         RWLockGuard lock(rwlock, SafeRWLock_WRITE);

         FileState oldState(this->getFileStateUnlocked());
         FileState newState(value);

         // Prevent file state changes if file has active sessions
         if (!checkAccessFlagTransition(oldState, newState))
            return FhgfsOpsErr_INUSE;

         this->inodeDiskData.setFileState(value);
         if (this->storeUpdatedInodeUnlocked(entryInfo))
            return FhgfsOpsErr_SUCCESS;
         else
            return FhgfsOpsErr_INTERNAL;
      }

      FhgfsOpsErr setFileState(EntryInfo* entryInfo, const FileState& state)
      {
         return setFileState(entryInfo, state.getRawValue());
      }

      /**
       * Validates whether a file access flag transition is permitted
       * based on the current active read/write sessions.
       * Transition rules:
       * - Acquiring stricter locks (adding flags): not allowed if conflicting sessions exist
       * - Relaxing locks (removing flags): not allowed if dependent sessions exist
       *   (This protects HSM/special clients using bypass access privileges)
       * - Full unlock (no flags): only allowed if there are no active sessions
       *
       * Special client handling using bypass access:
       * When a file is already locked (e.g., READ_LOCK), only special clients with
       * bypass permissions can have active read sessions. These sessions must finish
       * before removing the lock to ensure consistency of HSM operations.
       *
       * @param oldState The current file state
       * @param newState The requested new file state
       * @return true if the transition is allowed, false otherwise
       */
      bool checkAccessFlagTransition(const FileState& oldState, const FileState& newState) const
      {
         const uint8_t oldFlags = oldState.getAccessFlags();
         const uint8_t newFlags = newState.getAccessFlags();

         // no change in access flags
         // (e.g., READ_LOCK -> READ_LOCK)
         if (oldFlags == newFlags)
            return true;

         // --- Acquiring / Relaxing locks ---
         const uint8_t addedFlags   = newFlags & ~oldFlags;
         const uint8_t removedFlags = oldFlags & ~newFlags;

         // --- Acquiring stricter locks ---
         // Prevent upgrading to a stricter lock if there are conflicting sessions
         if ((addedFlags & AccessFlags::READ_LOCK) && numSessionsRead > 0)
            return false;
         if ((addedFlags & AccessFlags::WRITE_LOCK) && numSessionsWrite > 0)
            return false;

         // --- Relaxing locks ---
         // Prevent loosening restrictions while dependent sessions are active
         // These checks primarily protect special client operations using bypass access
         if ((removedFlags & AccessFlags::READ_LOCK) && numSessionsRead > 0)
            return false;
         if ((removedFlags & AccessFlags::WRITE_LOCK) && numSessionsWrite > 0)
            return false;

         return true;
      }

      FileState getFileState()
      {
         RWLockGuard lock(rwlock, SafeRWLock_READ);
         return FileState(this->getFileStateUnlocked());
      }

      bool incrementFileVersion(EntryInfo* entryInfo)
      {
         UniqueRWLock lock(rwlock, SafeRWLock_WRITE);

         inodeDiskData.setFileVersion(inodeDiskData.getFileVersion() + 1);
         return storeUpdatedInodeUnlocked(entryInfo, nullptr);
      }

      uint32_t getFileVersion()
      {
         UniqueRWLock lock(rwlock, SafeRWLock_WRITE);

         return inodeDiskData.getFileVersion();
      }

      bool incrementMetaVersion(EntryInfo* entryInfo)
      {
         UniqueRWLock lock(rwlock, SafeRWLock_WRITE);

         inodeDiskData.setMetaVersion(inodeDiskData.getMetaVersion() + 1);
         return storeUpdatedInodeUnlocked(entryInfo, nullptr);
      }

      uint32_t getMetaVersion()
      {
         UniqueRWLock lock(rwlock, SafeRWLock_WRITE);

         return inodeDiskData.getMetaVersion();
      }



   protected:

      void setOrigParentID(std::string origParentEntryId)
      {
         this->inodeDiskData.setDynamicOrigParentEntryID(origParentEntryId);
      }

      void setPersistentOrigParentID(std::string origParentEntryId)
      {
         this->inodeDiskData.setPersistentOrigParentEntryID(origParentEntryId);
      }

   private:

      /**
       * Increase link counter by one
       */
      void incDecNumHardlinksUnpersistentUnlocked(int value)
      {
         this->inodeDiskData.incDecNumHardlinks(value);
      }

      FileInodeStoreData* getInodeDiskData()
      {
         return &this->inodeDiskData;
      }

      void addFeatureFlagUnlocked(unsigned flag)
      {
         this->inodeDiskData.addInodeFeatureFlag(flag);
      }

      /**
       * Unlocked version of getIsBuddyMirrored().
       * Note: Caller must at least hold read lock.
       */
      bool getIsBuddyMirroredUnlocked()
      {
         return this->inodeDiskData.getIsBuddyMirrored();
      }

      bool getIsRstAvailableUnlocked()
      {
         return this->inodeDiskData.getIsRstAvailable();
      }

      uint8_t getFileStateUnlocked() const
      {
         return this->inodeDiskData.getFileState();
      }
};

