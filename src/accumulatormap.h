// Copyright (c) 2017 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef PHORE_ACCUMULATORMAP_H
#define PHORE_ACCUMULATORMAP_H

#include "accumulatorcheckpoints.h"
#include "libzerocoin/Accumulator.h"
#include "libzerocoin/Coin.h"

//A map with an accumulator for each denomination
class AccumulatorMap
{
private:
    std::map<libzerocoin::CoinDenomination, std::unique_ptr<libzerocoin::Accumulator> > mapAccumulators;
    libzerocoin::ZerocoinParams* params;
public:
    AccumulatorMap(libzerocoin::ZerocoinParams* currentParams);
    bool Load(uint256 nCheckpoint);
    void Load(const AccumulatorCheckpoints::Checkpoint& checkpoint);
    bool Accumulate(libzerocoin::PublicCoin pubCoin, bool fSkipValidation = false);
    CBigNum GetValue(libzerocoin::CoinDenomination denom);
    libzerocoin::ZerocoinParams* GetZerocoinParams();
    void SetZerocoinParams(libzerocoin::ZerocoinParams* params);
    uint256 GetCheckpoint();
    void Reset();
};
#endif //PHORE_ACCUMULATORMAP_H
