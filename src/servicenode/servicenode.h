// Copyright (c) 2019-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_SERVICENODE_SERVICENODE_H
#define BLOCKNET_SERVICENODE_SERVICENODE_H

#include <amount.h>
#include <base58.h>
#include <chainparams.h>
#include <hash.h>
#include <key.h>
#include <primitives/transaction.h>
#include <pubkey.h>
#include <script/standard.h>
#include <streams.h>
#include <sync.h>
#include <timedata.h>
#include <univalue.h>
#include <util/time.h>

#include <set>
#include <utility>
#include <vector>

#include <xrouter/xroutersettings.h>
#include <xrouter/xrouterutils.h>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>

extern int GetChainTipHeight();

/**
 * Servicenode namepsace
 */
namespace sn {

typedef std::function<bool(const COutPoint & out, CTransactionRef & tx)> TxFunc;
typedef std::function<bool(const uint32_t & blockNumber, const uint256 & blockHash, const bool & checkStale)> BlockValidFunc;

/**
 * Represents a legacy XBridge packet.
 */
class LegacyXBridgePacket {
public:
    /**
     * Deserializees the network packet.
     * @param packet
     */
    void CopyFrom(std::vector<unsigned char> packet) {
        unsigned int offset{20+8}; // ignore packet address (uint160) & timestamp (uint64_t)
        version   = *static_cast<uint32_t*>(static_cast<void*>(&packet[0]+offset)); offset += sizeof(uint32_t);
        command   = *static_cast<uint32_t*>(static_cast<void*>(&packet[0]+offset)); offset += sizeof(uint32_t);
        timestamp = *static_cast<uint32_t*>(static_cast<void*>(&packet[0]+offset)); offset += sizeof(uint32_t);
        bodysize  = *static_cast<uint32_t*>(static_cast<void*>(&packet[0]+offset)); offset += sizeof(uint32_t);
        pubkey    = CPubKey(packet.begin()+offset, packet.begin()+offset+CPubKey::COMPRESSED_PUBLIC_KEY_SIZE); offset += CPubKey::COMPRESSED_PUBLIC_KEY_SIZE;
        signature = std::vector<unsigned char>(packet.begin()+offset, packet.begin()+offset+64); offset += 64;
        body      = std::vector<unsigned char>(packet.begin()+offset, packet.end());
    }

public:
    uint32_t version;
    uint32_t command;
    uint32_t timestamp;
    uint32_t bodysize;
    CPubKey pubkey;
    std::vector<unsigned char> signature;
    std::vector<unsigned char> body;
};

/**
 * Servicenodes are responsible for providing services to the network.
 */
class ServiceNode {
public:
    /**
     * Default collateral for SPV servicenodes.
     */
    static const CAmount COLLATERAL_SPV = 5000 * COIN;
    /**
     * Grace period in blocks for invalid servicenode collateral.
     */
    static const int VALID_GRACEPERIOD_BLOCKS = 2;

    /**
     * Supported Servicenode Tiers
     */
    enum Tier : uint8_t {
        OPEN = 0,
        SPV = 50,
    };

    /**
     * Create a valid servicenode hash for use with signing.
     * @param snodePubKey
     * @param tier
     * @param paymentAddress
     * @param collateral
     * @param bestBlock
     * @param bestBlockHash
     */
    static uint256 CreateSigHash(const CPubKey & snodePubKey, const Tier & tier, const CKeyID & paymentAddress,
                                 const std::vector<COutPoint> & collateral,
                                 const uint32_t & bestBlock=0, const uint256 & bestBlockHash=uint256())
    {
        CHashWriter ss(SER_GETHASH, 0);
        ss << snodePubKey << static_cast<uint8_t>(tier) << paymentAddress << collateral << bestBlock << bestBlockHash;
        return ss.GetHash();
    }

public:
    /**
     * Constructor
     */
    explicit ServiceNode() : snodePubKey(CPubKey()), tier(Tier::OPEN), paymentAddress(CKeyID()),
                             collateral(std::vector<COutPoint>()), bestBlock(0), bestBlockHash(uint256()),
                             signature(std::vector<unsigned char>()), pingtime(0),
                             config(std::string()), pingBestBlock(0), pingBestBlockHash(uint256()) {}

