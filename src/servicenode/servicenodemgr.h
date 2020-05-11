// Copyright (c) 2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_SERVICENODE_SERVICENODEMGR_H
#define BLOCKNET_SERVICENODE_SERVICENODEMGR_H

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
#include <validationinterface.h>
#ifdef ENABLE_WALLET
#include <wallet/wallet.h>
#endif // ENABLE_WALLET

#include <iostream>
#include <numeric>
#include <set>
#include <utility>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

/**
 * Servicenode namepsace
 */
namespace sn {

extern CTxDestination ServiceNodePaymentAddress(const std::string & snode);

/**
 * Hasher used with unordered_map and unordered_set
 */
struct Hasher {
    size_t operator()(const CPubKey & pubkey) const { return ReadLE64(pubkey.begin()); }
};

/**
 * Service node configuration entry (from servicenode.conf).
 */
struct ServiceNodeConfigEntry {
    std::string alias;
    ServiceNode::Tier tier;
    CKey key;
    CTxDestination address{CNoDestination()};
    ServiceNodeConfigEntry() : tier(ServiceNode::Tier::OPEN) {}
    ServiceNodeConfigEntry(std::string alias, ServiceNode::Tier tier, CKey key, CTxDestination address)
                                                   : alias(std::move(alias)), tier(tier),
                                                     address(std::move(address)), key(std::move(key)) {}
    ServiceNodeConfigEntry& operator=(const ServiceNodeConfigEntry & other) = default;
    friend inline bool operator==(const ServiceNodeConfigEntry & a, const ServiceNodeConfigEntry & b) { return a.key == b.key; }
    friend inline bool operator!=(const ServiceNodeConfigEntry & a, const ServiceNodeConfigEntry & b) { return !(a.key == b.key); }
    friend inline bool operator<(const ServiceNodeConfigEntry & a, const ServiceNodeConfigEntry & b) { return a.alias.compare(b.alias) < 0; }
    bool isNull() const {
        return boost::get<CNoDestination>(&address);
    }
    CKeyID keyId() const {
        return key.GetPubKey().GetID();
    }
    CKeyID addressKeyId() const {
        return boost::get<CKeyID>(address);
    }
};

/**
 * Manages related servicenode functions including handling network messages and storing an active list
 * of valid servicenodes.
 */
class ServiceNodeMgr : public CValidationInterface {
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
        LOCK(mu);
        snodes.clear();
        pings.clear();
        seenPackets.clear();
        snodeEntries.clear();
        seenBlocks.clear();
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
    bool processRegistration(CDataStream & ss, ServiceNode & snode) {
        ServiceNode sn;
        try {
            ss >> sn;
        } catch (...) {
            return false;
        }
        if (seenPacket(sn.getHash()))
            return false;

        auto snptr = addSn(sn);
        if (!snptr)
            return false;

        snode = *snptr;
        return true;
    }

    /**
     * Process cached registration. This skips the stale snode check.
     * @param ss
     * @param snode
     * @return
     */
    bool processCachedRegistration(CDataStream & ss, ServiceNode & snode) {
        ServiceNode sn;
        try {
            ss >> sn;
        } catch (...) {
            return false;
        }

        // Skip the stale check on the cached registration because we've already validated this
        // before and this is our own snode's registration packet.
        auto snptr = addSn(sn, true, false);
        if (!snptr)
            return false;

        snode = *snptr;
        return true;
    }

    /**
     * Processes a servicenode ping message from the network.
     * @param ss
     * @param ping
     * @param skipValidation If true the validation checks are skipped.
     * @return
     */
    bool processPing(CDataStream & ss, ServiceNodePing & ping, const bool skipValidation = false) {
        try {
            ss >> ping;
        } catch (...) {
            return false;
        }
        if (seenPacket(ping.getHash()))
            return false;

        if (!ping.isValid(GetTxFunc, IsServiceNodeBlockValidFunc, skipValidation))
            return false; // bad ping

        if (!addPing(ping))
            return false;

        addSn(ping.getSnode(), false); // skip validity check here because it's checked in the ping's
        return true;
    }

#ifdef ENABLE_WALLET
    /**
     * Registers a snode on the network. This will also automatically search the wallet for required collateral.
     * This requires the wallet to be unlocked any snodes that are not "OPEN" (free) snodes.
     * @param entry
     * @param connman
     * @param wallets
     * @param failReason
     * @return
     */
    bool registerSn(const ServiceNodeConfigEntry & entry, CConnman *connman,
            const std::vector<std::shared_ptr<CWallet>> & wallets, std::string *failReason = nullptr)
    {
        return registerSn(entry.key, entry.tier, EncodeDestination(entry.address), connman, wallets, failReason);
    }

