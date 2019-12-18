// Copyright (c) 2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <governance/governancewallet.h>

#include <wallet/coincontrol.h>
#include <wallet/fees.h>

namespace gov {

std::tuple<int, VoteType, bool, CAmount> GetMyVotes(const uint256 & hash, CCoinsViewCache *coinsTip,
        std::vector<std::shared_ptr<CWallet>> & wallets, const Consensus::Params & consensus)
{
    auto copyVotes = Governance::instance().copyVotes();
    CAmount voteAmount{0};
    VoteType vtype{ABSTAIN};
    for (const auto & item : copyVotes) {
        const auto & vote = item.second;
        if (vote.getProposal() == hash && !vote.spent()) {
            const auto & utxo = vote.getUtxo();
            for (auto & w : wallets) {
                if (w->HaveKey(vote.getKeyID())) {
                    vtype = vote.getVote();
                    voteAmount += vote.getAmount();
                    break;
                }
            }
        }
    }

    bool voted{false};
    int voteCount{0};
    if (voteAmount > 0) {
        voted = true;
        voteCount = voteAmount/consensus.voteBalance;
    }
    return std::make_tuple(voteCount, vtype, voted, voteAmount);
}

bool SubmitProposal(const Proposal & proposal, const std::vector<std::shared_ptr<CWallet>> & wallets,
                    const Consensus::Params & params, CTransactionRef & tx, CConnman *connman,
                    std::string *failReasonRet)
{
    if (!proposal.isValid(params, failReasonRet))
        return error(failReasonRet->c_str());

    if (Governance::instance().hasProposal(proposal.getHash())
       || Governance::instance().hasProposal(proposal.getName(), proposal.getSuperblock()))
    {
        *failReasonRet = strprintf("Proposal %s scheduled for superblock %d was already submitted with hash %s",
                proposal.getName(), proposal.getSuperblock(), proposal.getHash().ToString());
        return error(failReasonRet->c_str());
    }

    CDataStream ss(SER_NETWORK, GOV_PROTOCOL_VERSION);
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

        if (wallet->GetBroadcastTransactions() && !connman) {
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
                if (!(dest == address))
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
        if (!wallet->CommitTransaction(tx, {}, {}, reservekey, connman, state)) {
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

bool SubmitVotes(const std::vector<ProposalVote> & proposalVotes, const std::vector<std::shared_ptr<CWallet>> & wallets,
                 const Consensus::Params & params, std::vector<CTransactionRef> & txsRet, CConnman *connman,
                 std::string *failReasonRet)
{
    if (proposalVotes.empty())
        return false; // no proposals specified, reject

    for (const auto & pv : proposalVotes) { // check if any proposals are invalid
        if (!pv.proposal.isValid(params)) {
            *failReasonRet = strprintf("Failed to vote on proposal (%s) because it's invalid", pv.proposal.getName());
            return error(failReasonRet->c_str());
        }
    }

    txsRet.clear(); // prep tx result
    CAmount totalBalance{0};

    // Make sure wallets are available
    if (wallets.empty()) {
        *failReasonRet = "No wallets were found";
        return error(failReasonRet->c_str());
    }

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

    // Store all voting transactions counter
    int txCounter{0};

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
            // Store the inputs in use for this round of votes. It's possible that there
            // are more votes than a single tx allows, as a result, only use the inputs
            // associated with the votes being used in this tx.
            std::map<CKeyID, const COutput*> inputsInUse;

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

                const auto & addr = boost::get<CKeyID>(dest);

                CKey key; // utxo private key
                {
                    const auto keyid = GetKeyForDestination(*wallet, dest);
                    if (keyid.IsNull())
                        continue;
                    if (!wallet->GetKey(keyid, key))
                        continue;
                }

                for (int j = 0; j < static_cast<int>(proposalVotes.size()); ++j) {
                    const auto & pv = proposalVotes[j];
                    const bool utxoAlreadyUsed = usedUtxos.count(coin.GetInputCoin().outpoint) > 0 &&
                                                 usedUtxos[coin.GetInputCoin().outpoint].count(pv.proposal.getHash()) > 0;
                    if (utxoAlreadyUsed)
                        continue;
                    const bool alreadyVoted = Governance::instance().hasVote(pv.proposal.getHash(), pv.vote, coin.GetInputCoin().outpoint);
                    if (alreadyVoted)
                        continue; // skip, already voted

                    // Create and serialize the vote data and insert in OP_RETURN script. The vote
                    // is signed with the utxo that is representing that vote. The signing must
                    // happen before the vote object is serialized.
                    CDataStream ss(SER_NETWORK, GOV_PROTOCOL_VERSION);
                    Vote vote(pv.proposal.getHash(), pv.vote, coin.GetInputCoin().outpoint,
                            makeVinHash(inputCoins[addr]->GetInputCoin().outpoint));
                    if (!vote.sign(key)) {
                        LogPrint(BCLog::GOVERNANCE, "WARNING: Failed to vote on {%s} proposal, utxo signing failed %s\n", pv.proposal.getName(), coin.GetInputCoin().outpoint.ToString());
                        continue;
                    }
                    if (!vote.isValid(params)) { // validate vote
                        LogPrint(BCLog::GOVERNANCE, "WARNING: Failed to vote on {%s} proposal, validation failed\n", pv.proposal.getName());
                        continue;
                    }
                    ss << vote;
                    voteOuts.push_back({CScript() << OP_RETURN << ToByteVector(ss), 0, false});

                    // Track inputs
                    if (!inputsInUse.count(addr))
                        inputsInUse[addr] = inputCoins[addr];

                    // Track utxos that already voted on this proposal
                    usedUtxos[coin.GetInputCoin().outpoint].insert(pv.proposal.getHash());

                    // Track whether we're on the last vote, used to break out while loop
                    completelyDone = (i == filtered.size() - 1 && j == proposalVotes.size() - 1);

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

            if (voteOuts.empty()) // Handle case where no votes were produced
                break;

            // Select the inputs for use with the transaction. Also add separate outputs to pay
            // back the vote inputs to their own addresses as change (requires estimating fees).
            CCoinControl cc;
            cc.fAllowOtherInputs = false;
            cc.destChange = CTxDestination(inputsInUse.begin()->first); // pay change to the first input coin
            FeeCalculation feeCalc;
            const auto feeBytes = static_cast<unsigned int>(inputsInUse.size()*175) + // TODO Blocknet accurate input size estimation required
                                  static_cast<unsigned int>(voteOuts.size()*(MAX_OP_RETURN_RELAY+75));
            CAmount payFee = GetMinimumFee(*wallet, feeBytes, cc, ::mempool, ::feeEstimator, &feeCalc);
            CAmount estimatedFeePerInput = payFee/static_cast<CAmount>(inputsInUse.size());

            // Select inputs and distribute fees equally across the change addresses (paid back to input addresses minus fee)
            for (const auto & inputItem : inputsInUse) {
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
            if (wallet->GetBroadcastTransactions() && !connman) {
                *failReasonRet = "Peer-to-peer functionality missing or disabled";
                return error(failReasonRet->c_str());
            }

            CValidationState state;
            if (!wallet->CommitTransaction(tx, {}, {}, reservekey, connman, state)) {
                *failReasonRet = strprintf("Failed to create the proposal submission transaction, it was rejected: %s", FormatStateMessage(state));
                return error(failReasonRet->c_str());
            }

            // Store the committed voting transaction
            txsRet.push_back(tx);
            // Clear vote outs
            voteOuts.clear();
            // Increment vote transaction counter
            ++txCounter;

        } while(!completelyDone);
    }

    // If not voting transactions were created, return error
    if (txCounter == 0) {
        *failReasonRet = strprintf("Failed to submit votes, all unspent transaction inputs might already be in use");
        return error(failReasonRet->c_str());
    }

    return true;
}

}