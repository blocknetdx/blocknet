// Copyright (c) 2019-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_GOVERNANCE_GOVERNANCE_H
#define BLOCKNET_GOVERNANCE_GOVERNANCE_H

#include <amount.h>
#include <dbwrapper.h>
#include <chain.h>
#include <consensus/params.h>
#include <consensus/validation.h>
#include <hash.h>
#include <index/base.h>
#include <key_io.h>
#include <policy/policy.h>
#include <script/standard.h>
#include <shutdown.h>
#include <streams.h>
#include <txdb.h>
#include <txmempool.h>
#include <uint256.h>
#include <util/moneystr.h>
#include <util/system.h>
#include <validation.h>
#include <validationinterface.h>

#include <regex>
#include <string>
#include <utility>

#include <boost/algorithm/string.hpp>
#include <boost/thread.hpp>

/**
 * Governance namespace.
 */
namespace gov {

class CDiskProposal;
class CDiskVote;
class CDiskSpentUtxo;

constexpr char DB_BEST_BLOCK = 'B';
constexpr char DB_PROPOSAL = 'p';
constexpr char DB_VOTE = 'v';
constexpr char DB_SPENT_UTXO = 's';

class GovernanceDB : public CValidationInterface {
public:
    explicit GovernanceDB(size_t n_cache_size, bool f_memory, bool f_wipe);
    void Reset(bool wipe);
    void Start();
    void Stop();
    ~GovernanceDB();
    const CBlockIndex* BestBlockIndex() const;

    /// Write the current chain block locator to the DB.
    bool WriteBestBlock(const CBlockIndex *pindex, const CChain & chain, CCriticalSection & chainMutex);

    void AddVote(const CDiskVote & vote);
    bool AddVotes(const std::vector<std::pair<uint256, CDiskVote>> & votes);
    void RemoveVote(const uint256 & vote);
    void AddProposal(const CDiskProposal & proposal);
    bool AddProposals(const std::vector<std::pair<uint256, CDiskProposal>> & proposals);
    void RemoveProposal(const uint256 & proposal);
    bool ReadSpentUtxo(const std::string & key, CDiskSpentUtxo & utxo);
    bool AddSpentUtxos(const std::vector<std::pair<std::string, CDiskSpentUtxo>> & utxos, bool sync=false);
    bool RemoveSpentUtxo(const CDiskSpentUtxo & utxo, bool sync=false);

    class DB : public CDBWrapper {
    public:
        explicit DB(size_t n_cache_size, bool f_memory = false, bool f_wipe = false);

        /// Read block locator of the chain that the govindex is in sync with.
        bool ReadBestBlock(CBlockLocator & locator) const;

        /// Write block locator of the chain that the govindex is in sync with.
        bool WriteBestBlock(const CBlockLocator & locator);

        /// Write proposals
        bool WriteProposal(const uint256 & hash, const CDiskProposal & proposal);
        bool WriteProposals(const std::vector<std::pair<uint256, CDiskProposal>> & proposals);

        /// Write votes
        bool WriteVote(const uint256 & hash, const CDiskVote & vote);
        bool WriteVotes(const std::vector<std::pair<uint256, CDiskVote>> & votes);

        /// Spent utxos
        bool ReadSpentUtxo(const std::string & key, CDiskSpentUtxo & utxo);
        bool WriteSpentUtxos(const std::vector<std::pair<std::string, CDiskSpentUtxo>> & utxos, bool sync=false);
    };

    DB & GetDB() {
        return *db;
    }

public:
    void BlockConnected(const std::shared_ptr<const CBlock> & block, const CBlockIndex *pindex,
                        const std::vector<CTransactionRef> & txn_conflicted) override;
    void BlockDisconnected(const std::shared_ptr<const CBlock> & block) override;
    void ChainStateFlushed(const CBlockLocator& locator) override;

protected:
    /// Get the name of the index for display in logs.
    const char* GetName() const { return "govindex"; }

protected:
    size_t cache{16*1024*1024};
    bool memory{false};
    /// The last block in the chain that the index is in sync with.
    std::atomic<const CBlockIndex*> bestBlockIndex{nullptr};

private:
    std::unique_ptr<DB> db;
};

/**
 * Governance types are used with OP_RETURN to indicate how the messages should be processed.
 */
enum Type : uint8_t {
    NONE         = 0,
    PROPOSAL     = 1,
    VOTE         = 2,
};

static const uint8_t NETWORK_VERSION = 0x01;
static const CAmount VOTING_UTXO_INPUT_AMOUNT = 1 * COIN;
static const int VINHASH_SIZE = 12;
static const int PROPOSAL_USERDEFINED_LIMIT = 139;
typedef std::array<unsigned char, VINHASH_SIZE> VinHash;

/**
 * Create VinHash from vin prevout.
 * @param COutPoint
 * @return
 */
static VinHash makeVinHash(const COutPoint & prevout) {
    CHashWriter hw(SER_GETHASH, 0);
    hw << prevout;
    const auto & hwhash = hw.GetHash();
    const auto & v = ToByteVector(hwhash);
    VinHash r;
    for (int i = 0; i < VINHASH_SIZE; ++i)
        r[i] = v[i];
    return r;
}

/**
 * Performs a lookup in the transaction index for the specified vote utxo. It must also
 * exist in a block that is on or after the governance start block and be in the main
 * chain. Transactions in blocks that are not in the main chain are not acceptable for
 * use in voting.
 * @param utxo
 * @param tx
 * @param keyid Voting utxo public key id
 * @param blockNumber Voting utxo must be in a block prior to the specified block height
 * @return
 */
static bool ValidateVoteUTXO(const COutPoint & utxo, CTransactionRef & tx, CKeyID & keyid, const int blockNumber) {
    uint256 hashBlock;
    if (!GetTransaction(utxo.hash, tx, Params().GetConsensus(), hashBlock))
        return false;
    if (utxo.n >= tx->vout.size())
        return false;
    CTxDestination dest;
    if (!ExtractDestination(tx->vout[utxo.n].scriptPubKey, dest))
        return false;
    {
        LOCK(cs_main);
        const auto pindex = LookupBlockIndex(hashBlock);
        if (!pindex                                                      // fail on bad index
            || pindex->nHeight < Params().GetConsensus().governanceBlock // fail on utxo prior to governance start
            || (blockNumber > 0 && pindex->nHeight > blockNumber)        // fail on utxo in the future
            || !chainActive.Contains(pindex))                            // fail on utxo not in main chain
                return false;
    }
    keyid = *boost::get<CKeyID>(&dest);
    return true;
}

/**
 * Returns the next superblock from the most recent chain tip by default.
 * If fromBlock is specified the superblock immediately after fromBlock
 * is returned.
 * @param params
 * @param fromBlock
 * @return
 */
static int NextSuperblock(const Consensus::Params & params, const int fromBlock = 0) {
    if (fromBlock == 0) {
        LOCK(cs_main);
        return chainActive.Height() - chainActive.Height() % params.superblock + params.superblock;
    }
    return fromBlock - fromBlock % params.superblock + params.superblock;
}

/**
 * Returns the previous superblock from the most recent chain tip by default.
 * If fromBlock is specified the superblock immediately preceeding fromBlock
 * is returned.
 * @param params
 * @param fromBlock
 * @return
 */
static int PreviousSuperblock(const Consensus::Params & params, const int fromBlock = 0) {
    const int nextSuperblock = NextSuperblock(params, fromBlock);
    return nextSuperblock - params.superblock;
}

/**
 * Encapsulates serialized OP_RETURN governance data.
 */
class NetworkObject {
public:
    explicit NetworkObject() = default;

    /**
     * Returns true if this network data contains the proper version.
     * @return
     */
    bool isValid() const {
        return version == NETWORK_VERSION;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(version);
        READWRITE(type);
    }

    const uint8_t & getType() const {
        return type;
    }

protected:
    uint8_t version{NETWORK_VERSION};
    uint8_t type{NONE};
};

/**
 * Proposals encapsulate the data required by the network to support voting and payments.
 * They can be created by anyone willing to pay the submission fee. The proposal hash is
 * cached. It is important to rehash the cached hash when any hash fields are mutated.
 */
class Proposal {
    friend class CDiskProposal;
public:
    explicit Proposal(std::string name, int superblock, CAmount amount, std::string address,
                      std::string url, std::string description) : name(std::move(name)), superblock(superblock),
                                              amount(amount), address(std::move(address)), url(std::move(url)),
                                              description(std::move(description)) {
        chash = getHash(false);
    }
    explicit Proposal(int blockNumber) : blockNumber(blockNumber) {
        chash = getHash(false);
    }
    Proposal() = default;
    Proposal(const Proposal &) = default;
    Proposal& operator=(const Proposal &) = default;
    friend inline bool operator==(const Proposal & a, const Proposal & b) { return a.getHash() == b.getHash(); }
    friend inline bool operator!=(const Proposal & a, const Proposal & b) { return !(a.getHash() == b.getHash()); }
    friend inline bool operator<(const Proposal & a, const Proposal & b) { return a.getHash() < b.getHash(); }

    /**
     * Null check
     * @return
     */
    bool isNull() const {
        return superblock == 0;
    }

    /**
     * Valid if the proposal properties are correct.
     * @param params
     * @param failureReasonRet
     * @return
     */
    bool isValid(const Consensus::Params & params, std::string *failureReasonRet=nullptr) const {
        static std::regex rrname("^\\w+[\\w\\-_ ]*\\w+$");
        if (!std::regex_match(name, rrname)) {
            if (failureReasonRet) *failureReasonRet = strprintf("Proposal name %s is invalid, only alpha-numeric characters are accepted", name);
            return false;
        }
        if (superblock % params.superblock != 0) {
            if (failureReasonRet) *failureReasonRet = strprintf("Bad superblock number, did you mean %d", gov::NextSuperblock(params));
            return false;
        }
        if (!(amount >= params.proposalMinAmount && amount <= std::min(params.proposalMaxAmount, params.GetBlockSubsidy(superblock, params)))) {
            if (failureReasonRet) *failureReasonRet = strprintf("Bad proposal amount, specify amount between %s - %s",
                    FormatMoney(params.proposalMinAmount), FormatMoney(std::min(params.proposalMaxAmount, params.GetBlockSubsidy(superblock, params))));
            return false;
        }
        if (!IsValidDestination(DecodeDestination(address))) {
            if (failureReasonRet) *failureReasonRet = strprintf("Bad payment address %s", address);
            return false;
        }
        if (type != PROPOSAL) {
            if (failureReasonRet) *failureReasonRet = strprintf("Bad proposal type, expected %d", PROPOSAL);
            return false;
        }
        if (version != NETWORK_VERSION) {
            if (failureReasonRet) *failureReasonRet = strprintf("Bad proposal network version, expected %d", NETWORK_VERSION);
            return false;
        }
        CDataStream ss(SER_NETWORK, GOV_PROTOCOL_VERSION);
        ss << version << type << name << superblock << amount << address << url << description;
        const int maxBytes = MAX_OP_RETURN_RELAY-3; // -1 for OP_RETURN -2 for pushdata opcodes
        // If this protocol changes update PROPOSAL_USERDEFINED_LIMIT so that gui can understand that limit
        const int nonUserBytes = 14;
        const int packetBytes = 4;
        if (ss.size() > maxBytes) {
            if (failureReasonRet) *failureReasonRet = strprintf("Proposal input is too long, try reducing the description by %u characters. You can use a combined total of %u characters across proposal name, url, description, and payment address fields.", ss.size()-maxBytes, maxBytes-nonUserBytes-packetBytes);
            return false;
        }
        return true;
    }

