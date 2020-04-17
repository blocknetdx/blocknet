// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2017-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow.h>

#include <arith_uint256.h>
#include <chain.h>
#include <sync.h>
#include <util/system.h>
#include <validation.h>

// from kernel.h
bool IsProofOfStake(int blockHeight);
bool CheckProofOfStake(const CBlockHeader & block, const CBlockIndex *pindexPrev, uint256 & hashProofOfStake, const Consensus::Params & consensusParams);

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader */*pblock*/, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);

    // Use blocknet work
    return BlocknetGetNextWorkRequired(pindexLast, params);

//    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();
//
//    // Only change once per difficulty adjustment interval
//    if ((pindexLast->nHeight+1) % params.DifficultyAdjustmentInterval() != 0)
//    {
//        if (params.fPowAllowMinDifficultyBlocks)
//        {
//            // Special difficulty rule for testnet:
//            // If the new block's timestamp is more than 2* 10 minutes
//            // then allow mining of a min-difficulty block.
//            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing*2)
//                return nProofOfWorkLimit;
//            else
//            {
//                // Return the last non-special-min-difficulty-rules-block
//                const CBlockIndex* pindex = pindexLast;
//                while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 && pindex->nBits == nProofOfWorkLimit)
//                    pindex = pindex->pprev;
//                return pindex->nBits;
//            }
//        }
//        return pindexLast->nBits;
//    }
//
//    // Go back by what we want to be 14 days worth of blocks
//    int nHeightFirst = pindexLast->nHeight - (params.DifficultyAdjustmentInterval()-1);
//    assert(nHeightFirst >= 0);
//    const CBlockIndex* pindexFirst = pindexLast->GetAncestor(nHeightFirst);
//    assert(pindexFirst);
//
//    return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params);
}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    if (nActualTimespan < params.nPowTargetTimespan/4)
        nActualTimespan = params.nPowTargetTimespan/4;
    if (nActualTimespan > params.nPowTargetTimespan*4)
        nActualTimespan = params.nPowTargetTimespan*4;

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}

bool CheckPoS(const CBlockHeader & block, CValidationState & state, uint256 & hashProofOfStake, const Consensus::Params & params)
{
    // Get prev block index
    CBlockIndex *pindexPrev = nullptr;
    {
        LOCK(cs_main);
        pindexPrev = LookupBlockIndex(block.hashPrevBlock);
    }
    if (!pindexPrev) {
        state.DoS(0, error("%s : prev block %s not found", __func__, block.hashPrevBlock.ToString().c_str()), 0, "bad-prevblk");
        return false;
    }
    if (pindexPrev->nStatus & BLOCK_FAILED_MASK) {
        state.DoS(100, error("%s : prev block invalid", __func__), REJECT_INVALID, "bad-prevblk");
        return false;
    }
    
    // Block height
    int currentHeight = pindexPrev->nHeight + 1;
    
    // If not PoS invalidate the block
    if (!IsProofOfStake(currentHeight)) {
        state.DoS(50, false, REJECT_INVALID, "bad-stake", false, "bad pow or pos block");
        return false;
    }

    unsigned int nBitsRequired = GetNextWorkRequired(pindexPrev, nullptr, params);
    if (block.nBits != nBitsRequired) {
        state.DoS(50, error("%s : prev block invalid", __func__), REJECT_INVALID, "bad-prevblk");
        return error("%s : incorrect work at %d", __func__, currentHeight);
    }

    if (!CheckProofOfStake(block, pindexPrev, hashProofOfStake, params)) {
        state.DoS(50, false, REJECT_INVALID, "bad-stake", false, "bad pow or pos block");
        return false;
    }

    return true;
}

unsigned int BlocknetGetNextWorkRequired(const CBlockIndex* pindexLast, const Consensus::Params& params)
{
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    /* current difficulty formula, DarkGravity v3, written by Evan Duffield - evan@dashpay.io */
    const CBlockIndex* BlockLastSolved = pindexLast;
    const CBlockIndex* BlockReading = pindexLast;
    int64_t nActualTimespan = 0;
    int64_t LastBlockTime = 0;
    int64_t PastBlocksMin = 24;
    int64_t PastBlocksMax = 24;
    int64_t CountBlocks = 0;
    arith_uint256 PastDifficultyAverage;
    arith_uint256 PastDifficultyAveragePrev;

    if (BlockLastSolved == NULL || BlockLastSolved->nHeight == 0 || BlockLastSolved->nHeight < PastBlocksMin)
        return nProofOfWorkLimit;

    // Use algo for non-POW blocks
    if (pindexLast->nHeight > params.lastPOWBlock) {
        arith_uint256 bnTargetLimit = UintToArith256(uint256S("0x000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"));
        // Support minimum difficulty blocks if flag set
        if (params.stakingAllowsMinDifficultyBlocks && pindexLast->GetBlockTime() - pindexLast->pprev->GetBlockTime() > params.nPowTargetSpacing)
            bnTargetLimit = UintToArith256(params.powLimit);

        int64_t nActualSpacing = 0;
        if (pindexLast->nHeight != 0)
            nActualSpacing = pindexLast->GetBlockTime() - pindexLast->pprev->GetBlockTime();

        if (nActualSpacing < 0)
            nActualSpacing = 1;

        // ppcoin: target change every block
        // ppcoin: retarget with exponential moving toward target spacing
        arith_uint256 bnNew;
        bnNew.SetCompact(pindexLast->nBits);

        int64_t nInterval = params.stakingPoSTargetTimespan / params.nPowTargetSpacing;
        bnNew *= ((nInterval - 1) * params.nPowTargetSpacing + nActualSpacing + nActualSpacing);
        bnNew /= ((nInterval + 1) * params.nPowTargetSpacing);

        if (bnNew <= 0 || bnNew > bnTargetLimit)
            bnNew = bnTargetLimit;

        return bnNew.GetCompact();
    }

    for (unsigned int i = 1; BlockReading && BlockReading->nHeight > 0; i++) {
        if (PastBlocksMax > 0 && i > PastBlocksMax) {
            break;
        }
        CountBlocks++;

        if (CountBlocks <= PastBlocksMin) {
            if (CountBlocks == 1) {
                PastDifficultyAverage.SetCompact(BlockReading->nBits);
            } else {
                PastDifficultyAverage = ((PastDifficultyAveragePrev * CountBlocks) + (UintToArith256(uint256()).SetCompact(BlockReading->nBits))) / (CountBlocks + 1);
            }
            PastDifficultyAveragePrev = PastDifficultyAverage;
        }

        if (LastBlockTime > 0) {
            int64_t Diff = (LastBlockTime - BlockReading->GetBlockTime());
            nActualTimespan += Diff;
        }
        LastBlockTime = BlockReading->GetBlockTime();

        if (BlockReading->pprev == NULL) {
            assert(BlockReading);
            break;
        }
        BlockReading = BlockReading->pprev;
    }

    arith_uint256 bnNew(PastDifficultyAverage);

    int64_t _nTargetTimespan = CountBlocks * params.nPowTargetSpacing;

    if (nActualTimespan < _nTargetTimespan / 3)
        nActualTimespan = _nTargetTimespan / 3;
    if (nActualTimespan > _nTargetTimespan * 3)
        nActualTimespan = _nTargetTimespan * 3;

    // Retarget
    bnNew *= nActualTimespan;
    bnNew /= _nTargetTimespan;

    if (bnNew > UintToArith256(params.powLimit))
        bnNew = nProofOfWorkLimit;

    return bnNew.GetCompact();
}