    /**
     * Constructor
     * @param snodePubKey
     * @param tier
     * @param collateral
     * @param bestBlock
     * @param bestBlockHash
     * @param signature
     */
    explicit ServiceNode(CPubKey snodePubKey, Tier tier, CKeyID paymentAddress, std::vector<COutPoint> collateral,
                         uint32_t bestBlock, uint256 bestBlockHash, std::vector<unsigned char> signature)
                              : snodePubKey(snodePubKey),        tier(tier),
                                paymentAddress(paymentAddress),  collateral(std::move(collateral)),
                                bestBlock(bestBlock),            bestBlockHash(bestBlockHash),
                                signature(std::move(signature)), pingtime(0),
                                pingBestBlock(bestBlock),        pingBestBlockHash(bestBlockHash),
                                config(std::string()) {}

    friend inline bool operator==(const ServiceNode & a, const ServiceNode & b) { return a.snodePubKey == b.snodePubKey; }
    friend inline bool operator!=(const ServiceNode & a, const ServiceNode & b) { return !(a.snodePubKey == b.snodePubKey); }
    friend inline bool operator<(const ServiceNode & a, const ServiceNode & b) { return a.snodePubKey < b.snodePubKey; }

    /**
     * Returns true if the servicenode is uninitialized (e.g. via empty constructor).
     */
    bool isNull() const {
        return !snodePubKey.IsValid();
    }

    /**
     * Returns true if the servicenode supports Enterprise XRouter requests.
     */
    bool isEXRCompatible() const {
        return exrCompatible;
    }

    /**
     * Returns the servicenode's public key.
     * @return
     */
    CPubKey getSnodePubKey() const {
        return snodePubKey;
    }

    /**
     * Returns the servicenode tier.
     * @return
     */
    Tier getTier() const {
        return static_cast<Tier>(tier);
    }

    /**
     * Returns the servicenode default payment address.
     * @return
     */
    CKeyID getPaymentAddress() const {
        // TODO Blocknet Servicenode allow overriding default payment address in config
        return paymentAddress;
    }

    /**
     * Returns the servicenode collateral.
     * @return
     */
    const std::vector<COutPoint>& getCollateral() const {
        return collateral;
    }

    /**
     * Returns the servicenode best block.
     * @return
     */
    const uint32_t& getBestBlock() const {
        return bestBlock;
    }

    /**
     * Returns the servicenode best block hash.
     * @return
     */
    const uint256& getBestBlockHash() const {
        return bestBlockHash;
    }

    /**
     * Returns the servicenode signature.
     * @return
     */
    const std::vector<unsigned char>& getSignature() const {
        return signature;
    }

    /**
     * Returns the servicenode last ping time in unix time.
     * @return
     */
    const int64_t& getPingTime() const {
        return pingtime;
    }

    /**
     * Returns the servicenode xbridge protocol version.
     * @return
     */
    const uint32_t& getXBridgeVersion() const {
        return xbridgeversion;
    }

    /**
     * Returns the servicenode xrouter protocol version.
     * @return
     */
    const uint32_t& getXRouterVersion() const {
        return xrouterversion;
    }

    /**
     * Returns true if the servicenode is running. A servicenode is considered
     * running if its last ping time was less than 5 minutes ago.
     * @return
     */
    bool running() const {
        return (!invalid || (currentBlock - invalidBlock >= 0 && currentBlock - invalidBlock < VALID_GRACEPERIOD_BLOCKS))
               && GetAdjustedTime() - pingtime < 300;
    }

    /**
     * Returns the servicenode last ping time in unix time.
     * @param reportedPingTime The ping time reported by the originator (i.e. service node sending the ping).
     * @return
     */
    void updatePing(const int64_t reportedPingTime = 0) {
        const auto currentTime = GetAdjustedTime();
        if (reportedPingTime == 0 || currentTime < reportedPingTime)
            pingtime = currentTime;
        else
            pingtime = reportedPingTime;
    }

    /**
     * Assigns the specified block information as the best block number and associated hash on the servicenode.
     * @param blockNumber
     * @param blockHash
     */
    void setBestBlock(const uint32_t & blockNumber, const uint256 & blockHash) {
        pingBestBlock = blockNumber;
        pingBestBlockHash = blockHash;
    }

