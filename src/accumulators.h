#ifndef PIVX_ACCUMULATORS_H
#define PIVX_ACCUMULATORS_H

#include "libzerocoin/Zerocoin.h"
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
    std::map<int, std::unique_ptr<libzerocoin::Accumulator> > mapAccumulators;
    std::map<uint256, int> mapPubCoins;
    std::map<unsigned int, CBigNum> mapAccumulatorValues;

    CAccumulators() { Setup(); }
    void Setup();

public:
    CAccumulators(CAccumulators const&) = delete;
    void operator=(CAccumulators const&) = delete;

    libzerocoin::Accumulator Get(libzerocoin::CoinDenomination denomination);
    void AddPubCoinToAccumulator(libzerocoin::CoinDenomination denomination, libzerocoin::PublicCoin publicCoin);
    void AddAccumulatorChecksum(const CBigNum &bnValue);
    CBigNum GetAccumulatorValueFromChecksum(const unsigned int nChecksum);
    //CBigNum GetAccumulatorValueFromBlock(CoinDenomination denomination, int nBlockHeight);
    //bool VerifyWitness(CoinDenomination denomination, int nBlockHeight, CBigNum witness);
};


#endif //PIVX_ACCUMULATORS_H
