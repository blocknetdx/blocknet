/* @flow */
// Copyright (c) 2012-2013 The PPCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/assign/list_of.hpp>
#include <boost/lexical_cast.hpp>

#include <db.h>
#include <hash.h>
#include <kernel.h>
#include <script/interpreter.h>
#include <timedata.h>

using namespace std;

bool fTestNet = false;

// Modifier interval: time to elapse before new modifier is computed
// Set to 3-hour for production network and 20-minute for test network
unsigned int nModifierInterval;
int nStakeTargetSpacing = 60;
unsigned int getIntervalVersion(bool fTestNet)
{
    if (fTestNet)
        return MODIFIER_INTERVAL_TESTNET;
    else
        return MODIFIER_INTERVAL;
}

// Hard checkpoints of stake modifiers to ensure they are deterministic
static std::map<int, unsigned int> mapStakeModifierCheckpoints =
    boost::assign::map_list_of(0, 0xfd11f4e7u);

// Get the last stake modifier and its generation time from a given block
static bool GetLastStakeModifier(const CBlockIndex* pindex, uint64_t& nStakeModifier, int64_t& nModifierTime)
{
    if (!pindex)
        return error("GetLastStakeModifier: null pindex");
    while (pindex && pindex->pprev && !pindex->GeneratedStakeModifier())
        pindex = pindex->pprev;
    if (!pindex->GeneratedStakeModifier())
        return error("GetLastStakeModifier: no generation at genesis block");
    nStakeModifier = pindex->nStakeModifier;
    nModifierTime = pindex->GetBlockTime();
    return true;
}

// Get selection interval section (in seconds)
static int64_t GetStakeModifierSelectionIntervalSection(int nSection)
{
    assert(nSection >= 0 && nSection < 64);
    int64_t a = getIntervalVersion(fTestNet) * 63 / (63 + ((63 - nSection) * (MODIFIER_INTERVAL_RATIO - 1)));
    return a;
}

// Get stake modifier selection interval (in seconds)
static int64_t GetStakeModifierSelectionInterval()
{
    int64_t nSelectionInterval = 0;
    for (int nSection = 0; nSection < 64; nSection++) {
        nSelectionInterval += GetStakeModifierSelectionIntervalSection(nSection);
    }
    return nSelectionInterval;
}

// select a block from the candidate blocks in vSortedByTimestamp, excluding
// already selected blocks in vSelectedBlocks, and with timestamp up to
// nSelectionIntervalStop.
#include <boost/foreach.hpp>
static bool SelectBlockFromCandidates(
    vector<pair<int64_t, uint256> >& vSortedByTimestamp,
    map<uint256, const CBlockIndex*>& mapSelectedBlocks,
    int64_t nSelectionIntervalStop,
    uint64_t nStakeModifierPrev,
    const CBlockIndex** pindexSelected)
{
    bool fModifierV2 = false;
    bool fModifierV3 = false;
    bool fFirstRun = true;
    bool fSelected = false;
    arith_uint256 hashBest = 0;
    *pindexSelected = (const CBlockIndex*)0;
    BOOST_FOREACH (const auto & item, vSortedByTimestamp) {
        if (!mapBlockIndex.count(item.second))
            return error("SelectBlockFromCandidates: failed to find block index for candidate block %s", item.second.ToString().c_str());

        const CBlockIndex* pindex = mapBlockIndex[item.second];
        if (fSelected && pindex->GetBlockTime() > nSelectionIntervalStop)
            break;

        //if the lowest block height (vSortedByTimestamp[0]) is >= switch height, use new modifier calc
        if (fFirstRun) {
            fModifierV2 = pindex->nHeight >= Params().GetConsensus().stakingModiferV2Block;
            fModifierV3 = IsProtocolV05(pindex->GetBlockTime());
            fFirstRun = false;
        }

        if (mapSelectedBlocks.count(pindex->GetBlockHash()) > 0)
            continue;

        // compute the selection hash by hashing an input that is unique to that block
        uint256 hashProof;
        if (fModifierV3)
            hashProof = pindex->hashProofOfStake;
        else if (fModifierV2)
            hashProof = pindex->GetBlockHash();
        else
            hashProof = IsProofOfStake(pindex->nHeight) ? ArithToUint256(0) : pindex->GetBlockHash();

        CDataStream ss(SER_GETHASH, 0);
        ss << hashProof << nStakeModifierPrev;
        arith_uint256 hashSelection = UintToArith256(Hash(ss.begin(), ss.end()));

        // the selection hash is divided by 2**32 so that proof-of-stake block
        // is always favored over proof-of-work block. this is to preserve
        // the energy efficiency property
        if (IsProofOfStake(pindex->nHeight))
            hashSelection >>= 32;

        if (fSelected && hashSelection < hashBest) {
            hashBest = hashSelection;
            *pindexSelected = (const CBlockIndex*)pindex;
        } else if (!fSelected) {
            fSelected = true;
            hashBest = hashSelection;
            *pindexSelected = (const CBlockIndex*)pindex;
        }
    }
    if (gArgs.GetBoolArg("-printstakemodifier", false))
        LogPrintf("SelectBlockFromCandidates: selection hash=%s\n", hashBest.ToString().c_str());
    return fSelected;
}