    /**
     * Assigns the specified config to the servicenode.
     * @param c
     * @param chainparams Chain parameters for use with mainnet, testnet, regtest chains
     */
    void setConfig(const std::string & c, const CChainParams & chainparams) {
        config = c;
        services.clear();
        parseConfig(config, chainparams);
    }

    /**
     * Return the servicenode config.
     * @return
     */
    std::string getConfig() const {
        return config;
    }

    /**
     * Return the servicenode config filtered by the specified key name. The config's
     * json object matching the name of the specified filter will be returned.
     * @param filter Filter the config by the specified json object key.
     * @return
     */
    std::string getConfig(const std::string & filter) const {
        UniValue uv;
        if (!uv.read(config))
            return "";
        return find_value(uv, filter).write();
    }

    /**
     * Return the host cnet address.
     * @return
     */
    const CService & getHostAddr() const {
        return addr;
    }

    /**
     * Return the host (DNS or IP from config).
     * @return
     */
    std::string getHost() const {
        return host.empty() ? addr.ToStringIP() : host;
    }

    /**
     * Return the host and port.
     * @return
     */
    std::string getHostPort() const {
        if (exrCompatible)
            return host.empty() ? addr.ToStringIPPort() : (host + ":" + addr.ToStringPort());
        else
            return addr.ToStringIPPort();
    }

    /**
     * Returns true if this servicenode supports the specified service.
     * @param service
     * @return
     */
    bool hasService(const std::string & service) const {
        for (const auto & s : services) {
            if (s == service)
                return true;
        }
        return false;
    }

    /**
     * Returns the spv services list (supported tokens).
     * @return
     */
    const std::vector<std::string> & serviceList() const {
        return services;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(snodePubKey);
        READWRITE(tier);
        READWRITE(paymentAddress);
        READWRITE(collateral);
        READWRITE(bestBlock);
        READWRITE(bestBlockHash);
        READWRITE(signature);
        if (ser_action.ForRead()) {
            pingBestBlock = bestBlock;
            pingBestBlockHash = bestBlockHash;
        }
    }

    /**
     * Returns the hash used in signing.
     * @return
     */
    uint256 sigHash() const {
        return CreateSigHash(snodePubKey, static_cast<Tier>(tier), paymentAddress, collateral, bestBlock, bestBlockHash);
    }

    /**
     * Returns the servicenode's hash (includes the signature).
     * @return
     */
    uint256 getHash() const {
        CHashWriter ss(SER_GETHASH, 0);
        ss << snodePubKey << static_cast<uint8_t>(tier) << paymentAddress << collateral << bestBlock << bestBlockHash
           << config << signature;
        return ss.GetHash();
    }

