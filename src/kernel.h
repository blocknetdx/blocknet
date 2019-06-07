// Copyright (c) 2012-2013 The PPCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_KERNEL_H
#define BITCOIN_KERNEL_H

#include <chainparams.h>
#include <chain.h>
#include <streams.h>
#include <logging.h>
#include <arith_uint256.h>
#include <util/system.h>
#include <validation.h>

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

// Check whether stake kernel meets hash target
// Sets hashProofOfStake on success return
uint256 stakeHash(unsigned int nTimeTx, CDataStream ss, unsigned int prevoutIndex, uint256 prevoutHash,
        unsigned int nTimeBlockFrom);
bool stakeTargetHit(uint256 hashProofOfStake, int64_t nValueIn, arith_uint256 bnTargetPerCoinDay);
bool CheckStakeKernelHash(unsigned int nBits, const uint256 txInBlockHash, const int64_t txInBlockTime,
        const CAmount txInAmount, const COutPoint prevout, unsigned int& nTimeTx, unsigned int nHashDrift, bool fCheck,
        uint256& hashProofOfStake, bool fPrintProofOfStake = false);

// Check kernel hash target and coinstake signature
// Sets hashProofOfStake on success return
bool CheckProofOfStake(const CBlockHeader & block, uint256 & hashProofOfStake, const Consensus::Params & consensusParams);

// Get stake modifier checksum
unsigned int GetStakeModifierChecksum(const CBlockIndex* pindex);

static inline bool error(const char* format) {
    auto log_msg = std::string("ERROR: ") + format + "\n";
    LogInstance().LogPrintStr(log_msg);
    return false;
}
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
