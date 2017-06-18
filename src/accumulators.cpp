#include "accumulators.h"
#include "chainparams.h"
#include "main.h"
#include "txdb.h"
#include "init.h"

using namespace libzerocoin;

void CAccumulators::Setup()
{
    //construct accumulators for all denominations
    for (auto& denom : zerocoinDenomList) {
        unique_ptr<Accumulator> uptr(new Accumulator(Params().Zerocoin_Params(), denom));
        mapAccumulators.insert(make_pair(ZerocoinDenominationToValue(denom), move(uptr)));
    }
}

Accumulator CAccumulators::Get(CoinDenomination denomination)
{
    return Accumulator(Params().Zerocoin_Params(), denomination, mapAccumulators.at(ZerocoinDenominationToValue(denomination))->getValue());
}

//Public Coins have large 'values' that are not ideal to store in lists.
uint256 HashPublicCoin(PublicCoin publicCoin)
{
    CDataStream ss(SER_GETHASH, 0);
    ss << publicCoin.getValue() << ZerocoinDenominationToValue(publicCoin.getDenomination());

    return Hash(ss.begin(), ss.end());
}

bool CAccumulators::AddPubCoinToAccumulator(const PublicCoin& publicCoin)
{
    //see if we have already added this coin to the accumulator
    //todo: note sure if we need to check this
//    uint256 hash = HashPublicCoin(publicCoin);
//    if(mapPubCoins.find(hash) != mapPubCoins.end())
//        return false;

//    mapPubCoins.insert(make_pair(hash, ZerocoinDenominationToValue(publicCoin.getDenomination())));
    CoinDenomination denomination = publicCoin.getDenomination();
    mapAccumulators.at(ZerocoinDenominationToValue(denomination))->accumulate(publicCoin);

    return true;
}

uint32_t CAccumulators::GetChecksum(const CBigNum &bnValue)
{
    LogPrintf("GetChecksum()\n");
    CDataStream ss(SER_GETHASH, 0);
    ss << bnValue;
    uint256 hash = Hash(ss.begin(), ss.end());

    return hash.Get32();
}

uint32_t CAccumulators::GetChecksum(const Accumulator &accumulator)
{
    LogPrintf("GetChecksum()\n");
    return GetChecksum(accumulator.getValue());
}

void CAccumulators::AddAccumulatorChecksum(const uint32_t nChecksum, const CBigNum &bnValue, bool fMemoryOnly)
{
    if(!fMemoryOnly)
        zerocoinDB->WriteAccumulatorValue(nChecksum, bnValue);
    LogPrintf("*** %s checksum %d val %s\n", __func__, nChecksum, bnValue.GetHex());
    mapAccumulatorValues.insert(make_pair(nChecksum, bnValue));
    LogPrintf("*** %s map val %s\n", __func__, mapAccumulatorValues[nChecksum].GetHex());
}

void CAccumulators::LoadAccumulatorValuesFromDB(const uint256 nCheckpoint)
{
    for (auto& denomination : zerocoinDenomList) {
        uint32_t nChecksum = ParseChecksum(nCheckpoint, denomination);
        //if read is not successful then we are not in a state to verify zerocoin transactions
        CBigNum bnValue;
        assert(zerocoinDB->ReadAccumulatorValue(nChecksum, bnValue));
        mapAccumulatorValues.insert(make_pair(nChecksum, bnValue));
    }
}

bool CAccumulators::EraseAccumulatorValuesInDB(const uint256& nCheckpoint)
{
    for (auto& denomination : zerocoinDenomList) {
        uint32_t nChecksum = ParseChecksum(nCheckpoint, denomination);

        if(!zerocoinDB->EraseAccumulatorValue(nChecksum))
            return false;
    }

    return true;
}

uint32_t ParseChecksum(uint256 nChecksum, CoinDenomination denomination)
{
    //shift to the beginning bit of this denomimnation and trim any remaining bits by returning 32 bits only
    int pos = distance(zerocoinDenomList.begin(), find(zerocoinDenomList.begin(), zerocoinDenomList.end(), denomination));
    nChecksum = nChecksum >> (32*((zerocoinDenomList.size() - 1) - pos));
    return nChecksum.Get32();
}