    /**
     * Returns true if the Servicenode is valid. The stale check defaults to true, by default this adds additional
     * measures to verify a Servicenode. The Servicenode ping will change this state periodically, therefore it may
     * be necessary to specifically disable the stale check if initial validation checks passed at the time of the
     * initial Servicenode ping.
     * @param getTxFunc
     * @param isBlockValid
     * @param checkStale
     * @return
     */
    bool isValid(const TxFunc & getTxFunc, const BlockValidFunc & isBlockValid, const bool & checkStale=true) const
    {
        // Block reported by snode must be ancestor of our chain tip
        if (!isBlockValid(pingBestBlock, pingBestBlockHash, checkStale))
            return false;

        // Validate the snode pubkey
        if (!snodePubKey.IsFullyValid())
            return false;

        // TODO Blocknet OPEN tier snodes, support non-SPV snode tiers (enable unit tests)
        if (tier != ServiceNode::Tier::SPV)
            return false;

        // If open tier, the signature should be generated from the snode pubkey
        if (tier == Tier::OPEN) {
            const auto & sighash = sigHash();
            CPubKey pubkey2;
            if (!pubkey2.RecoverCompact(sighash, signature))
                return false; // not valid if bad sig
            return snodePubKey.GetID() == pubkey2.GetID();
        }

        //
        // Paid tiers signatures should be derived from the collateral privkey.
        //

        // only require this check on paid tiers
        if (paymentAddress.IsNull())
            return false; // must have valid payment address

        // If not on the open tier, check collateral
        if (collateral.empty() || collateral.size() > Params().GetConsensus().snMaxCollateralCount)
            return false; // not valid if no collateral or too many collateral inputs

        // Check for duplicate collateral utxos
        const std::set<COutPoint> dups(collateral.begin(), collateral.end());
        if (dups.size() != collateral.size())
            return false; // not valid if duplicates

        const auto & sighash = sigHash();
        CPubKey pubkey;
        if (!pubkey.RecoverCompact(sighash, signature))
            return false; // not valid if bad sig

        CAmount total{0}; // Track the total collateral amount
        std::set<CScriptID> processed; // Track already processed utxos

        // Determine if all collateral utxos validate the sig
        for (const auto & op : collateral) {
            CTransactionRef tx;
            auto success = getTxFunc(op, tx);
            if (!tx)
                return false;
            if (invalidBlock > 0) {
                if (GetChainTipHeight() - invalidBlock >= VALID_GRACEPERIOD_BLOCKS)
                    return false; // if grace period has expired
            } else if (!success)
                return false; // not valid if no transaction found or utxo is already spent

            if (tx->vout.size() <= op.n)
                return false; // not valid if bad vout index

            const auto & out = tx->vout[op.n];
            total += out.nValue;

            if (processed.count(CScriptID(out.scriptPubKey)))
                continue;

            CTxDestination address;
            if (!ExtractDestination(out.scriptPubKey, address))
                return false; // not valid if bad address

            CKeyID *keyid = boost::get<CKeyID>(&address);
            if (pubkey.GetID() != *keyid)
                return false; // fail if pubkeys don't match

            processed.insert(CScriptID(out.scriptPubKey));
        }

        if (tier == Tier::SPV && total >= COLLATERAL_SPV) // check SPV collateral amount
            return true;

        // Other Tiers here

        return false;
    }

    /**
     * Marks or unmarks this snode as invalid.
     * @param flag Invalid state
     * @param blockNumber when this snode was marked invalid
     */
    void markInvalid(const bool flag = true, const int blockNumber = 0) {
        invalid = flag;
        if (!flag)
            invalidBlock = 0; // reset state
        else
            invalidBlock = blockNumber;
    }

    /**
     * Returns the invalid state.
     * @return
     */
    bool getInvalid() const {
        return invalid;
    }

    /**
     * Returns the block number of invalid state.
     * @return
     */
    int getInvalidBlockNumber() const {
        return invalidBlock;
    }

    /**
     * Set the snode's last known current block number.
     * @param blockNumber
     */
    void setCurrentBlock(const int blockNumber) {
        currentBlock = blockNumber;
    }