    /**
     * Proposal name
     * @return
     */
    const std::string & getName() const {
        return name;
    }

    /**
     * Proposal superblock
     * @return
     */
    const int & getSuperblock() const {
        return superblock;
    }

    /**
     * Proposal amount
     * @return
     */
    const CAmount & getAmount() const {
        return amount;
    }

    /**
     * Proposal address
     * @return
     */
    const std::string & getAddress() const {
        return address;
    }

    /**
     * Proposal url (for more information)
     * @return
     */
    const std::string & getUrl() const {
        return url;
    }

    /**
     * Proposal description
     * @return
     */
    const std::string & getDescription() const {
        return description;
    }

    /**
     * Proposal block number
     * @return
     */
    const int & getBlockNumber() const {
        return blockNumber;
    }

    /**
     * Proposal hash
     * @param cache Default true, return the cached hash.
     * @return
     */
    uint256 getHash(bool cache=true) const {
        if (cache)
            return chash;
        CHashWriter ss(SER_GETHASH, 0);
        ss << version << type << name << superblock << amount << address << url << description;
        return ss.GetHash();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(version);
        READWRITE(type);
        READWRITE(superblock);
        READWRITE(amount);
        READWRITE(address);
        READWRITE(name);
        READWRITE(url);
        READWRITE(description);
        if (ser_action.ForRead())
            chash = getHash(false);
    }

protected:
    uint8_t version{NETWORK_VERSION};
    uint8_t type{PROPOSAL};
    std::string name;
    int superblock{0};
    CAmount amount{0};
    std::string address;
    std::string url;
    std::string description;

protected: // memory only
    int blockNumber{0}; // block containing this proposal
    uint256 chash; // cached hash
};

enum VoteType : uint8_t {
    NO      = 0,
    YES     = 1,
    ABSTAIN = 2,
};

/**
 * Votes can be cast on proposals and ultimately lead to unlocking funds for proposals that meet
 * the minimum requirements and minimum required votes.
 */
class Vote {
    friend class CDiskVote;
public:
    explicit Vote(const uint256 & proposal, const VoteType & vote,
                  const COutPoint & utxo, const VinHash & vinhash) : proposal(proposal),
                                                                     vote(vote),
                                                                     utxo(utxo),
                                                                     vinhash(vinhash) {
        loadVoteUTXO();
        chash = getHash(false);
        csighash = sigHash(false);
    }
    explicit Vote(const uint256 & proposal, const VoteType & vote,
                  const COutPoint & utxo, const VinHash & vinhash,
                  const CKeyID & keyid, const CAmount & amount) : proposal(proposal),
                                                                  vote(vote),
                                                                  utxo(utxo),
                                                                  vinhash(vinhash),
                                                                  keyid(keyid),
                                                                  amount(amount) {
        chash = getHash(false);
        csighash = sigHash(false);
    }
    explicit Vote(const COutPoint & outpoint, const int64_t & time = 0, const int & blockNumber = 0) : outpoint(outpoint),
                                                                                                       time(time),
                                                                                                       blockNumber(blockNumber) { }
    Vote() = default;
    Vote(const Vote &) = default;
    Vote& operator=(const Vote &) = default;
    friend inline bool operator==(const Vote & a, const Vote & b) { return a.getHash() == b.getHash(); }
    friend inline bool operator!=(const Vote & a, const Vote & b) { return !(a.getHash() == b.getHash()); }
    friend inline bool operator<(const Vote & a, const Vote & b) { return a.getHash() < b.getHash(); }

    /**
     * Returns true if a valid vote string type was converted.
     * @param strVote
     * @param voteType Mutated with the converted vote type.
     * @return
     */
    static bool voteTypeForString(std::string strVote, VoteType & voteType) {
        boost::to_lower(strVote, std::locale::classic());
        if (strVote == "yes") {
            voteType = YES;
        } else if (strVote == "no") {
            voteType = NO;
        } else if (strVote == "abstain") {
            voteType = ABSTAIN;
        } else {
            return false;
        }
        return true;
    }

    /**
     * Returns the string representation of the vote type.
     * @param voteType
     * @param valid true if conversion was successful, otherwise false.
     * @return
     */
    static std::string voteTypeToString(const VoteType & voteType, bool *valid = nullptr) {
        std::string strVote;
        if (voteType == YES) {
            strVote = "yes";
        } else if (voteType == NO) {
            strVote = "no";
        } else if (voteType == ABSTAIN) {
            strVote = "abstain";
        } else {
            if (valid) *valid = false;
        }
        if (valid) *valid = true;
        return strVote;
    }

    /**
     * Null check
     * @return
     */
    bool isNull() {
        return utxo.IsNull();
    }

    /**
     * Returns true if the vote properties are valid and the utxo pubkey
     * matches the pubkey of the signature.
     * @return
     */
    bool isValid(const Consensus::Params & params) const {
        if (!(version == NETWORK_VERSION && type == VOTE && isValidVoteType(vote)))
            return false;
        if (amount < params.voteMinUtxoAmount) // n bounds checked in ValidateVoteUTXO
            return false;
        // Ensure the pubkey of the utxo matches the pubkey of the vote signature
        if (keyid.IsNull())
            return false;
        if (pubkey.GetID() != keyid)
            return false;
        return true;
    }

    /**
     * Returns true if the vote properties are valid and the utxo pubkey
     * matches the pubkey of the signature as well as the added check
     * that the hash of the prevout matches the expected vin hash. This
     * check will prevent vote replay attacks by ensuring that the vin
     * associated with the vote matches the expected vin hash sent
     * with the vote's OP_RETURN data.
     * @param vinHashes Set of truncated vin prevout hashes.
     * @paramk params
     * @return
     */
    bool isValid(const std::set<VinHash> & vinHashes, const Consensus::Params & params) const {
        if (!isValid(params))
            return false;
        // Check that the expected vin hash matches an expected vin prevout
        return isValidVinHash(vinHashes);
    }

    /**
     * Returns true if the vote's vinhash is found in the set of specified
     * vin hashes.
     * @param vinHashes
     * @return
     */
    bool isValidVinHash(const std::set<VinHash> & vinHashes) const {
        return vinHashes.count(vinhash) > 0;
    }

    /**
     * Initialize the keyid and amount from the vote's utxo.
     */
    bool loadVoteUTXO() {
        CTransactionRef tx;
        if (ValidateVoteUTXO(utxo, tx, keyid, blockNumber)) {
            amount = tx->vout[utxo.n].nValue;
            return true;
        }
        return false;
    }

    /**
     * Sign the vote with the specified private key.
     * @param key
     * @return
     */
    bool sign(const CKey & key) {
        signature.clear();
        if (!key.SignCompact(sigHash(), signature))
            return false;
        return pubkey.RecoverCompact(sigHash(), signature);
    }

    /**
     * Marks the vote utxo as being spent.
     * @params block Height of the block spending the vote utxo.
     * @params txhash Hash of transaction spending the vote utxo.
     * @return
     */
    void spend(const int & block, const uint256 & txhash) {
        spentBlock = block;
        spentHash = txhash;
    }

    /**
     * Unspends the vote. Returns true if the vote was successfully
     * unspent, otherwise returns false.
     * @params block Height of the block that spent the vote utxo.
     * @params txhash Hash of transaction that spent the vote utxo.
     * @return
     */
    bool unspend(const int & block, const uint256 & txhash) {
        if (spentBlock == block && spentHash == txhash) {
            spentBlock = 0;
            spentHash.SetNull();
            return true;
        }
        return false;
    }

    /**
     * Marks the vote utxo as being spent.
     * @param block
     * @return
     */
    bool spent() const {
        return spentBlock > 0;
    }

    /**
     * Proposal hash
     * @return
     */
    const uint256 & getProposal() const {
        return proposal;
    }

    /**
     * Proposal vote
     * @return
     */
    VoteType getVote() const {
        return static_cast<VoteType>(vote);
    }

    /**
     * Proposal vote signature
     * @return
     */
    const std::vector<unsigned char> & getSignature() const {
        return signature;
    }

    /**
     * Proposal utxo containing the vote
     * @return
     */
    const COutPoint & getUtxo() const {
        return utxo;
    }

    /**
     * Vote's vin hash (truncated vin prevout spending this vote).
     * @return
     */
    const VinHash & getVinHash() const {
        return vinhash;
    }

    /**
     * Vote hash
     * @param cache Default true, returns the hash from cache.
     * @return
     */
    uint256 getHash(bool cache=true) const {
        if (cache)
            return chash;
        CHashWriter ss(SER_GETHASH, 0);
        ss << version << type << proposal << utxo; // exclude vote from hash to properly handle changing votes
        return ss.GetHash();
    }

    /**
     * Vote signature hash
     * @param cache Default true, returns the hash from cache.
     * @return
     */
    uint256 sigHash(bool cache=true) const {
        if (cache)
            return csighash;
        CHashWriter ss(SER_GETHASH, 0);
        ss << version << type << proposal << vote << utxo << vinhash;
        return ss.GetHash();
    }

    /**
     * Get the pubkey associated with the vote's signature.
     * @return
     */
    const CPubKey & getPubKey() const {
        return pubkey;
    }

    /**
     * Get the COutPoint of the vote. This is the outpoint of the OP_RETURN data
     * in the "voting" transaction. This shouldn't be confused with the vote's
     * utxo (the unspent transaction output representing the vote).
     * @return
     */
    const COutPoint & getOutpoint() const {
        return outpoint;
    }

    /**
     * Get the time of the vote.
     * @return
     */
    const int64_t & getTime() const {
        return time;
    }

