// Copyright (c) 2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_GOVERNANCE_H
#define BLOCKNET_GOVERNANCE_H

#include <amount.h>
#include <consensus/params.h>
#include <consensus/validation.h>
#include <hash.h>
#include <key_io.h>
#include <net.h>
#include <policy/policy.h>
#include <script/standard.h>
#include <streams.h>
#include <txmempool.h>
#include <uint256.h>
#include <util/moneystr.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/fees.h>
#include <wallet/wallet.h>

#include <regex>
#include <string>
#include <utility>

#include <boost/algorithm/string.hpp>

/**
 * Governance namespace.
 */
namespace gov {

/**
 * Governance types are used with OP_RETURN to indicate how the messages should be processed.
 */
enum Type : uint8_t {
    NONE         = 0,
    PROPOSAL     = 1,
    VOTE         = 2,
};

static const uint8_t NETWORK_VERSION = 0x01;
static const CAmount VOTING_UTXO_INPUT_AMOUNT = 0.1 * COIN;

/**
 * Return the CKeyID for the specified utxo.
 * @param utxo
 * @param keyid
 * @return
 */
static bool GetKeyIDForUTXO(const COutPoint & utxo, CTransactionRef & tx, CKeyID & keyid) {
    uint256 hashBlock;
    if (!GetTransaction(utxo.hash, tx, Params().GetConsensus(), hashBlock))
        return false;
    if (utxo.n >= tx->vout.size())
        return false;
    CTxDestination dest;
    if (!ExtractDestination(tx->vout[utxo.n].scriptPubKey, dest))
        return false;
    keyid = *boost::get<CKeyID>(&dest);
    return true;
}

/**
 * Returns the next superblock from the most recent chain tip.
 * @param params
 * @return
 */
static int NextSuperblock(const Consensus::Params & params) {
    LOCK(cs_main);
    return chainActive.Height() - chainActive.Height() % params.superblock + params.superblock;
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
 * They can be created by anyone willing to pay the submission fee.
 */
class Proposal {
public:
    explicit Proposal(std::string name, int superblock, CAmount amount, std::string address,
                      std::string url, std::string description) : name(std::move(name)), superblock(superblock),
                                                                  amount(amount), address(std::move(address)), url(std::move(url)),
                                                                  description(std::move(description)) {}
    Proposal() = default;
    Proposal(const Proposal &) = default;
    Proposal& operator=(const Proposal &) = default;
    friend inline bool operator==(const Proposal & a, const Proposal & b) { return a.getHash() == b.getHash(); }
    friend inline bool operator!=(const Proposal & a, const Proposal & b) { return !(a.getHash() == b.getHash()); }
    friend inline bool operator<(const Proposal & a, const Proposal & b) { return a.getName().compare(b.getName()) < 0; }

    /**
     * Null check
     * @return
     */
    bool isNull() {
        return superblock == 0;
    }

    /**
     * Valid if the proposal properties are correct.
     * @param params
     * @param failureReasonRet
     * @return
     */
    bool isValid(const Consensus::Params & params, std::string *failureReasonRet=nullptr) const {
        static std::regex rrname("^\\w+[\\w- ]*\\w+$");
        if (!std::regex_match(name, rrname)) {
            if (failureReasonRet) *failureReasonRet = strprintf("Proposal name %s is invalid, only alpha-numeric characters are accepted", name);
            return false;
        }
        if (superblock % params.superblock != 0) {
            if (failureReasonRet) *failureReasonRet = strprintf("Bad superblock number, did you mean %d", gov::NextSuperblock(params));
            return false;
        }
        if (!(amount >= params.proposalMinAmount && amount <= params.GetBlockSubsidy(superblock, params))) {
            if (failureReasonRet) *failureReasonRet = strprintf("Bad proposal amount, specify amount between %s - %s", FormatMoney(params.proposalMinAmount), FormatMoney(params.proposalMaxAmount));
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
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << version << type << name << superblock << amount << address << url << description;
        const int maxBytes = MAX_OP_RETURN_RELAY-3; // -1 for OP_RETURN -2 for pushdata opcodes
        if (ss.size() > maxBytes) {
            if (failureReasonRet) *failureReasonRet = strprintf("Proposal data is too long, try reducing the description by %d characters, expected total of %d bytes, received %d", ss.size()-maxBytes, maxBytes, ss.size());
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
     * Proposal hash
     * @return
     */
    uint256 getHash() const {
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
public:
    explicit Vote(const uint256 & proposal, const VoteType & vote, const COutPoint & utxo) : proposal(proposal),
                                                                                             vote(vote),
                                                                                             utxo(utxo) {}

    Vote() = default;
    Vote(const Vote &) = default;
    Vote& operator=(const Vote &) = default;
    friend inline bool operator==(const Vote & a, const Vote & b) { return a.getHash() == b.getHash(); }
    friend inline bool operator!=(const Vote & a, const Vote & b) { return !(a.getHash() == b.getHash()); }
    friend inline bool operator<(const Vote & a, const Vote & b) { return a.getProposal() < b.getProposal(); }

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
        if (!(version == NETWORK_VERSION && isValidVoteType(vote)))
            return false;
        // Ensure the pubkey of the utxo matches the pubkey of the vote signature
        CTransactionRef tx;
        CKeyID keyid;
        if (!GetKeyIDForUTXO(utxo, tx, keyid))
            return false;
        if (tx->vout[utxo.n].nValue < params.voteMinUtxoAmount) // n bounds checked in GetKeyIDForUTXO
            return false;
        if (keyid.IsNull())
            return false;
        if (pubkey.GetID() != keyid)
            return false;
        { // Check that utxo isn't already spent
            LOCK(mempool.cs);
            CCoinsViewMemPool view(pcoinsTip.get(), mempool);
            Coin coin;
            if (!view.GetCoin(utxo, coin) || mempool.isSpent(utxo))
                return false;
        }
        return true;
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
     * Proposal vote
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
     * Proposal hash
     * @return
     */
    uint256 getHash() const {
        CHashWriter ss(SER_GETHASH, 0);
        ss << version << type << proposal << vote << utxo;
        return ss.GetHash();
    }

    /**
     * Proposal signature hash
     * @return
     */
    uint256 sigHash() const {
        CHashWriter ss(SER_GETHASH, 0);
        ss << version << type << proposal << vote << utxo;
        return ss.GetHash();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(version);
        READWRITE(type);
        READWRITE(proposal);
        READWRITE(vote);
        READWRITE(utxo);
        READWRITE(signature);
        if (ser_action.ForRead())
            pubkey.RecoverCompact(sigHash(), signature);
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
    std::vector<unsigned char> signature;
    COutPoint utxo;

private:
    CPubKey pubkey;
};

/**
 * ProposalVote associates a proposal with a specific vote.
 */
struct ProposalVote {
    Proposal proposal;
    VoteType vote;
};

/**
 * Manages related servicenode functions including handling network messages and storing an active list
 * of valid servicenodes.
 */
class Governance : public CValidationInterface {
public:
    explicit Governance() = default;

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
     * Returns true if the vote with the specified hash exists.
     * @param hash
     * @return
     */
    bool hasVote(const uint256 & hash) {
        LOCK(mu);
        return votes.count(hash) > 0;
    }

    /**
     * Returns true if the specified utxo matches a known vote.
     * @param utxo
     * @return
     */
    bool hasVote(const uint256 & proposal, const COutPoint & utxo) {
        LOCK(mu);
        for (const auto & item : votes) {
            const auto & vote = item.second;
            if (vote.getUtxo() == utxo && vote.getProposal() == proposal)
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
        return true;
    }

    /**
     * Loads the governance data from the past and current superblocks.
     * @return
     */
    bool loadGovernanceData(const CChain & chain, const Consensus::Params & consensus) {
        bool result{true};
        const int nextSuperblock = chain.Height() - chain.Height() % consensus.superblock + consensus.superblock;
        const int prevousSuperblock = nextSuperblock - consensus.superblock;
        const int beginningOflastSuperblock = prevousSuperblock == 0 // genesis check
                                                    ? 1
                                                    : prevousSuperblock - consensus.superblock + 1;
        for (int i = beginningOflastSuperblock; i <= chain.Height(); ++i) {
            const auto blockIndex = chain[i];
            CBlock block;
            if (!ReadBlockFromDisk(block, blockIndex, consensus)) {
                result = false;
                continue;
            }
            // Process blocks
            const auto sblock = std::make_shared<const CBlock>(block);
            BlockConnected(sblock, blockIndex, {});
        }
        return result;
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
        return std::move(Proposal{});
    }

    /**
     * Fetch the specified vote.
     * @param hash Vote hash
     * @return
     */
    Vote getVote(const uint256 & hash) {
        LOCK(mu);
        if (votes.count(hash) > 0)
            return votes[hash];
        return std::move(Vote{});
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
        return std::move(props);
    }

    /**
     * Fetch the list of all known votes.
     * @return
     */
    std::vector<Vote> getVotes() {
        LOCK(mu);
        std::vector<Vote> vos;
        vos.reserve(votes.size());
        for (const auto & item : votes)
            vos.push_back(item.second);
        return std::move(vos);
    }

public: // static

    /**
     * Singleton instance.
     * @return
     */
    static Governance & instance() {
        static Governance gov;
        return gov;
    }

    /**
     * Returns the upcoming superblock.
     * @param params
     * @return
     */
    static int nextSuperblock(const Consensus::Params & params) {
        return NextSuperblock(params);
    }

    /**
     * Cast votes on proposals.
     * @param proposals
     * @param params
     * @param txsRet List of transactions containing proposal votes
     * @param failReason Error message (empty if no error)
     * @return
     */
    static bool submitVotes(const std::vector<ProposalVote> & proposals, const Consensus::Params & params, std::vector<CTransactionRef> & txsRet, std::string *failReasonRet=nullptr) {
        if (proposals.empty())
            return false; // no proposals specified, reject

        for (const auto & pv : proposals) { // check if any proposals are invalid
            if (!pv.proposal.isValid(params)) {
                *failReasonRet = strprintf("Failed to vote on proposal (%s) because it's invalid", pv.proposal.getName());
                return error(failReasonRet->c_str());
            }
        }

        txsRet.clear(); // prep tx result
        CAmount totalBalance{0};
        auto wallets = GetWallets();

        // Make sure there's enough coin to cast a vote
        for (auto & wallet : wallets) {
            if (wallet->IsLocked()) {
                *failReasonRet = "All wallets must be unlocked to vote";
                return error(failReasonRet->c_str());
            }
            totalBalance += wallet->GetBalance();
        }
        if (totalBalance < params.voteBalance) {
            *failReasonRet = strprintf("Not enough coin to cast a vote, %s is required", FormatMoney(params.voteBalance));
            return error(failReasonRet->c_str());
        }

        // Create the transactions that will required to casts votes
        // An OP_RETURN is required for each UTXO casting a vote
        // towards each proposal. This may require multiple txns
        // to properly cast all votes across all proposals.
        //
        // A single input from each unique address is required to
        // prove ownership over the associated utxo. Each OP_RETURN
        // vote must contain the signature generated from the
        // associated utxo casting the vote.

        // Store all voting transactions
        std::map<CWallet*, std::vector<CTransactionRef>> txns;

        // Store the utxos that are associated with votes map<utxo, proposal hash>
        std::map<COutPoint, std::set<uint256>> usedUtxos;

        // Minimum vote input amount
        const auto voteMinAmount = static_cast<CAmount>(gArgs.GetArg("-voteinputamount", VOTING_UTXO_INPUT_AMOUNT));

        for (auto & wallet : wallets) {
            auto locked_chain = wallet->chain().lock();
            LOCK(wallet->cs_wallet);

            bool completelyDone{false}; // no votes left
            do {
                // Obtain all valid coin from this wallet that can be used in casting votes
                std::vector<COutput> coins;
                wallet->AvailableCoins(*locked_chain, coins);
                std::sort(coins.begin(), coins.end(), [](const COutput & out1, const COutput & out2) -> bool { // sort ascending (smallest first)
                    return out1.GetInputCoin().txout.nValue < out2.GetInputCoin().txout.nValue;
                });

                // Do not proceed if no inputs were found
                if (coins.empty())
                    break;

                // Filter the coins that meet the minimum requirement for utxo amount. These
                // inputs are used as the inputs to the vote transaction. Need one unique
                // input per address in the wallet that's being used in voting.
                std::map<CKeyID, const COutput*> inputCoins;

                // Select the coin set that meets the utxo amount requirements for use with
                // vote outputs in the tx.
                std::vector<COutput> filtered;
                for (const auto & coin : coins) {
                    if (!coin.fSpendable)
                        continue;
                    CTxDestination dest;
                    if (!ExtractDestination(coin.GetInputCoin().txout.scriptPubKey, dest))
                        continue;
                    // Input selection assumes "coins" is sorted ascending by nValue
                    const auto & addr = boost::get<CKeyID>(dest);
                    if (!inputCoins.count(addr) && coin.GetInputCoin().txout.nValue >= static_cast<CAmount>((double)voteMinAmount*0.6)) {
                        inputCoins[addr] = &coin; // store smallest coin meeting vote input amount requirement
                        continue; // do not use in the vote b/c it's being used in the input
                    }
                    if (coin.GetInputCoin().txout.nValue < params.voteMinUtxoAmount)
                        continue;
                    filtered.push_back(coin);
                }

                // Do not proceed if no coins or inputs were found
                if (filtered.empty() || inputCoins.empty())
                    break;

                // Store all the votes for each proposal across all participating utxos. Each
                // utxo can be used to vote towards each proposal.
                std::vector<CRecipient> voteOuts;

                bool doneWithPendingVotes{false}; // do we have any votes left

                // Create all votes, i.e. as many that will fit in a single transaction
                for (int i = 0; i < static_cast<int>(filtered.size()); ++i) {
                    const auto &coin = filtered[i];

                    CTxDestination dest;
                    if (!ExtractDestination(coin.GetInputCoin().txout.scriptPubKey, dest))
                        continue;
                    CKey key; // utxo private key
                    {
                        const auto keyid = GetKeyForDestination(*wallet, dest);
                        if (keyid.IsNull())
                            continue;
                        if (!wallet->GetKey(keyid, key))
                            continue;
                    }

                    for (int j = 0; j < static_cast<int>(proposals.size()); ++j) {
                        const auto & pv = proposals[j];
                        const bool utxoAlreadyUsed = usedUtxos.count(coin.GetInputCoin().outpoint) > 0 &&
                                usedUtxos[coin.GetInputCoin().outpoint].count(pv.proposal.getHash()) > 0;
                        const bool alreadyVoted = instance().hasVote(pv.proposal.getHash(), coin.GetInputCoin().outpoint);
                        if (utxoAlreadyUsed || alreadyVoted)
                            continue; // skip,  already voted

                        // Create and serialize the vote data and insert in OP_RETURN script. The vote
                        // is signed with the utxo that is representing that vote. The signing must
                        // happen before the vote object is serialized.
                        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
                        Vote vote(pv.proposal.getHash(), pv.vote, coin.GetInputCoin().outpoint);
                        if (!vote.sign(key)) {
                            LogPrint(BCLog::GOVERNANCE,
                                     "WARNING: Failed to vote on {%s} proposal, utxo signing failed %s",
                                     pv.proposal.getName(), coin.GetInputCoin().outpoint.ToString());
                            continue;
                        }
                        if (!vote.isValid(params)) { // validate vote
                            LogPrint(BCLog::GOVERNANCE, "WARNING: Failed to vote on {%s} proposal, validation failed",
                                     pv.proposal.getName());
                            continue;
                        }
                        ss << vote;
                        voteOuts.push_back({CScript() << OP_RETURN << ToByteVector(ss), 0, false});

                        // Track utxos that already voted on this proposal
                        usedUtxos[coin.GetInputCoin().outpoint].insert(pv.proposal.getHash());

                        // Track whether we're on the last vote, used to break out while(true)
                        completelyDone = (i == filtered.size() - 1 && j == proposals.size() - 1);

                        if (voteOuts.size() == MAX_OP_RETURN_IN_TRANSACTION) {
                            doneWithPendingVotes = !completelyDone;
                            if (doneWithPendingVotes)
                                break;
                        }
                    }

                    // Do not proceed iterating if we can't fit any more votes in the current transaction
                    if (doneWithPendingVotes)
                        break;
                }

                // At this point the code assumes that MAX_OP_RETURN_IN_TRANSACTION is reached
                // or that we've reached the last known vote (last item in all iterations)

                if (voteOuts.empty()) { // Handle case where no votes were produced
                    *failReasonRet = strprintf("Failed to submit votes, no votes were created, is the wallet unlocked and have sufficient funds? Funds required: %s", FormatMoney(params.voteBalance));
                    return error(failReasonRet->c_str());
                }

                // Select the inputs for use with the transaction. Also add separate outputs to pay
                // back the vote inputs to their own addresses as change (requires estimating fees).
                CCoinControl cc;
                cc.fAllowOtherInputs = false;
                cc.destChange = CTxDestination(inputCoins.begin()->first); // pay change to the first input coin
                FeeCalculation fee_calc;
                const auto feeBytes = static_cast<unsigned int>(inputCoins.size()*150) + // TODO Blocknet accurate input size estimation required
                                      static_cast<unsigned int>(voteOuts.size()*MAX_OP_RETURN_RELAY);
                CAmount payFee = GetMinimumFee(*wallet, feeBytes, cc, ::mempool, ::feeEstimator, &fee_calc);
                CAmount estimatedFeePerInput = payFee/(CAmount)inputCoins.size();

                // Select inputs and distribute fees equally across the change addresses (paid back to input addresses minus fee)
                for (const auto & inputItem : inputCoins) {
                    cc.Select(inputItem.second->GetInputCoin().outpoint);
                    voteOuts.push_back({GetScriptForDestination({inputItem.first}),
                                        inputItem.second->GetInputCoin().txout.nValue - estimatedFeePerInput,
                                        false});
                }

                // Create and send the transaction
                CReserveKey reservekey(wallet.get());
                CAmount nFeeRequired;
                std::string strError;
                int nChangePosRet = -1;
                CTransactionRef tx;
                if (!wallet->CreateTransaction(*locked_chain, voteOuts, tx, reservekey, nFeeRequired, nChangePosRet, strError, cc)) {
                    *failReasonRet = strprintf("Failed to create the proposal submission transaction: %s", strError);
                    return error(failReasonRet->c_str());
                }

                // Send all voting transaction to the network. If there's a failure
                // at any point in the process, bail out.
                if (wallet->GetBroadcastTransactions() && !g_connman) {
                    *failReasonRet = "Peer-to-peer functionality missing or disabled";
                    return error(failReasonRet->c_str());
                }

                CValidationState state;
                if (!wallet->CommitTransaction(tx, {}, {}, reservekey, g_connman.get(), state)) {
                    *failReasonRet = strprintf("Failed to create the proposal submission transaction, it was rejected: %s", FormatStateMessage(state));
                    return error(failReasonRet->c_str());
                }

                // Store the committed voting transaction
                txsRet.push_back(tx);
                // Clear vote outs
                voteOuts.clear();

            } while(!completelyDone);
        }

        return true;
    }

    /**
     * Submits a proposal to the network and returns true. If there's an issue with the proposal or it's
     * not valid false is returned.
     * @param proposal
     * @param params
     * @param tx Transaction containing proposal submission
     * @param failReasonRet Error message (empty if no error)
     * @return
     */
    static bool submitProposal(const Proposal & proposal, const Consensus::Params & params, CTransactionRef & tx, std::string *failReasonRet) {
        if (!proposal.isValid(params)) {
            *failReasonRet = "Proposal is not valid";
            return error(failReasonRet->c_str()); // TODO Blocknet indicate what isn't valid
        }

        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << proposal;

        std::string strAddress = gArgs.GetArg("-proposaladdress", "");
        bool proposalAddressSpecified = !strAddress.empty();

        CTxDestination address;
        if (proposalAddressSpecified) {
            if (!IsValidDestinationString(strAddress)) {
                *failReasonRet = "Bad proposal address specified in 'proposaladdress' config option. Make sure it's a valid legacy address";
                return error(failReasonRet->c_str());
            }
            address = DecodeDestination(strAddress);
            CScript s = GetScriptForDestination(address);
            std::vector<std::vector<unsigned char> > solutions;
            if (Solver(s, solutions) != TX_PUBKEYHASH) {
                *failReasonRet = "Bad proposal address specified in 'proposaladdress' config option. Only p2pkh (pay-to-pubkey-hash) addresses are accepted";
                return error(failReasonRet->c_str());
            }
        }

        bool send{false};
        auto wallets = GetWallets();

        // Iterate over all wallets and attempt to submit proposal fee transaction.
        // If a proposal address is specified via config option and the amount
        // doesn't meet the requirements, the proposal transaction will not be sent.
        // The first valid wallet that succeeds in creating a valid proposal tx
        // will be used. This does not support sending transactions with inputs
        // shared across multiple wallets.
        for (auto & wallet : wallets) {
            auto locked_chain = wallet->chain().lock();
            LOCK(wallet->cs_wallet);

            const auto & balance = wallet->GetAvailableBalance();
            if (balance <= params.proposalFee || wallet->IsLocked())
                continue;

            if (wallet->GetBroadcastTransactions() && !g_connman) {
                *failReasonRet = "Peer-to-peer functionality missing or disabled";
                return error(failReasonRet->c_str());
            }

            // Sort coins ascending to use up all the undesirable utxos
            std::vector<COutput> coins;
            wallet->AvailableCoins(*locked_chain, coins, true);
            if (coins.empty())
                continue;

            CCoinControl cc;
            if (proposalAddressSpecified) { // if a specific proposal address was specified, only spend from that address
                // Sort ascending
                std::sort(coins.begin(), coins.end(), [](const COutput & out1, const COutput & out2) -> bool {
                    return out1.GetInputCoin().txout.nValue < out2.GetInputCoin().txout.nValue;
                });

                CAmount selectedAmount{0};
                for (const COutput & out : coins) { // add coins to cover proposal fee
                    if (!out.fSpendable)
                        continue;
                    CTxDestination dest;
                    if (!ExtractDestination(out.GetInputCoin().txout.scriptPubKey, dest))
                        continue;
                    if (dest != address)
                        continue; // skip if address isn't proposal address
                    cc.Select(out.GetInputCoin().outpoint);
                    selectedAmount += out.GetInputCoin().txout.nValue;
                    if (selectedAmount > params.proposalFee)
                        break;
                }

                if (selectedAmount <= params.proposalFee)
                    continue; // bail out if not enough funds (need to account for network fee, i.e. > proposalFee)

            } else { // set change address to address of largest utxo
                std::sort(coins.begin(), coins.end(), [](const COutput & out1, const COutput & out2) -> bool {
                    return out1.GetInputCoin().txout.nValue > out2.GetInputCoin().txout.nValue; // Sort descending
                });
                for (const auto & coin : coins) {
                    if (ExtractDestination(coin.GetInputCoin().txout.scriptPubKey, address))
                        break;
                }
            }
            cc.destChange = address;

            // Create and send the transaction
            CReserveKey reservekey(wallet.get());
            CAmount nFeeRequired;
            std::string strError;
            std::vector<CRecipient> vecSend;
            int nChangePosRet = -1;
            CRecipient recipient = {CScript() << OP_RETURN << ToByteVector(ss), params.proposalFee, false};
            vecSend.push_back(recipient);
            if (!wallet->CreateTransaction(*locked_chain, vecSend, tx, reservekey, nFeeRequired, nChangePosRet, strError, cc)) {
                CAmount totalAmount = params.proposalFee + nFeeRequired;
                if (totalAmount > balance) {
                    *failReasonRet = strprintf("This transaction requires a transaction fee of at least %s: %s", FormatMoney(nFeeRequired), strError);
                    return error(failReasonRet->c_str());
                }
                return error("Failed to create the proposal submission transaction: %s", strError);
            }

            CValidationState state;
            if (!wallet->CommitTransaction(tx, {}, {}, reservekey, g_connman.get(), state)) {
                *failReasonRet = strprintf("Failed to create the proposal submission transaction, it was rejected: %s", FormatStateMessage(state));
                return error(failReasonRet->c_str());
            }

            send = true;
            break; // done
        }

        if (!send) {
            *failReasonRet = strprintf("Failed to create proposal, check that your wallet is unlocked with a balance of at least %s", FormatMoney(params.proposalFee));
            return error(failReasonRet->c_str());
        }

        return true;
    }

protected:
    void BlockConnected(const std::shared_ptr<const CBlock>& block, const CBlockIndex* pindex,
                        const std::vector<CTransactionRef>& txn_conflicted) override
    {
        for (const auto & tx : block->vtx) {
            for (const auto & out : tx->vout) {
                if (out.scriptPubKey[0] != OP_RETURN)
                    continue; // no proposal data
                CScript::const_iterator pc = out.scriptPubKey.begin();
                std::vector<unsigned char> data;
                while (pc < out.scriptPubKey.end()) {
                    opcodetype opcode;
                    if (!out.scriptPubKey.GetOp(pc, opcode, data))
                        break;
                    if (!data.empty())
                        break;
                }

                CDataStream ss(data, SER_NETWORK, PROTOCOL_VERSION);
                NetworkObject obj; ss >> obj;
                if (!obj.isValid())
                    continue; // must match expected version

                if (obj.getType() == PROPOSAL) {
                    CDataStream ss2(data, SER_NETWORK, PROTOCOL_VERSION);
                    Proposal proposal; ss2 >> proposal;
                    if (proposal.isValid(Params().GetConsensus())) {
                        LOCK(mu);
                        proposals[proposal.getHash()] = proposal;
                    }
                } else if (obj.getType() == VOTE) {
                    CDataStream ss2(data, SER_NETWORK, PROTOCOL_VERSION);
                    Vote vote; ss2 >> vote;
                    if (vote.isValid(Params().GetConsensus())) {
                        LOCK(mu);
                        votes[vote.getHash()] = vote;
                    }
                }
            }
        }
    }

    void BlockDisconnected(const std::shared_ptr<const CBlock>& block) override {
        for (const auto & tx : block->vtx) {
            for (const auto & out : tx->vout) {
                if (out.scriptPubKey[0] != OP_RETURN)
                    continue; // no proposal data

                CScript::const_iterator pc = out.scriptPubKey.begin();
                std::vector<unsigned char> data;
                while (pc < out.scriptPubKey.end()) {
                    opcodetype opcode;
                    if (!out.scriptPubKey.GetOp(pc, opcode, data))
                        break;
                    if (!data.empty())
                        break;
                }

                CDataStream ss(data, SER_NETWORK, PROTOCOL_VERSION);
                NetworkObject obj; ss >> obj;
                if (!obj.isValid())
                    continue; // must match expected version

                if (obj.getType() == PROPOSAL) {
                    CDataStream ss2(data, SER_NETWORK, PROTOCOL_VERSION);
                    Proposal proposal; ss2 >> proposal;
                    if (proposal.isValid(Params().GetConsensus())) {
                        LOCK(mu);
                        proposals.erase(proposal.getHash());
                    }
                } else if (obj.getType() == VOTE) {
                    CDataStream ss2(data, SER_NETWORK, PROTOCOL_VERSION);
                    Vote vote; ss2 >> vote;
                    if (vote.isValid(Params().GetConsensus())) {
                        LOCK(mu);
                        votes.erase(vote.getHash());
                    }
                }
            }
        }
    }

protected:
    Mutex mu;
    std::map<uint256, Proposal> proposals GUARDED_BY(mu);
    std::map<uint256, Vote> votes GUARDED_BY(mu);
};

}

#endif //BLOCKNET_GOVERNANCE_H
