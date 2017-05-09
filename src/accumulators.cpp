#include "accumulators.h"
#include "hash.h"
#include "utilstrencodings.h"


//todo add this to chain params instead of copy and paste it everywhere.
#define ZEROCOIN_MODULUS   "25195908475657893494027183240048398571429282126204032027777137836043662020707595556264018525880784406918290641249515082189298559149176184502808489120072844992687392807287776735971418347270261896375014971824691165077613379859095700097330459748808428401797429100642458691817195118746121515172654632282216869987549182422433637259085141865462043576798423387184774447920739934236584823824281198163815010674810451660377306056201619676256133844143603833904414952634432190114657544454178424020924616515723350778707749817125772467962926386356373289912154831438167899885040445364023527381951378636564391212010397122822120720357"
static CBigNum bnTrustedModulus;
bool setParams = bnTrustedModulus.SetHexBool(ZEROCOIN_MODULUS);
static libzerocoin::Params *ZCParams = new libzerocoin::Params(bnTrustedModulus);

void CAccumulators::Setup()
{
    //construct accumulators for all denominations
    for(int i = 0; i < CoinDenomination::ZQ_WILLIAMSON; i++) {
        CoinDenomination denomination = (CoinDenomination)i;
        unique_ptr<Accumulator> uptr(new Accumulator(ZCParams, denomination));
        mapAccumulators.insert(make_pair((int)denomination, move(uptr)));
    }
}

//Public Coins have large 'values' that are not ideal to store in lists.
uint256 HashPublicCoin(PublicCoin publicCoin)
{
    uint256 hashOut;
    CBigNum coinValue = publicCoin.getValue();
    Hash(BEGIN(coinValue), coinValue.bitSize()/8, hashOut.begin()); //8 bits per byte

    return hashOut;
}

void CAccumulators::AddPubCoinToAccumulator(CoinDenomination denomination, PublicCoin publicCoin)
{
    uint256 hash = HashPublicCoin(publicCoin);
    if(mapPubCoins.find(hash) == mapPubCoins.end())
        return;

    mapPubCoins.insert(make_pair(hash, (int)publicCoin.getDenomination()));
    *(mapAccumulators.at(denomination)) += publicCoin;
}