// Stake Modifier (hash modifier of proof-of-stake):
// The purpose of stake modifier is to prevent a txout (coin) owner from
// computing future proof-of-stake generated by this txout at the time
// of transaction confirmation. To meet kernel protocol, the txout
// must hash with a future stake modifier to generate the proof.
// Stake modifier consists of bits each of which is contributed from a
// selected block of a given block group in the past.
// The selection of a block is based on a hash of the block's proof-hash and
// the previous stake modifier.
// Stake modifier is recomputed at a fixed time interval instead of every
// block. This is to make it difficult for an attacker to gain control of
// additional bits in the stake modifier, even after generating a chain of
// blocks.
bool ComputeNextStakeModifier(const CBlockIndex* pindexPrev, uint64_t& nStakeModifier, bool& fGeneratedStakeModifier)
{
    nStakeModifier = 0;
    fGeneratedStakeModifier = false;
    if (!pindexPrev) {
        fGeneratedStakeModifier = true;
        return true; // genesis block's modifier is 0
    }
    if (pindexPrev->nHeight == 0) {
        //Give a stake modifier to the first block
        fGeneratedStakeModifier = true;
        nStakeModifier = uint64_t("stakemodifier");
        return true;
    }

    // First find current stake modifier and its generation block time
    // if it's not old enough, return the same stake modifier
    int64_t nModifierTime = 0;
    if (!GetLastStakeModifier(pindexPrev, nStakeModifier, nModifierTime))
        return error("ComputeNextStakeModifier: unable to get last modifier");

    if (gArgs.GetBoolArg("-printstakemodifier", false))
        LogPrintf("ComputeNextStakeModifier: prev modifier= %s time=%s\n", boost::lexical_cast<std::string>(nStakeModifier).c_str(), DateTimeStrFormat("%Y-%m-%d %H:%M:%S", nModifierTime).c_str());

    if (nModifierTime / getIntervalVersion(fTestNet) >= pindexPrev->GetBlockTime() / getIntervalVersion(fTestNet))
        return true;

    // Sort candidate blocks by timestamp
    vector<pair<int64_t, uint256> > vSortedByTimestamp;
    vSortedByTimestamp.reserve(64 * getIntervalVersion(fTestNet) / nStakeTargetSpacing);
    int64_t nSelectionInterval = GetStakeModifierSelectionInterval();
    int64_t nSelectionIntervalStart = (pindexPrev->GetBlockTime() / getIntervalVersion(fTestNet)) * getIntervalVersion(fTestNet) - nSelectionInterval;
    const CBlockIndex* pindex = pindexPrev;

    while (pindex && pindex->GetBlockTime() >= nSelectionIntervalStart) {
        vSortedByTimestamp.push_back(make_pair(pindex->GetBlockTime(), pindex->GetBlockHash()));
        pindex = pindex->pprev;
    }

    int nHeightFirstCandidate = pindex ? (pindex->nHeight + 1) : 0;
    reverse(vSortedByTimestamp.begin(), vSortedByTimestamp.end());
    // Need to handle a side effect in the staking protocol, where selection of modifier
    // breaks a tie based on hash instead of block number, see comparator below checking
    // for this case.
    sort(vSortedByTimestamp.begin(), vSortedByTimestamp.end(),
            [](const pair<int64_t, uint256> & a, const pair<int64_t, uint256> & b) -> bool {
                if (a.first == b.first)
                    return UintToArith256(a.second) < UintToArith256(b.second);
                return a.first < b.first;
            });

    // Select 64 blocks from candidate blocks to generate stake modifier
    uint64_t nStakeModifierNew = 0;
    int64_t nSelectionIntervalStop = nSelectionIntervalStart;
    map<uint256, const CBlockIndex*> mapSelectedBlocks;
    for (int nRound = 0; nRound < min(64, (int)vSortedByTimestamp.size()); nRound++) {
        // add an interval section to the current selection round
        nSelectionIntervalStop += GetStakeModifierSelectionIntervalSection(nRound);

        // select a block from the candidates of current round
        if (!SelectBlockFromCandidates(vSortedByTimestamp, mapSelectedBlocks, nSelectionIntervalStop, nStakeModifier, &pindex))
            return error("ComputeNextStakeModifier: unable to select block at round %d", nRound);

        // write the entropy bit of the selected block
        const auto ebit = pindex->GetStakeEntropyBit();
        if (ebit)
            nStakeModifierNew |= 1ULL << nRound;
        else
            nStakeModifierNew &= ~(1ULL << nRound);

        // add the selected block from candidates to selected list
        mapSelectedBlocks.insert(make_pair(pindex->GetBlockHash(), pindex));
        if (gArgs.GetBoolArg("-printstakemodifier", false))
            LogPrintf("ComputeNextStakeModifier: selected round %d stop=%s height=%d bit=%d\n",
                nRound, DateTimeStrFormat("%Y-%m-%d %H:%M:%S", nSelectionIntervalStop).c_str(), pindex->nHeight, ebit);
    }

    // Print selection map for visualization of the selected blocks
    if (gArgs.GetBoolArg("-printstakemodifier", false)) {
        string strSelectionMap = "";
        // '-' indicates proof-of-work blocks not selected
        strSelectionMap.insert(0, pindexPrev->nHeight - nHeightFirstCandidate + 1, '-');
        pindex = pindexPrev;
        while (pindex && pindex->nHeight >= nHeightFirstCandidate) {
            // '=' indicates proof-of-stake blocks not selected
            if (IsProofOfStake(pindex->nHeight))
                strSelectionMap.replace(pindex->nHeight - nHeightFirstCandidate, 1, "=");
            pindex = pindex->pprev;
        }
        for (const auto & item : mapSelectedBlocks) {
            // 'S' indicates selected proof-of-stake blocks
            // 'W' indicates selected proof-of-work blocks
            strSelectionMap.replace(item.second->nHeight - nHeightFirstCandidate, 1, IsProofOfStake(item.second->nHeight) ? "S" : "W");
        }
        LogPrintf("ComputeNextStakeModifier: selection height [%d, %d] map %s\n", nHeightFirstCandidate, pindexPrev->nHeight, strSelectionMap.c_str());
    }
    if (gArgs.GetBoolArg("-printstakemodifier", false)) {
        LogPrintf("ComputeNextStakeModifier: new modifier=%s time=%s\n", boost::lexical_cast<std::string>(nStakeModifierNew).c_str(), DateTimeStrFormat("%Y-%m-%d %H:%M:%S", pindexPrev->GetBlockTime()).c_str());
    }

    nStakeModifier = nStakeModifierNew;
    fGeneratedStakeModifier = true;
    return true;
}

