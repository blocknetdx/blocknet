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

namespace gov {

enum ProposalType : uint8_t {
    NONE        = 0,
    DEFAULT     = 1,
};

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
                     && IsValidDestination(DecodeDestination(address));
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << type << name << superblock << amount << address << url << description;
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
        ss << type << name << superblock << amount << address << url << description;
        return ss.GetHash();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(type);
        READWRITE(superblock);
        READWRITE(amount);
        READWRITE(address);
        READWRITE(name);
        READWRITE(url);
        READWRITE(description);
    }

protected:
    uint8_t type{DEFAULT};
    std::string name;
    int superblock{0};
    CAmount amount{0};
    std::string address;
    std::string url;
    std::string description;
};

/**
 * Manages related servicenode functions including handling network messages and storing an active list
 * of valid servicenodes.
 */
class Governance {
public:
    Governance() = default;

    /**
     * Singleton instance.
     * @return
     */
    static Governance & instance() {
        static Governance gov;
        return gov;
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
            mapValue_t mapValue;
            if (!wallet->CommitTransaction(tx, std::move(mapValue), {}, reservekey, g_connman.get(), state))
                return error("Failed to create the proposal submission transaction, it was rejected: %s", FormatStateMessage(state));

            send = true;
            break; // done
        }

        if (!send)
            return error("Failed to create proposal, check that your wallet is unlocked with a balance of at least %s", FormatMoney(params.proposalFee));

        return true;
    }
};

}

#endif //BLOCKNET_GOVERNANCE_H
