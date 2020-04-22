// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2019-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_PARAMS_H
#define BITCOIN_CONSENSUS_PARAMS_H

#include <uint256.h>
#include <amount.h>
#include <limits>
#include <map>
#include <policy/feerate.h>
#include <string>
#include <functional>

namespace Consensus {

enum DeploymentPos
{
    DEPLOYMENT_TESTDUMMY,
    DEPLOYMENT_CSV, // Deployment of BIP68, BIP112, and BIP113.
    DEPLOYMENT_SEGWIT, // Deployment of BIP141, BIP143, and BIP147.
    DEPLOYMENT_NETWORKFEES, // Deployment of network fees
    DEPLOYMENT_STAKEP2PKH, // Deployment of p2pkh stakes
    // NOTE: Also add new deployments to VersionBitsDeploymentInfo in versionbits.cpp
    MAX_VERSION_BITS_DEPLOYMENTS
};

/**
 * Struct for each individual consensus rule change using BIP9.
 */
struct BIP9Deployment {
    /** Bit position to select the particular bit in nVersion. */
    int bit;
    /** Start MedianTime for version bits miner confirmation. Can be a date in the past */
    int64_t nStartTime;
    /** Timeout/expiry MedianTime for the deployment attempt. */
    int64_t nTimeout;

    /** Constant for nTimeout very far in the future. */
    static constexpr int64_t NO_TIMEOUT = std::numeric_limits<int64_t>::max();

    /** Special value for nStartTime indicating that the deployment is always active.
     *  This is useful for testing, as it means tests don't need to deal with the activation
     *  process (which takes at least 3 BIP9 intervals). Only tests that specifically test the
     *  behaviour during activation cannot use this. */
    static constexpr int64_t ALWAYS_ACTIVE = -1;
};

/**
 * Parameters that influence chain consensus.
 */
struct Params {
    uint256 hashGenesisBlock;
    int nSubsidyHalvingInterval;
    /* Block hash that is excepted from BIP16 enforcement */
    uint256 BIP16Exception;
    /** Block height and hash at which BIP34 becomes active */
    int BIP34Height;
    uint256 BIP34Hash;
    /** Block height at which BIP65 becomes active */
    int BIP65Height;
    /** Block height at which BIP66 becomes active */
    int BIP66Height;
    /**
     * Minimum blocks including miner confirmation of the total of 2016 blocks in a retargeting period,
     * (nPowTargetTimespan / nPowTargetSpacing) which is also used for BIP9 deployments.
     * Examples: 1916 for 95%, 1512 for testchains.
     */
    uint32_t nRuleChangeActivationThreshold;
    uint32_t nMinerConfirmationWindow;
    BIP9Deployment vDeployments[MAX_VERSION_BITS_DEPLOYMENTS];
    int lastCheckpointHeight{0};
    /** Proof of work parameters */
    uint256 powLimit;
    bool fPowAllowMinDifficultyBlocks;
    bool fPowNoRetargeting;
    int64_t nPowTargetSpacing;
    int64_t nPowTargetTimespan;
    int64_t DifficultyAdjustmentInterval() const { return nPowTargetTimespan / nPowTargetSpacing; }
    uint256 nMinimumChainWork;
    uint256 defaultAssumeValid;
    int lastPOWBlock;
    std::function<CAmount(const int&, const Params &)> GetBlockSubsidy;
    /** Proof of stake parameters */
    int stakeMinAge;
    int stakingModiferV2Block;
    int coinMaturity;
    bool stakingAllowsMinDifficultyBlocks{false};
    int64_t stakingPoSTargetTimespan{60*40};
    int64_t stakingV05UpgradeTime{0};
    int64_t stakingV06UpgradeTime{0};
    int64_t stakingV07UpgradeTime{0};
    int64_t PoSFutureBlockTimeLimit(const int64_t blockTime) const { // changing this will break consensus!
        if (blockTime >= stakingV07UpgradeTime)
            return 15; // seconds in the future
        else
            return nPowTargetSpacing * 3;
    }
    /** Service node parameters */
    int snMaxCollateralCount{10}; // max utxos for use with service node collateral
    /** Governance parameters */
    int superblock;
    int votingCutoff; // blocks prior to superblock
    int proposalCutoff; // blocks prior to superblock
    int governanceBlock{1}; // block number indicating when governance system was enabled
    CAmount proposalMinAmount{10 * COIN};
    CAmount proposalMaxAmount{40000 * COIN};
    CAmount proposalFee{10 * COIN};
    CAmount voteBalance{5000 * COIN};
    CAmount voteMinUtxoAmount{100 * COIN};
    /** Fallback fee **/
    CFeeRate defaultFallbackFee;
};
} // namespace Consensus

#endif // BITCOIN_CONSENSUS_PARAMS_H
