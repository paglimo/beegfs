#ifndef NETWORKINTERFACECARD_H_
#define NETWORKINTERFACECARD_H_

#include <common/Common.h>
#include <common/toolkit/StringTk.h>
#include <common/toolkit/NicAddressFilter.h>
#include <common/net/sock/NicAddress.h>
#include <common/net/sock/NicAddressList.h>
#include <common/net/sock/NicAddressListIter.h>

extern void NIC_findAll(NicAddressFilter *nicAddressFilter, bool useRDMA, bool onlyRDMA, int domain,
   NicAddressList* outList);

extern const char* NIC_nicTypeToString(NicAddrType_t nicType);
extern char* NIC_nicAddrToString(NicAddress* nicAddr);

extern bool NIC_supportsRDMA(NicAddressList* nicList);
extern void NIC_supportedCapabilities(NicAddressList* nicList,
   NicListCapabilities* outCapabilities);

#endif /*NETWORKINTERFACECARD_H_*/