    /**
     * Returns the snode's last known current block number.
     * @return
     */
    int getCurrentBlock() const {
        return currentBlock;
    }

protected:
    /**
     * Return true if the config was successfully parsed.
     * @return
     */
    bool parseConfig(const std::string & conf, const CChainParams & chainparams) {
        try {
            UniValue uv;
            if (!uv.read(conf))
                return false; // do not continue processing if config is bad json

            // Get the config version
            const auto uxbver = find_value(uv, "xbridgeversion");
            if (uxbver.isNull() || !uxbver.isNum())
                return false; // do not continue processing the config on bad protocol version
            xbridgeversion = uxbver.get_int();

            // Get the config version
            const auto uxrver = find_value(uv, "xrouterversion");
            if (uxrver.isNull() || !uxrver.isNum())
                return false; // do not continue processing the config on bad protocol version
            xrouterversion = uxrver.get_int();

            // Parse xbridge config if it's specified
            const auto uxb = find_value(uv, "xbridge");
            if (!uxb.isNull() && uxb.isArray()) {
                auto us = uxb.getValues();
                for (const auto & s : us) {
                    if (tier == SPV) // xbridge only supports SPV nodes
                        services.push_back(s.get_str());
                }
            }

            // Parse xrouter config if it's specified
            const auto uxr = find_value(uv, "xrouter");
            if (!uxr.isNull() && uxr.isObject()) {
                const auto uxrconf = find_value(uxr, "config");
                if (uxrconf.isNull() || !uxrconf.isStr())
                    return false; // do not continue processing if bad config format

                // Parse config
                xrouter::XRouterSettings settings(snodePubKey, false); // not our config
                if (!settings.init(uxrconf.get_str()))
                    return false;

                // Enterprise XRouter compatibility check
                if (settings.getAddr().GetPort() != 0) {
                    const auto snodeXRPort = static_cast<int>(settings.getAddr().GetPort());
                    const auto defaultPort = chainparams.GetDefaultPort();
                    exrCompatible = snodeXRPort != defaultPort;
                }

                // Parse plugins
                const auto uxrplugins = find_value(uxr, "plugins");
                std::map<std::string, UniValue> kv;
                uxrplugins.getObjMap(kv);
                for (const auto & item : kv) {
                    const auto plugin = item.first;
                    const auto pluginconf = item.second.get_str();
                    try {
                        auto psettings = std::make_shared<xrouter::XRouterPluginSettings>(false); // not our config
                        psettings->read(pluginconf);
                        // Only add plugins on OPEN tier if they are free
                        if (!(tier == Tier::OPEN && psettings->fee() > std::numeric_limits<double>::epsilon()))
                            settings.addPlugin(plugin, psettings);
                    } catch (...) { }
                }

                addr = settings.getAddr();
                host = settings.host(xrouter::xrDefault);
                services.push_back(xrouter::xr); // add the general xrouter service

                for (const auto & s : settings.getWallets()) {
                    if (tier == Tier::SPV) // Wallets only supported on SPV snodes
                        services.push_back(xrouter::walletCommandKey(s));
                }

                for (const auto & p : settings.getPlugins()) {
                    if (!settings.isAvailableCommand(xrouter::xrService, p)) // exclude any disabled plugins
                        continue;
                    services.push_back(xrouter::pluginCommandKey(p));
                }
            }
        } catch (...) {
            return false;
        }
        return true;
    }

protected: // included in network serialization
    CPubKey snodePubKey;
    uint8_t tier;
    CKeyID paymentAddress;
    std::vector<COutPoint> collateral;
    uint32_t bestBlock;
    uint256 bestBlockHash;
    std::vector<unsigned char> signature;

protected: // in-memory only
    int64_t pingtime;
    uint32_t pingBestBlock;
    uint256 pingBestBlockHash;
    std::string config;
    uint32_t xbridgeversion{0};
    uint32_t xrouterversion{0};
    CService addr;
    std::string host;
    std::vector<std::string> services;
    bool invalid{false};
    int invalidBlock{0};
    int currentBlock{0};
    bool exrCompatible{false};
};

typedef std::shared_ptr<ServiceNode> ServiceNodePtr;

/**
 * The Servicenode ping is responsible for notifying peers of the latest servicenode details. The ping
 * indicates whether a snode is still online and valid, and also includes the snode config, which
 * details the snode's associated services.
 */
class ServiceNodePing {
public:
    /**
     * Constructor
     */
    explicit ServiceNodePing() : snodePubKey(CPubKey()), bestBlock(0), bestBlockHash(uint256()), pingTime(GetTime()),
                        config(std::string()), snode(ServiceNode()), signature(std::vector<unsigned char>()) {}

    /**
     * Constructor
     * @param snodePubKey
     * @param bestBlock
     * @param bestBlockHash
     * @param pingTime
     * @param config
     * @param snode
     */
    explicit ServiceNodePing(CPubKey snodePubKey, uint32_t bestBlock, uint256 bestBlockHash, uint32_t pingTime,
                             std::string config, ServiceNode snode) :
                                 snodePubKey(snodePubKey), bestBlock(bestBlock), bestBlockHash(bestBlockHash),
                                 pingTime(pingTime), config(std::move(config)), snode(std::move(snode)),
                                 signature(std::vector<unsigned char>()) {
        this->snode.setBestBlock(this->bestBlock, this->bestBlockHash);
        this->snode.setConfig(this->config, Params());
        this->snode.updatePing(pingTime);
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(snodePubKey);
        READWRITE(bestBlock);
        READWRITE(bestBlockHash);
        READWRITE(pingTime);
        READWRITE(config);
        READWRITE(snode);
        READWRITE(signature);
        if (ser_action.ForRead()) { // on read stream, set the snode best block and ping
            snode.setBestBlock(bestBlock, bestBlockHash);
            snode.setConfig(config, Params());
            snode.updatePing(pingTime);
        }
    }

