#include "BumpFileVersion.h"

static void BumpFileVersionMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   struct BumpFileVersionMsg* msg = container_of(this, struct BumpFileVersionMsg, netMessage);

   EntryInfo_serialize(msg->entryInfo, ctx);

   if (this->msgHeader.msgFeatureFlags & BUMPFILEVERSIONMSG_FLAG_HASEVENT)
      FileEvent_serialize(msg->fileEvent, ctx);
}

const struct NetMessageOps BumpFileVersionMsg_Ops = {
   .serializePayload = BumpFileVersionMsg_serializePayload,
   .deserializePayload = _NetMessage_deserializeDummy,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
   .supportsSequenceNumbers = true,
};
