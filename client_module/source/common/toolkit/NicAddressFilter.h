#ifndef NICADDRESSFILTER_H_
#define NICADDRESSFILTER_H_

#include <common/Common.h>
#include <common/net/sock/NicAddress.h>

typedef struct NicAddressFilter NicAddressFilter;

bool NicAddressFilter_isContained(NicAddressFilter* this, struct in6_addr addr);
bool NicAddressFilter_isAllowed(NicAddressFilter* this, const NicAddress *nicAddr);
size_t NicAddressFilter_getNumFilterEntries(NicAddressFilter *this);
size_t NicAddressFilter_getPosition(NicAddressFilter* this, const NicAddress *nicAddr);

NicAddressFilter *NicAddressFilter_construct(StrCpyList *filterList);
void NicAddressFilter_destruct(NicAddressFilter *this);

#endif /* NICADDRESSFILTER_H_ */