bool IsProtocolV05(uint64_t nTimeTx) {
    return nTimeTx >= Params().GetConsensus().stakingV05UpgradeTime;
}

// Get the stake modifier specified by the protocol to hash for a stake kernel
bool GetKernelStakeModifier(const CBlockIndex* pindexPrev, const uint256 & hashBlockFrom, const unsigned int & nTimeTx, uint64_t & nStakeModifier, int & nStakeModifierHeight, int64_t & nStakeModifierTime, bool fPrintProofOfStake)
{
    if (IsProtocolV05(nTimeTx))
        return GetKernelStakeModifierBlocknet(pindexPrev, hashBlockFrom, nTimeTx, nStakeModifier, nStakeModifierHeight, nStakeModifierTime, fPrintProofOfStake);
    else
        return GetKernelStakeModifierV03(hashBlockFrom, nStakeModifier, nStakeModifierHeight, nStakeModifierTime, fPrintProofOfStake);
}

// Select the modifier from the most recent block index.
// Fails if:
// 1) stake input block is not indexed
// 2) stake input hasn't matured
// 3) stake modifier doesn't exist on the specified index
//
// Implementation modified from peercoin (https://github.com/peercoin/peercoin/blob/70e86347e126a3dbd00a5e65b23305b2a768cb56/src/kernel.cpp#L336)
bool GetKernelStakeModifierBlocknet(const CBlockIndex* pindexPrev, const uint256 & hashBlockFrom, const unsigned int & nTimeTx, uint64_t & nStakeModifier, int & nStakeModifierHeight, int64_t & nStakeModifierTime, bool fPrintProofOfStake)
{
    const auto stakeTime = static_cast<int64_t>(nTimeTx);
    nStakeModifierHeight = pindexPrev->nHeight;
    nStakeModifierTime = pindexPrev->GetBlockTime();

    if (!mapBlockIndex.count(hashBlockFrom))
        return error("GetKernelStakeModifierBlocknet() block not indexed %s", hashBlockFrom.ToString());
    const auto & blockFromTime = mapBlockIndex[hashBlockFrom]->GetBlockTime();

    // Do not allow picking a modifier that is generated before or at the time the utxo is confirmed in a block
    const auto useInterval = static_cast<int64_t>(Params().GetConsensus().stakeMinAge);
    if (stakeTime - useInterval <= blockFromTime)
        return error("GetKernelStakeModifierBlocknet() stake min age check failed");

    nStakeModifier = pindexPrev->nStakeModifier;
    return true;
}

