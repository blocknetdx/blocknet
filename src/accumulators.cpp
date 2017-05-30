#include "accumulators.h"
#include "chainparams.h"
#include "main.h"


void CAccumulators::Setup()
{
    //construct accumulators for all denominations
    for(int i = 0; i < libzerocoin::CoinDenomination::ZQ_WILLIAMSON; i++) {
        libzerocoin::CoinDenomination denomination = static_cast<libzerocoin::CoinDenomination>(i);
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
    ss << publicCoin.getValue() << static_cast<unsigned int>(publicCoin.getDenomination());

    return Hash(ss.begin(), ss.end());
}

void CAccumulators::AddPubCoinToAccumulator(libzerocoin::CoinDenomination denomination, libzerocoin::PublicCoin publicCoin)
{
    uint256 hash = HashPublicCoin(publicCoin);
    if(mapPubCoins.find(hash) == mapPubCoins.end())
        return;

    mapPubCoins.insert(make_pair(hash, static_cast<int>(publicCoin.getDenomination())));
    *(mapAccumulators.at(denomination)) += publicCoin;
}

uint32_t CAccumulators::GetChecksum(const CBigNum &bnValue)
{
    CDataStream ss(SER_GETHASH, 0);
    ss << bnValue;
    uint256 hash = Hash(ss.begin(), ss.end());

    return hash.Get32();
}

uint32_t CAccumulators::GetChecksum(const libzerocoin::Accumulator &accumulator)
{
    return GetChecksum(accumulator.getValue());
}

void CAccumulators::AddAccumulatorChecksum(const CBigNum &bnValue)
{
    mapAccumulatorValues.insert(make_pair(GetChecksum(bnValue), bnValue));
}

uint32_t ParseChecksum(uint256 nChecksum, libzerocoin::CoinDenomination denomination)
{
    nChecksum >>= (32*(7-denomination)); //7 because 8 denominations
    return nChecksum.Get32();
}

CBigNum CAccumulators::GetAccumulatorValueFromChecksum(const uint256 nChecksum, libzerocoin::CoinDenomination denomination)
{
    uint32_t nDenominationChecksum = ParseChecksum(nChecksum, denomination);

    if(!mapAccumulatorValues.count(nDenominationChecksum))
        return CBigNum(0);

    return mapAccumulatorValues[nDenominationChecksum];
}

bool CAccumulators::IntializeWitnessAndAccumulator(const CZerocoinMint &zerocoinSelected, const libzerocoin::PublicCoin &pubcoinSelected, libzerocoin::Accumulator& accumulator, libzerocoin::AccumulatorWitness& witness)
{
    int nHeightMintAddedToBlockchain = zerocoinSelected.GetHeight();

    list<CZerocoinMint> vMintsToAddToWitness;
    uint256 nChecksumBeforeMint = 0, nChecksumAfterMint = 0, nChecksumContainingMint = 0;
    CBlockIndex* pindex = chainActive[nHeightMintAddedToBlockchain];
    int nChanges = 0;

    //find the checksum when this was added to the accumulator officially, which will be two checksum changes later
    while (pindex->nHeight < chainActive.Tip()->nHeight) {
        if(pindex->nHeight == nHeightMintAddedToBlockchain) {
            nChecksumBeforeMint = pindex->nAccumulatorChecksum;
            nChecksumContainingMint = pindex->nAccumulatorChecksum;
            continue;
        }

        //check if the next checksum was generated
        if(pindex->nAccumulatorChecksum != nChecksumContainingMint) {
            nChecksumContainingMint = pindex->nAccumulatorChecksum;
            nChanges ++;

            if(nChanges == 1)
                nChecksumAfterMint = pindex->nAccumulatorChecksum; // this is where we will init the witness and start adding pubcoins to
            else if(nChanges == 2)
                break;
        }
    }

    //get block height that nChecksumBeforeMint was generated on
    pindex = chainActive[nHeightMintAddedToBlockchain];
    int nChecksumBeforeMintHeight = 0;
    while(pindex->nHeight > 2) {
        //if the previous block has a different checksum, it means this is the height to begin adding pubcoins to
        if(pindex->pprev->nAccumulatorChecksum != nChecksumBeforeMint) {
            nChecksumBeforeMintHeight = pindex->nHeight;
            break;
        }

        pindex = pindex->pprev;
    }

    //Get the accumulator that is right before the cluster of blocks containing our mint was added to the accumulator
    CBigNum bnAccValue = GetAccumulatorValueFromChecksum(nChecksumAfterMint, pubcoinSelected.getDenomination()); //todo: pass appropriate checksum
    if(bnAccValue == 0)
        return false;

    accumulator = libzerocoin::Accumulator(Params().Zerocoin_Params(), pubcoinSelected.getDenomination(), bnAccValue);
    witness = libzerocoin::AccumulatorWitness(Params().Zerocoin_Params(), accumulator, pubcoinSelected);

    //add the pubcoins up to the next checksum starting from the block
    pindex = chainActive[nChecksumBeforeMintHeight];
    int nSecurityLevel = 10; //todo: this will be user defined, the more pubcoins that are added to the accumulator that is used, the more secure and untraceable it will be
    int nAccumulatorsCheckpointsAdded = 0;
    uint256 nPreviousChecksum = 0;
    while(pindex->nHeight < chainActive.Height()) {

        if(nPreviousChecksum != 0 && nPreviousChecksum != pindex->nAccumulatorChecksum)
            ++nAccumulatorsCheckpointsAdded;

        //if a new checkpoint was generated on this block, and we have added the specified amount of checkpointed accumulators,
        //then break here
        if(nAccumulatorsCheckpointsAdded >= nSecurityLevel)
            break;

        //grab mints from this block
        CBlock block;
        if(!ReadBlockFromDisk(block, pindex)) {
            LogPrintf("%s: failed to read block from disk while adding pubcoins to witness\n", __func__);
            return false;
        }

        std::list<CZerocoinMint> listMints;
        if(!BlockToZerocoinMintList(block, listMints)) {
            LogPrintf("%s: failed to get zerocoin mintlist from block %n\n", __func__, pindex->nHeight);
            return false;
        }

        //add the mints to the witness
        for(const CZerocoinMint mint : listMints) {
            libzerocoin::PublicCoin pubCoin(Params().Zerocoin_Params(), mint.GetValue(), static_cast<libzerocoin::CoinDenomination>(mint.GetDenomination()));
            witness += pubCoin;
        }

        pindex = chainActive[pindex->nHeight + 1];
        nPreviousChecksum = block.nAccumulatorChecksum;
    }

    return true;
}