    /**
     * Registers a snode on the network. This will also automatically search the wallet for required collateral.
     * This requires the wallet to be unlocked any snodes that are not "OPEN" (free) snodes.
     * @param key
     * @param tier
     * @param address
     * @param connman
     * @param wallets
     * @param failReason
     * @return
     */
    bool registerSn(const CKey & key, const ServiceNode::Tier & tier, const std::string & address, CConnman *connman,
                    const std::vector<std::shared_ptr<CWallet>> & wallets, std::string *failReason = nullptr)
    {
        const auto & snodePubKey = key.GetPubKey();

        // 1) Iterate over all wallets
        // 2) Iterate over all coins
        // 3) Add coin matching utxo address (exclude already used coin)
        // 4) Run pairing algo to pick utxos for snode
        // 5) Relay valid snode to peers

        std::vector<unsigned char> sig;
        std::vector<COutPoint> collateral;
        CKeyID addressId;

        if (tier != ServiceNode::Tier::OPEN) {
            CTxDestination dest = DecodeDestination(address);
            if (!IsValidDestination(dest)) {
                const auto errMsg = strprintf("service node registration failed, bad address: %s", address);
                if (failReason) *failReason = errMsg;
                return error(errMsg.c_str()); // fail on bad address
            }

            std::shared_ptr<CWallet> wallet;
            bool haveAddr{false};
            for (auto & w : wallets) {
                if (w->HaveKey(boost::get<CKeyID>(dest))) {
                    haveAddr = true;
                    wallet = w;
                    break;
                }
            }
            if (!haveAddr) {
                const std::string errMsg = "No wallets found with the specified address";
                if (failReason) *failReason = errMsg;
                return error(errMsg.c_str()); // stop if wallets do not have the collateral address
            }

            if (wallet->IsLocked()) {
                const std::string errMsg = "Failed to register service node because wallet is locked";
                if (failReason) *failReason = errMsg;
                return error(errMsg.c_str());
            }

            // Exclude already used collateral utxos
            std::set<COutPoint> alreadyAllocatedUtxos;
            {
                LOCK(mu);
                for (const auto & item : snodes) {
                    const auto & s = item.second;
                    if (s->getSnodePubKey() != snodePubKey) { // exclude registering snode
                        const auto & c = s->getCollateral();
                        alreadyAllocatedUtxos.insert(c.begin(), c.end());
                    }
                }
            }

            if (!findCollateral(tier, dest, {wallet}, collateral, alreadyAllocatedUtxos)) {
                const auto errMsg = strprintf("service node registration failed, bad collateral: %s", address);
                if (failReason) *failReason = errMsg;
                return error(errMsg.c_str()); // fail on bad collateral
            }

            addressId = boost::get<CKeyID>(dest);
            const auto & sighash = sn::ServiceNode::CreateSigHash(snodePubKey, tier, addressId, collateral,
                                                                  chainActive.Height(), chainActive.Tip()->GetBlockHash());

            // Sign the servicenode with the collateral's private key
            CKey sign;
            CKeyID keyid = GetKeyForDestination(*wallet, dest);
            if (keyid.IsNull() || !wallet->GetKey(keyid, sign) || !sign.SignCompact(sighash, sig) || sig.empty()) {
                const auto errMsg = strprintf("service node registration failed, bad signature, is the wallet unlocked? %s", address);
                if (failReason) *failReason = errMsg;
                return error(errMsg.c_str()); // key not found in wallets
            }

        } else { // OPEN tier
            const auto & sighash = sn::ServiceNode::CreateSigHash(snodePubKey, tier, addressId, collateral,
                                                                  chainActive.Height(), chainActive.Tip()->GetBlockHash());

            if (!key.SignCompact(sighash, sig) || sig.empty()) { // sign with snode pubkey
                const auto errMsg = strprintf("service node registration failed, bad signature, is the servicenode.conf populated? %s", address);
                if (failReason) *failReason = errMsg;
                return error(errMsg.c_str());
            }
        }

        ServiceNode snode(snodePubKey, tier, addressId, collateral, chainActive.Height(),
                chainActive.Tip()->GetBlockHash(), sig);
        auto snodePtr = addSn(snode);
        if (!snodePtr) {
            const std::string errMsg = "service node registration failed";
            if (failReason) *failReason = errMsg;
            return error(errMsg.c_str()); // failed to add snode
        }

        // Write latest snode registration to disk if active snode matches registration
        const auto & activesn = getActiveSn();
        if (!activesn.isNull()) {
            auto s = getSn(activesn.key.GetPubKey());
            if (!s.isNull())
                writeSnRegistration(s);
        }

        // Relay
        connman->ForEachNode([&](CNode* pnode) {
            if (!pnode->fSuccessfullyConnected)
                return;
            const CNetMsgMaker msgMaker(pnode->GetSendVersion());
            connman->PushMessage(pnode, msgMaker.Make(NetMsgType::SNREGISTER, *snodePtr));
        });

        return true;
    }
#endif // ENABLE_WALLET

