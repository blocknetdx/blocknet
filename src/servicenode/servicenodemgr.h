// Copyright (c) 2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_SERVICENODEMGR_H
#define BLOCKNET_SERVICENODEMGR_H

#include <amount.h>
#include <key_io.h>
#include <net.h>
#include <netmessagemaker.h>
#include <servicenode/servicenode.h>
#include <script/standard.h>
#include <streams.h>
#include <sync.h>
#include <util/strencodings.h>
#include <util/system.h>
#include <validation.h>
#include <wallet/wallet.h>

#include <iostream>
#include <set>

/**
 * Servicenode namepsace
 */
namespace sn {

extern const char* REGISTER;
extern const char* PING;

/**
 * Manages related servicenode functions including handling network messages and storing an active list
 * of valid servicenodes.
 */
class ServiceNodeMgr {
public:
    ServiceNodeMgr() = default;

    /**
     * Singleton instance.
     * @return
     */
    static ServiceNodeMgr& instance() {
        static ServiceNodeMgr smgr;
        return smgr;
    }

    /**
     * Processes xbridge packets.
     * @param packet
     * @return
     */
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

    /**
     * Processes a servicenode registration message from the network.
     * @param ss
     * @param snode
     * @return
     */
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

    /**
     * Processes a servicenode ping message from the network.
     * @param ss
     * @param ping
     * @return
     */
    bool processPing(CDataStream & ss, ServiceNodePing & ping) {
        try {
            ss >> ping;
        } catch (...) {
            return false;
        }
        if (seenPacket(ping.getHash()))
            return false;

        addSn(ping.getSnode()); // add or replace existing snode
        return true;
    }

    /**
     * Registers a snode on the network. This will also automatically search the wallet for required collateral.
     * This requires the wallet to be unlocked any snodes that are not "OPEN" (free) snodes.
     * @param key
     * @param tier
     * @param address
     * @param connman
     * @return
     */
    bool registerSn(const CKey & key, const ServiceNode::Tier & tier, const std::string & address, CConnman *connman) {
        const auto & snodePubKey = key.GetPubKey();

        // 1) Iterate over all wallets
        // 2) Iterate over all coins
        // 3) Add coin matching utxo address (exclude already used coin)
        // 4) Run pairing algo to pick utxos for snode
        // 5) Relay valid snode to peers

        std::vector<unsigned char> sig;
        std::vector<COutPoint> collateral;

        if (tier != ServiceNode::Tier::OPEN) {
            CTxDestination dest = DecodeDestination(address);
            if (!IsValidDestination(dest)) {
                LogPrint(BCLog::SNODE, "service node registration failed, bad address: %s\n", address);
                return false; // fail on bad address
            }

            if (!findCollateral(tier, dest, collateral)) {
                LogPrint(BCLog::SNODE, "service node registration failed, bad collateral: %s\n", address);
                return false; // fail on bad collateral
            }

            const auto & sighash = sn::ServiceNode::CreateSigHash(snodePubKey, tier, collateral,
                                                                  chainActive.Height(), chainActive.Tip()->GetBlockHash());

            // Sign the servicenode with the collateral's private key
            auto wallets = GetWallets();
            for (auto & wallet : wallets) {
                CKey sign;
                {
                    auto locked_chain = wallet->chain().lock();
                    LOCK(wallet->cs_wallet);
                    const auto keyid = GetKeyForDestination(*wallet, dest);
                    if (keyid.IsNull())
                        continue;
                    if (!wallet->GetKey(keyid, sign))
                        continue;
                }
                if (sign.SignCompact(sighash, sig))
                    break; // sign successful
            }

            if (sig.empty()) {
                LogPrint(BCLog::SNODE, "service node registration failed, bad signature, is the wallet unlocked? %s\n", address);
                return false; // key not found in wallets
            }
        } else { // OPEN tier
            const auto & sighash = sn::ServiceNode::CreateSigHash(snodePubKey, tier, collateral,
                                                                  chainActive.Height(), chainActive.Tip()->GetBlockHash());

            if (!key.SignCompact(sighash, sig) || sig.empty()) { // sign with snode pubkey
                LogPrint(BCLog::SNODE, "service node registration failed, bad signature, is the servicenode.conf populated? %s\n", address);
                return false;
            }
        }

        ServiceNode snode(snodePubKey, tier, collateral, chainActive.Height(), chainActive.Tip()->GetBlockHash(), sig);
        auto snodePtr = addSn(snode);
        if (!snodePtr) {
            LogPrint(BCLog::SNODE, "service node registration failed\n");
            return false; // failed to add snode
        }

        // Relay
        connman->ForEachNode([&](CNode* pnode) {
            if (!pnode->fSuccessfullyConnected)
                return;
            const CNetMsgMaker msgMaker(pnode->GetSendVersion());
            connman->PushMessage(pnode, msgMaker.Make(sn::REGISTER, snode));
        });

        return true;
    }

