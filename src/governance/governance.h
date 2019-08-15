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
#include <script/standard.h>
#include <streams.h>
#include <uint256.h>
#include <util/moneystr.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/wallet.h>

#include <regex>
#include <string>
#include <utility>

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
    explicit Proposal() = default;
    Proposal& operator=(const Proposal & other) = default;
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
     * @return
     */
    bool isValid(const Consensus::Params & params) const {
        static std::regex rrname("^\\w+[\\w- ]*\\w+$");
        bool valid = std::regex_match(name, rrname) && (superblock % params.superblock == 0)
                     && amount >= params.proposalMinAmount && amount <= params.GetBlockSubsidy(superblock, params)
                     && IsValidDestination(DecodeDestination(address))
                     && type == PROPOSAL && version == NETWORK_VERSION;
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << version << type << name << superblock << amount << address << url << description;
        return valid && ss.size() <= MAX_OP_RETURN_RELAY-3; // -1 for OP_RETURN -2 for pushdata opcodes
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
    explicit Vote() = default;
    Vote& operator=(const Vote & other) = default;
    friend inline bool operator==(const Vote & a, const Vote & b) { return a.getHash() == b.getHash(); }
    friend inline bool operator!=(const Vote & a, const Vote & b) { return !(a.getHash() == b.getHash()); }
    friend inline bool operator<(const Vote & a, const Vote & b) { return a.getProposal() < b.getProposal(); }

    /**
     * Null check
     * @return
     */
    bool isNull() {
        return utxo.IsNull();
    }

    /**
     * Valid if the proposal properties are correct.
     * @param params
     * @return
     */
    bool isValid() const {
        return version == NETWORK_VERSION && isValidVoteType(vote) && pubkey.IsFullyValid()
                       && pubkey.Verify(sigHash(), signature);
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
        ss << version << type << proposal << vote << utxo << pubkey << signature;
        return ss.GetHash();
    }

    /**
     * Proposal signature hash
     * @return
     */
    uint256 sigHash() const {
        CHashWriter ss(SER_GETHASH, 0);
        ss << version << type << proposal << vote << utxo << pubkey;
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
    CPubKey pubkey;
    COutPoint utxo;
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
     * Cast a vote on the specified proposal.
     * @param proposals
     * @param params
     * @param tx Transaction containing proposal votes
     * @return
     */
    static bool submitVotes(const std::vector<ProposalVote> & proposals, const Consensus::Params & params, CTransactionRef & tx) {
        // TODO Blocknet implement
        return false;
    }

    /**
     * Submits a proposal to the network and returns true. If there's an issue with the proposal or it's
     * not valid false is returned.
     * @param proposal
     * @param params
     * @param tx Transaction containing proposal submission
     * @return
     */
    static bool submitProposal(const Proposal & proposal, const Consensus::Params & params, CTransactionRef & tx) {
        if (!proposal.isValid(params))
            return error("Proposal is not valid"); // TODO Blocknet indicate what isn't valid

        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << proposal;

        std::string strAddress = gArgs.GetArg("-proposaladdress", "");
        bool proposalAddressSpecified = !strAddress.empty();

        CTxDestination address;
        if (proposalAddressSpecified) {
            if (!IsValidDestinationString(strAddress))
                return error("Bad proposal address specified in 'proposaladdress' config option. Make sure it's a valid legacy address");
            address = DecodeDestination(strAddress);
            CScript s = GetScriptForDestination(address);
            std::vector<std::vector<unsigned char> > solutions;
            if (Solver(s, solutions) != TX_PUBKEYHASH)
                return error("Bad proposal address specified in 'proposaladdress' config option. Only p2pkh (pay-to-pubkey-hash) addresses are accepted");
        }

        bool send{false};
        auto wallets = GetWallets();
        for (auto & wallet : wallets) {
            auto locked_chain = wallet->chain().lock();
            LOCK(wallet->cs_wallet);

            const auto & balance = wallet->GetAvailableBalance();
            if (balance <= params.proposalFee || wallet->IsLocked())
                continue;

            if (wallet->GetBroadcastTransactions() && !g_connman)
                return error("Peer-to-peer functionality missing or disabled");

            CCoinControl cc;
            if (proposalAddressSpecified) { // if a specific proposal address was specified, only spend from that address
                cc.destChange = address;

                // Sort coins ascending to use up all the undesirable utxos
                std::vector<COutput> coins;
                wallet->AvailableCoins(*locked_chain, coins, true);
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
            }

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
                if (totalAmount > balance)
                    return error("This transaction requires a transaction fee of at least %s: %s", FormatMoney(nFeeRequired), strError);
                return error("Failed to create the proposal submission transaction: %s", strError);
            }

            CValidationState state;
            if (!wallet->CommitTransaction(tx, {}, {}, reservekey, g_connman.get(), state))
                return error("Failed to create the proposal submission transaction, it was rejected: %s", FormatStateMessage(state));

            send = true;
            break; // done
        }

        if (!send)
            return error("Failed to create proposal, check that your wallet is unlocked with a balance of at least %s", FormatMoney(params.proposalFee));

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
                    if (vote.isValid()) {
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
                    if (vote.isValid()) {
                        LOCK(mu);
                        votes.erase(vote.getHash());
                    }
                }
            }
        }
    }

protected:
    std::map<uint256, Proposal> proposals;
    std::map<uint256, Vote> votes;
    Mutex mu;
};

}

#endif //BLOCKNET_GOVERNANCE_H