    /**
     * Get the amount associated with the vote.
     * @return
     */
    const CAmount & getAmount() const {
        return amount;
    }

    /**
     * Vote block number
     * @return
     */
    const int & getBlockNumber() const {
        return blockNumber;
    }

    /**
     * Return the public key id associated with the vote's utxo.
     * @return
     */
    const CKeyID & getKeyID() const {
        return keyid;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(version);
        READWRITE(type);
        READWRITE(proposal);
        READWRITE(vote);
        READWRITE(utxo);
        READWRITE(vinhash);
        READWRITE(signature);
        if (ser_action.ForRead()) { // assign memory only fields
            chash = getHash(false);
            csighash = sigHash(false);
            pubkey.RecoverCompact(csighash, signature);
        }
    }

protected:
    /**
     * Returns true if the unsigned char is a valid vote type enum.
     * @param voteType
     * @return
     */
    bool isValidVoteType(const uint8_t & voteType) const {
        return voteType >= NO && voteType <= ABSTAIN;
    }

protected:
    uint8_t version{NETWORK_VERSION};
    uint8_t type{VOTE};
    uint256 proposal;
    uint8_t vote{ABSTAIN};
    VinHash vinhash;
    std::vector<unsigned char> signature;
    COutPoint utxo; // voting on behalf of this utxo

protected: // memory only
    CPubKey pubkey;
    COutPoint outpoint; // of vote's OP_RETURN outpoint
    int64_t time{0}; // block time of vote
    CAmount amount{0}; // of vote's utxo (this is not the OP_RETURN outpoint amount, which is 0)
    CKeyID keyid; // CKeyID of vote's utxo
    int blockNumber{0}; // block containing this vote
    int spentBlock{0}; // block where this vote's utxo was spent (which invalidates it)
    uint256 spentHash; // tx hash where this vote's utxo was spent (which invalidates it)
    uint256 chash; // cached hash
    uint256 csighash; // cached sighash
};

/**
 * Check that utxo isn't already spent
 * @param vote
 * @param currentBlock If utxo is confirmed after this block number it is marked spent
 * @param governanceStart Block at which the governance system starts
 * @param mempoolCheck Will check the mempool for spent votes
 * @return
 */
static bool IsVoteSpent(const Vote & vote, const int & currentBlock, const int & governanceStart, const bool & mempoolCheck = false) {
    Coin coin;
    if (mempoolCheck) {
        LOCK2(cs_main, mempool.cs);
        CCoinsViewMemPool view(pcoinsTip.get(), mempool);
        if (!view.GetCoin(vote.getUtxo(), coin) || mempool.isSpent(vote.getUtxo()))
            return true;
    } else {
        LOCK(cs_main);
        if (!pcoinsTip->GetCoin(vote.getUtxo(), coin))
            return true;
        if (coin.nHeight < governanceStart || coin.nHeight > currentBlock)
            return true;
    }
    return false;
}

/**
 * ProposalVote associates a proposal with a specific vote.
 */
struct ProposalVote {
    Proposal proposal;
    VoteType vote{ABSTAIN};
    CTxDestination dest;
    explicit ProposalVote() = default;
    explicit ProposalVote(const Proposal & proposal, const VoteType & vote) : proposal(proposal), vote(vote), dest(CNoDestination()) {}
    explicit ProposalVote(const Proposal & proposal, const VoteType & vote, const CTxDestination & dest) : proposal(proposal), vote(vote), dest(dest) {}
};
/**
 * Way to obtain all votes for a specific proposal
 */
struct Tally {
    Tally() = default;
    CAmount cyes{0};
    CAmount cno{0};
    CAmount cabstain{0};
    int yes{0};
    int no{0};
    int abstain{0};
    bool payout{false};
    double passing() const {
        return static_cast<double>(yes) / static_cast<double>(yes + no);
    }
    int netyes() const {
        return yes - no;
    }
    bool operator==(const Tally & rhs) {
        return cyes == rhs.cyes
                && cno == rhs.cno
                && cabstain == rhs.cabstain
                && yes == rhs.yes
                && no == rhs.no
                && abstain == rhs.abstain
                && payout == rhs.payout
                ;
    }
};

/**
 * Proposal on disk data model.
 */
class CDiskProposal : public Proposal {
public:
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(version);
        READWRITE(type);
        READWRITE(superblock);
        READWRITE(amount);
        READWRITE(address);
        READWRITE(name);
        READWRITE(url);
        READWRITE(description);
        READWRITE(blockNumber);
        if (!ser_action.ForRead() && chash.IsNull())
            chash = getHash(false);
        READWRITE(chash);
    }

    void SetNull() {
        version = NETWORK_VERSION;
        type = PROPOSAL;
        name = "";
        superblock = 0;
        amount = 0;
        address = "";
        url = "";
        description = "";
        blockNumber = 0;
        chash.SetNull();
    }

    CDiskProposal() {
        SetNull();
    }

    explicit CDiskProposal(const Proposal & proposal) {
        version = proposal.version;
        type = proposal.type;
        superblock = proposal.superblock;
        amount = proposal.amount;
        address = proposal.address;
        name = proposal.name;
        url = proposal.url;
        description = proposal.description;
        blockNumber = proposal.blockNumber;
        chash = proposal.chash;
    }
};

/**
 * Vote on disk data model.
 */
class CDiskVote : public Vote {
public:
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(version);
        READWRITE(type);
        READWRITE(proposal);
        READWRITE(vote);
        READWRITE(vinhash);
        READWRITE(signature);
        READWRITE(utxo);
        READWRITE(pubkey);
        READWRITE(outpoint);
        READWRITE(time);
        READWRITE(amount);
        READWRITE(keyid);
        READWRITE(blockNumber);
        READWRITE(spentBlock);
        READWRITE(spentHash);
        if (!ser_action.ForRead()) {
            if (chash.IsNull())
                chash = getHash(false);
            if (csighash.IsNull())
                csighash = getHash(false);
        }
        READWRITE(chash);
        READWRITE(csighash);
    }

    void SetNull() {
        version = NETWORK_VERSION;
        type = VOTE;
        proposal.SetNull();
        vote = ABSTAIN;
        vinhash = VinHash();
        signature.clear();
        utxo.SetNull();
        pubkey = CPubKey();
        outpoint.SetNull();
        time = 0;
        amount = 0;
        keyid.SetNull();
        blockNumber = 0;
        spentBlock = 0;
        spentHash.SetNull();
        chash.SetNull();
        csighash.SetNull();
    }

    CDiskVote() {
        SetNull();
    }

    explicit CDiskVote(const Vote & v) {
        version = v.version;
        type = v.type;
        proposal = v.proposal;
        vote = v.vote;
        vinhash = v.vinhash;
        signature = v.signature;
        utxo = v.utxo;
        pubkey = v.pubkey;
        outpoint = v.outpoint;
        time = v.time;
        amount = v.amount;
        keyid = v.keyid;
        blockNumber = v.blockNumber;
        spentBlock = v.spentBlock;
        spentHash = v.spentHash;
        chash = v.chash;
        csighash = v.csighash;
    }
};

/**
 * Spent utxo on disk data model.
 */
class CDiskSpentUtxo {
public:
    COutPoint outpoint;
    uint32_t blockNumber{0};
    uint256 spentHash;
    uint8_t version{1};

    CDiskSpentUtxo() = default;
    explicit CDiskSpentUtxo(COutPoint outpoint, uint32_t blockNumber, uint256 spentHash) : outpoint(outpoint)
                                                                                         , blockNumber(blockNumber)
                                                                                         , spentHash(spentHash) { }
    CDiskSpentUtxo(const CDiskSpentUtxo &) = default;
    CDiskSpentUtxo& operator=(const CDiskSpentUtxo &) = default;
    friend inline bool operator==(const CDiskSpentUtxo & a, const CDiskSpentUtxo & b) { return a.outpoint == b.outpoint; }
    friend inline bool operator!=(const CDiskSpentUtxo & a, const CDiskSpentUtxo & b) { return !(a.outpoint == b.outpoint); }
    friend inline bool operator<(const CDiskSpentUtxo & a, const CDiskSpentUtxo & b) { return a.outpoint < b.outpoint; }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(version);
        READWRITE(outpoint);
        READWRITE(blockNumber);
        READWRITE(spentHash);
    }

    std::string Key() const {
        return outpoint.hash.GetHex().substr(10) + ":" + std::to_string(outpoint.n);
    }

    void SetNull() {
        version = 1;
        outpoint.SetNull();
        blockNumber = 0;
        spentHash.SetNull();
    }
};

/**
 * Hasher used with unordered_map and unordered_set
 */
struct Hasher {
    size_t operator()(const CKeyID & keyID) const { return ReadLE64(keyID.begin()); }
    size_t operator()(const uint256 & hash) const { return ReadLE64(hash.begin()); }
    size_t operator()(const COutPoint & out) const { return (CHashWriter(SER_GETHASH, 0) << out).GetCheapHash(); }
    size_t operator()(const CDiskSpentUtxo & utxo) const { return (CHashWriter(SER_GETHASH, 0) << (utxo.outpoint)).GetCheapHash(); }
};

/**
 * Manages related servicenode functions including handling network messages and storing an active list
 * of valid servicenodes.
 */
class Governance : public CValidationInterface {
public:
    explicit Governance(const int64_t cache) {
        db = MakeUnique<GovernanceDB>(cache, false, fReindex);
    }

    /**
     * Returns true if the proposal with the specified name exists.
     * @param name
     * @return
     */
    bool hasProposal(const std::string & name, const int & superblock) {
        LOCK(mu);
        for (const auto & item : proposals) {
            if (item.second.getSuperblock() == superblock && item.second.getName() == name)
                return true;
        }
        return false;
    }

    /**
     * Returns true if the proposal with the specified hash exists.
     * @param hash
     * @return
     */
    bool hasProposal(const uint256 & hash) {
        LOCK(mu);
        return proposals.count(hash) > 0;
    }

    /**
     * Returns true if the proposal with the specified hash exists and that it exists
     * prior to the specified block.
     * @param hash
     * @param blockNumber
     * @return
     */
    bool hasProposal(const uint256 & hash, const int & blockNumber) {
        LOCK(mu);
        return proposals.count(hash) > 0 && proposals[hash].getBlockNumber() < blockNumber;
    }

    /**
     * Returns true if the vote with the specified hash exists.
     * @param hash
     * @return
     */
    bool hasVote(const uint256 & hash) {
        LOCK(mu);
        return votes.count(hash) > 0;
    }

