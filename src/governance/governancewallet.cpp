// Copyright (c) 2019-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <governance/governancewallet.h>

#include <wallet/coincontrol.h>
#include <wallet/fees.h>

namespace gov {

struct VoteCoin {
    COutPoint outpoint;
    CAmount nValue;
    CScript scriptPubKey;
};

std::tuple<int, VoteType, bool, CAmount> GetMyVotes(const uint256 & proposalHash, std::vector<std::shared_ptr<CWallet>> & wallets,
        const Consensus::Params & consensus)
{
    auto copyVotes = Governance::instance().getVotes(proposalHash);
    CAmount voteAmount{0};
    VoteType vtype{ABSTAIN};
    for (const auto & vote : copyVotes) {
        const auto & utxo = vote.getUtxo();
        for (auto & w : wallets) {
            if (w->HaveKey(vote.getKeyID())) {
                vtype = vote.getVote();
                voteAmount += vote.getAmount();
                break;
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

    std::set<CTxDestination> allowedDestinations;
    for (const auto & pv : proposalVotes) { // check if any proposals are invalid
        if (!pv.proposal.isValid(params)) {
            *failReasonRet = strprintf("Failed to vote on proposal (%s) because it's invalid", pv.proposal.getName());
            return error(failReasonRet->c_str());
        }
        if (!boost::get<CNoDestination>(&pv.dest))
            allowedDestinations.insert(pv.dest); // only store if valid destination
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
        totalBalance += wallet->GetBalance() + wallet->GetImmatureBalance();
    }
    if (totalBalance <= params.voteBalance) {
        *failReasonRet = strprintf("Not enough coin to cast a vote, more than %s BLOCK is required, including a small "
                                   "voting input for vote validation and network fees (transaction fee for vote submission)",
                                   FormatMoney(params.voteBalance));
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

    // Desired vote input amount
    const auto desiredVoteInputAmt = static_cast<CAmount>(gArgs.GetArg("-voteinputamount", VOTING_UTXO_INPUT_AMOUNT));

    for (auto & wallet : wallets) {
        auto locked_chain = wallet->chain().lock();
        LOCK(wallet->cs_wallet);

        bool completelyDone{false}; // no votes left
        do {
            // Obtain all valid coin from this wallet that can be used in casting votes
            std::vector<COutput> iptcoins;
            wallet->AvailableCoins(*locked_chain, iptcoins, false);
            // sort ascending (smallest first)
            std::sort(iptcoins.begin(), iptcoins.end(), [](const COutput & out1, const COutput & out2) -> bool {
                return out1.GetInputCoin().txout.nValue < out2.GetInputCoin().txout.nValue;
            });

            // Do not proceed if no inputs were found
            if (iptcoins.empty())
                break;

            // Filter the coins that meet the minimum requirement for utxo amount. These
            // inputs are used as the inputs to the vote transaction. Need one unique
            // input per address in the wallet that's being used in voting.
            std::map<CKeyID, VoteCoin> inputCoins;
            // Store the inputs in use for this round of votes. It's possible that there
            // are more votes than a single tx allows, as a result, only use the inputs
            // associated with the votes being used in this tx.
            std::map<CKeyID, VoteCoin> inputsInUse;

            // Other input coins available for use in case we need more coins to cover fees
            std::vector<VoteCoin> otherInputCoins;

            // Select the coin set that meets the utxo amount requirements for use with
            // vote outputs in the tx.
            std::map<CKeyID, CAmount> inputDiffs;
            for (const auto & coin : iptcoins) {
                if (!coin.fSpendable)
                    continue;
                CTxDestination dest;
                if (!ExtractDestination(coin.GetInputCoin().txout.scriptPubKey, dest))
                    continue;
                if (!allowedDestinations.empty() && !allowedDestinations.count(dest))
                    continue; // skip coin from addresses that are not allowed
                const VoteCoin vcoin{coin.GetInputCoin().outpoint, coin.GetInputCoin().txout.nValue, coin.GetInputCoin().txout.scriptPubKey};
                // Add coin to other inputs for use to cover extra fees
                otherInputCoins.push_back(vcoin);
                // Find ideal input size
                const auto & addr = *boost::get<CKeyID>(&dest);
                auto inputDiff = std::numeric_limits<CAmount>::max();
                if (inputDiffs.count(addr))
                    inputDiff = inputDiffs[addr];
                // Find the coin closest to the desired vote input amount
                const auto cdiff = static_cast<CAmount>(llabs(vcoin.nValue - desiredVoteInputAmt));
                if (cdiff < inputDiff)  {
                    inputDiffs[addr] = cdiff;
                    inputCoins[addr] = vcoin;
                }
            }

            if (inputCoins.empty())
                break;

            // Select coins for use with voting
            std::vector<COutput> votingCoins;
            wallet->VotingCoins(*locked_chain, votingCoins, params.voteMinUtxoAmount);

            // Add voting utxos and filter out any ones being used as tx inputs
            std::vector<VoteCoin> filteredVotingCoins;
            std::unordered_map<CKeyID, CAmount, Hasher> tallyForEachAddress;
            for (const auto & coin : votingCoins) {
                CTxDestination dest;
                if (!ExtractDestination(coin.GetInputCoin().txout.scriptPubKey, dest))
                    continue;
                if (!allowedDestinations.empty() && !allowedDestinations.count(dest))
                    continue; // skip coin from addresses that are not allowed
                const VoteCoin vcoin{coin.GetInputCoin().outpoint, coin.GetInputCoin().txout.nValue, coin.GetInputCoin().txout.scriptPubKey};
                // Do not add vote utxos that are being used as vote inputs
                const auto & addr = *boost::get<CKeyID>(&dest);
                if (inputCoins.count(addr) && inputCoins[addr].outpoint != vcoin.outpoint) {
                    filteredVotingCoins.push_back(vcoin);
                    // Add up all valid vote utxo amounts
                    if (vcoin.nValue >= params.voteMinUtxoAmount)
                        tallyForEachAddress[addr] += vcoin.nValue;
                }
            }

            if (filteredVotingCoins.empty())
                break; // Do not proceed if no coins or inputs were found

            // Remove any coins from addresses that don't have more than minimum required vote balance
            auto removeCoin = [&tallyForEachAddress,params](const VoteCoin & coin) {
                CTxDestination dest;
                if (!ExtractDestination(coin.scriptPubKey, dest))
                    return true; // remove if address is bad
                const auto & addr = *boost::get<CKeyID>(&dest);
                const bool remove = tallyForEachAddress[addr] < params.voteBalance;
                return remove;
            };
            filteredVotingCoins.erase(std::remove_if(filteredVotingCoins.begin(), filteredVotingCoins.end(),removeCoin), filteredVotingCoins.end());
            otherInputCoins.erase(std::remove_if(otherInputCoins.begin(), otherInputCoins.end(), removeCoin), otherInputCoins.end());
            for (auto it = inputCoins.cbegin(); it != inputCoins.cend();) {
                CTxDestination dest;
                if (!ExtractDestination(it->second.scriptPubKey, dest)) {
                    inputCoins.erase(it++);
                    continue;
                }
                const auto & addr = *boost::get<CKeyID>(&dest);
                if (tallyForEachAddress[addr] < params.voteBalance)
                    inputCoins.erase(it++);
                else
                    ++it;
            }

            // Store all the votes for each proposal across all participating utxos. Each
            // utxo can be used to vote towards each proposal.
            std::vector<CRecipient> voteOuts;

            bool doneWithPendingVotes{false}; // do we have any votes left

            // Create all votes, i.e. as many that will fit in a single transaction
            for (int i = 0; i < static_cast<int>(filteredVotingCoins.size()); ++i) {
                const auto & coin = filteredVotingCoins[i];

                CTxDestination dest;
                if (!ExtractDestination(coin.scriptPubKey, dest))
                    continue;

                const auto & addr = *boost::get<CKeyID>(&dest);

                CKey key; // utxo private key
                {
                    const auto keyid = GetKeyForDestination(*wallet, dest);
                    if (keyid.IsNull())
                        continue;
                    if (!wallet->GetKey(keyid, key))
                        continue;
                }

                // Cast all votes
                for (int j = 0; j < static_cast<int>(proposalVotes.size()); ++j) {
                    const auto & pv = proposalVotes[j];
                    const bool utxoAlreadyUsed = usedUtxos.count(coin.outpoint) > 0 &&
                                                 usedUtxos[coin.outpoint].count(pv.proposal.getHash()) > 0;
                    if (utxoAlreadyUsed)
                        continue;
                    const bool alreadyVoted = Governance::instance().hasVote(pv.proposal.getHash(), pv.vote, coin.outpoint);
                    if (alreadyVoted)
                        continue; // skip, already voted

                    // Create and serialize the vote data and insert in OP_RETURN script. The vote
                    // is signed with the utxo that is representing that vote. The signing must
                    // happen before the vote object is serialized.
                    CDataStream ss(SER_NETWORK, GOV_PROTOCOL_VERSION);
                    Vote vote(pv.proposal.getHash(), pv.vote, coin.outpoint, makeVinHash(inputCoins[addr].outpoint));
                    if (!vote.sign(key)) {
                        LogPrint(BCLog::GOVERNANCE, "WARNING: Failed to vote on {%s} proposal, utxo signing failed %s\n", pv.proposal.getName(), coin.outpoint.ToString());
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
                    usedUtxos[coin.outpoint].insert(pv.proposal.getHash());

                    // Track whether we're on the last vote, used to break out while loop
                    completelyDone = (i == filteredVotingCoins.size() - 1 && j == proposalVotes.size() - 1);

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
            // or that we've reached the last known vote (last voting utxo in all iterations)

            if (voteOuts.empty()) // Handle case where no votes were produced
                break;

            // Exclude other inputs used in votes or used in selected vote input coins
            otherInputCoins.erase(std::remove_if(otherInputCoins.begin(), otherInputCoins.end(), [&usedUtxos,&inputsInUse](const VoteCoin & coin) -> bool {
                if (usedUtxos.count(coin.outpoint) > 0)
                    return true;
                for (const auto & c : inputsInUse) {
                    if (c.second.outpoint == coin.outpoint)
                        return true;
                }
                return false;
            }), otherInputCoins.end());
            // Sort other input coins ascending (smallest first)
            std::sort(otherInputCoins.begin(), otherInputCoins.end(), [](const VoteCoin & coin1, const VoteCoin & coin2) -> bool {
                return coin1.nValue < coin2.nValue;
            });

            // Select the inputs for use with the transaction. Also add separate outputs to pay
            // back the vote inputs to their own addresses as change (requires estimating fees).
            CCoinControl cc;
            cc.fAllowOtherInputs = false;
            cc.destChange = CTxDestination(inputsInUse.begin()->first); // pay change to the first input coin
            FeeCalculation feeCalc;
            // TODO Blocknet accurate input size estimation required
            const auto feeBytes = static_cast<unsigned int>(inputsInUse.size()*180) // inputs (~180 bytes)
                                  + static_cast<unsigned int>(voteOuts.size()*(MAX_OP_RETURN_RELAY+75)) // vote outs (~235 bytes)
                                  + static_cast<unsigned int>(inputsInUse.size()*50); // change, 1 per input (~50 bytes)
            CAmount payFee = GetMinimumFee(*wallet, feeBytes, cc, ::mempool, ::feeEstimator, &feeCalc);
            CAmount estimatedFeePerInput = payFee/static_cast<CAmount>(inputsInUse.size());

            // Add up input amounts total
            CAmount inputsTotal{0};

            // Coin control select inputs
            for (const auto & inputItem : inputsInUse) {
                cc.Select(inputItem.second.outpoint); // make sure coincontrol uses our vote inputs
                inputsTotal += inputItem.second.nValue;

                // Distribute fees equally across the change addresses (paid back to input addresses minus fee)
                if (CTxDestination(inputItem.first) != cc.destChange) { // let coin control handle change to default addr
                    const auto changeAmt = inputItem.second.nValue - estimatedFeePerInput;
                    const auto script = GetScriptForDestination({inputItem.first});
                    if (!IsDust(CTxOut(changeAmt, script), ::dustRelayFee))
                        voteOuts.push_back({script, changeAmt, false});
                }
            }

            // Do not create voting transaction if inputs do not cover fees
            if (inputsTotal < payFee) {
                // Add additional other inputs to cover fee if necessary
                for (const auto & coin : otherInputCoins) {
                    if (inputsTotal >= payFee)
                        break;
                    cc.Select(coin.outpoint);
                    inputsTotal += coin.nValue;
                }
                // If we're still under the required amount to cover fees, then abort with error
                if (inputsTotal < payFee) {
                    std::string reason = strprintf("Voting inputs do not cover fees: received %s, require %s", FormatMoney(inputsTotal), FormatMoney(payFee));
                    *failReasonRet = strprintf("Failed to create the vote submission transaction: %s", reason);
                    return error(failReasonRet->c_str());
                }
            }

            // Create and send the transaction
            CReserveKey reservekey(wallet.get());
            CAmount nFeeRequired;
            std::string strError;
            int nChangePosRet = -1;
            CTransactionRef tx;
            if (!wallet->CreateTransaction(*locked_chain, voteOuts, tx, reservekey, nFeeRequired, nChangePosRet, strError, cc)) {
                *failReasonRet = strprintf("Failed to create the vote submission transaction: %s", strError);
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
                *failReasonRet = strprintf("Failed to create the vote submission transaction, it was rejected: %s", FormatStateMessage(state));
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

bool RevoteOnStake(const int & stakedHeight, const COutPoint & utxo, const CKey & key, const std::pair<CTxOut,COutPoint> & stakeUtxo,
        CWallet *wallet, CTransactionRef & tx, const Consensus::Params & params)
{
    auto props = Governance::instance().getProposals(); // copy
    std::vector<std::pair<Proposal, Vote>> selprops;
    // Find all proposals in current or future superblock that have votes that
    // match the staked utxo
    for (auto & ps : props) {
        // Are we in a current or future proposal
        if (ps.getSuperblock() > stakedHeight && !Governance::insideVoteCutoff(ps.getSuperblock(), stakedHeight, params)) {
            auto vvs = Governance::instance().getVotes(ps.getHash()); // copy
            for (const auto & vs : vvs) {
                if (vs.getUtxo() == utxo && ps.isValid(params)) {
                    selprops.emplace_back(ps, vs);
                    break;
                }
            }
        }
    }

    if (selprops.empty())
        return false; // no votes to change

    // Find vote input that matches the staking address
    std::vector<std::pair<CTxOut,COutPoint>> coins;
    std::vector<std::pair<CTxOut,COutPoint>> otherCoins;
    {
        auto locked_chain = wallet->chain().lock();
        LOCK(wallet->cs_wallet);

        // Obtain all valid coin from this wallet that can be used in casting votes
        std::vector<COutput> cs;
        wallet->AvailableCoins(*locked_chain, cs);

        // Do not proceed if not enough inputs were found (expecting at least 2, one for vote input, one for vote utxo)
        if (cs.empty() || cs.size() < 2)
            return false;

        for (auto & c : cs) {
            if (c.GetInputCoin().outpoint == utxo) // if coin matches utxo of staked input, skip
                continue;
            CTxDestination dest;
            if (!ExtractDestination(c.GetInputCoin().txout.scriptPubKey, dest))
                continue;
            auto keyid = *boost::get<CKeyID>(&dest);
            if (keyid == key.GetPubKey().GetID()) // this dest matches keyid of staking input
                coins.emplace_back(c.GetInputCoin().txout, c.GetInputCoin().outpoint);
            else
                otherCoins.emplace_back(c.GetInputCoin().txout, c.GetInputCoin().outpoint);
        }
    }

    // Need 1 coin for vote input
    if (coins.empty())
        return false; // not enough inputs!

    // sort ascending (smallest first)
    std::sort(coins.begin(), coins.end(),
        [](const std::pair<CTxOut,COutPoint> & out1, const std::pair<CTxOut,COutPoint> & out2) -> bool {
            return out1.first.nValue < out2.first.nValue;
        });
    std::sort(otherCoins.begin(), otherCoins.end(),
        [](const std::pair<CTxOut,COutPoint> & out1, const std::pair<CTxOut,COutPoint> & out2) -> bool {
            return out1.first.nValue < out2.first.nValue;
        });

    // Desired vote input amount
    const auto voteInputDesiredAmount = static_cast<CAmount>(gArgs.GetArg("-voteinputamount", VOTING_UTXO_INPUT_AMOUNT));
    // Find suitable sized input
    auto selinput = coins.front();
    // Only search for a larger sized coin if the smallest one is less than the target input amount
    auto smallestInputBounds = abs(selinput.first.nValue - voteInputDesiredAmount);
    if (selinput.first.nValue < voteInputDesiredAmount && coins.size() > 1) {
        for (auto & item : coins) {
            if (selinput == item)
                continue;
            auto sbounds = abs(item.first.nValue - voteInputDesiredAmount);
            if (sbounds > smallestInputBounds)
                break; // go with the smallest if the next one is further from desired input size
            smallestInputBounds = sbounds;
            selinput = item;
        }
    }

    // Create all votes (limited by max OP_RETURN in tx)
    std::vector<CRecipient> voteOuts;
    for (auto & item : selprops) {
        const auto & selproposal = item.first;
        const auto & selvote = item.second;

        // Create and serialize the vote data and insert in OP_RETURN script. The vote
        // is signed with the utxo that is representing that vote. The signing must
        // happen before the vote object is serialized.
        CDataStream ss(SER_NETWORK, GOV_PROTOCOL_VERSION);
        Vote vote(selproposal.getHash(), selvote.getVote(), stakeUtxo.second, gov::makeVinHash(selinput.second), key.GetPubKey().GetID(), stakeUtxo.first.nValue);
        if (!vote.sign(key)) {
            LogPrint(BCLog::GOVERNANCE, "WARNING: Failed to vote on {%s} proposal in stake, utxo signing failed %s\n", selproposal.getName(), stakeUtxo.second.ToString());
            continue;
        }
        if (!vote.isValid(params)) { // validate vote
            LogPrint(BCLog::GOVERNANCE, "WARNING: Failed to vote on {%s} proposal in stake, validation failed\n", selproposal.getName());
            continue;
        }
        ss << vote;

        voteOuts.push_back({CScript() << OP_RETURN << ToByteVector(ss), 0, false});
        if (voteOuts.size() == MAX_OP_RETURN_IN_TRANSACTION)
            break; // no more room
    }

    if (voteOuts.empty())
        return false; // no votes

    CMutableTransaction votetx;
    CCoinControl cc;
    if (IsNetworkFeesEnabled(chainActive.Tip(), params))
        cc.m_zero_fee = true;
    cc.fAllowOtherInputs = false;
    cc.destChange = CTxDestination(key.GetPubKey().GetID()); // pay change to address of staking utxo
    // Select first voting input
    cc.Select(selinput.second);

    CReserveKey reservekey(wallet);
    CAmount nFeeRequired{0};
    std::string strError;
    int nChangePosRet = -1;
    auto locked_chain = wallet->chain().lock();
    if (!wallet->CreateTransaction(*locked_chain, voteOuts, tx, reservekey, nFeeRequired, nChangePosRet, strError, cc)) {
        // If default input isn't enough to cover the fee, create another transaction with additional inputs
        auto allcoins = coins;
        allcoins.insert(allcoins.end(), otherCoins.begin(), otherCoins.end());
        std::set<COutPoint> sallcoins;
        for (const auto & c : allcoins)
            sallcoins.insert(c.second);
        // We want to exclude spending any inputs that are already being used in other votes
        std::set<COutPoint> excluded;
        gov::Governance::instance().utxosInVotes(sallcoins, stakedHeight, excluded, params);

        CAmount inputAmt = selinput.first.nValue;
        if (inputAmt <= nFeeRequired && allcoins.size() > 1) {
            for (const auto & vin : allcoins) {
                // Skip already selected input and inputs already used in other votes
                if (vin.second == selinput.second || excluded.count(vin.second))
                    continue;
                inputAmt += vin.first.nValue;
                cc.Select(vin.second);
                if (inputAmt > nFeeRequired)
                    break;
            }
            // If we have enough inputs to cover fee, try creating new transaction
            if (inputAmt > nFeeRequired) {
                strError.clear();
                if (wallet->CreateTransaction(*locked_chain, voteOuts, tx, reservekey, nFeeRequired, nChangePosRet, strError, cc))
                    return true;
            }
        }

        // unrecoverable error
        return error("Failed to create vote transaction on stake: %s", strError);
    }

    return true;
}

}