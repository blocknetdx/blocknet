#include "accumulators.h"
#include "chainparams.h"
#include "main.h"
#include "txdb.h"

using namespace libzerocoin;

void CAccumulators::Setup()
{
    //construct accumulators for all denominations
    //todo fix loop once additional denoms added
    for(int i = 0; i < libzerocoin::CoinDenomination::ZQ_WILLIAMSON; i++) {
        libzerocoin::CoinDenomination denomination = static_cast<libzerocoin::CoinDenomination>(i);
        unique_ptr<libzerocoin::Accumulator> uptr(new libzerocoin::Accumulator(Params().Zerocoin_Params(), denomination));
        mapAccumulators.insert(make_pair((int)denomination, move(uptr)));
    }
}

Accumulator CAccumulators::Get(libzerocoin::CoinDenomination denomination)
{
    return Accumulator(Params().Zerocoin_Params(), denomination, mapAccumulators.at(denomination)->getValue());
}

//Public Coins have large 'values' that are not ideal to store in lists.
uint256 HashPublicCoin(libzerocoin::PublicCoin publicCoin)
{
    CDataStream ss(SER_GETHASH, 0);
    ss << publicCoin.getValue() << static_cast<unsigned int>(publicCoin.getDenomination());

    return Hash(ss.begin(), ss.end());
}