// The stake modifier used to hash for a stake kernel is chosen as the stake
// modifier about a selection interval later than the coin generating the kernel
bool GetKernelStakeModifierV03(uint256 hashBlockFrom, uint64_t& nStakeModifier, int& nStakeModifierHeight, int64_t& nStakeModifierTime, bool fPrintProofOfStake)
{
    nStakeModifier = 0;
    if (!mapBlockIndex.count(hashBlockFrom))
        return error("GetKernelStakeModifier() : block not indexed");
    const CBlockIndex* pindexFrom = mapBlockIndex[hashBlockFrom];
    nStakeModifierHeight = pindexFrom->nHeight;
    nStakeModifierTime = pindexFrom->GetBlockTime();
    int64_t nStakeModifierSelectionInterval = GetStakeModifierSelectionInterval();
    const CBlockIndex* pindex = pindexFrom;
    CBlockIndex* pindexNext = chainActive[pindexFrom->nHeight + 1];
    auto slowSearch = [](std::map<int, CBlockIndex*> & mbi, const int blockNumber) -> CBlockIndex* {
        return mbi[blockNumber];
    };
    if (!pindexNext) // slow search
        pindexNext = slowSearch(mapHeaderIndex, pindexFrom->nHeight + 1);

    // loop to find the stake modifier later by a selection interval
    while (nStakeModifierTime < pindexFrom->GetBlockTime() + nStakeModifierSelectionInterval) {
        if (!pindexNext) {
            // Should never happen
            return error("Null pindexNext\n");
        }

        pindex = pindexNext;
        const int nextBlock = pindexNext->nHeight + 1;
        pindexNext = chainActive[nextBlock];
        if (!pindexNext) // slow search
            pindexNext = slowSearch(mapHeaderIndex, nextBlock);
        if (pindex->GeneratedStakeModifier()) {
            nStakeModifierHeight = pindex->nHeight;
            nStakeModifierTime = pindex->GetBlockTime();
        }
    }
    nStakeModifier = pindex->nStakeModifier;
    return true;
}