    /**
     * Returns true if the specified proposal and utxo matches a known vote.
     * @param proposal
     * @param voteType
     * @param utxo
     * @return
     */
    bool hasVote(const uint256 & proposal, const VoteType & voteType, const COutPoint & utxo) {
        LOCK(mu);
        if (!proposals.count(proposal))
            return false; // no proposal

        const auto & prop = proposals[proposal];
        if (!sbvotes.count(prop.getSuperblock()))
            return false; // no superblock proposal

        const auto & vs = sbvotes[prop.getSuperblock()];
        for (const auto & item : vs) {
            const auto & vote = item.second;
            if (vote.getUtxo() == utxo && vote.getProposal() == proposal && vote.getVote() == voteType)
                return true;
        }
        return false;
    }

    /**
     * Resets the governance state.
     * @return
     */
    bool reset() {
        LOCK(mu);
        proposals.clear();
        votes.clear();
        stackvotes.clear();
        sbvotes.clear();
        db->Reset(true);
        return true;
    }

    /**
     * Loads the governance data from the blockchain ledger. It's possible to optimize
     * this further by creating a separate leveldb for goverance data. Currently, this
     * method will read every block on the chain beginning with the governance start
     * block and search for goverance data. Requires the entire chainstate to be loaded
     * at this point, including the transaction index.
     * @return
     */
    bool loadGovernanceData(const CChain & chain, CCriticalSection & chainMutex, const Consensus::Params & consensus,
            std::string & failReasonRet, const int & nthreads=0)
    {
        int bestBlockHeight{0};
        int blockHeight{0};
        const CBlockIndex *bestBlockIndex = nullptr;
        {
            LOCK(chainMutex);
            blockHeight = chain.Height();
            // Load the db data
            db->Start();
            if (blockHeight >= consensus.governanceBlock) {
                if (!db->BestBlockIndex())
                    db->WriteBestBlock(chain[consensus.governanceBlock], chain, chainMutex);
                bestBlockIndex = chain.FindFork(db->BestBlockIndex());
                bestBlockHeight = bestBlockIndex->nHeight;
            }
        }
        // No need to load any governance data if we're on the genesis block
        // or if the governance system hasn't been enabled yet.
        if (blockHeight == 0 || blockHeight < consensus.governanceBlock)
            return true;

        // Load data from db
        if (bestBlockHeight >= consensus.governanceBlock) {
            LOCK(mu);
            std::unique_ptr<CDBIterator> pcursor(db->GetDB().NewIterator());
            pcursor->SeekToFirst();
            while (pcursor->Valid()) {
                std::pair<char, uint256> key;
                if (!pcursor->GetKey(key)) {
                    pcursor->Next();
                    continue;
                }
                if (key.first == DB_PROPOSAL) {
                    CDiskProposal proposal;
                    if (pcursor->GetValue(proposal))
                        addProposal(proposal, false);
                    else
                        return error("%s: failed to read proposal", __func__);
                } else if (key.first == DB_VOTE) {
                    CDiskVote vote;
                    if (pcursor->GetValue(vote))
                        addVote(vote, false);
                    else
                        return error("%s: failed to read vote", __func__);
                }
                pcursor->Next();
            }
        }

        if (bestBlockHeight >= blockHeight)
            return true; // done loading

        boost::thread_group tg;
        Mutex mut; // manage access to shared data
        const auto cores = nthreads == 0 ? GetNumCores() : nthreads;
        std::unordered_map<COutPoint, CDiskSpentUtxo, Hasher> spentPrevouts;
        bool useThreadGroup{false};

        // Shard the blocks into num equivalent to available cores
        const int totalBlocks = blockHeight - bestBlockHeight;
        int slice = totalBlocks / cores;
        bool failed{false};

        auto p1 = [&spentPrevouts,&failed,&failReasonRet,&chain,&chainMutex,&mut,this]
                  (const int start, const int end, const Consensus::Params & consensus) -> bool
        {
            for (int blockNumber = start; blockNumber < end; ++blockNumber) {
                if (ShutdownRequested()) { // don't hold up shutdown requests
                    LOCK(mut);
                    failed = true;
                    return false;
                }

                CBlockIndex *blockIndex;
                {
                    LOCK(chainMutex);
                    blockIndex = chain[blockNumber];
                }
                if (!blockIndex) {
                    LOCK(mut);
                    failed = true;
                    failReasonRet += strprintf("Failed to read block index for block %d\n", blockNumber);
                    return false;
                }

                CBlock block;
                if (!ReadBlockFromDisk(block, blockIndex, consensus)) {
                    LOCK(mut);
                    failed = true;
                    failReasonRet += strprintf("Failed to read block from disk for block %d\n", blockNumber);
                    return false;
                }
                // Store all vins in order to use as a lookup for spent votes
                for (const auto & tx : block.vtx) {
                    LOCK(mut);
                    const auto & txhash = tx->GetHash();
                    for (const auto & vin : tx->vin)
                        spentPrevouts[vin.prevout] = CDiskSpentUtxo{vin.prevout, static_cast<uint32_t>(blockIndex->nHeight), txhash};
                }
                // Process block
                processBlock(&block, blockIndex->nHeight, consensus, false);
            }
            return true;
        };

        for (int k = 0; k < cores; ++k) {
            const int start = bestBlockHeight + k*slice;
            const int end = k == cores-1 ? blockHeight+1 // check bounds, +1 due to "<" logic below, ensure inclusion of last block
                                         : start+slice;
            // try single threaded on failure
            try {
                // TODO Blocknet governance: concurrency causing state issues
//                if (cores > 1) {
//                    tg.create_thread([start,end,consensus,&p1] {
//                        RenameThread("blocknet-governance");
//                        p1(start, end, consensus);
//                    });
//                    useThreadGroup = true;
//                } else
                    p1(start, end, consensus);
            } catch (...) {
                try {
                    p1(start, end, consensus);
                } catch (std::exception & e) {
                    failed = true;
                    failReasonRet += strprintf("Failed to create thread to load governance data: %s\n", e.what());
                    return false; // fatal error
                }
            }
        }

        // Wait for all threads to complete
        if (useThreadGroup)
            tg.join_all();

        if (failed)
            return false;

        bool haveVotes{false};
        {
            LOCK(mu);
            haveVotes = !votes.empty();
        }
        if (haveVotes) {
            // Now that all votes are loaded, check and remove any invalid ones.
            // Invalid votes can be evaluated using multiple threads since we
            // have the complete dataset in memory. Below the votes are sliced
            // up into shards and each available thread works on its own shard.
            std::vector<std::pair<uint256, Vote>> tmpvotes;
            {
                LOCK(mu);
                tmpvotes.reserve(votes.size());
                std::copy(votes.cbegin(), votes.cend(), std::back_inserter(tmpvotes));
            }

            auto p2 = [&tmpvotes,&spentPrevouts,&failed,&mut,this](const int start, const int end,
                    const Consensus::Params & consensus) -> bool
            {
                for (int i = start; i < end; ++i) {
                    if (ShutdownRequested()) { // don't hold up shutdown requests
                        LOCK(mut);
                        failed = true;
                        return false;
                    }

                    auto & vote = tmpvotes[i].second;

                    // Remove votes that are not associated with a proposal
                    if (!hasProposal(vote.getProposal(), vote.getBlockNumber())) {
                        LOCK(mu);
                        removeVote(vote, true, false);
                        continue;
                    }

                    // Remove votes that are inside the cutoff
                    const auto & proposal = getProposal(vote.getProposal());
                    {
                        // If a vote is in the cutoff it will mutate the vote here with the
                        // next non-cutoff vote in the stack. This only applies if votes
                        // were changed in the cutoff period. The most recent valid non-cutoff
                        // period vote will be used. If no valid votes are left after this
                        // check move to the next iteration.
                        LOCK(mu);
                        if (removeVotesInCutoff(vote, consensus))
                            continue;
                    }

                    // Mark vote as spent if its utxo is spent before or on the associated proposal's superblock.
                    auto utxoSpent = spentPrevouts.count(vote.getUtxo());
                    auto spent = utxoSpent;
                    CDiskSpentUtxo spentUtxo;
                    if (!utxoSpent) { // check db for spent utxo
                        spentUtxo.outpoint = vote.getUtxo();
                        spent = db->ReadSpentUtxo(spentUtxo.Key(), spentUtxo);
                    } else
                        spentUtxo = spentPrevouts[vote.getUtxo()];
                    // Check if spent within the superblock voting period
                    if (spent && spentUtxo.blockNumber <= proposal.getSuperblock()) {
                        LOCK(mu);
                        spendVote(vote.getHash(), spentUtxo.blockNumber, spentUtxo.spentHash, false);
                        continue;
                    }

                    // Prevent voting on utxos in the future, all votes must reference utxos already confirmed on-chain.
                    // a) Find the block height where vote utxo was included on chain.
                    // b) The vote is ignored if this height is after the height where the vote was included on chain.
                    CTransactionRef tx;
                    uint256 hashBlock;
                    if (!GetTransaction(vote.getUtxo().hash, tx, consensus, hashBlock)) {
                        LOCK(mu);
                        removeVote(vote, true, false);
                        continue;
                    }
                    CBlockIndex *pindex = nullptr;
                    {
                        LOCK(cs_main);
                        pindex = LookupBlockIndex(hashBlock);
                    }
                    if (!pindex || pindex->nHeight > vote.getBlockNumber()) {
                        LOCK(mu);
                        removeVote(vote, true, false);
                        continue;
                    }
                }
                return true;
            };

            useThreadGroup = false; // initial state

            slice = static_cast<int>(tmpvotes.size()) / cores;
            for (int k = 0; k < cores; ++k) {
                const int start = k*slice;
                const int end = k == cores-1 ? static_cast<int>(tmpvotes.size())
                                             : start+slice;
                // try single threaded on failure
                try {
                    if (cores > 1) {
                        tg.create_thread([start,end,consensus,&p2] {
                            RenameThread("blocknet-governance");
                            p2(start, end, consensus);
                        });
                        useThreadGroup = true;
                    } else
                        p2(start, end, consensus);
                } catch (...) {
                    try {
                        p2(start, end, consensus);
                    } catch (std::exception & e) {
                        failed = true;
                        failReasonRet += strprintf("Failed to create thread to load governance votes: %s\n", e.what());
                        return false; // fatal error
                    }
                }
            }
            // Wait for all threads to complete
            if (useThreadGroup)
                tg.join_all();
        }

        if (failed)
            return false;

        std::vector<std::pair<uint256, CDiskProposal>> savepps;
        auto pps = copyProposals();
        for (auto & pitem : pps) {
            if (pitem.second.getBlockNumber() > bestBlockHeight)
                savepps.emplace_back(pitem.first, CDiskProposal(pitem.second));
        }
        std::vector<std::pair<uint256, CDiskVote>> savevvs;
        auto vvs = copyVotes();
        for (auto & vitem : vvs) {
            if (vitem.second.getBlockNumber() > bestBlockHeight)
                savevvs.emplace_back(vitem.first, CDiskVote(vitem.second));
        }
        std::vector<std::pair<std::string, CDiskSpentUtxo>> saveutxos;
        for (auto & uitem : spentPrevouts) {
            if (uitem.second.blockNumber > bestBlockHeight)
                saveutxos.emplace_back(uitem.second.Key(), uitem.second);
        }
        if (!savepps.empty() && !db->AddProposals(savepps)) {
            failReasonRet += "Failed to save proposals to the database\n";
            return false;
        }
        if (!savevvs.empty() && !db->AddVotes(savevvs)) {
            failReasonRet += "Failed to save votes to the database\n";
            return false;
        }
        if (!saveutxos.empty() && !db->AddSpentUtxos(saveutxos)) {
            failReasonRet += "Failed to save spent utxos to the database\n";
            return false;
        }

        {
            LOCK(chainMutex);
            if (!db->WriteBestBlock(chain[blockHeight], chain, chainMutex)) {
                failReasonRet += "Failed to save governance system best block to the database\n";
                return false;
            }
        }

        return true;
    }

