#include "MgmtNodeEx.h"

MgmtNodeEx::MgmtNodeEx(std::string nodeID, NumNodeID nodeNumID, unsigned short portUDP,
      unsigned short portTCP, unsigned version, const BitStore& featuresFlags,
      NicAddressList& nicList) :
      Node(nodeID, nodeNumID, portUDP, portTCP, nicList)
{}
