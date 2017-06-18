#ifndef PIVX_ACCUMULATORS_H
#define PIVX_ACCUMULATORS_H

#include "libzerocoin/Accumulator.h"
#include "libzerocoin/Denominations.h"
#include "libzerocoin/Coin.h"
#include "primitives/zerocoin.h"
#include "uint256.h"

class CAccumulators
{
public:
    static CAccumulators& getInstance()
    {
        static CAccumulators instance;
        return instance;
    }
private:
    std::map<int64_t, std::unique_ptr<libzerocoin::Accumulator> > mapAccumulators;
    std::map<uint256, int> mapPubCoins;
    std::map<uint32_t, CBigNum> mapAccumulatorValues;

    CAccumulators() { Setup(); }
    void Setup();

public:
    CAccumulators(CAccumulators const&) = delete;
    void operator=(CAccumulators const&) = delete;

    libzerocoin::Accumulator Get(libzerocoin::CoinDenomination denomination);
    bool AddPubCoinToAccumulator(const libzerocoin::PublicCoin& publicCoin);
    bool IntializeWitnessAndAccumulator(const CZerocoinMint &zerocoinSelected, const libzerocoin::PublicCoin &pubcoinSelected, libzerocoin::Accumulator& accumulator, libzerocoin::AccumulatorWitness& witness);

    //checksum/checkpoint
    void AddAccumulatorChecksum(const uint32_t nChecksum, const CBigNum &bnValue, bool fMemoryOnly = false);
    void LoadAccumulatorValuesFromDB(const uint256 nCheckpoint);
    bool EraseAccumulatorValuesInDB(const uint256& nCheckpoint);
    uint32_t GetChecksum(const CBigNum &bnValue);
    uint32_t GetChecksum(const libzerocoin::Accumulator &accumulator);
    uint256 GetCheckpoint();
    bool GetCheckpoint(int nHeight, uint256& nCheckpoint);
    CBigNum GetAccumulatorValueFromChecksum(const uint32_t& nChecksum);
    CBigNum GetAccumulatorValueFromCheckpoint(const uint256& nCheckpoint, libzerocoin::CoinDenomination denomination);
    bool ResetToCheckpoint(const uint256& nCheckpoint);
};

uint32_t ParseChecksum(uint256 nChecksum, libzerocoin::CoinDenomination denomination);


#endif //PIVX_ACCUMULATORS_H