    /**
     * Submits a servicenode ping to the network. This method will also update our own servicenode
     * @param key
     * @param connman
     * @return
     */
    bool sendPing(const uint32_t & protocol, const std::string & config, CConnman *connman) {
        if (!hasActiveSn()) {
            LogPrint(BCLog::SNODE, "service node ping failed, service node not found\n");
            return false;
        }

        const auto & activesn = getActiveSn();
        const auto & snode = findSn(activesn.key.GetPubKey());
        if (!snode) {
            LogPrint(BCLog::SNODE, "service node ping failed, service node not running\n");
            return false;
        }

        const uint32_t bestBlock = getActiveChainHeight();
        const uint256 & bestBlockHash = getActiveChainHash(bestBlock);

        snode->setConfig(config, Params());
        snode->updatePing();

        ServiceNodePing ping(activesn.key.GetPubKey(), bestBlock, bestBlockHash, static_cast<uint32_t>(GetTime()), config, *snode);
        ping.sign(activesn.key);
        if (!ping.isValid(GetTxFunc, IsServiceNodeBlockValidFunc)) {
            LogPrint(BCLog::SNODE, "service node ping failed\n");
            return false;
        }

        if (!addPing(ping)) {
            LogPrint(BCLog::SNODE, "service node ping failed: existing newer ping already exists\n");
            return false; // failed to add our own ping
        }

        addSn(ping.getSnode(), false); // skip validity check here because it's checked in the ping's

        // Relay
        connman->ForEachNode([&](CNode* pnode) {
            if (!pnode->fSuccessfullyConnected)
                return;
            const CNetMsgMaker msgMaker(pnode->GetSendVersion());
            connman->PushMessage(pnode, msgMaker.Make(NetMsgType::SNPING, ping));
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
        for (const auto & item : snodes)
           l.push_back(*item.second);
        return l;
    }

    /**
     * Returns the servicenode ping with the specified snode pubkey.
     * @param snodePubKey
     * @return
     */
    const ServiceNodePing & getPing(const CPubKey & snodePubKey) {
        LOCK(mu);
        if (!pings.count(snodePubKey))
            return std::move(ServiceNodePing{});
        return pings[snodePubKey];
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
     * Returns the servicenode with the specified node address.
     * @param nodeAddr
     * @return
     */
    ServiceNode getSn(const std::string & nodeAddr) {
        LOCK(mu);
        for (const auto & s : snodes)
            if (s.second->getHostPort() == nodeAddr)
                return *s.second;
        return ServiceNode{};
    }

    /**
     * Returns a copy of the currently loaded snode entries.
     * @return
     */
    std::vector<ServiceNodeConfigEntry> getSnEntries() {
        LOCK(mu);
        return std::vector<ServiceNodeConfigEntry>(snodeEntries.begin(), snodeEntries.end());
    }

    /**
     * Returns the specific entry if it exists, otherwise returns a null entry. See isNull()
     * @param id The CKeyID of the service node entry you want.
     * @return
     */
    ServiceNodeConfigEntry getSnEntry(const CKeyID & id) {
        LOCK(mu);
        for (const auto & entry : snodeEntries) {
            if (id == entry.keyId())
                return entry;
        }
        return ServiceNodeConfigEntry{};
    }

    /**
     * Returns true if the snode is one of ours.
     * @param snode
     * @param entryRet Return the matched snode entry
     * @return
     */
    bool isMine(const ServiceNode & snode, ServiceNodeConfigEntry & entryRet) {
        LOCK(mu);
        for (const auto & entry : snodeEntries) {
            if (entry.keyId() == snode.getSnodePubKey().GetID()) {
                entryRet = entry;
                return true;
            }
        }
        return false;
    }

    /**
     * Returns true if an active service node exists. This node must be a servicenode
     * indicated by the "-servicenode=1" flag on the command line or in the config.
     * @return
     */
    bool hasActiveSn() {
        LOCK(mu);
        return gArgs.GetBoolArg("-servicenode", false) && !snodeEntries.empty();
    }

    /**
     * Removes the specified snode entry.
     * @return
     */
    void removeSnEntry(const ServiceNodeConfigEntry & entry) {
        {
            LOCK(mu);
            if (!snodeEntries.count(entry))
                return;
            snodeEntries.erase(entry);
            writeSnConfig(std::vector<ServiceNodeConfigEntry>(snodeEntries.begin(), snodeEntries.end()), false);
        }
        if (hasSn(entry.key.GetPubKey()))
            removeSn(entry.key.GetPubKey());
    }

    /**
     * Remove snode entries as well as their respective servicenode pointers.
     */
    void removeSnEntries() {
        LOCK(mu);
        for (const auto & entry : snodeEntries)
            snodes.erase(entry.key.GetPubKey());
        snodeEntries.clear();
    }

    /**
     * Returns the active service node entry (the first in the list).
     * @return
     */
    ServiceNodeConfigEntry getActiveSn() {
        if (!gArgs.GetBoolArg("-servicenode", false))
            return ServiceNodeConfigEntry{};
        LOCK(mu);
        if (!snodeEntries.empty())
            return *snodeEntries.begin();
        return ServiceNodeConfigEntry{};
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
                LogPrintf("Failed to setup servicenode, bad servicenode.conf snodekey: %s %s\n", alias, saddress);
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
        return GetDataDir() / "servicenode.conf";
    }

    /**
     * Returns the servicenode registration file.
     * @return
     */
    static boost::filesystem::path getServiceNodeRegistrationConf() {
        return GetDataDir() / ".servicenoderegistration";
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
        if (stier == "spv") {
            tier = ServiceNode::SPV;
            return true;
        } else if (stier == "open") {
            tier = ServiceNode::OPEN;
            return true;
        } else
            return false; // no valid tier found
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

    /**
     * Writes the specified snode packet to disk if it's for the active service node.
     * @param snode
     * @return
     */
    static bool writeSnRegistration(const ServiceNode & snode) {
        if (snode.isNull())
            return false; // do not process if snode is null
        boost::filesystem::path fp = getServiceNodeRegistrationConf();
        try {
            CDataStream ss(SER_DISK, 0);
            ss << snode;
            boost::filesystem::ofstream file;
            file.exceptions(std::ofstream::failbit | std::ofstream::badbit);
            std::ios_base::openmode opts = std::ios_base::binary;
            file.open(fp, opts);
            file.write(ss.str().c_str(), ss.size());
        } catch (std::exception & e) {
            LogPrint(BCLog::SNODE, "Failed to write to .servicenoderegistration: %s\n", e.what());
            return false;
        } catch (...) {
            LogPrint(BCLog::SNODE, "Failed to write to .servicenoderegistration unknown error\n");
            return false;
        }

        return true;
    }

    /**
     * Loads the snode packet from disk if it exists. Returns false if cached registration does not exist.
     * Also returns false on error.
     * @param snode
     * @return
     */
    bool loadSnRegistrationFromDisk(ServiceNode & snode) {
        boost::filesystem::path fp = getServiceNodeRegistrationConf();
        boost::filesystem::ifstream fs(fp);
        if (fs.good()) {
            std::stringstream buffer;
            buffer << fs.rdbuf();
            auto str = buffer.str();
            CDataStream ss(std::vector<char>(str.begin(), str.end()), SER_DISK, 0);
            return processCachedRegistration(ss, snode);
        }
        return false;
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
               "# Format: alias tier snodekey address\n"
               "#   - alias can be any name, no spaces\n"
               "#   - tier can be either SPV or OPEN\n"
               "#   - snodekey must be a valid base58 encoded private key\n"
               "#   - address must be a valid base58 encoded public key that contains the service node collateral\n"
               "# SPV tier requires 5000 BLOCK collateral and an associated BLOCK address and can charge fees for services\n"
               "# OPEN tier doesn't require any collateral, can't charge fees, and can only support XCloud plugins\n"
               "# Example: dev OPEN 6BeBjrnd4DP5rEvUtxBQVu1DTPXUn6mCY5kPB2DWiy9CwEB2qh1\n"
               "# Example: xrouter SPV 6B4VvHTn6BbHM3DRsf6M3Sk3jLbgzm1vp5jNe9ZYZocSyRDx69d Bj2w9gHtGp4FbVdR19tJZ9UHwWQhDXxGCM\n";
    }

protected:

    /**
     * Add the service node ping. Returns true if the ping was added, otherwise returns
     * false.
     * @param ping
     * @return
     */
    bool addPing(const ServiceNodePing & ping) {
        LOCK(mu);
        const auto & pubkey = ping.getSnodePubKey();
        // only add if this ping is newer than last known ping
        if (!pings.count(pubkey) || pings[pubkey].getPingTime() < ping.getPingTime()) {
            pings[pubkey] = ping;
            return true;
        }
        return false;
    }

    /**
     * Add servicenode if valid and returns the added snode, otherwise returns nullptr.
     * @param snode
     * @param checkValid default true, skips validity check if false
     * @param staleCheck default true, skips stale check if false
     * @return
     */
    ServiceNodePtr addSn(const ServiceNode & snode, const bool checkValid = true, const bool staleCheck = true) {
        if (checkValid && !snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc, staleCheck))
            return nullptr;
        removeSnWithCollateral(snode);
        auto ptr = std::make_shared<ServiceNode>(snode);
        {
            LOCK(mu);
            snodes[ptr->getSnodePubKey()] = ptr;
        }
        return ptr;
    }

    /**
     * Find and return the servicenode or nullptr if none found.
     * @param snodePubKey
     * @return
     */
    ServiceNodePtr findSn(const CPubKey & snodePubKey) {
        LOCK(mu);
        if (snodes.count(snodePubKey))
            return snodes[snodePubKey];
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
    bool removeSn(const CPubKey & snodePubKey) {
        if (!hasSn(snodePubKey))
            return false;
        LOCK(mu);
        snodes.erase(snodePubKey);
        return true;
    }

    /**
     * Returns true if the snode is known.
     * @param snode
     * @return
     */
    bool hasSn(const CPubKey & snodePubKey) {
        LOCK(mu);
        return snodes.count(snodePubKey) > 0;
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
     * Removes existing snodes that match the collateral utxos of
     * the specified snode. i.e. This method will mutate the snode
     * list and remove the existing snodes matching the specified
     * snode's collateral. This will prevent duplicate snodes
     * pointing to the same collateral inputs.
     * @param snode
     */
    void removeSnWithCollateral(const ServiceNode & snode) {
        LOCK(mu);
        std::map<COutPoint, ServiceNodePtr> utxos;
        for (const auto & item : snodes) {
            const auto & s = item.second;
            if (s->getSnodePubKey() != snode.getSnodePubKey()) { // exclude specified snode
                for (const auto & utxo : s->getCollateral())
                    utxos[utxo] = s;
            }
        }
        for (const auto & utxo : snode.getCollateral()) {
            if (utxos.count(utxo) && snodes.count(utxos[utxo]->getSnodePubKey()))
                snodes.erase(utxos[utxo]->getSnodePubKey());
        }
    }

#ifdef ENABLE_WALLET
    /**
     * Finds collateral for the specifed servicenode tier.
     * @param tier
     * @param dest
     * @param wallets
     * @param collateral
     * @param excludedUtxos
     * @return
     */
    bool findCollateral(const ServiceNode::Tier & tier, const CTxDestination & dest,
            const std::vector<std::shared_ptr<CWallet>> & wallets, std::vector<COutPoint> & collateral,
            const std::set<COutPoint> & excludedUtxos)
    {
        std::vector<COutput> allCoins;
        for (auto & wallet : wallets) {
            std::vector<COutput> coins;
            {
                auto locked_chain = wallet->chain().lock();
                registrationCoins(*locked_chain, wallet.get(), coins);
            }
            for (const auto & coin : coins) {
                if (coin.nDepth < 2)
                    continue; // skip coin that doesn't have required confirmations
                if (excludedUtxos.count(coin.GetInputCoin().outpoint) > 0)
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

        int maxUtxoCollateralCount{Params().GetConsensus().snMaxCollateralCount};
        std::vector<COutput> selected;
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

            // Conduct search rounds and find most suitable coin selections. The
            // purpose of the search below is to find combination of the smallest
            // coins required to meet the collateral. The list is sorted largest
            // coins first, so we iterate backwards. Search rounds indicate the
            // maximum number of utxos allowed to be associated with a snode.
            CAmount running{0};
            std::set<COutput> runningCoin;
            std::vector<COutput> searchCoins(allCoins.begin()+1, allCoins.end());
            for (int k = 0; k < maxUtxoCollateralCount; ++k) {
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

    /**
     * Find coins that meet the requirements for use in service node collateral. Note that immature coins are
     * valid a service node collateral because they are still under the ownership of the service node operator.
     * The majority of this code was taken from CWallet::AvailableCoins
     * @param locked_chain
     * @param wallet
     * @param vCoins
     */
    void registrationCoins(interfaces::Chain::Lock & locked_chain, CWallet *wallet, std::vector<COutput> & vCoins) const {
        LOCK2(cs_main, wallet->cs_wallet);
        vCoins.clear();

        for (const auto & entry : wallet->mapWallet) {
            const uint256& wtxid = entry.first;
            const CWalletTx* pcoin = &entry.second;

            if (!CheckFinalTx(*pcoin->tx))
                continue;

            int nDepth = pcoin->GetDepthInMainChain(locked_chain);
            if (nDepth <= 0) // require confirmations
                continue;

            if (!pcoin->IsTrusted(locked_chain))
                continue;

            for (unsigned int i = 0; i < pcoin->tx->vout.size(); i++) {
                if (wallet->IsLockedCoin(entry.first, i))
                    continue;

                if (wallet->IsSpent(locked_chain, wtxid, i))
                    continue;

                isminetype mine = wallet->IsMine(pcoin->tx->vout[i]);
                if (mine == ISMINE_NO)
                    continue;

                bool solvable = IsSolvable(*wallet, pcoin->tx->vout[i].scriptPubKey);
                bool spendable = ((mine & ISMINE_SPENDABLE) != ISMINE_NO) || ((mine & ISMINE_WATCH_ONLY) != ISMINE_NO);

                vCoins.emplace_back(pcoin, i, nDepth, spendable, solvable, true);
            }
        }
    }
#endif // ENABLE_WALLET

protected:
    /**
     * Records when the last known block was received.
     */
    void addRecentBlock() {
        LOCK(mu);
        if (seenBlocks.size() >= 2)
            seenBlocks.erase(seenBlocks.begin());
        seenBlocks.push_back(GetTime());
    }

    /**
     * Returns true if the last time a block was received was within N seconds
     * of the specified time.
     * @param seconds
     * @return
     */
    bool seenBlockRecently(const int seconds=2) {
        LOCK(mu);
        if (seenBlocks.size() < 2)
            return false;
        const auto diff = seenBlocks.back() - seenBlocks.front();
        return diff <= seconds;
    }

protected:
    void BlockConnected(const std::shared_ptr<const CBlock>& block, const CBlockIndex* pindex,
                        const std::vector<CTransactionRef>& txn_conflicted) override
    {
        processValidationBlock(block, true, pindex->nHeight);
    }

    void BlockDisconnected(const std::shared_ptr<const CBlock>& block) override {
        processValidationBlock(block, false);
    }

    void UpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *pindexFork, bool fInitialDownload) override {
#ifndef ENABLE_WALLET
        return;
#else
        addRecentBlock();
        if (fInitialDownload || seenBlockRecently())
            return; // do not try and register snode during initial download

        // Check if any of our snodes have inputs that were spent and/or staked
        std::set<ServiceNodeConfigEntry> entries;
        {
            LOCK(mu);
            // Update current block number on snode list
            for (auto & item : snodes)
                item.second->setCurrentBlock(pindexNew->nHeight);
            // copy entries
            entries = snodeEntries;
        }
        if (entries.empty())
            return; // do not proceed if no snode entries

        // Only consider re-registration attempts on snode entries that have collateral
        // inputs associated with a key in our wallet.
        auto wallets = GetWallets();
        for (const auto & entry : entries) {
            const auto & snode = getSn(entry.key.GetPubKey());
            if (snode.isNull())
                continue; // skip snodes we don't know about

            if (!snode.getInvalid() && snode.isValid(GetTxFunc, IsServiceNodeBlockValidFunc))
                continue; // skip valid snodes

            // At this point we want to try and re-register any snodes that are marked
            // invalid.

            // Make sure the snode collateral is in our wallet
            bool haveAddr{false};
            for (auto & w : wallets) {
                if (w->HaveKey(boost::get<CKeyID>(entry.address))) {
                    haveAddr = true;
                    break;
                }
            }
            if (!haveAddr)
                continue; // snode collateral not in wallet

            // Try re-registering
            std::string failReason;
            if (registerSn(entry, g_connman.get(), wallets, &failReason))
                LogPrintf("Service node registration succeeded for %s\n", entry.alias);
            else
                LogPrintf("Retrying service node %s registration on the next block\n", entry.alias);
        }
#endif // ENABLE_WALLET
    }

    void processValidationBlock(const std::shared_ptr<const CBlock>& block, const bool connected, const int blockNumber=0) {
        // Store all spent vins
        std::set<COutPoint> spent;
        for (const auto & tx : block->vtx) {
            if (connected) { // when block is being added check for spent utxos in vin list
                for (const auto & vin : tx->vin)
                    spent.insert(vin.prevout);
            } else { // when block is being disconnected mark utxos in the vout list as spent
                const auto hash = tx->GetHash();
                for (int i = 0; i < tx->vout.size(); ++i)
                    spent.insert(COutPoint{hash, static_cast<uint32_t>(i)});
            }
        }

        // Check that existing snodes are valid
        {
            LOCK(mu);
            for (auto & item : snodes) {
                auto snode = item.second;
                for (const auto & collateral : snode->getCollateral()) {
                    if (spent.count(collateral)) {
                        snode->markInvalid(true, blockNumber);
                        break;
                    }
                }
                // Re-validate snodes on potential reorg (on block disconnected)
                if (!connected) {
                    snode->markInvalid(false); // reset state before is valid check
                    snode->markInvalid(!snode->isValid(GetTxFunc, IsServiceNodeBlockValidFunc));
                }
            }
        }
    }

protected:
    Mutex mu;
    std::map<CPubKey, ServiceNodePtr> snodes;
    std::unordered_map<CPubKey, ServiceNodePing, Hasher> pings;
    std::set<uint256> seenPackets;
    std::set<ServiceNodeConfigEntry> snodeEntries;
    std::vector<int> seenBlocks;
};

}

#endif //BLOCKNET_SERVICENODE_SERVICENODEMGR_H
