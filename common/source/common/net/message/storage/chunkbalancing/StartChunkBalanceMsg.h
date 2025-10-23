#pragma once

#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/Serialization.h>
#include <common/storage/EntryInfo.h>
#include <components/chunkbalancer/SyncCandidate.h>
#include <common/storage/FileEvent.h>

#define STARTCHUNKBALANCEMSG_FLAG_HAS_EVENT            1  /* contains file event logging information */


class StartChunkBalanceMsg : public MirroredMessageBase<StartChunkBalanceMsg>
{
   public:
     StartChunkBalanceMsg(uint8_t idType, std::string relativePath, const UInt16Vector* targetIDs, const UInt16Vector* destinationIDs, EntryInfo* entryInfo) :
         BaseType(NETMSGTYPE_StartChunkBalance)
      {
         this->idType = idType;
         this->relativePath = relativePath.c_str();
         this->targetIDsPtr = targetIDs;
         this->destinationIDsPtr = destinationIDs;
         this->entryInfoPtr = entryInfo;
      }

      StartChunkBalanceMsg() : BaseType(NETMSGTYPE_StartChunkBalance)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->idType
            % obj->relativePath
            % serdes::backedPtr(obj->targetIDsPtr, obj->targetIDs)
            % serdes::backedPtr(obj->destinationIDsPtr, obj->destinationIDs)
            % serdes::backedPtr(obj->entryInfoPtr, obj->entryInfo);
            
            if (obj->isMsgHeaderFeatureFlagSet(STARTCHUNKBALANCEMSG_FLAG_HAS_EVENT))
            ctx % obj->fileEvent;
      }
      bool supportsMirroring() const {return true; }
   private:

      uint8_t idType; 
      std::string relativePath;
      FileEvent fileEvent; // for file event logging

      // for serialization
      EntryInfo* entryInfoPtr;
      const UInt16Vector* targetIDsPtr;
      const UInt16Vector* destinationIDsPtr;

      // for deserialization
      UInt16Vector targetIDs;
      UInt16Vector destinationIDs;
      EntryInfo entryInfo;

   public:

      IdType getIdType() const
      {
         return static_cast<IdType>(idType);
      }

      const UInt16Vector& getTargetIDs() const
      {
         return this->targetIDs;
      }

      const UInt16Vector& getDestinationIDs() const
      {
         return this->destinationIDs;
      }

      const std::string& getRelativePath() const
      {
         return relativePath;
      }

      EntryInfo* getEntryInfo(void)
      {
         return &this->entryInfo;
      }

      FileEvent* getFileEvent() 
      {
         if (isMsgHeaderFeatureFlagSet(STARTCHUNKBALANCEMSG_FLAG_HAS_EVENT))
            return &fileEvent;
         else
            return nullptr;
      }
   protected:
      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return STARTCHUNKBALANCEMSG_FLAG_HAS_EVENT;
      }  

};