    /**
     * Returns true if the ping is null.
     * @return
     */
    bool isNull() const {
        return snode.isNull();
    }

    /**
     * Public key associated with the ping.
     * @return
     */
    CPubKey getSnodePubKey() const {
        return snodePubKey;
    }

    /**
     * Underlying servicenode associated with the ping.
     */
    const ServiceNode& getSnode() const {
        return snode;
    }

    /**
     * Signature of the ping.
     * @return
     */
    const std::vector<unsigned char>& getSignature() const {
        return signature;
    }

    /**
     * Configuration associated with the servicenode.
     * @return
     */
    const std::string& getConfig() const {
        return config;
    }

    /**
     * Time of ping as reported by the service node.
     * @return
     */
    uint32_t getPingTime() const {
        return pingTime;
    }

    /**
     * Hash used in signing.
     * @return
     */
    uint256 sigHash() const {
        CHashWriter ss(SER_GETHASH, 0);
        ss << snodePubKey << bestBlock << bestBlockHash << pingTime << config << snode;
        return ss.GetHash();
    }

    /**
     * Hash of the ping including the signature.
     * @return
     */
    uint256 getHash() const {
        CHashWriter ss(SER_GETHASH, 0);
        ss << snodePubKey << bestBlock << bestBlockHash << pingTime << config << snode << signature;
        return ss.GetHash();
    }

    /**
     * Sign's the servicenode ping with the specified key.
     * @param key
     * @return
     */
    bool sign(const CKey & key) {
        return key.SignCompact(sigHash(), signature);
    }

    /**
     * Returns true if this servicenode ping is valid. Service node pubkey and associated signatures are checked for
     * validity. The ping is signed by the snode privkey while the registration is signed by the snode collateral
     * privkey. The exception is OPEN (free) tier nodes always sign with their snode privkeys since they are not
     * allowed to accept payments.
     * @param getTxFunc
     * @param isBlockValid
     * @param skipBlockchainValidation If true the blockchain validation is skipped. Other validation is performed.
     */
    bool isValid(const TxFunc & getTxFunc, const BlockValidFunc & isBlockValid, const bool skipBlockchainValidation = false) const {
        if (!skipBlockchainValidation && !isBlockValid(bestBlock, bestBlockHash, true))
            return false; // fail if ping is stale

        // TODO Blocknet OPEN tier snodes, support non-SPV snode tiers (enable unit tests)
        if (snode.getTier() != ServiceNode::Tier::SPV)
            return false;

        // Ensure ping key matches snode key
        if (!snodePubKey.IsFullyValid() || snodePubKey != snode.getSnodePubKey())
            return false; // not valid if bad snode pubkey

        if (snode.serviceList().empty()) // check for valid services
            return false;

        // OPEN tier can only advertise on xrs:: namespace
        if (snode.getTier() == ServiceNode::Tier::OPEN) {
            const auto & slist = snode.serviceList();
            if (slist.size() == 1)
                return false; // expecting [xr,xrs::SomePlugin]
            for (const auto & s : slist) {
                if (s == xrouter::xr) // skip the default xrouter service flag
                    continue;
                if (!boost::algorithm::istarts_with(s, xrouter::xrs + xrouter::xrdelimiter))
                    return false;
            }
        }

        CPubKey pubkey;
        if (!pubkey.RecoverCompact(sigHash(), signature))
            return false; // not valid if bad sig

        if (pubkey.GetID() != snodePubKey.GetID())
            return false; // fail if pubkeys don't match

        if (!skipBlockchainValidation)
            return snode.isValid(getTxFunc, isBlockValid, false); // stale check not required here, it happens above on isBlockValid

        return true;
    }

protected:
    CPubKey snodePubKey;
    uint32_t bestBlock{0};
    uint256 bestBlockHash;
    uint32_t pingTime{0};
    std::string config;
    ServiceNode snode;
    std::vector<unsigned char> signature;
};

}

#endif //BLOCKNET_SERVICENODE_SERVICENODE_H
