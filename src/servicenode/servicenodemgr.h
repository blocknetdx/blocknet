// Copyright (c) 2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_SERVICENODEMGR_H
#define BLOCKNET_SERVICENODEMGR_H

#include <servicenode/servicenode.h>
#include <streams.h>
#include <sync.h>
#include <util/strencodings.h>
#include <util/system.h>
#include <validation.h>

#include <iostream>
#include <set>

namespace sn {

extern const char* REGISTER;
extern const char* PING;

class ServiceNodeMgr {
public:
    ServiceNodeMgr() = default;

    static ServiceNodeMgr& instance() {
        static ServiceNodeMgr smgr;
        return smgr;
    }

    bool processXBridge(const std::vector<unsigned char> & packet) {
        if (seenPacket(packet))
            return false;

        try { // Check if legacy packet
            LegacyXBridgePacket p;
            p.CopyFrom(packet);
            if (p.command > 0 && p.command != 50)
                return true; // ignore all packets except service ping
            // TODO Handle legacy snode ping packet
        } catch (...) { } // ignore errors

        return true;
    }

    bool processRegistration(CDataStream & ss, ServiceNodePtr & snode) {
        ServiceNode sn;
        try {
            ss >> sn;
        } catch (...) {
            return false;
        }
        if (seenPacket(sn.getHash()))
            return false;

        snode = addSn(sn);
        return true;
    }

    bool processPing(CDataStream & ss, ServiceNodePing & ping) {
        try {
            ss >> ping;
        } catch (...) {
            return false;
        }
        if (seenPacket(ping.getHash()))
            return false;
        // TODO Handle ping
        return true;
    }

    ServiceNode getSn(const std::vector<unsigned char> & snodePubKey) {
        auto snode = findSn(snodePubKey);
        if (!snode)
            return ServiceNode();
        return *snode;
    }

    ServiceNode getSn(const CPubKey & snodePubKey) {
        return getSn(std::vector<unsigned char>{snodePubKey.begin(), snodePubKey.end()});
    }

    void updatePing(const CPubKey & snodePubKey) {
        auto snode = findSn(snodePubKey);
        if (snode) {
            LOCK(mu);
            snode->updatePing();
        }
    }

protected:
    ServiceNodePtr addSn(const ServiceNodePtr & snode) {
        if (!snode)
            return nullptr;
        LOCK(mu);
        snodes.insert(snode);
        return snode;
    }

    ServiceNodePtr addSn(const ServiceNode & snode) {
        if (!snode.isValid(GetTxFunc, IsBlockValidFunc))
            return nullptr;
        auto ptr = std::make_shared<ServiceNode>(snode);
        return addSn(ptr);
    }

    ServiceNodePtr findSn(const CPubKey & snodePubKey) {
        LOCK(mu);
        auto it = std::find_if(snodes.begin(), snodes.end(),
                               [&snodePubKey](const ServiceNodePtr & snode) {
                                   return snode->getSnodePubKey() == CPubKey(snodePubKey);
                               });
        if (it != snodes.end())
            return *it;
        return nullptr;
    }

    ServiceNodePtr findSn(const std::vector<unsigned char> & snodePubKey) {
        return findSn(CPubKey(snodePubKey));
    }

    bool removeSn(const ServiceNodePtr & snode) {
        if (!hasSn(snode))
            return false;
        LOCK(mu);
        snodes.erase(snode);
        return true;
    }

    bool hasSn(const ServiceNodePtr & snode) {
        LOCK(mu);
        return snodes.count(snode) > 0;
    }

    /**
     * Returns true if the hash has already been seen.
     * @param hash
     * @return
     */
    bool seenPacket(const uint256 & hash) {
        LOCK(mu);
        if (seenPackets.count(hash))
            return true; // already seen
        if (seenPackets.size() > 350000)
            seenPackets.clear(); // mem mgmt, ~12MB (32bytes * 350k)
        seenPackets.insert(hash);
        return false;
    }

    /**
     * Returns true if the packet has already been seen.
     * @param packet
     * @return
     */
    bool seenPacket(const std::vector<unsigned char> & packet) {
        const auto & hash = Hash(packet.begin(), packet.end());
        return seenPacket(hash);
    }

protected:
    Mutex mu;
    std::set<ServiceNodePtr> snodes;
    std::set<uint256> seenPackets;
};

}

#endif //BLOCKNET_SERVICENODEMGR_H