    /**
     * Fetch the specified proposal.
     * @param hash Proposal hash
     * @return
     */
    Proposal getProposal(const uint256 & hash) {
        LOCK(mu);
        if (proposals.count(hash) > 0)
            return proposals[hash];
        return Proposal{};
    }

    /**
     * Fetch the specified vote by its hash.
     * @param hash Vote hash
     * @return
     */
    Vote getVote(const uint256 & hash) {
        LOCK(mu);
        if (votes.count(hash) > 0)
            return votes[hash];
        return Vote{};
    }

    /**
     * Fetch the list of all known proposals.
     * @return
     */
    std::vector<Proposal> getProposals() {
        LOCK(mu);
        std::vector<Proposal> props;
        props.reserve(proposals.size());
        for (const auto & item : proposals)
            props.push_back(item.second);
        return props;
    }

    /**
     * Fetch the list of all known proposals in the specified superblock.
     * @param superblock Block number of the superblock
     * @return
     */
    std::vector<Proposal> getProposals(const int & superblock) {
        LOCK(mu);
        std::vector<Proposal> props;
        for (const auto & item : proposals) {
            if (item.second.getSuperblock() == superblock)
                props.push_back(item.second);
        }
        return props;
    }

    /**
     * Fetch the list of all known proposals who's superblocks are ahead of the specified block.
     * @param since Block number to start search from
     * @return
     */
    std::vector<Proposal> getProposalsSince(const int & since) {
        LOCK(mu);
        std::vector<Proposal> props;
        for (const auto & item : proposals) {
            if (item.second.getSuperblock() >= since)
                props.push_back(item.second);
        }
        return props;
    }

    /**
     * Return copy of all votes.
     * @return
     */
    std::unordered_map<uint256, Vote, Hasher> copyVotes() {
        LOCK(mu);
        return votes;
    }

    /**
     * Return copy of all proposals.
     * @return
     */
    std::unordered_map<uint256, Proposal, Hasher> copyProposals() {
        LOCK(mu);
        return proposals;
    }

    /**
     * Fetch the list of all known votes that haven't been spent.
     * @return
     */
    std::vector<Vote> getVotes() {
        LOCK(mu);
        std::vector<Vote> vos;
        for (const auto & item : votes) {
            if (!item.second.spent())
                vos.push_back(item.second);
        }
        return vos;
    }

    /**
     * Fetch all votes for the specified proposal that haven't been spent. Optionally return spent votes.
     * @param proposalHash Proposal hash
     * @param returnSpent Includes spent votes, defaults to false
     * @return
     */
    std::vector<Vote> getVotes(const uint256 & proposalHash, const bool & returnSpent = false) {
        LOCK(mu);
        std::vector<Vote> vos;
        if (!proposals.count(proposalHash))
            return vos;

        const auto & proposal = proposals[proposalHash];
        if (!sbvotes.count(proposal.getSuperblock()))
            return vos;

        auto & vs = sbvotes[proposal.getSuperblock()];
        for (auto & item : vs) {
            if (item.second.getProposal() == proposalHash && (returnSpent || !item.second.spent()))
                vos.push_back(item.second);
        }
        return vos;
    }

    /**
     * Fetch all votes in the specified superblock that haven't been spent.
     * @param superblock Block number of superblock
     * @return
     */
    std::vector<Vote> getVotes(const int & superblock) {
        LOCK(mu);
        std::vector<Vote> vos;
        if (!sbvotes.count(superblock))
            return vos;

        const auto & vs = sbvotes[superblock];
        for (const auto & item : vs) {
            if (!item.second.spent())
                vos.push_back(item.second);
        }
        return vos;
    }

    /**
     * Spends the vote and ensures other data providers are synced. If the specified vote
     * is associated with a superblock that's prior to the block number, the vote is not
     * marked spent.
     * @param voteHash
     * @param block Block number
     * @param txhash prevout of spent vote
     * @param savedb Write to db
     */
    void spendVote(const uint256 & voteHash, const int & block, const uint256 & txhash, bool savedb=true) EXCLUSIVE_LOCKS_REQUIRED(mu) {
        if (!votes.count(voteHash))
            return;
        // Vote ref
        auto & vote = votes[voteHash];

        if (!proposals.count(vote.getProposal()))
            return;

        if (block > proposals[vote.getProposal()].getSuperblock())
            return; // do not spend a vote on a block that's after the vote's superblock

        // Update current vote
        vote.spend(block, txhash);
        // Update sbvotes data provider
        if (sbvotes.count(proposals[vote.getProposal()].getSuperblock())) {
            auto & mv = sbvotes[proposals[vote.getProposal()].getSuperblock()];
            if (mv.count(voteHash)) {
                auto & v = mv[voteHash];
                v.spend(block, txhash);
            }
        }
        stackvotes[voteHash].back().spend(block, txhash);

        // Update db
        if (savedb)
            db->AddVote(CDiskVote(vote));
    }

    /**
     * Unspend the vote and ensures other data providers are updated. Only unspends the vote
     * if the block number is prior to the vote's associated superblock.
     * @param voteHash
     * @param block Block number
     * @param txhash prevout of spent vote
     * @param savedb Write to db
     */
    void unspendVote(const uint256 & voteHash, const int & block, const uint256 & txhash, bool savedb=true) EXCLUSIVE_LOCKS_REQUIRED(mu) {
        if (!votes.count(voteHash))
            return;
        // Vote ref
        auto & vote = votes[voteHash];

        if (!proposals.count(vote.getProposal()))
            return;

        if (block > proposals[vote.getProposal()].getSuperblock())
            return; // do not unspend votes who's superblocks are after the specified block

        // Update current vote
        vote.unspend(block, txhash);
        // Update sbvotes data provider
        if (sbvotes.count(proposals[vote.getProposal()].getSuperblock())) {
            auto & mv = sbvotes[proposals[vote.getProposal()].getSuperblock()];
            if (mv.count(voteHash)) {
                auto & v = mv[voteHash];
                v.unspend(block, txhash);
            }
        }
        stackvotes[voteHash].back().unspend(block, txhash);

        // Update db
        if (savedb)
            db->AddVote(CDiskVote(vote));
    }

    /**
     * Obtains all votes and proposals from the specified block.
     * @param block
     * @param proposalsRet
     * @param votesRet
     * @param vinHashesRet
     * @param blockHeight
     * @return
     */
    void dataFromBlock(const CBlock *block, std::set<Proposal> & proposalsRet, std::set<Vote> & votesRet,
            std::map<uint256, std::set<VinHash>> & vinHashesRet, const Consensus::Params & params, const int blockHeight)
    {
        proposalsRet.clear();
        votesRet.clear();
        vinHashesRet.clear();
        std::map<uint256,std::set<VinHash>> vhashes;

        for (const auto & tx : block->vtx) {
            if (tx->IsCoinBase())
                continue;
            // cache vin hashes
            for (const auto & vin : tx->vin) {
                const auto & vhash = makeVinHash(vin.prevout);
                vhashes[tx->GetHash()].insert(vhash);
            }
            for (int n = 0; n < static_cast<int>(tx->vout.size()); ++n) {
                const auto & out = tx->vout[n];
                if (out.scriptPubKey[0] != OP_RETURN)
                    continue; // no proposal data
                CScript::const_iterator pc = out.scriptPubKey.begin();
                std::vector<unsigned char> data;
                opcodetype opcode{OP_FALSE};
                bool checkdata{false};
                while (pc < out.scriptPubKey.end()) {
                    opcode = OP_FALSE;
                    if (!out.scriptPubKey.GetOp(pc, opcode, data))
                        break;
                    checkdata = (opcode == OP_PUSHDATA1 || opcode == OP_PUSHDATA2 || opcode == OP_PUSHDATA4)
                            || (opcode < OP_PUSHDATA1 && opcode == data.size());
                    if (checkdata && !data.empty())
                        break;
                }
                if (!checkdata || data.empty())
                    continue; // skip if no data

                CDataStream ss(data, SER_NETWORK, GOV_PROTOCOL_VERSION);
                NetworkObject obj; ss >> obj;
                if (!obj.isValid())
                    continue; // must match expected version

                if (obj.getType() == PROPOSAL) {
                    CDataStream ss2(data, SER_NETWORK, GOV_PROTOCOL_VERSION);
                    Proposal proposal(blockHeight); ss2 >> proposal;
                    proposalsRet.insert(proposal); // proposals never overwrite prior proposals (set insertion rules)
                } else if (obj.getType() == VOTE) {
                    CDataStream ss2(data, SER_NETWORK, GOV_PROTOCOL_VERSION);
                    Vote vote({tx->GetHash(), static_cast<uint32_t>(n)}, block->GetBlockTime(), blockHeight);
                    ss2 >> vote;
                    // Add vin hashes
                    vinHashesRet[vote.getHash()] = vhashes[tx->GetHash()];
                    // Votes in later transactions in the block always overwrite
                    // earlier votes.
                    if (votesRet.count(vote))
                        votesRet.erase(vote);
                    votesRet.insert(vote);
                }
            }
        }
    }

