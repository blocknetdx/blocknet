#include "accumulators.h"
#include "chainparams.h"
#include "hash.h"
#include "streams.h"
#include "utilstrencodings.h"

void CAccumulators::Setup()
{
    //construct accumulators for all denominations
    for(int i = 0; i < libzerocoin::CoinDenomination::ZQ_WILLIAMSON; i++) {
        libzerocoin::CoinDenomination denomination = (libzerocoin::CoinDenomination)i;
        unique_ptr<libzerocoin::Accumulator> uptr(new libzerocoin::Accumulator(Params().Zerocoin_Params(), denomination));
        mapAccumulators.insert(make_pair((int)denomination, move(uptr)));
    }
}

libzerocoin::Accumulator CAccumulators::Get(libzerocoin::CoinDenomination denomination)
{
    return libzerocoin::Accumulator(Params().Zerocoin_Params(), denomination, mapAccumulators.at(denomination)->getValue());
}

//Public Coins have large 'values' that are not ideal to store in lists.
uint256 HashPublicCoin(libzerocoin::PublicCoin publicCoin)
{
    CDataStream ss(SER_GETHASH, 0);
    ss << publicCoin.getValue() << (unsigned int)publicCoin.getDenomination();

    return Hash(ss.begin(), ss.end());
}

void CAccumulators::AddPubCoinToAccumulator(libzerocoin::CoinDenomination denomination, libzerocoin::PublicCoin publicCoin)
{
    uint256 hash = HashPublicCoin(publicCoin);
    if(mapPubCoins.find(hash) == mapPubCoins.end())
        return;

    mapPubCoins.insert(make_pair(hash, (int)publicCoin.getDenomination()));
    *(mapAccumulators.at(denomination)) += publicCoin;
}

void CAccumulators::AddAccumulatorChecksum(const CBigNum &bnValue)
{
    CDataStream ss(SER_GETHASH, 0);
    ss << bnValue;
    uint256 hash = Hash(ss.begin(), ss.end());
    unsigned int checksum;
    checksum = (unsigned int)hash.Get64();
    mapAccumulatorValues.insert(make_pair(checksum, bnValue));
}

CBigNum CAccumulators::GetAccumulatorValueFromChecksum(const unsigned int nChecksum)
{
    if(!mapAccumulatorValues.count(nChecksum))
        return CBigNum(0);

    return mapAccumulatorValues[nChecksum];
}