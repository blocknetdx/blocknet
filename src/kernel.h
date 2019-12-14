// Copyright (c) 2012-2013 The PPCoin developers
// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_KERNEL_H
#define BITCOIN_KERNEL_H

#include <chain.h>
#include <streams.h>

#include <boost/date_time/posix_time/posix_time.hpp>

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
// Stake modifier selection interval
int64_t GetStakeModifierSelectionInterval();

// Stake modifier selection upgrade
bool IsProtocolV05(uint64_t nTimeTx);

uint256 stakeHash(unsigned int nTimeTx, CDataStream ss, unsigned int prevoutIndex, uint256 prevoutHash,unsigned int nTimeBlockFrom);
uint256 stakeHashV05(CDataStream ss, const unsigned int & nTimeBlockFrom, const int & blockHeight, const unsigned int & prevoutIndex, const unsigned int & nTimeTx);

// Check whether stake kernel meets hash target
bool stakeTargetHit(uint256 hashProofOfStake, int64_t nValueIn, arith_uint256 bnTargetPerCoinDay);

bool CheckStakeKernelHash(const CBlockIndex *pindexPrev, unsigned int nBits, const uint256 txInBlockHash, const int64_t txInBlockTime,
        const CAmount txInAmount, const COutPoint prevout, unsigned int& nTimeTx, unsigned int nHashDrift, bool fCheck,
        uint256& hashProofOfStake, bool fPrintProofOfStake = false);
bool GetKernelStakeModifier(const CBlockIndex *pindexPrev, const uint256 & hashBlockFrom, const unsigned int & nTimeTx, uint64_t& nStakeModifier, int& nStakeModifierHeight, int64_t& nStakeModifierTime, bool fPrintProofOfStake);
bool GetKernelStakeModifierV03(uint256 hashBlockFrom, uint64_t& nStakeModifier, int& nStakeModifierHeight, int64_t& nStakeModifierTime, bool fPrintProofOfStake);
bool GetKernelStakeModifierBlocknet(const CBlockIndex *pindexPrev, const uint256 & hashBlockFrom, const unsigned int & nTimeTx, uint64_t & nStakeModifier, int & nStakeModifierHeight, int64_t & nStakeModifierTime, bool fPrintProofOfStake);

// Check kernel hash target and coinstake signature
// Sets hashProofOfStake on success return
bool CheckProofOfStake(const CBlockHeader & block, const CBlockIndex *pindexPrev, uint256 & hashProofOfStake, const Consensus::Params & consensusParams);

// peercoin: For use with Staking Protocol V05.
unsigned int GetStakeEntropyBit(const uint256 & blockHash, const int64_t & blockTime);

// Get stake modifier checksum
unsigned int GetStakeModifierChecksum(const CBlockIndex* pindex);

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