    /**
     * Processes and validates the governance data parsed from a block.
     * @param psRet
     * @param vsRet
     * @param vh
     * @param params
     * @param blockHeight
     * @param processingChainTip
     */
    void filterDataFromBlock(std::set<Proposal> & psRet, std::set<Vote> & vsRet, const std::map<uint256,std::set<VinHash>> & vh,
            const Consensus::Params & params, const int blockHeight = 0, const bool processingChainTip = false)
    {
        const auto ps = psRet;
        const auto vs = vsRet;
        psRet.clear();
        vsRet.clear();

        // Insert proposals first because vote insert requires an existing proposal
        for (auto & proposal : ps) {
            // Do not allow proposals with the same parameters to replace
            // existing proposals. Only add proposals that are outside the
            // cutoff period.
            if (blockHeight > 0 && !outsideProposalCutoff(proposal, blockHeight, params))
                continue;
            if (proposal.isValid(params))
                psRet.insert(proposal);
        }
        // Insert votes after proposals in case votes depend on proposals in
        // the same block.
        for (auto vote : vs) {
            // If we are processing the chain tip we want to perform a proposal
            // check here. Check that the vote is associated with a valid proposal.
            if (processingChainTip) {
                LOCK(mu);
                if (!proposals.count(vote.getProposal()))
                    continue; // skip votes without valid proposals
                // If we are processing the chain tip we want to check that the vote
                // meets the cutoff requirements. A valid proposal for this vote must
                // exist in a previous block otherwise the vote is discarded.
                if (blockHeight > 0) {
                    const auto & proposal = proposals[vote.getProposal()];
                    if (proposal.getBlockNumber() > vote.getBlockNumber() // skip votes who's proposal is after vote
                        || !outsideVotingCutoff(proposal, blockHeight, params)) // skip votes inside cutoff period
                        continue;
                }
            }

            const auto voteHash = vote.getHash();

            // Load the vote utxo (performs cs_main lock)
            if (!vote.loadVoteUTXO())
                continue;
            const auto vhash = vh.find(voteHash);
            if (vhash == vh.end() || !vote.isValid(vhash->second, params))
                continue;

            // Handle vote changes, if a vote already exists and the user
            // is submitting a change, only count the vote with the most
            // recent timestamp.
            {
                LOCK(mu);
                if (votes.count(voteHash)) {
                    if (vote.getBlockNumber() >= votes[voteHash].getBlockNumber()) {
                        if (vsRet.count(vote))
                            vsRet.erase(vote);
                        vsRet.insert(std::move(vote)); // overwrite existing vote
                        continue;
                    }
                }
            }

            // Only check the mempool and coincache for spent utxos if
            // we're currently processing the chain tip.
            bool spent{false};
            if (processingChainTip)
                spent = IsVoteSpent(vote, blockHeight, params.governanceBlock, false); // check that utxo is unspent
            if (spent)
                continue;
            if (vsRet.count(vote))
                vsRet.erase(vote);
            vsRet.insert(std::move(vote)); // overwrite existing vote if necessary
        }
    }

    /**
     * Return the superblock results for all the proposals scheduled for the specified superblock.
     * @param superblock
     * @param params
     * @param includeExcluded If this flag is set, the tallies for proposals that didn't make the
     *        superblock will be returned.
     * @return
     */
    std::map<Proposal, Tally> getSuperblockResults(const int & superblock, const Consensus::Params & params, const bool & includeExcluded = false) {
        std::map<Proposal, Tally> r;
        if (!isSuperblock(superblock, params))
            return r;

        std::set<COutPoint> unique;
        std::vector<Proposal> ps;
        std::vector<Vote> vs;
        getProposalsForSuperblock(superblock, ps, vs);

        CAmount uniqueAmount{0};
        for (const auto & vote : vs) { // count all the unique voting utxos
            if (unique.count(vote.getUtxo()))
                continue;
            unique.insert(vote.getUtxo());
            uniqueAmount += vote.getAmount();
        }
        const auto uniqueVotes = static_cast<int>(uniqueAmount / params.voteBalance);

        for (const auto & proposal : ps) // get results for each proposal
            r[proposal] = getTally(proposal.getHash(), vs, params);

        // a) Exclude proposals that don't have the required yes votes.
        //    60% of votes must be "yes" on a passing proposal.
        // b) Exclude proposals that don't have at least 25% of all participating
        //    votes. i.e. at least 25% of all votes cast this superblock must have
        //    voted on this proposal.
        // c) Exclude proposals with 0 yes votes in all circumstances
        for (auto it = r.begin(); it != r.end(); ) {
            const auto & tally = it->second;
            const int total = tally.yes+tally.no+tally.abstain;
            const int yaynay = tally.yes + tally.no;
            if ((yaynay == 0 || static_cast<double>(tally.yes) / static_cast<double>(yaynay) < 0.6
              || static_cast<double>(total) < static_cast<double>(uniqueVotes) * 0.25
              || tally.yes <= 0)) {
                if (!includeExcluded) {
                    r.erase(it++);
                    continue;
                }
            } else
                it->second.payout = true;
            ++it;
        }

        return r;
    }

    /**
     * Fetch the list of proposals scheduled for the specified superblock. Requires loadGovernanceData to have been run
     * on chain load.
     * @param superblock
     * @param allProposals
     * @param allVotes Votes specific to the selected proposals.
     */
    void getProposalsForSuperblock(const int & superblock, std::vector<Proposal> & allProposals, std::vector<Vote> & allVotes) {
        auto ps = getProposals(superblock);
        auto vs = getVotes(superblock);
        std::set<uint256> proposalHashes;
        for (const auto & p : ps) {
            if (p.getSuperblock() == superblock) {
                allProposals.push_back(p);
                proposalHashes.insert(p.getHash());
            }
        }
        // Find all votes associated with the selected proposals
        for (const auto & v : vs) {
            if (proposalHashes.count(v.getProposal()))
                allVotes.push_back(v);
        }
    }

    /**
     * Returns true if the specified block is a valid superblock, including matching the expected proposal
     * payouts to the superblock payees.
     * @param block
     * @param blockHeight
     * @param params
     * @param paymentsRet Total superblock payment
     * @return
     */
    bool isValidSuperblock(const CBlock *block, const int & blockHeight, const Consensus::Params & params, CAmount & paymentsRet)
    {
        if (!isSuperblock(blockHeight, params))
            return false;

        // Superblock payout must be in the coinstake
        if (!block->IsProofOfStake())
            return false;

        // Get the results and sort descending by passing percent.
        // We want to sort descending because the most valuable
        // proposals are those with the highest passing percentage,
        // in this case we want them at the beginning of the list.
        const auto & results = getSuperblockResults(blockHeight, params);
        if (results.empty())
            return true;

        auto payees = getSuperblockPayees(blockHeight, results, params);
        if (payees.empty())
            return false;

        // Add up the total expected superblock payment
        paymentsRet = 0;
        for (const auto & payee : payees)
            paymentsRet += payee.nValue;

        auto vouts = block->vtx[1]->vout;
        if (static_cast<int>(vouts.size()) - static_cast<int>(payees.size()) > 2) // allow 1 vout for coinbase and 1 vout for the staker's payment
            return false;

        vouts.erase(std::remove_if(vouts.begin(), vouts.end(),
            [&payees](const CTxOut & vout) {
                bool found{false};
                for (int i = (int)payees.size()-1; i >= 0; --i) {
                    if (vout == payees[i]) {
                        found = true;
                        payees.erase(payees.begin()+i);
                        break;
                    }
                }
                return found;
            }), vouts.end());

        // The superblock payment is valid if all payees
        // are accounted for.
        return vouts.size() <= 2 && payees.empty();
    }

    /**
     * Returns true if the specified utxo exists in an active and valid proposal who's voting period has ended.
     * @param utxo
     * @param tipHeight
     * @param params
     * @return
     */
    bool utxoInVoteCutoff(const COutPoint & utxo, const int & tipHeight, const Consensus::Params & params) {
        const auto superblock = NextSuperblock(params, tipHeight);
        if (!insideVoteCutoff(superblock, tipHeight, params))
            return false; // if tip isn't in the non-voting period then return

        // Check if the utxo is in a valid proposal who's voting period has ended
        std::vector<Proposal> sproposals;
        std::vector<Vote> svotes;
        getProposalsForSuperblock(superblock, sproposals, svotes);

        for (const auto & vote : svotes) {
            if (utxo == vote.getUtxo())
                return true;
        }

        return false;
    }

    /**
     * Returns true if the specified utxo is associated with a vote in an active and valid proposal.
     * @param utxo
     * @param blockHeight
     * @param params
     * @return
     */
    bool utxoInVote(const COutPoint & utxo, const int & blockHeight, const Consensus::Params & params) {
        const auto sproposals = getProposalsSince(blockHeight);
        for (const auto & proposal : sproposals) {
            const auto svotes = getVotes(proposal.getHash());
            for (const auto & vote : svotes) {
                if (utxo == vote.getUtxo())
                    return true;
            }
        }
        return false;
    }

    /**
     * Returns a list of utxos from a set that are associated with a vote in an active and valid proposal.
     * @param utxos
     * @param blockHeight
     * @param utxosRet Filtered with utxos that were found in votes
     * @param params
     * @return
     */
    void utxosInVotes(const std::set<COutPoint> & utxos, const int & blockHeight, std::set<COutPoint> & utxosRet, const Consensus::Params & params) {
        utxosRet.clear();
        const auto sproposals = getProposalsSince(blockHeight);
        for (const auto & proposal : sproposals) {
            const auto svotes = getVotes(proposal.getHash());
            for (const auto & vote : svotes) {
                if (utxos.count(vote.getUtxo()))
                    utxosRet.insert(vote.getUtxo());
            }
        }
    }

public: // static

    /**
     * Singleton instance.
     * @param cache
     * @return
     */
    static Governance & instance(const int64_t cache=nMaxGovDBCache*1024*1024) {
        static Governance gov(cache);
        return gov;
    }

    /**
     * Returns the upcoming superblock.
     * @param params
     * @return
     */
    static int nextSuperblock(const Consensus::Params & params, const int fromBlock = 0) {
        return NextSuperblock(params, fromBlock);
    }

    /**
     * If the vote's pubkey matches the specified vin's pubkey returns true, otherwise
     * returns false.
     * @param vote
     * @param vin
     * @return
     */
    static bool matchesVinPubKey(const Vote & vote, const CTxIn & vin) {
        CScript::const_iterator pc = vin.scriptSig.begin();
        std::vector<unsigned char> data;
        bool isPubkey{false};
        while (pc < vin.scriptSig.end()) {
            opcodetype opcode;
            if (!vin.scriptSig.GetOp(pc, opcode, data))
                break;
            if (data.size() == CPubKey::PUBLIC_KEY_SIZE || data.size() == CPubKey::COMPRESSED_PUBLIC_KEY_SIZE) {
                isPubkey = true;
                break;
            }
        }

        if (!isPubkey)
            return false; // skip, no match

        CPubKey pubkey(data);
        return pubkey.GetID() == vote.getPubKey().GetID();
    }

