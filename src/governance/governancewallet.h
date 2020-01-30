// Copyright (c) 2019-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_GOVERNANCE_GOVERNANCEWALLET_H
#define BLOCKNET_GOVERNANCE_GOVERNANCEWALLET_H

#include <governance/governance.h>

#include <coins.h>
#include <wallet/wallet.h>

namespace gov {

/**
 * Fetch vote information for a proposal that haven't been spent and were cast by the
 * specified wallets.
 * @param hash Proposal hash
 * @param coinsTip Chainstate coins tip
 * @param wallets User wallets to search
 * @param consensus Chain params
 * @return
 */
std::tuple<int, VoteType, bool, CAmount> GetMyVotes(const uint256 & hash, std::vector<std::shared_ptr<CWallet>> & wallets,
        const Consensus::Params & consensus);

/**
 * Submits a proposal to the network and returns true. If there's an issue with the proposal or it's
 * not valid false is returned.
 * @param proposal
 * @param params
 * @param tx Transaction containing proposal submission
 * @param failReasonRet Error message (empty if no error)
 * @return
 */
bool SubmitProposal(const Proposal & proposal, const std::vector<std::shared_ptr<CWallet>> & wallets,
                    const Consensus::Params & params, CTransactionRef & tx, CConnman *connman,
                    std::string *failReasonRet);

/**
 * Cast votes on proposals.
 * @param proposalVotes
 * @param params
 * @param txsRet List of transactions containing proposal votes
 * @param failReason Error message (empty if no error)
 * @return
 */
bool SubmitVotes(const std::vector<ProposalVote> & proposalVotes, const std::vector<std::shared_ptr<CWallet>> & wallets,
                 const Consensus::Params & params, std::vector<CTransactionRef> & txsRet, CConnman *connman,
                 std::string *failReasonRet);

/**
 * Applies new votes for all proposals associated with this utxo.
 * @param stakedHeight (i.e. chaintip + 1)
 * @param utxo Old voting utxo
 * @param key Private key of staking utxo
 * @param stakeUtxo Utxo of the new stake
 * @param wallet Contains stake key
 * @param tx Voting transaction
 * @param params Chain params
 * @return
 */
bool RevoteOnStake(const int & stakedHeight, const COutPoint & utxo, const CKey & key, const std::pair<CTxOut,COutPoint> & stakeUtxo,
                   CWallet *wallet, CTransactionRef & tx, const Consensus::Params & params);

}

#endif //BLOCKNET_GOVERNANCE_GOVERNANCEWALLET_H