uint256 stakeHash(unsigned int nTimeTx, CDataStream ss, unsigned int prevoutIndex, uint256 prevoutHash, unsigned int nTimeBlockFrom)
{
    //Blocknetdx will hash in the transaction hash and the index number in order to make sure each hash is unique
    ss << nTimeBlockFrom << prevoutIndex << prevoutHash << nTimeTx;
    return Hash(ss.begin(), ss.end());
}

// Blocknet staking protocol (based on ppcoin V05 stake protocol)
uint256 stakeHashV05(CDataStream ss, const unsigned int & nTimeBlockFrom, const int & blockHeight, const unsigned int & prevoutIndex, const unsigned int & nTimeTx) {
    ss << nTimeBlockFrom << blockHeight << prevoutIndex << nTimeTx;
    return Hash(ss.begin(), ss.end());
}

//test hash vs target
bool stakeTargetHit(uint256 hashProofOfStake, int64_t nValueIn, arith_uint256 bnTargetPerCoinDay)
{
    //get the stake weight - weight is equal to coin amount
    const auto bnCoinDayWeight = arith_uint256(nValueIn) / 100;
    // Now check if proof-of-stake hash meets target protocol
    return (UintToArith256(hashProofOfStake) < bnCoinDayWeight * bnTargetPerCoinDay);
}

//instead of looping outside and reinitializing variables many times, we will give a nTimeTx and also search interval so that we can do all the hashing here
bool CheckStakeKernelHash(const CBlockIndex* pindexPrev, unsigned int nBits, const uint256 txInBlockHash, const int64_t txInBlockTime,
        const CAmount txInAmount, const COutPoint prevout, unsigned int& nTimeTx, unsigned int nHashDrift,
        bool fCheck, uint256& hashProofOfStake, bool fPrintProofOfStake)
{
    //assign new variables to make it easier to read
    int64_t nValueIn = txInAmount;
    unsigned int nTimeBlockFrom = txInBlockTime;

    if (nTimeTx < nTimeBlockFrom) // Transaction timestamp violation
        return error("CheckStakeKernelHash() : nTime violation");

    if (nTimeBlockFrom + Params().GetConsensus().stakeMinAge > nTimeTx)
        return error("CheckStakeKernelHash() : min age violation - nTimeBlockFrom=%d nStakeMinAge=%d nTimeTx=%d",
                nTimeBlockFrom, Params().GetConsensus().stakeMinAge, nTimeTx);

    //grab difficulty
    arith_uint256 bnTargetPerCoinDay;
    bnTargetPerCoinDay.SetCompact(nBits);

    //grab stake modifier
    uint64_t nStakeModifier = 0;
    int nStakeModifierHeight = 0;
    int64_t nStakeModifierTime = 0;
    if (!GetKernelStakeModifier(pindexPrev, txInBlockHash, nTimeTx, nStakeModifier, nStakeModifierHeight, nStakeModifierTime, fPrintProofOfStake)) {
        LogPrintf("CheckStakeKernelHash(): failed to get kernel stake modifier \n");
        return false;
    }

    //create data stream once instead of repeating it in the loop
    CDataStream ss(SER_GETHASH, 0);
    ss << nStakeModifier;

    bool v05StakeProtocol = IsProtocolV05(nTimeTx);
    const int currentBlock = pindexPrev->nHeight + 1;

    //if wallet is simply checking to make sure a hash is valid
    if (fCheck) {
        hashProofOfStake = v05StakeProtocol ? stakeHashV05(ss, nTimeBlockFrom, currentBlock, prevout.n, nTimeTx)
                                            : stakeHash(nTimeTx, ss, prevout.n, prevout.hash, nTimeBlockFrom);
        auto targetHit = stakeTargetHit(hashProofOfStake, nValueIn, bnTargetPerCoinDay);
        return targetHit;
    }

    bool fSuccess = false;
    unsigned int nTryTime = 0;
    unsigned int i;
    for (i = 0; i < (nHashDrift); i++) //iterate the hashing
    {
        //hash this iteration
        nTryTime = nTimeTx + nHashDrift - i;
        v05StakeProtocol = IsProtocolV05(nTryTime);
        hashProofOfStake = v05StakeProtocol ? stakeHashV05(ss, nTimeBlockFrom, currentBlock, prevout.n, nTryTime)
                                            : stakeHash(nTryTime, ss, prevout.n, prevout.hash, nTimeBlockFrom);

        // if stake hash does not meet the target then continue to next iteration
        if (!stakeTargetHit(hashProofOfStake, nValueIn, bnTargetPerCoinDay))
            continue;

        fSuccess = true; // if we make it this far then we have successfully created a stake hash
        nTimeTx = nTryTime;

        if (fPrintProofOfStake) {
            LogPrintf("CheckStakeKernelHash() : using modifier %s at height=%d timestamp=%s for block from height=%d timestamp=%s\n",
                boost::lexical_cast<std::string>(nStakeModifier).c_str(), nStakeModifierHeight,
                DateTimeStrFormat("%Y-%m-%d %H:%M:%S", nStakeModifierTime).c_str(),
                mapBlockIndex[txInBlockHash]->nHeight,
                DateTimeStrFormat("%Y-%m-%d %H:%M:%S", txInBlockTime).c_str());
            LogPrintf("CheckStakeKernelHash() : pass protocol=%s modifier=%s nTimeBlockFrom=%u prevoutHash=%s nTimeTxPrev=%u nPrevout=%u nTimeTx=%u hashProof=%s\n",
                v05StakeProtocol ? "0.5" : "0.3",
                boost::lexical_cast<std::string>(nStakeModifier).c_str(),
                nTimeBlockFrom, prevout.hash.ToString().c_str(), nTimeBlockFrom, prevout.n, nTryTime,
                hashProofOfStake.ToString().c_str());
        }
        break;
    }

    return fSuccess;
}