CBigNum CAccumulators::GetAccumulatorValueFromCheckpoint(const uint256& nCheckpoint, CoinDenomination denomination)
{
    LogPrintf("%s checkpoint:%d\n", __func__, nCheckpoint.GetHex());
    uint32_t nDenominationChecksum = ParseChecksum(nCheckpoint, denomination);
    LogPrintf("%s checksum:%d\n", __func__, nDenominationChecksum);

    return GetAccumulatorValueFromChecksum(nDenominationChecksum);
}

CBigNum CAccumulators::GetAccumulatorValueFromChecksum(const uint32_t& nChecksum)
{
    LogPrintf("%s %d\n", __func__, nChecksum);
    if(!mapAccumulatorValues.count(nChecksum))
        return CBigNum(0);

    return mapAccumulatorValues[nChecksum];
}

//set all of the accumulators held by mapAccumulators to a certain checkpoint
bool CAccumulators::ResetToCheckpoint(const uint256& nCheckpoint)
{
    for (auto& denom : zerocoinDenomList) {
        CBigNum bnValue = GetAccumulatorValueFromCheckpoint(nCheckpoint, denom);
        if (bnValue == 0) {
            LogPrintf("%s: checkpoint is empty\n", __func__);
            continue; //note temporary hack ?
        }

        mapAccumulators.at(ZerocoinDenominationToValue(denom))->setValue(bnValue);
    }

    return true;
}

//Get checkpoint value from the current state of our accumulator map
uint256 CAccumulators::GetCheckpoint()
{
    uint256 nCheckpoint;
    for (auto& denom : zerocoinDenomList) {
        CBigNum bnValue = mapAccumulators.at(ZerocoinDenominationToValue(denom))->getValue();
        LogPrintf("%s: ZCPRINT acc value:%s\n", __func__, bnValue.GetHex());
        uint32_t nCheckSum = GetChecksum(bnValue);

        AddAccumulatorChecksum(nCheckSum, bnValue);
        LogPrintf("%s: ZCPRINT checksum value:%d\n", __func__, nCheckSum);
        nCheckpoint = nCheckpoint << 32 | nCheckSum;
        LogPrintf("%s: ZCPRINT checkpoint %s\n", __func__, nCheckpoint.GetHex());
    }

    return nCheckpoint;
}

//Get checkpoint value for a specific block height
bool CAccumulators::GetCheckpoint(int nHeight, uint256& nCheckpoint)
{
    if(nHeight < 11)
        return 0;

    //the checkpoint is updated every ten blocks, return current active checkpoint if not update block
    if (nHeight % 10 != 0) {
        nCheckpoint = chainActive[nHeight - 1]->nAccumulatorCheckpoint;
        return true;
    }

    //set the accumulators to last checkpoint value
    if(!ResetToCheckpoint(chainActive[nHeight - 1]->nAccumulatorCheckpoint)) {
        LogPrintf("%s: failed to reset to previous checkpoint\n", __func__);
        return false;
    }

    //Accumulate all coins over the last ten blocks that havent been accumulated (height-20 through height-11)
    CBlockIndex *pindex = chainActive[nHeight - 20];
    while (pindex->nHeight < nHeight - 10) {
        //grab mints from this block
        CBlock block;
        if(!ReadBlockFromDisk(block, pindex)) {
            LogPrintf("%s: failed to read block from disk\n", __func__);
            return false;
        }
        LogPrintf("%s ZCPRINT checking block %i\n", __func__, pindex->nHeight);
        std::list<CZerocoinMint> listMints;
        if(!BlockToZerocoinMintList(block, listMints)) {
            LogPrintf("%s: failed to get zerocoin mintlist from block %n\n", __func__, pindex->nHeight);
            return false;
        }
        LogPrintf("%s ZCPRINT found %d mints\n", __func__, listMints.size());
        //add the pubcoins to accumulator
        for(const CZerocoinMint mint : listMints) {
            CoinDenomination denomination = PivAmountToZerocoinDenomination(mint.GetDenomination());
            PublicCoin pubCoin(Params().Zerocoin_Params(), mint.GetValue(), denomination);
            if(!AddPubCoinToAccumulator(pubCoin)) {
                LogPrintf("%s: failed to add pubcoin to accumulator at height %n\n", __func__, pindex->nHeight);
                return false;
            }
        }
        pindex = chainActive[pindex->nHeight + 1];
    }

    nCheckpoint = GetCheckpoint();
    LogPrintf("%s ZCPRINT checkpoint=%s\n", __func__, nCheckpoint.GetHex());
    return true;
}