bool CAccumulators::AddPubCoinToAccumulator(libzerocoin::PublicCoin publicCoin)
{
    //see if we have already stored this coin - todo: how to handle reset to a certain block with this
    uint256 hash = HashPublicCoin(publicCoin);
    if(mapPubCoins.find(hash) != mapPubCoins.end())
        return false;

    mapPubCoins.insert(make_pair(hash, static_cast<int>(publicCoin.getDenomination())));
    *(mapAccumulators.at(publicCoin.getDenomination())) += publicCoin;

    return true;
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

void CAccumulators::AddAccumulatorChecksum(const uint32_t nChecksum, const CBigNum &bnValue)
{
    zerocoinDB->WriteAccumulatorValue(nChecksum, bnValue);
    mapAccumulatorValues.insert(make_pair(nChecksum, bnValue));
}

void CAccumulators::LoadAccumulatorValuesFromDB(const uint256 nCheckpoint)
{
    //todo fix loop once additional denoms added
    for (int i = 0; i < libzerocoin::CoinDenomination::ZQ_WILLIAMSON; i++) {
        CoinDenomination denomination = static_cast<CoinDenomination>(i);
        uint32_t nChecksum = ParseChecksum(nCheckpoint, denomination);
        //if read is not successful then we are not in a state to verify zerocoin transactions
        CBigNum bnValue;
        assert(zerocoinDB->ReadAccumulatorValue(nChecksum, bnValue));
        mapAccumulatorValues.insert(make_pair(nChecksum, bnValue));
    }
}

uint32_t ParseChecksum(uint256 nChecksum, libzerocoin::CoinDenomination denomination)
{
    nChecksum >>= (32*(7-denomination)); //7 because 8 denominations
    return nChecksum.Get32();
}

CBigNum CAccumulators::GetAccumulatorValueFromCheckpoint(const uint256 nCheckpoint, libzerocoin::CoinDenomination denomination)
{
    uint32_t nDenominationChecksum = ParseChecksum(nCheckpoint, denomination);

    return GetAccumulatorValueFromChecksum(nDenominationChecksum);
}

CBigNum CAccumulators::GetAccumulatorValueFromChecksum(uint32_t nChecksum)
{
    if(!mapAccumulatorValues.count(nChecksum))
        return CBigNum(0);

    return mapAccumulatorValues[nChecksum];
}

//set all of the accumulators held by mapAccumulators to a certain checkpoint
bool CAccumulators::ResetToCheckpoint(uint256 nCheckpoint)
{
    //todo fix loop once additional denoms added
    for (int i = 0; i < libzerocoin::CoinDenomination::ZQ_WILLIAMSON; i++) {
        CoinDenomination denomination = static_cast<CoinDenomination>(i);

        CBigNum bnValue = GetAccumulatorValueFromCheckpoint(nCheckpoint, denomination);
        if (bnValue == 0)
            return false;

        mapAccumulators.at(denomination)->setValue(bnValue);
    }

    return true;
}

//Get checkpoint value from the current state of our accumulator map
uint256 CAccumulators::GetCheckpoint()
{
    uint256 nCheckpoint; //todo: this will not work properly until we have 8 denominations (it won't consume all bits)
    for (int i = 0; i < libzerocoin::CoinDenomination::ZQ_WILLIAMSON; i++) {
        CoinDenomination denomination = static_cast<CoinDenomination>(i);

        uint32_t nCheckSum = GetChecksum(mapAccumulators.at(denomination)->getValue());
        AddAccumulatorChecksum(nCheckSum, mapAccumulators.at(denomination)->getValue());

        nCheckpoint <<= 32 | nCheckSum;
    }

    return nCheckpoint;
}

//Get checkpoint value for a specific block height
uint256 CAccumulators::GetCheckpoint(int nHeight)
{
    if(nHeight < 11)
        return 0;

    //the checkpoint is updated every ten blocks, return current active checkpoint if not update block
    if (nHeight % 10 != 0)
        return chainActive[nHeight - 1]->nAccumulatorCheckpoint;

    //set the accumulators to last checkpoint value
    if(!ResetToCheckpoint(chainActive[nHeight - 1]->nAccumulatorCheckpoint))
        return 0;

    //Accumulate all coins over the last ten blocks that havent been accumulated (height-20 through height-11)
    CBlockIndex *pindex = chainActive[nHeight - 20];
    while (pindex->nHeight < nHeight - 10) {
        //grab mints from this block
        CBlock block;
        if(!ReadBlockFromDisk(block, pindex)) {
            LogPrintf("%s: failed to read block from disk\n", __func__);
            return false;
        }

        std::list<CZerocoinMint> listMints;
        if(!BlockToZerocoinMintList(block, listMints)) {
            LogPrintf("%s: failed to get zerocoin mintlist from block %n\n", __func__, pindex->nHeight);
            return false;
        }

        //add the pubcoins to accumulator
        for(const CZerocoinMint mint : listMints) {
            CoinDenomination denomination = static_cast<libzerocoin::CoinDenomination>(mint.GetDenomination());
            libzerocoin::PublicCoin pubCoin(Params().Zerocoin_Params(), mint.GetValue(), denomination);
            if(!AddPubCoinToAccumulator(pubCoin)) {
                LogPrintf("%s: failed to add pubcoin to accumulator at height %n\n", __func__, pindex->nHeight);
                return false;
            }
        }
        pindex = chainActive[pindex->nHeight + 1];
    }

    return GetCheckpoint();
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
            nChecksumBeforeMint = pindex->nAccumulatorCheckpoint;
            nChecksumContainingMint = pindex->nAccumulatorCheckpoint;
            continue;
        }

        //check if the next checksum was generated
        if(pindex->nAccumulatorCheckpoint != nChecksumContainingMint) {
            nChecksumContainingMint = pindex->nAccumulatorCheckpoint;
            nChanges ++;

            if(nChanges == 1)
                nChecksumAfterMint = pindex->nAccumulatorCheckpoint; // this is where we will init the witness and start adding pubcoins to
            else if(nChanges == 2)
                break;
        }
    }

    //get block height that nChecksumBeforeMint was generated on
    pindex = chainActive[nHeightMintAddedToBlockchain];
    int nChecksumBeforeMintHeight = 0;
    while(pindex->nHeight > 2) {
        //if the previous block has a different checksum, it means this is the height to begin adding pubcoins to
        if(pindex->pprev->nAccumulatorCheckpoint != nChecksumBeforeMint) {
            nChecksumBeforeMintHeight = pindex->nHeight;
            break;
        }

        pindex = pindex->pprev;
    }

    //Get the accumulator that is right before the cluster of blocks containing our mint was added to the accumulator
    CBigNum bnAccValue = GetAccumulatorValueFromCheckpoint(nChecksumAfterMint, pubcoinSelected.getDenomination());
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

        if(nPreviousChecksum != 0 && nPreviousChecksum != pindex->nAccumulatorCheckpoint)
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
        nPreviousChecksum = block.nAccumulatorCheckpoint;
    }

    return true;
}