bool CheckProofOfStake(const CBlockHeader & block, const CBlockIndex* pindexPrev, uint256 & hashProofOfStake, const Consensus::Params & consensusParams) {
    CBlockIndex *pindex = mapBlockIndex[block.hashStakeBlock];
    if (!pindex)
        return error("read block failed %s", __func__);

    unsigned int nInterval = 0;
    unsigned int nTime = block.nTime;
    const CAmount stakeAmount = block.nStakeAmount;
    if (!CheckStakeKernelHash(pindexPrev, block.nBits, pindex->GetBlockHash(), pindex->GetBlockTime(), stakeAmount,
            { block.hashStake, block.nStakeIndex }, nTime, nInterval, true, hashProofOfStake, false))
    {
        return error("check kernel failed on coinstake %s, hashProof=%s %s", // may occur during initial download or if behind on block chain sync
                     block.hashStake.ToString().c_str(), hashProofOfStake.ToString().c_str(), __func__);
    }

    return true;
}

/**
 * peercoin
 * For use with Staking Protocol V05.
 * @param blockHash
 * @param blockTime
 * @return
 */
unsigned int GetStakeEntropyBit(const uint256 & blockHash, const int64_t & blockTime) {
    if (IsProtocolV05(static_cast<uint64_t>(blockTime))) {
        unsigned int nEntropyBit = 0;
        nEntropyBit = static_cast<unsigned int>(UintToArith256(blockHash).GetLow64() & 1llu); // last bit of block hash
        LogPrint(BCLog::ALL, "GetStakeEntropyBit: hashBlock=%s nEntropyBit=%u\n", blockHash.ToString().c_str(), nEntropyBit);
        return nEntropyBit;
    }

    unsigned int nEntropyBit = static_cast<unsigned int>(UintToArith256(blockHash).Get64() & 1);
    LogPrint(BCLog::ALL, "GetStakeEntropyBit: hashBlock=%s nEntropyBit=%u\n", blockHash.ToString().c_str(), nEntropyBit);
    return nEntropyBit;
}

bool IsProofOfStake(int blockHeight, const Consensus::Params & consensusParams) {
    return blockHeight > consensusParams.lastPOWBlock;
}
bool IsProofOfStake(int blockHeight) {
    return IsProofOfStake(blockHeight, Params().GetConsensus());
}