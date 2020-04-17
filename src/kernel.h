// Copyright (c) 2012-2013 The PPCoin developers
// Copyright (c) 2017-2020 The Blocknet developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_KERNEL_H
#define BITCOIN_KERNEL_H

#include <chain.h>
#include <streams.h>

#include <boost/date_time/posix_time/posix_time.hpp>

// Compute the hash modifier for proof-of-stake
bool ComputeNextStakeModifier(const CBlockIndex* pindexPrev, uint64_t& nStakeModifier, bool& fGeneratedStakeModifier,
                              const Consensus::Params & consensus);
// Stake modifier selection interval
int64_t GetStakeModifierSelectionInterval();

// Stake modifier selection upgrade
bool IsProtocolV05(uint64_t nTimeTx);
bool IsProtocolV06(uint64_t nTimeTx, const Consensus::Params & consensusParams);
bool IsProtocolV07(uint64_t nTimeTx, const Consensus::Params & consensusParams);

uint256 stakeHash(unsigned int nTimeTx, CDataStream ss, unsigned int prevoutIndex, uint256 prevoutHash,unsigned int nTimeBlockFrom);
uint256 stakeHashV05(CDataStream ss, const unsigned int & nTimeBlockFrom, const int & blockHeight, const unsigned int & prevoutIndex, const unsigned int & nTimeTx);
uint256 stakeHashV06(CDataStream ss, const uint256 & hashBlockFrom, const unsigned int & nTimeBlockFrom, const int & blockHeight, const unsigned int & prevoutIndex, const unsigned int & nTimeTx);

// Check whether stake kernel meets hash target
bool stakeTargetHit(const uint256 & hashProofOfStake, const int64_t & nValueIn, const arith_uint256 & bnTargetPerCoinDay);
bool stakeTargetHitV06(const uint256 & hashProofOfStake, const int64_t & nValueIn, const arith_uint256 & bnTargetPerCoinDay);
bool stakeTargetHitV07(const uint256 & hashProofOfStake, const int64_t & currentStakingTime, const int64_t & prevStakingTime, const int64_t & nValueIn, const arith_uint256 & bnTargetPerCoinDay, const int & nPowTargetSpacing);

bool CheckStakeKernelHash(const CBlockIndex *pindexPrev, const CBlockIndex *pindexStake, const unsigned int & nBits,
        const CAmount & txInAmount, const COutPoint & prevout, const int64_t & nBlockTime, const unsigned int & nNonce,
        uint256 & hashProofOfStake, const Consensus::Params & consensus);
bool GetKernelStakeModifier(const CBlockIndex *pindexPrev, const CBlockIndex *pindexStake, const int64_t & nBlockTime, uint64_t & nStakeModifier, int & nStakeModifierHeight, int64_t & nStakeModifierTime);
bool GetKernelStakeModifierV03(const CBlockIndex *pindexStake, uint64_t & nStakeModifier, int & nStakeModifierHeight, int64_t & nStakeModifierTime);
bool GetKernelStakeModifierBlocknet(const CBlockIndex *pindexPrev, const CBlockIndex *pindexStake, const int64_t & blockStakeTime, uint64_t & nStakeModifier, int & nStakeModifierHeight, int64_t & nStakeModifierTime);

// Check kernel hash target and coinstake signature
// Sets hashProofOfStake on success return
bool CheckProofOfStake(const CBlockHeader & block, const CBlockIndex *pindexPrev, uint256 & hashProofOfStake, const Consensus::Params & consensusParams);

// peercoin: For use with Staking Protocol V05.
unsigned int GetStakeEntropyBit(const uint256 & blockHash, const int64_t & blockTime);

static std::string DateTimeStrFormat(const char* pszFormat, int64_t nTime) {
    // std::locale takes ownership of the pointer
    std::locale loc(std::locale::classic(), new boost::posix_time::time_facet(pszFormat));
    std::stringstream ss;
    ss.imbue(loc);
    ss << boost::posix_time::from_time_t(nTime);
    return ss.str();
}
bool IsProofOfStake(int blockHeight, const Consensus::Params & consensusParams);
bool IsProofOfStake(int blockHeight);

#endif // BITCOIN_KERNEL_H
