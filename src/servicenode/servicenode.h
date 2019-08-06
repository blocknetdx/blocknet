// Copyright (c) 2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_SERVICENODE_H
#define BLOCKNET_SERVICENODE_H

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

#include <set>
#include <utility>
#include <vector>

/**
 * Servicenode namepsace
 */
namespace sn {

typedef std::function<CTransactionRef(const COutPoint & out)> TxFunc;
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
     * @param collateral
     * @param bestBlock
     * @param bestBlockHash
     */
    static uint256 CreateSigHash(const CPubKey & snodePubKey, const Tier & tier,
                                 const std::vector<COutPoint> & collateral,
                                 const uint32_t & bestBlock=0, const uint256 & bestBlockHash=uint256())
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << snodePubKey << static_cast<uint8_t>(tier) << collateral << bestBlock << bestBlockHash;
        return ss.GetHash();
    }

public:
    /**
     * Constructor
     */
    explicit ServiceNode() : snodePubKey(CPubKey()), tier(Tier::OPEN), collateral(std::vector<COutPoint>()),
                             bestBlock(0), bestBlockHash(uint256()), signature(std::vector<unsigned char>()),
                             regtime(GetAdjustedTime()), pingtime(0), config(std::string()), pingBestBlock(0),
                             pingBestBlockHash(uint256()) {}

    /**
     * Constructor
     * @param snodePubKey
     * @param tier
     * @param collateral
     * @param bestBlock
     * @param bestBlockHash
     * @param signature
     */
    explicit ServiceNode(CPubKey snodePubKey,
                         Tier tier,
                         std::vector<COutPoint> collateral,
                         uint32_t bestBlock,
                         uint256 bestBlockHash,
                         std::vector<unsigned char> signature) : snodePubKey(snodePubKey), tier(tier),
                                           collateral(std::move(collateral)), bestBlock(bestBlock),
                                           bestBlockHash(bestBlockHash), signature(std::move(signature)),
                                           regtime(GetAdjustedTime()), pingBestBlock(bestBlock),
                                           pingBestBlockHash(bestBlockHash), pingtime(0), config(std::string()) {}

    friend inline bool operator==(const ServiceNode & a, const ServiceNode & b) { return a.snodePubKey == b.snodePubKey; }
    friend inline bool operator!=(const ServiceNode & a, const ServiceNode & b) { return a.snodePubKey != b.snodePubKey; }
    friend inline bool operator<(const ServiceNode & a, const ServiceNode & b) { return a.regtime < b.regtime; }

    /**
     * Returns true if the servicenode is uninitialized (e.g. via empty constructor).
     */
    bool isNull() const {
        return !snodePubKey.IsValid();
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
     * Returns the servicenode registration time in unix time.
     * @return
     */
    const int64_t& getRegTime() const {
        return regtime;
    }

    /**
     * Returns the servicenode last ping time in unix time.
     * @return
     */
    void updatePing() {
        pingtime = GetAdjustedTime();
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
     * Assigsn the specified config to the servicenode.
     * @param c
     */
    void setConfig(const std::string & c) {
        config = c;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(snodePubKey);
        READWRITE(tier);
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
        return CreateSigHash(snodePubKey, static_cast<Tier>(tier), collateral, bestBlock, bestBlockHash);
    }

    /**
     * Returns the servicenode's hash (includes the signature).
     * @return
     */
    uint256 getHash() const {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << snodePubKey << static_cast<uint8_t>(tier) << collateral << bestBlock << bestBlockHash
           << config << signature << regtime;
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

        // If open tier, the signature should be generated from the snode pubkey
        if (tier == Tier::OPEN) {
            const auto & sighash = sigHash();
            CPubKey pubkey2;
            if (!pubkey2.RecoverCompact(sighash, signature))
                return false; // not valid if bad sig
            return snodePubKey.GetID() == pubkey2.GetID();
        }

        // If not on the open tier, check collateral
        if (collateral.empty())
            return false; // not valid if no collateral

        const auto & sighash = sigHash();
        CAmount total{0}; // Track the total collateral amount
        std::set<COutPoint> unique_collateral(collateral.begin(), collateral.end()); // prevent duplicates

        // Determine if all collateral utxos validate the sig
        for (const auto & op : unique_collateral) {
            CTransactionRef tx = getTxFunc(op);
            if (!tx)
                return false; // not valid if no transaction found or utxo is already spent

            if (tx->vout.size() <= op.n)
                return false; // not valid if bad vout index

            const auto & out = tx->vout[op.n];
            total += out.nValue;

            CTxDestination address;
            if (!ExtractDestination(out.scriptPubKey, address))
                return false; // not valid if bad address

            CKeyID *keyid = boost::get<CKeyID>(&address);
            CPubKey pubkey;
            if (!pubkey.RecoverCompact(sighash, signature))
                return false; // not valid if bad sig
            if (pubkey.GetID() != *keyid)
                return false; // fail if pubkeys don't match
        }

        if (tier == Tier::SPV && total >= COLLATERAL_SPV)
            return true;

        // Other Tiers here

        return false;
    }

protected: // included in network serialization
    CPubKey snodePubKey;
    uint8_t tier;
    std::vector<COutPoint> collateral;
    uint32_t bestBlock;
    uint256 bestBlockHash;
    std::vector<unsigned char> signature;

protected: // in-memory only
    int64_t regtime;
    int64_t pingtime;
    uint32_t pingBestBlock;
    uint256 pingBestBlockHash;
    std::string config;
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
    explicit ServiceNodePing() : snodePubKey(CPubKey()), bestBlock(0), bestBlockHash(uint256()),
                        config(std::string()), snode(ServiceNode()), signature(std::vector<unsigned char>()) {}

    /**
     * Constructor
     * @param snodePubKey
     * @param bestBlock
     * @param bestBlockHash
     * @param config
     * @param snode
     */
    explicit ServiceNodePing(CPubKey snodePubKey, uint32_t bestBlock, uint256 bestBlockHash,
                             std::string config, ServiceNode snode) :
                                 snodePubKey(snodePubKey), bestBlock(bestBlock), bestBlockHash(bestBlockHash),
                                 config(std::move(config)), snode(std::move(snode)),
                                 signature(std::vector<unsigned char>()) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(snodePubKey);
        READWRITE(bestBlock);
        READWRITE(bestBlockHash);
        READWRITE(config);
        READWRITE(snode);
        READWRITE(signature);
        if (!ser_action.ForRead()) { // on write, set the snode best block and ping
            snode.setBestBlock(bestBlock, bestBlockHash);
            snode.setConfig(config);
            snode.updatePing();
        }
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
     * Hash used in signing.
     * @return
     */
    uint256 sigHash() const {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << snodePubKey << bestBlock << bestBlockHash << config << snode;
        return ss.GetHash();
    }

    /**
     * Hash of the ping including the signature.
     * @return
     */
    uint256 getHash() const {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << snodePubKey << bestBlock << bestBlockHash << config << snode << signature;
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
     * Returns true if this servicenode ping is valid. Public keys and associated signatures are checked for
     * validity.
     * @param getTxFunc
     * @param isBlockValid
     */
    bool isValid(const TxFunc & getTxFunc, const BlockValidFunc & isBlockValid) const {
        if (!isBlockValid(bestBlock, bestBlockHash, true))
            return false; // fail if ping is stale

        // Ensure ping key matches snode key
        if (!snodePubKey.IsFullyValid() || snodePubKey != snode.getSnodePubKey())
            return false; // not valid if bad snode pubkey

        CPubKey pubkey;
        if (!pubkey.RecoverCompact(sigHash(), signature))
            return false; // not valid if bad sig

        if (pubkey.GetID() != snodePubKey.GetID())
            return false; // fail if pubkeys don't match

        return snode.isValid(getTxFunc, isBlockValid, false); // stale check not required here, it happens above on isBlockValid
    }

protected:
    CPubKey snodePubKey;
    uint32_t bestBlock;
    uint256 bestBlockHash;
    std::string config;
    ServiceNode snode;
    std::vector<unsigned char> signature;
};

}

#endif //BLOCKNET_SERVICENODE_H