    /**
     * Returns true if the specified block height is the superblock.
     * @param blockHeight
     * @param params
     * @return
     */
    static bool isSuperblock(const int & blockHeight, const Consensus::Params & params) {
        return blockHeight >= params.governanceBlock && blockHeight % params.superblock == 0;
    }

    /**
     * Returns true if a vote is found in the specified transaction output.
     * This does not verify that the vote found has a valid utxo.
     * @param out
     * @param vote
     * @return
     */
    static bool isVoteInTxOut(const CTxOut & out, Vote & vote) {
        if (out.scriptPubKey[0] != OP_RETURN)
            return false;
        CScript::const_iterator pc = out.scriptPubKey.begin();
        std::vector<unsigned char> data;
        while (pc < out.scriptPubKey.end()) {
            opcodetype opcode;
            if (!out.scriptPubKey.GetOp(pc, opcode, data))
                break;
            if (!data.empty())
                break;
        }
        CDataStream ss(data, SER_NETWORK, GOV_PROTOCOL_VERSION);
        NetworkObject obj; ss >> obj;
        if (obj.getType() == VOTE) {
            CDataStream ss2(data, SER_NETWORK, GOV_PROTOCOL_VERSION);
            ss2 >> vote;
            return true;
        }
        return false;
    }

    /**
     * Returns true if the proposal is not yet in the cutoff period. Make sure mutex (mu) is not held.
     * @param proposal
     * @param blockNumber
     * @param params
     * @return
     */
    static bool outsideProposalCutoff(const Proposal & proposal, const int & blockNumber, const Consensus::Params & params) {
        if (proposal.isNull()) // check if valid
            return false;
        // Proposals can happen multiple superblocks in advance if a proposal
        // is created for a future superblock. As a result, a proposal meets
        // the cutoff if it's included in a block that's prior to its scheduled
        // superblock.
        return blockNumber < proposal.getSuperblock() - params.proposalCutoff;
    }

    /**
     * Returns true if the vote is not yet in the cutoff period. Make sure mutex (mu) is not held.
     * @param proposal
     * @param blockNumber
     * @param params
     * @return
     */
    static bool outsideVotingCutoff(const Proposal & proposal, const int & blockNumber, const Consensus::Params & params) {
        if (proposal.isNull()) // check if valid
            return false;
        // Votes can happen multiple superblocks in advance if a proposal is
        // created for a future superblock. As a result, a vote meets the
        // cutoff for a block number that's prior to the superblock of its
        // associated proposal.
        return blockNumber < proposal.getSuperblock() - params.votingCutoff;
    }

    /**
     * Returns true if the block number is in the vote cutoff. The vote cutoff is considered 1 block
     * prior to the protocol's cutoff since at least 1 block is required to confirm.
     * @param superblock
     * @param blockNumber
     * @param params
     * @return
     */
    static bool insideVoteCutoff(const int & superblock, const int & blockNumber, const Consensus::Params & params) {
        return blockNumber >= superblock - params.votingCutoff && blockNumber <= superblock;
    }

    /**
     * Returns the vote tally for the specified proposal.
     * @param proposal
     * @param votes Vote array to search
     * @param params
     * @return
     */
    static Tally getTally(const uint256 & proposal, const std::vector<Vote> & votes, const Consensus::Params & params) {
        // Organize votes by tx hash to designate common votes (from same user)
        // We can assume all the votes in the same tx are associated with the
        // same user (i.e. all privkeys in the votes are known by the tx signer)
        std::map<uint256, std::set<Vote>> userVotes;
        // Cross reference all votes associated with a destination. If a vote
        // is associated with a common destination we can assume the same user
        // casted the vote. All votes in the tx imply the same user and all
        // votes associated with the same destination imply the same user.
        std::map<CTxDestination, std::set<Vote>> userVotesDest;

        std::vector<Vote> proposalVotes = votes;
        // remove all votes that don't match the proposal
        proposalVotes.erase(std::remove_if(proposalVotes.begin(), proposalVotes.end(), [&proposal](const Vote & vote) -> bool {
            return proposal != vote.getProposal();
        }), proposalVotes.end());

        // Prep our search containers
        for (const auto & vote : proposalVotes) {
            std::set<Vote> & v = userVotes[vote.getOutpoint().hash];
            v.insert(vote);
            userVotes[vote.getOutpoint().hash] = v;

            std::set<Vote> & v2 = userVotesDest[CTxDestination{vote.getPubKey().GetID()}];
            v2.insert(vote);
            userVotesDest[CTxDestination{vote.getPubKey().GetID()}] = v2;
        }

        // Iterate over all transactions and associated votes. In order to
        // prevent counting too many votes we need to tally up votes
        // across users separately and only count up their respective
        // votes in lieu of the maximum vote balance requirements.
        std::set<Vote> counted; // track counted votes
        std::vector<Tally> tallies;
        for (const auto & item : userVotes) {
            // First count all unique votes associated with the same tx.
            // This indicates they're all likely from the same user or
            // group of users pooling votes.
            std::set<Vote> allUnique;
            allUnique.insert(item.second.begin(), item.second.end());
            for (const auto & vote : item.second) {
                // Add all unique votes associated with the same destination.
                // Since we're first iterating over all the votes in the
                // same tx, and then over the votes based on common destination
                // we're able to get all the votes associated with a user.
                // The only exception is if a user votes from different wallets
                // and doesn't reveal the connection by combining into the same
                // tx. As a result, there's an optimal way to cast votes and that
                // should be taken into consideration on the voting client.
                const auto & destVotes = userVotesDest[CTxDestination{vote.getPubKey().GetID()}];
                allUnique.insert(destVotes.begin(), destVotes.end());
            }

            // Prevent counting votes more than once
            for (auto it = allUnique.begin(); it != allUnique.end(); ) {
                if (counted.count(*it) > 0)
                    allUnique.erase(it++);
                else ++it;
            }

            if (allUnique.empty())
                continue; // nothing to count
            counted.insert(allUnique.begin(), allUnique.end());

            Tally tally;
            for (const auto & vote : allUnique)  {
                if (vote.getVote() == gov::YES)
                    tally.cyes += vote.getAmount();
                else if (vote.getVote() == gov::NO)
                    tally.cno += vote.getAmount();
                else if (vote.getVote() == gov::ABSTAIN)
                    tally.cabstain += vote.getAmount();
            }
            tally.yes = static_cast<int>(tally.cyes / params.voteBalance);
            tally.no = static_cast<int>(tally.cno / params.voteBalance);
            tally.abstain = static_cast<int>(tally.cabstain / params.voteBalance);
            // Sanity checks
            if (tally.yes < 0)
                tally.yes = 0;
            if (tally.no < 0)
                tally.no = 0;
            if (tally.abstain < 0)
                tally.abstain = 0;
            tallies.push_back(tally);
        }

        // Tally all votes across all users that voted on this proposal
        Tally finalTally;
        for (const auto & tally : tallies) {
            finalTally.yes += tally.yes;
            finalTally.no += tally.no;
            finalTally.abstain += tally.abstain;
            finalTally.cyes += tally.cyes;
            finalTally.cno += tally.cno;
            finalTally.cabstain += tally.cabstain;
        }
        return finalTally;
    }

    /**
     * List the expected superblock payees for the specified result set.
     * @param superblock
     * @param results
     * @param params
     * @return
     */
    static std::vector<CTxOut> getSuperblockPayees(const int & superblock, const std::map<Proposal, Tally> & results, const Consensus::Params & params) {
        std::vector<CTxOut> r;
        if (results.empty())
            return r;

        // Superblock payees are sorted in the following manner:
        // 1) Net "yes" votes
        // 2) if tied then by most "yes" votes
        // 3) if still tied then by block height proposal was created
        // 4) if still tied the code will probably self destruct
        std::vector<std::pair<Proposal, Tally>> props(results.begin(), results.end());
        std::sort(props.begin(), props.end(),
            [](const std::pair<Proposal, Tally> & a, const std::pair<Proposal, Tally> & b) {
                if (a.second.netyes() == b.second.netyes() && a.second.yes == b.second.yes)
                    return a.first.getBlockNumber() < b.first.getBlockNumber(); // proposal submission block number as tie breaker
                else if (a.second.netyes() == b.second.netyes())
                    return a.second.yes > b.second.yes; // use "yes" percent as tie breaker
                else
                    return a.second.netyes() > b.second.netyes(); // sort net yes votes descending
            });

        // Fill as many proposals into the payee list as possible.
        // Proposals that do not fit are skipped and the other
        // remaining proposals are filled in its place.
        std::map<CTxDestination, CAmount> payees;
        CAmount superblockTotal = std::min(params.proposalMaxAmount, params.GetBlockSubsidy(superblock, params));
        do {
            // Add the payee if the requested amount fits
            // in the superblock.
            const auto & result = props[0];
            if (superblockTotal - result.first.getAmount() >= 0) {
                superblockTotal -= result.first.getAmount();
                r.emplace_back(result.first.getAmount(), GetScriptForDestination(DecodeDestination(result.first.getAddress())));
            }
            // done with this result
            props.erase(props.begin());
        } while (!props.empty());

        return r;
    }

protected:
    void BlockConnected(const std::shared_ptr<const CBlock>& block, const CBlockIndex* pindex,
                        const std::vector<CTransactionRef>& txn_conflicted) override {
        const auto & params = Params().GetConsensus();
        if (pindex->nHeight < params.governanceBlock)
            return;
        processBlock(block.get(), pindex->nHeight, params);
        db->BlockConnected(block, pindex, txn_conflicted);
    }

