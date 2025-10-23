#pragma once

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>
#include <common/toolkit/serialization/Serialization.h>
#include <common/storage/FileEvent.h>

#define UPDATESTRIPEPATTERNMSG_FLAG_HAS_EVENT            1  /* contains file event logging information */

class UpdateStripePatternMsg : public MirroredMessageBase<UpdateStripePatternMsg>
{
   public:
      UpdateStripePatternMsg(uint16_t targetID, uint16_t destinationID, EntryInfo* entryInfo,std::string* relativePath, FhgfsOpsErr resyncRes, FileEvent* fileEvent) :
         BaseType(NETMSGTYPE_UpdateStripePattern)
      {
         this->targetID = targetID;
         this->destinationID = destinationID;
         this->relativePath = relativePath;
         this->entryInfoPtr = entryInfo;
         this->resyncRes = (int32_t)resyncRes; 
         this->fileEvent = *fileEvent;
      }

      UpdateStripePatternMsg() : BaseType(NETMSGTYPE_UpdateStripePattern) 
      {
      } 
      
      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->targetID
            % obj->destinationID
            % serdes::backedPtr(obj->entryInfoPtr, obj->entryInfo)
            % serdes::backedPtr(obj->relativePath, obj->parsed.relativePath)
            % obj->resyncRes;

            if (obj->isMsgHeaderFeatureFlagSet(UPDATESTRIPEPATTERNMSG_FLAG_HAS_EVENT))
            ctx % obj->fileEvent;
      }


      bool supportsMirroring() const { return true; } 
      
   private:
      uint16_t targetID;
      uint16_t destinationID;
      FileEvent fileEvent;
      // for serialization
      std::string* relativePath;
      EntryInfo* entryInfoPtr;
      int32_t  resyncRes;   //result of doResync()

      // for deserialization
      struct {
         std::string relativePath;
      } parsed;
      EntryInfo entryInfo;

   public:
      uint16_t getTargetID() const
      {
         return targetID;
      }

      uint16_t getDestinationID() const
      {
         return destinationID;
      }

      std::string& getRelativePath()
      {
         return *relativePath;
      }

      EntryInfo* getEntryInfo()
      {
         return &this->entryInfo;
      }   

      FileEvent* getFileEvent() 
      {
         if (isMsgHeaderFeatureFlagSet(UPDATESTRIPEPATTERNMSG_FLAG_HAS_EVENT))
            return &fileEvent;
         else
            return nullptr;
      }

      FhgfsOpsErr getResyncResult()
      {
         return (FhgfsOpsErr)resyncRes;
      }

   protected:
      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return UPDATESTRIPEPATTERNMSG_FLAG_HAS_EVENT;
      }  

};