    /**
     * Submits a servicenode ping to the network. This method will also update our own servicenode
     * @param key
     * @param connman
     * @return
     */
    bool sendPing(const CKey & key, CConnman *connman) {
        ServiceNodePtr snode = findSn(key.GetPubKey());
        if (!snode) {
            LogPrint(BCLog::SNODE, "service node ping failed, service node not found\n");
            return false;
        }

        const uint32_t bestBlock = getActiveChainHeight();
        const uint256 & bestBlockHash = getActiveChainHash(bestBlock);
        std::string config; // TODO Blocknet Add snode config

        ServiceNodePing ping(key.GetPubKey(), bestBlock, bestBlockHash, config, *snode);
        ping.sign(key);
        if (!ping.isValid(GetTxFunc, IsServiceNodeBlockValidFunc)) {
            LogPrint(BCLog::SNODE, "service node ping failed\n");
            return false;
        }

        snode->setConfig(config);
        snode->updatePing();

        // Relay
        connman->ForEachNode([&](CNode* pnode) {
            if (!pnode->fSuccessfullyConnected)
                return;
            const CNetMsgMaker msgMaker(pnode->GetSendVersion());
            connman->PushMessage(pnode, msgMaker.Make(sn::PING, ping));
        });

        return true;
    }

    /**
     * Returns a copy of the most recent servicenode list.
     * @return
     */
    std::vector<ServiceNode> list() {
        LOCK(mu);
        std::vector<ServiceNode> l; l.reserve(snodes.size());
        std::for_each(snodes.begin(), snodes.end(), [&l](const ServiceNodePtr & snode){
           l.push_back(*snode);
        });
        return std::move(l);
    }

    /**
     * Returns the servicenode with the specified pubkey.
     * @param snodePubKey
     * @return
     */
    ServiceNode getSn(const std::vector<unsigned char> & snodePubKey) {
        auto snode = findSn(snodePubKey);
        if (!snode)
            return ServiceNode();
        return *snode;
    }

    /**
     * Returns the servicenode with the specified pubkey.
     * @param snodePubKey
     * @return
     */
    ServiceNode getSn(const CPubKey & snodePubKey) {
        return getSn(std::vector<unsigned char>{snodePubKey.begin(), snodePubKey.end()});
    }

protected:
    /**
     * Returns the height of the longest chain.
     * @return
     */
    static int getActiveChainHeight() {
        LOCK(cs_main);
        return chainActive.Height();
    }

    /**
     * Returns the hash for the block at the specified height.
     * @param blockHeight
     * @return
     */
    static uint256 getActiveChainHash(int blockHeight) {
        LOCK(cs_main);
        return chainActive[blockHeight]->GetBlockHash();
    }

    /**
     * Returns the collateral amount required for the specified tier.
     * @param tier
     * @return
     */
    static CAmount collateralAmountForTier(const ServiceNode::Tier & tier = ServiceNode::Tier::OPEN) {
        if (tier == ServiceNode::SPV)
            return ServiceNode::COLLATERAL_SPV;
        else
            return 0;
    }

protected:
    /**
     * Add servicenode if valid and returns the added snode, otherwise returns nullptr.
     * @param snode
     * @return
     */
    ServiceNodePtr addSn(const ServiceNodePtr & snode) {
        if (!snode)
            return nullptr;
        LOCK(mu);
        snodes.insert(snode);
        return snode;
    }

    /**
     * Add servicenode if valid and returns the added snode, otherwise returns nullptr.
     * @param snode
     * @return
     */
    ServiceNodePtr addSn(const ServiceNode & snode) {
        if (!snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc))
            return nullptr;
        auto ptr = std::make_shared<ServiceNode>(snode);
        return addSn(ptr);
    }

    /**
     * Find and return the servicenode or nullptr if none found.
     * @param snodePubKey
     * @return
     */
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