bool CAccumulators::IntializeWitnessAndAccumulator(const CZerocoinMint &zerocoinSelected, const PublicCoin &pubcoinSelected, Accumulator& accumulator, AccumulatorWitness& witness)
{
    LogPrintf("ZCPRINT %s\n", __func__);
    uint256 txMintedHash;
    if(!zerocoinDB->ReadCoinMint(zerocoinSelected.GetValue(), txMintedHash)) {
        LogPrintf("ZCPRINT %s failed to read mint from db\n", __func__);
        return false;
    }
    CTransaction txMinted;
    uint256 blockHash;
    if(!GetTransaction(txMintedHash, txMinted, blockHash))
    {
        LogPrintf("ZCPRINT %s failed to read tx\n", __func__);
        return false;
    }


    int nHeightMintAddedToBlockchain = mapBlockIndex[blockHash]->nHeight;

    list<CZerocoinMint> vMintsToAddToWitness;
    uint256 nChecksumBeforeMint = 0, nChecksumAfterMint = 0, nChecksumContainingMint = 0;
    int nChecksumBeforeMintHeight = 0;
    CBlockIndex* pindex = chainActive[nHeightMintAddedToBlockchain];
    int nChanges = 0;

    //find the checksum when this was added to the accumulator officially, which will be two checksum changes later
    LogPrintf("ZCPRINT %s before while\n", __func__);
    while (pindex->nHeight < chainActive.Tip()->nHeight - 1) {
        if(pindex->nHeight == nHeightMintAddedToBlockchain) {
            LogPrintf("ZCPRINT %s height added to chain %d\n", __func__, pindex->nHeight);
            pindex = chainActive[pindex->nHeight + 1];
            continue;
        }

        //check if the next checksum was generated
        if(pindex->nHeight % 10 == 0) {
            nChecksumContainingMint = pindex->nAccumulatorCheckpoint;
            nChanges++;

            if(nChanges == 1) {
                nChecksumBeforeMintHeight = pindex->nHeight;
                nChecksumBeforeMint = pindex->nAccumulatorCheckpoint;
            }

            LogPrintf("ZCPRINT %s using checkpoint %s from block %d\n", __func__, pindex->nAccumulatorCheckpoint.GetHex(), pindex->nHeight);

            if(nChanges == 2)
                break;
        }
        LogPrintf("ZCPRINT %s height=%d\n", __func__, pindex->nHeight);
        pindex = chainActive[pindex->nHeight + 1];
    }
    LogPrintf("ZCPRINT %s get checksum before mint\n", __func__);

    //the height to start accumulating coins to add to witness
    int nStartAccumulationHeight = nHeightMintAddedToBlockchain - (nHeightMintAddedToBlockchain % 10);


    //Get the accumulator that is right before the cluster of blocks containing our mint was added to the accumulator
    if(nChecksumBeforeMint != 2301755253) { //this is a zero value and wont initialize the accumulator. use existing.
        LogPrintf("ZCPRINT %s get acc val from checkpoint\n", __func__);
        CBigNum bnAccValue = GetAccumulatorValueFromCheckpoint(nChecksumBeforeMint, pubcoinSelected.getDenomination());
        if(bnAccValue == 0)
            return false;
        LogPrintf("ZCPRINT %s acc val %s\n", __func__, bnAccValue.GetHex());
        LogPrintf("netx\n");
        //accumulator = Accumulator(Params().Zerocoin_Params(), pubcoinSelected.getDenomination(), bnAccValue);
        //witness = AccumulatorWitness(Params().Zerocoin_Params(), accumulator, pubcoinSelected);
    }

    //add the pubcoins up to the next checksum starting from the block
    LogPrintf("ZCPRINT %s add pubcoins\n", __func__);
    pindex = chainActive[nStartAccumulationHeight];
    int nSecurityLevel = 1; //todo: this will be user defined, the more pubcoins that are added to the accumulator that is used, the more secure and untraceable it will be
    int nAccumulatorsCheckpointsAdded = 0;
    uint256 nPreviousChecksum = 0;
    while(pindex->nHeight < chainActive.Height() - 1) {

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
            PublicCoin pubCoin(Params().Zerocoin_Params(), mint.GetValue(), PivAmountToZerocoinDenomination(mint.GetDenomination()));
            witness += pubCoin;
            accumulator += pubCoin;
        }

        pindex = chainActive[pindex->nHeight + 1];
        nPreviousChecksum = block.nAccumulatorCheckpoint;
    }
    LogPrintf("%s done\n", __func__);
    return true;
}
