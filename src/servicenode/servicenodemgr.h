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
#include <utility>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

/**
 * Servicenode namepsace
 */
namespace sn {

extern const char* REGISTER;
extern const char* PING;

struct ServiceNodeConfigEntry {
    std::string alias;
    ServiceNode::Tier tier;
    CKey key;
    CTxDestination address;
    ServiceNodeConfigEntry(std::string alias, ServiceNode::Tier tier, CKey key, CTxDestination address)
                                                   : alias(std::move(alias)), tier(tier),
                                                     address(std::move(address)), key(std::move(key)) {}
    friend inline bool operator==(const ServiceNodeConfigEntry & a, const ServiceNodeConfigEntry & b) { return a.key == b.key; }
    friend inline bool operator!=(const ServiceNodeConfigEntry & a, const ServiceNodeConfigEntry & b) { return !(a.key == b.key); }
    friend inline bool operator<(const ServiceNodeConfigEntry & a, const ServiceNodeConfigEntry & b) { return a.alias.compare(b.alias) < 0; }
};

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
     * Clears the internal state.
     */
    void reset() {
        snodes.clear();
        seenPackets.clear();
        snodeUtxos.clear();
        snodeEntries.clear();
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
     * @param entry
     * @param connman
     * @return
     */
    bool registerSn(const ServiceNodeConfigEntry & entry, CConnman *connman) {
        return registerSn(entry.key, entry.tier, EncodeDestination(entry.address), connman);
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

    /**
     * Returns a copy of the currently loaded snode entries.
     * @return
     */
    std::vector<ServiceNodeConfigEntry> getSnEntries() {
        LOCK(mu);
        return std::move(std::vector<ServiceNodeConfigEntry>(snodeEntries.begin(), snodeEntries.end()));
    }

    /**
     * Load the servicenode config from disk. Returns true if config was successfully read, otherwise false
     * if there was a fatal error.
     * @param entries
     * @return
     */
    bool loadSnConfig(std::set<ServiceNodeConfigEntry> & entries) {
        boost::filesystem::path fp = getServiceNodeConf();
        boost::filesystem::ifstream fs(fp);

        auto closeOpenfs = [](boost::filesystem::ifstream & stream) {
            try {
                if (stream.is_open())
                    stream.close();
            } catch (...) {}
        };

        if (!fs.good()) {
            try {
                FILE *file = fopen(fp.string().c_str(), "a");
                if (file == nullptr)
                    return true;
                std::string strHeader = getSnConfigHelp();
                fwrite(strHeader.c_str(), std::strlen(strHeader.c_str()), 1, file);
                fclose(file);
            } catch (std::exception & e) {
                LogPrint(BCLog::SNODE, "Failed to read servicenode.conf: %s\n", e.what());
                closeOpenfs(fs);
                return false;
            }
        }

        entries.clear(); // prep the storage

        std::string line;
        while (std::getline(fs, line)) {
            if (line.empty())
                continue;

            std::istringstream iss(line);
            iss.imbue(std::locale::classic());
            std::string comment, alias, stier, skey, saddress;

            if (iss >> comment) {
                if (comment.at(0) == '#') continue;
                iss.str(line);
                iss.clear();
            }

            if (!(iss >> alias >> stier >> skey)) {
                iss.str(line);
                iss.clear();
                if (!(iss >> alias >> stier >> skey)) {
                    closeOpenfs(fs);
                    LogPrintf("Failed to setup servicenode, bad servicenode.conf entry: %s\n", line);
                    return false;
                }
            }

            iss >> saddress; // set address (this is optional and only required by non-OPEN tiers)
            boost::trim(saddress); // remove whitespace
            CTxDestination addr = DecodeDestination(saddress);

            ServiceNode::Tier tier;
            if (!tierFromString(stier, tier)) {
                LogPrintf("Failed to setup servicenode, bad servicenode.conf tier: %s %s %s\n", stier, alias, saddress);
                continue;
            }

            // Only validate address if this tier is not in the free tier or if optional address was specified
            if ((!freeTier(tier) || !saddress.empty()) && !IsValidDestination(addr)) {
                LogPrintf("Failed to setup servicenode, bad servicenode.conf address: %s %s\n", saddress, alias);
                continue;
            }

            CKey key = DecodeSecret(skey);
            if (!key.IsValid()) {
                LogPrintf("Failed to setup servicenode, bad servicenode.conf servicenodeprivkey: %s %s %s\n", alias, saddress);
                continue;
            }

            entries.insert(ServiceNodeConfigEntry(alias, tier, key, addr));
        }

        // Close the stream if it's open
        closeOpenfs(fs);

        {
            LOCK(mu);
            snodeEntries.clear();
            snodeEntries.insert(entries.begin(), entries.end());
        }

        return true;
    }

public:
    /**
     * Returns the servicenode configuration path.
     * @return
     */
    static boost::filesystem::path getServiceNodeConf() {
        return std::move(GetDataDir() / "servicenode.conf");
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

    /**
     * Returns the tier flag for the specified string, e.g. "OPEN" or "open".
     * @param tier
     * @return
     */
    static bool tierFromString(std::string stier, ServiceNode::Tier & tier) {
        boost::to_lower(stier, std::locale::classic());
        if (stier == "spv")
            tier = ServiceNode::SPV;
        else if (stier == "open")
            tier = ServiceNode::OPEN;
        else return false; // no valid tier found

        return true;
    }

    /**
     * Returns the string representation of the tier.
     * @param tier
     * @return
     */
    static std::string tierString(const ServiceNode::Tier & tier) {
        if (tier == ServiceNode::SPV)
            return "SPV";
        else if (tier == ServiceNode::OPEN)
            return "OPEN";
        return "INVALID";
    }

    /**
     * Returns true if the specified tier doesn't require collateral.
     * @param tier
     * @return
     */
    static bool freeTier(const ServiceNode::Tier & tier) {
        return ServiceNode::OPEN == tier;
    }

    /**
     * Returns string representation of the config entry.
     * @param entry
     * @return
     */
    static std::string configEntryToString(const ServiceNodeConfigEntry & entry) {
        return strprintf("%s %s %s %s", entry.alias, tierString(entry.tier),
                EncodeSecret(entry.key), EncodeDestination(entry.address));
    }

    /**
     * Writes the specified entries to the servicenode.conf. Note that this overwrites existing data.
     * @param entries
     * @return
     */
    static bool writeSnConfig(const std::vector<ServiceNodeConfigEntry> & entries, const bool & append=true) {
        boost::filesystem::path fp = getServiceNodeConf();
        try {
            std::string eol = "\n";
#ifdef WIN32
            eol = "\r\n";
#endif
            boost::filesystem::ofstream file;
            file.exceptions(std::ofstream::failbit | std::ofstream::badbit);
            std::ios_base::openmode opts = std::ios_base::binary;
            if (append)
                opts |= std::ios_base::app;
            file.open(fp, opts);

            if (!append) { // write the help only if not appending
                auto help = getSnConfigHelp() + eol;
                file.write(help.c_str(), help.size());
            }

            // Write entries
            for (const auto & entry : entries) {
                const auto & s = configEntryToString(entry) + eol;
                file.write(s.c_str(), s.size());
            }

        } catch (std::exception & e) {
            LogPrint(BCLog::SNODE, "Failed to write to servicenode.conf: %s\n", e.what());
            return false;
        } catch (...) {
            LogPrint(BCLog::SNODE, "Failed to write to servicenode.conf unknown error\n");
            return false;
        }

        return true;
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
     * Returns the servicenode.conf help text.
     * @return
     */
    static std::string getSnConfigHelp() {
        return "# Service Node config\n"
               "# Format: alias tier servicenodeprivkey address\n"
               "#   - alias can be any name, no spaces\n"
               "#   - tier can be either SPV or OPEN\n"
               "#   - servicenodeprivkey must be a valid base58 encoded private key\n"
               "#   - address must be a valid base58 encoded public key that contains the service node collateral\n"
               "# SPV tier requires 5000 BLOCK collateral and an associated BLOCK address and can charge fees for services\n"
               "# OPEN tier doesn't require any collateral, can't charge fees, and can only support XCloud plugins\n"
               "# Example: dev OPEN 6BeBjrnd4DP5rEvUtxBQVu1DTPXUn6mCY5kPB2DWiy9CwEB2qh1\n"
               "# Example: xrouter SPV 6B4VvHTn6BbHM3DRsf6M3Sk3jLbgzm1vp5jNe9ZYZocSyRDx69d Bj2w9gHtGp4FbVdR19tJZ9UHwWQhDXxGCM\n";
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
            LogPrint(BCLog::SNODE, "bad service node collateral, no coins %s\n", EncodeDestination(dest));
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
                LogPrint(BCLog::SNODE, "bad service node collateral, not enough coins %s\n", EncodeDestination(dest));
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
            LogPrint(BCLog::SNODE, "bad service node collateral, not enough coins %s\n", EncodeDestination(dest));
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
    std::set<ServiceNodeConfigEntry> snodeEntries;
};

}

#endif //BLOCKNET_SERVICENODEMGR_H