    /**
     * Find and return the servicenode or nullptr if none found.
     * @param snodePubKey
     * @return
     */
    ServiceNodePtr findSn(const std::vector<unsigned char> & snodePubKey) {
        return findSn(CPubKey(snodePubKey));
    }

    /**
     * Removes the specified servicenode.
     * @param snode
     * @return
     */
    bool removeSn(const ServiceNodePtr & snode) {
        if (!hasSn(snode))
            return false;
        LOCK(mu);
        snodes.erase(snode);
        return true;
    }

    /**
     * Returns true if the snode is known.
     * @param snode
     * @return
     */
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

    /**
     * Finds collateral for the specifed servicenode tier.
     * @param tier
     * @param dest
     * @param collateral
     * @return
     */
    bool findCollateral(const ServiceNode::Tier & tier, const CTxDestination & dest, std::vector<COutPoint> & collateral) {
        std::vector<COutput> allCoins;
        auto wallets = GetWallets();
        for (auto & wallet : wallets) {
            std::vector<COutput> coins;
            {
                auto locked_chain = wallet->chain().lock();
                LOCK2(cs_main, wallet->cs_wallet);
                wallet->AvailableCoins(*locked_chain, coins);
            }
            for (const auto & coin : coins) {
                if (coin.nDepth < 1)
                    continue; // skip coin that doesn't have confirmations
                if (snodeUtxos.count(coin.GetInputCoin().outpoint) > 0)
                    continue; // skip coin already used in other snodes
                CTxDestination destination;
                if (!ExtractDestination(coin.GetInputCoin().txout.scriptPubKey, destination))
                    continue; // skip incompatible addresses
                if (dest == destination)
                    allCoins.push_back(coin); // coin matches address
            }
        }

        if (allCoins.empty()) {
            LogPrint(BCLog::SNODE, "bad service node collateral, no coins %s", EncodeDestination(dest));
            return false; // not enough coin for snode
        }

        // Run the algo to determine snode utxo selections

        // sort descending
        std::sort(allCoins.begin(), allCoins.end(), [](const COutput & a, const COutput & b) -> bool {
            return a.GetInputCoin().txout.nValue > b.GetInputCoin().txout.nValue;
        });

        std::vector<COutput> selected;
        int searchRounds{10}; // used to track number of search iterations
        bool done{false};

        while (true) { // use while loop for breaking out of control flow
            const auto & largest = allCoins[0];
            selected.push_back(largest);
            if (largest.GetInputCoin().txout.nValue >= collateralAmountForTier(tier)) {
                done = true;
                break;
            }

            if (allCoins.size() == 1) {
                LogPrint(BCLog::SNODE, "bad service node collateral, not enough coins %s", EncodeDestination(dest));
                return false; // nothing to search
            }

            // Conduct search rounds and find most suitable coin selections
            CAmount running{0};
            std::set<COutput> runningCoin;
            std::vector<COutput> searchCoins(allCoins.begin()+1, allCoins.end());
            for (int k = 0; k < searchRounds; ++k) {
                for (int j = static_cast<int>(searchCoins.size()) - 1; j >= 0; --j) {
                    const auto & next = searchCoins[j];
                    if (runningCoin.count(next) > 0)
                        continue; // skip already selected coin

                    if (largest.GetInputCoin().txout.nValue + running + next.GetInputCoin().txout.nValue >= collateralAmountForTier(tier)) {
                        runningCoin.insert(next);
                        done = true;
                        break;
                    }
                    if (j == 0) { // store the largest possible value of this search round
                        running += next.GetInputCoin().txout.nValue;
                        runningCoin.insert(next);
                        searchCoins.erase(searchCoins.begin());
                    }
                }
                if (done) {
                    selected.insert(selected.end(), runningCoin.begin(), runningCoin.end());
                    break;
                }
            }

            break;
        }

        if (!done) {
            LogPrint(BCLog::SNODE, "bad service node collateral, not enough coins %s", EncodeDestination(dest));
            return false; // failed to find enough coin for snode
        }

        collateral.clear(); collateral.reserve(selected.size());
        for (const auto & coin : selected)
            collateral.push_back(coin.GetInputCoin().outpoint);

        return true;
    }

protected:
    Mutex mu;
    std::set<ServiceNodePtr> snodes;
    std::set<uint256> seenPackets;
    std::set<COutPoint> snodeUtxos;
};

}

#endif //BLOCKNET_SERVICENODEMGR_H