    void BlockDisconnected(const std::shared_ptr<const CBlock>& block) override {
        const auto & params = Params().GetConsensus();
        // By default use max int for block height in case no
        // index is found we don't want to mark votes as unspent
        // if an accurate spent height can't be verified.
        const auto maxInt = std::numeric_limits<int>::max();
        int blockHeight{maxInt};
        {
            LOCK(cs_main);
            const auto pindex = LookupBlockIndex(block->GetHash());
            blockHeight = pindex->nHeight;
        }
        if (blockHeight < params.governanceBlock)
            return;

        // Update db
        db->BlockDisconnected(block);

        std::set<Proposal> ps;
        std::set<Vote> vs;
        std::map<uint256, std::set<VinHash>> vh;
        dataFromBlock(block.get(), ps, vs, vh, params, blockHeight);

        std::unordered_set<uint256, Hasher> vouts;
        for (const auto & tx : block->vtx)
            vouts.insert(tx->GetHash());

        // Remove votes added by this block
        for (auto vote : vs) {
            const auto & voteHash = vote.getHash();
            Vote stvote;
            {
                LOCK(mu);
                if (!votes.count(voteHash))
                    continue;
                stvote = votes[voteHash];
            }

            // If the vote utxo matches a vout in this block then remove the vote
            // only if the vote's signature matches the known vote's pubkey.
            auto vhashes = vh.find(voteHash);
            if (vouts.count(vote.getUtxo().hash)
                && stvote.isValidVinHash(vhashes->second) // known vote must match vin hash in associated tx
                && stvote.getPubKey().GetID() == vote.getPubKey().GetID()) // derived pubkeys must match
            {
                LOCK(mu);
                removeVote(vote);
                continue;
            }

            // At this point a vote's utxo can't be a vout in this block (handled above)
            // Make sure the vote is valid before removal to prevent the potential for
            // bad votes in disconnected blocks from being used to remove valid votes.
            if (!vote.loadVoteUTXO())
                continue;
            if (!vote.isValid(vhashes->second, params))
                continue;
            if (stvote.getBlockNumber() == blockHeight) {
                LOCK(mu);
                removeVote(vote);
                continue;
            }
        }

        // Remove proposals after votes because vote removal depends on an existing proposal
        for (const auto & proposal : ps) {
            LOCK(mu);
            if (!proposals.count(proposal.getHash()) || !proposal.isValid(params))
                continue;
            const auto & stprop = proposals[proposal.getHash()];
            if (stprop.getBlockNumber() == blockHeight)
                removeProposal(proposal);
        }

        if (blockHeight == maxInt)
            return; // do not unspend votes if block height is undefined

        // Unspend any vote utxos that were spent by this
        // block. Only unspend those votes where the block
        // index that tried to spend them was prior to
        // the proposal's superblock.
        std::map<COutPoint, uint256> prevouts; // map<outpoint, txhash>
        for (const auto & tx : block->vtx) {
            for (const auto & vin : tx->vin)
                prevouts[vin.prevout] = tx->GetHash();
        }

        // Get a list of all proposals with a superblock that is on or
        // after the current block index.
        auto sprops = getProposalsSince(blockHeight);
        // Obtain all votes for these proposals
        std::vector<Vote> svotes;
        for (const auto & p : sprops) {
            auto s = getVotes(p.getHash(), true);
            svotes.insert(svotes.end(), s.begin(), s.end());
        }
        // Unspend votes that match spent vins
        if (!svotes.empty()) {
            LOCK(mu);
            for (auto & v : svotes) {
                if (!prevouts.count(v.getUtxo()))
                    continue;
                // Unspend this vote if it was spent in this block
                unspendVote(v.getHash(), blockHeight, prevouts[v.getUtxo()]);
            }
        }
    }

    /**
     * Flush the governance db.
     * @param locator
     */
    void ChainStateFlushed(const CBlockLocator & locator) override {
        db->ChainStateFlushed(locator);
    }

    /**
     * Processes governance data from the specified block and index. Setting the processing chain tip flag will
     * result in contextually performing additional validation including proposal and vote cutoff period checks
     * and checking whether votes have been spent.
     * @param block
     * @param blockHeight
     * @param processingChainTip
     */
    void processBlock(const CBlock *block, const int blockHeight, const Consensus::Params & params, const bool processingChainTip = true) {
        std::set<Proposal> ps;
        std::set<Vote> vs;
        std::map<uint256,std::set<VinHash>> vh;
        dataFromBlock(block, ps, vs, vh, params, blockHeight);
        filterDataFromBlock(ps, vs, vh, params, blockHeight, processingChainTip);

        // Add all filtered proposals and votes
        {
            LOCK(mu);
            for (const auto & p : ps)
                addProposal(p, processingChainTip);
            for (const auto & v : vs)
                addVote(v, processingChainTip);
            // If not processing tip or no votes, then no more to do
            if (!processingChainTip || votes.empty())
                return;
        }

        // This section requires chain tip processing flag to be set
        // (all proposals should be loaded at this point).

        // Mark votes as spent, i.e. any votes that have had their
        // utxos spent in this block. We'll store all the vin prevouts
        // and then check any votes that share those utxos to determine
        // if they've been spent. Only mark votes as spent if the vote's
        // utxo is spent before the proposal expires (on its superblock).
        std::map<COutPoint, uint256> prevouts; // map<outpoint, txhash>
        for (const auto & tx : block->vtx) {
            for (const auto & vin : tx->vin)
                prevouts[vin.prevout] = tx->GetHash();
        }
        // Get a list of all proposals with a superblock that is on or
        // after the current block index.
        auto sprops = getProposalsSince(blockHeight);
        // Obtain all votes for these proposals
        std::vector<Vote> svotes;
        for (const auto & p : sprops) {
            auto s = getVotes(p.getHash(), false);
            svotes.insert(svotes.end(), s.begin(), s.end());
        }
        // Spend votes that match spent vins
        if (!svotes.empty()) {
            LOCK(mu);
            for (auto & v : svotes) {
                if (!prevouts.count(v.getUtxo()))
                    continue;
                // Only mark the vote as spent if it happens before or on its
                // proposal's superblock.
                spendVote(v.getHash(), blockHeight, prevouts[v.getUtxo()], processingChainTip);
            }
        }
    }

    /**
     * Records a vote, requires the proposal to be known.
     * @param vote
     * @param savedb Write to db
     */
    void addVote(const Vote & vote, bool savedb=true) EXCLUSIVE_LOCKS_REQUIRED(mu) {
        if (!proposals.count(vote.getProposal()))
            return;

        const auto & voteHash = vote.getHash();
        stackvotes[voteHash].push_back(vote);
        votes[voteHash] = vote; // add to votes data provider

        const auto & proposal = proposals[vote.getProposal()];
        auto & vs = sbvotes[proposal.getSuperblock()];
        vs[voteHash] = vote;

        if (savedb)
            db->AddVote(CDiskVote(vote));
    }

    /**
     * Removes and erases the specified vote from data providers.
     * @param vote
     * @param force Remove all vote history regardless of prior state
     * @param savedb Write to db
     */
    void removeVote(const Vote & vote, const bool & force=false, bool savedb=true) EXCLUSIVE_LOCKS_REQUIRED(mu) {
        const auto & voteHash = vote.getHash();
        if (!votes.count(voteHash))
            return;

        // Remove from votes data provider. If force is set, remove all vote history
        // under all circumstances. Useful for initial blockchain load.
        if (!force) {
            stackvotes[voteHash].pop_back();
            if (stackvotes[voteHash].empty()) {
                stackvotes.erase(voteHash);
                votes.erase(voteHash);
            } else
                votes[voteHash] = stackvotes[voteHash].back();
        } else {
            stackvotes.erase(voteHash);
            votes.erase(voteHash);
        }

        // Erase from db
        if (savedb)
            db->RemoveVote(voteHash);

        if (!proposals.count(vote.getProposal()))
            return;

        const auto & proposal = proposals[vote.getProposal()];
        if (!sbvotes.count(proposal.getSuperblock()))
            return; // no votes found for superblock, skip

        auto & vs = sbvotes[proposal.getSuperblock()];
        if (!vs.count(voteHash))
            return;
        // Remove from superblock votes data provider
        if (!stackvotes.count(voteHash))
            vs.erase(voteHash);
        else
            vs[voteHash] = stackvotes[voteHash].back();
    }

    /**
     * Removes and erases the specified votes in the cutoff period. This will effectively
     * remove all "changed votes" during the period. This will return true if all votes
     * were removed (including changed votes).
     * @param vote
     * @param consensus
     * @return
     */
    bool removeVotesInCutoff(Vote & vote, const Consensus::Params & consensus) EXCLUSIVE_LOCKS_REQUIRED(mu) {
        const auto & voteHash = vote.getHash();
        if (!votes.count(voteHash))
            return true;
        if (!proposals.count(vote.getProposal()))
            return true;

        const auto & proposal = proposals[vote.getProposal()];
        while (!outsideVotingCutoff(proposal, vote.getBlockNumber(), consensus)) {
            // Remove from votes data provider
            stackvotes[voteHash].pop_back();
            if (stackvotes[voteHash].empty()) {
                stackvotes.erase(voteHash);
                votes.erase(voteHash);
            } else
                votes[voteHash] = stackvotes[voteHash].back();

            if (!sbvotes.count(proposal.getSuperblock()))
                return true; // no votes found for superblock, skip

            auto & vs = sbvotes[proposal.getSuperblock()];
            if (!vs.count(voteHash))
                return true;
            // Remove from superblock votes data provider
            if (!stackvotes.count(voteHash)) {
                vs.erase(voteHash);
                return true;
            }

            vs[voteHash] = stackvotes[voteHash].back();
            vote = stackvotes[voteHash].back();
        }

        return false;
    }

    /**
     * Adds the proposal
     * @param proposal
     * @param savedb Write to db
     */
    void addProposal(const Proposal & proposal, bool savedb=true) EXCLUSIVE_LOCKS_REQUIRED(mu) {
        if (proposals.count(proposal.getHash()))
            return; // do not overwrite existing proposals
        proposals[proposal.getHash()] = proposal;
        if (savedb)
            db->AddProposal(CDiskProposal(proposal));
    }

    /**
     * Removes the proposal
     * @param proposal
     * @param savedb Write to db
     */
    void removeProposal(const Proposal & proposal, bool savedb=true) EXCLUSIVE_LOCKS_REQUIRED(mu) {
        const auto hash = proposal.getHash();
        proposals.erase(hash);
        if (savedb)
            db->RemoveProposal(hash);
    }

protected:
    Mutex mu;
    std::unordered_map<uint256, Proposal, Hasher> proposals GUARDED_BY(mu);
    std::unordered_map<uint256, Vote, Hasher> votes GUARDED_BY(mu);
    std::unordered_map<uint256, std::vector<Vote>, Hasher> stackvotes GUARDED_BY(mu);
    std::unordered_map<int, std::unordered_map<uint256, Vote, Hasher>> sbvotes GUARDED_BY(mu);
    std::unique_ptr<GovernanceDB> db;
};

}

#endif //BLOCKNET_GOVERNANCE_GOVERNANCE_H
