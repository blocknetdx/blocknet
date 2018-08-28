// Copyright (c) 2018 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XBRIDGESERVICESPACKET_H
#define XBRIDGESERVICESPACKET_H

#include "xbridgepacket.h"

#include "pubkey.h"

class XBridgeServicesPacket : public XBridgePacket
{
public:
    std::vector<std::string> services;
    ::CPubKey nodePubKey;
};

#endif // XBRIDGESERVICESPACKET_H
