// Copyright (c) 2012-2013 The PPCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_KERNEL_H
#define BITCOIN_KERNEL_H

#include "main.h"


// MODIFIER_INTERVAL: time to elapse before new modifier is computed
static const unsigned int MODIFIER_INTERVAL = 60;
static const unsigned int MODIFIER_INTERVAL_TESTNET = 60;
extern unsigned int nModifierInterval;
extern unsigned int getIntervalVersion(bool fTestNet);

// MODIFIER_INTERVAL_RATIO:
// ratio of group interval length between the last group and the first group
static const int MODIFIER_INTERVAL_RATIO = 3;

// Compute the hash modifier for proof-of-stake
bool ComputeNextStakeModifier(const CBlockIndex* pindexPrev, uint64_t& nStakeModifier, bool& fGeneratedStakeModifier);

// Stake modifier selection upgrade
bool IsProtocolV05(uint64_t nTimeTx);

uint256 stakeHash(unsigned int nTimeTx, CDataStream ss, unsigned int prevoutIndex, uint256 prevoutHash, unsigned int nTimeBlockFrom);
uint256 stakeHashV05(CDataStream ss, const unsigned int & nTimeBlockFrom, const int & blockHeight, const unsigned int & prevoutIndex, const unsigned int & nTimeTx);
bool stakeTargetHit(uint256 hashProofOfStake, int64_t nValueIn, uint256 bnTargetPerCoinDay);

bool CheckStakeKernelHash(unsigned int nBits, const CBlockIndex* pindexPrev, const CBlock blockFrom, const CTransaction txPrev, const COutPoint prevout, unsigned int& nTimeTx, unsigned int nHashDrift, bool fCheck, uint256& hashProofOfStake, bool fPrintProofOfStake = false);
bool GetKernelStakeModifier(const CBlockIndex *pindexPrev, const uint256 & hashBlockFrom, const unsigned int & nTimeTx, uint64_t& nStakeModifier, int & nStakeModifierHeight, int64_t & nStakeModifierTime, bool fPrintProofOfStake);
bool GetKernelStakeModifierBlocknet(const CBlockIndex *pindexPrev, const uint256 & hashBlockFrom, const unsigned int & nTimeTx, uint64_t& nStakeModifier, int & nStakeModifierHeight, int64_t & nStakeModifierTime, bool fPrintProofOfStake);
bool GetKernelStakeModifierV03(uint256 hashBlockFrom, uint64_t & nStakeModifier, int & nStakeModifierHeight, int64_t & nStakeModifierTime, bool fPrintProofOfStake);

// Check kernel hash target and coinstake signature
// Sets hashProofOfStake on success return
bool CheckProofOfStake(const CBlock block, const CBlockIndex* pindexPrev, uint256& hashProofOfStake);

// Check whether the coinstake timestamp meets protocol
bool CheckCoinStakeTimestamp(int64_t nTimeBlock, int64_t nTimeTx);

// Get stake modifier checksum
unsigned int GetStakeModifierChecksum(const CBlockIndex* pindex);

// Check stake modifier hard checkpoints
bool CheckStakeModifierCheckpoints(int nHeight, unsigned int nStakeModifierChecksum);

// Get time weight using supplied timestamps
int64_t GetWeight(int64_t nIntervalBeginning, int64_t nIntervalEnd);

/**
 * peercoin
 * For use with Staking Protocol V05.
 * @param blockHash
 * @param blockTime
 * @return
 */
unsigned int GetStakeEntropyBit(const uint256 & blockHash, const int64_t & blockTime);

#endif // BITCOIN_KERNEL_H
