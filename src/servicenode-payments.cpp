// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The PIVX developers
// Copyright (c) 2015-2018 The Blocknet developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "servicenode-payments.h"
#include "addrman.h"
#include "servicenode-budget.h"
#include "servicenode-sync.h"
#include "servicenodeman.h"
#include "obfuscation.h"
#include "spork.h"
#include "sync.h"
#include "util.h"
#include "utilmoneystr.h"
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

/** Object for who's going to get paid on which blocks */
CServicenodePayments servicenodePayments;

CCriticalSection cs_vecPayments;
CCriticalSection cs_mapServicenodeBlocks;
CCriticalSection cs_mapServicenodePayeeVotes;

//
// CServicenodePaymentDB
//

CServicenodePaymentDB::CServicenodePaymentDB()
{
    pathDB = GetDataDir() / "mnpayments.dat";
    strMagicMessage = "ServicenodePayments";
}

bool CServicenodePaymentDB::Write(const CServicenodePayments& objToSave)
{
    int64_t nStart = GetTimeMillis();

    // serialize, checksum data up to that point, then append checksum
    CDataStream ssObj(SER_DISK, CLIENT_VERSION);
    ssObj << strMagicMessage;                   // servicenode cache file specific magic message
    ssObj << FLATDATA(Params().MessageStart()); // network specific magic number
    ssObj << objToSave;
    uint256 hash = Hash(ssObj.begin(), ssObj.end());
    ssObj << hash;

    // open output file, and associate with CAutoFile
    FILE* file = fopen(pathDB.string().c_str(), "wb");
    CAutoFile fileout(file, SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("%s : Failed to open file %s", __func__, pathDB.string());

    // Write and commit header, data
    try {
        fileout << ssObj;
    } catch (std::exception& e) {
        return error("%s : Serialize or I/O error - %s", __func__, e.what());
    }
    fileout.fclose();

    LogPrintf("Written info to mnpayments.dat  %dms\n", GetTimeMillis() - nStart);

    return true;
}

CServicenodePaymentDB::ReadResult CServicenodePaymentDB::Read(CServicenodePayments& objToLoad, bool fDryRun)
{
    int64_t nStart = GetTimeMillis();
    // open input file, and associate with CAutoFile
    FILE* file = fopen(pathDB.string().c_str(), "rb");
    CAutoFile filein(file, SER_DISK, CLIENT_VERSION);
    if (filein.IsNull()) {
        error("%s : Failed to open file %s", __func__, pathDB.string());
        return FileError;
    }

    // use file size to size memory buffer
    int fileSize = boost::filesystem::file_size(pathDB);
    int dataSize = fileSize - sizeof(uint256);
    // Don't try to resize to a negative number if file is small
    if (dataSize < 0)
        dataSize = 0;
    vector<unsigned char> vchData;
    vchData.resize(dataSize);
    uint256 hashIn;

    // read data and checksum from file
    try {
        filein.read((char*)&vchData[0], dataSize);
        filein >> hashIn;
    } catch (std::exception& e) {
        error("%s : Deserialize or I/O error - %s", __func__, e.what());
        return HashReadError;
    }
    filein.fclose();

    CDataStream ssObj(vchData, SER_DISK, CLIENT_VERSION);

    // verify stored checksum matches input data
    uint256 hashTmp = Hash(ssObj.begin(), ssObj.end());
    if (hashIn != hashTmp) {
        error("%s : Checksum mismatch, data corrupted", __func__);
        return IncorrectHash;
    }

    unsigned char pchMsgTmp[4];
    std::string strMagicMessageTmp;
    try {
        // de-serialize file header (servicenode cache file specific magic message) and ..
        ssObj >> strMagicMessageTmp;

        // ... verify the message matches predefined one
        if (strMagicMessage != strMagicMessageTmp) {
            error("%s : Invalid servicenode payement cache magic message", __func__);
            return IncorrectMagicMessage;
        }


        // de-serialize file header (network specific magic number) and ..
        ssObj >> FLATDATA(pchMsgTmp);

        // ... verify the network matches ours
        if (memcmp(pchMsgTmp, Params().MessageStart(), sizeof(pchMsgTmp))) {
            error("%s : Invalid network magic number", __func__);
            return IncorrectMagicNumber;
        }

        // de-serialize data into CServicenodePayments object
        ssObj >> objToLoad;
    } catch (std::exception& e) {
        objToLoad.Clear();
        error("%s : Deserialize or I/O error - %s", __func__, e.what());
        return IncorrectFormat;
    }

    LogPrintf("Loaded info from mnpayments.dat  %dms\n", GetTimeMillis() - nStart);
    LogPrintf("  %s\n", objToLoad.ToString());
    if (!fDryRun) {
        LogPrintf("Servicenode payments manager - cleaning....\n");
        objToLoad.CleanPaymentList();
        LogPrintf("Servicenode payments manager - result:\n");
        LogPrintf("  %s\n", objToLoad.ToString());
    }

    return Ok;
}

void DumpServicenodePayments()
{
    int64_t nStart = GetTimeMillis();

    CServicenodePaymentDB paymentdb;
    CServicenodePayments tempPayments;

    LogPrintf("Verifying mnpayments.dat format...\n");
    CServicenodePaymentDB::ReadResult readResult = paymentdb.Read(tempPayments, true);
    // there was an error and it was not an error on file opening => do not proceed
    if (readResult == CServicenodePaymentDB::FileError)
        LogPrintf("Missing budgets file - mnpayments.dat, will try to recreate\n");
    else if (readResult != CServicenodePaymentDB::Ok) {
        LogPrintf("Error reading mnpayments.dat: ");
        if (readResult == CServicenodePaymentDB::IncorrectFormat)
            LogPrintf("magic is ok but data has invalid format, will try to recreate\n");
        else {
            LogPrintf("file format is unknown or invalid, please fix it manually\n");
            return;
        }
    }
    LogPrintf("Writting info to mnpayments.dat...\n");
    paymentdb.Write(servicenodePayments);

    LogPrintf("Budget dump finished  %dms\n", GetTimeMillis() - nStart);
}

bool IsBlockValueValid(const CBlock& block, CAmount nExpectedValue, CAmount nMinted)
{
    CBlockIndex* pindexPrev = chainActive.Tip();
    if (pindexPrev == NULL) return true;

    int nHeight = 0;
    if (pindexPrev->GetBlockHash() == block.hashPrevBlock) {
        nHeight = pindexPrev->nHeight + 1;
    } else { //out of order
        BlockMap::iterator mi = mapBlockIndex.find(block.hashPrevBlock);
        if (mi != mapBlockIndex.end() && (*mi).second)
            nHeight = (*mi).second->nHeight + 1;
    }

    if (nHeight == 0) {
        LogPrintf("IsBlockValueValid() : WARNING: Couldn't find previous block\n");
    }

    if (!servicenodeSync.IsSynced()) {
        // Check for superblock and allow superblock budget when in non-sync'd state due to
        // unavailability of budget data
        if (nHeight > 0 && nHeight % GetBudgetPaymentCycleBlocks() == 0) {
            return nMinted <= CBudgetManager::GetTotalBudget(nHeight) + nExpectedValue;
        } else if (nMinted > nExpectedValue) {
            return false;
        }
    } else {
        // Ignore superblock check if it's disabled
        if (!IsSporkActive(SPORK_13_ENABLE_SUPERBLOCKS)) {
            return nMinted <= nExpectedValue;
        }

        // Check for valid superblock and allow superblock payment
        if (budget.IsBudgetPaymentBlock(nHeight)) {
            return nMinted <= CBudgetManager::GetTotalBudget(nHeight) + nExpectedValue;
        } else if (nMinted > nExpectedValue) {
            return false;
        }
    }

    return true;
}

bool IsBlockPayeeValid(const CBlock& block, int nBlockHeight)
{
    if (!servicenodeSync.IsSynced()) { //there is no budget data to use to check anything -- find the longest chain
        LogPrint("mnpayments", "Client not synced, skipping block payee checks\n");
        return true;
    }

    const CTransaction& txNew = (nBlockHeight > Params().LAST_POW_BLOCK() ? block.vtx[1] : block.vtx[0]);

    // If superblock check that payees are valid
    if (IsSporkActive(SPORK_13_ENABLE_SUPERBLOCKS) && budget.IsBudgetPaymentBlock(nBlockHeight)) {
        if (budget.IsTransactionValid(txNew, nBlockHeight))
            return true;

        LogPrintf("Invalid budget payment detected %s\n", txNew.ToString().c_str());
        if (IsSporkActive(SPORK_9_SERVICENODE_BUDGET_ENFORCEMENT))
            return false;

        LogPrintf("Budget enforcement is disabled, accepting block\n");
        return true;
    }

    //check for servicenode payee
    if (servicenodePayments.IsTransactionValid(txNew, nBlockHeight))
        return true;
    LogPrintf("Invalid mn payment detected %s\n", txNew.ToString().c_str());

    if (IsSporkActive(SPORK_8_SERVICENODE_PAYMENT_ENFORCEMENT))
        return false;
    LogPrintf("Servicenode payment enforcement is disabled, accepting block\n");

    return true;
}


void FillBlockPayee(CMutableTransaction& txNew, int64_t nFees, bool fProofOfStake)
{
    CBlockIndex* pindexPrev = chainActive.Tip();
    if (!pindexPrev) return;

    // Handle superblock payments
    int superblock = pindexPrev->nHeight + 1;
    if (IsSporkActive(SPORK_13_ENABLE_SUPERBLOCKS) && budget.IsBudgetPaymentBlock(superblock)) {
        // If the budget payment fails, pay a servicenode. Only supporting budget payments on PoS blocks
        if (!fProofOfStake || !budget.FillBlockPayees(txNew, superblock))
            servicenodePayments.FillBlockPayee(txNew, nFees, fProofOfStake);
        return;
    }

    servicenodePayments.FillBlockPayee(txNew, nFees, fProofOfStake);
}

std::string GetRequiredPaymentsString(int nBlockHeight)
{
    if (IsSporkActive(SPORK_13_ENABLE_SUPERBLOCKS) && budget.IsBudgetPaymentBlock(nBlockHeight)) {
        return budget.GetRequiredPaymentsString(nBlockHeight);
    } else {
        return servicenodePayments.GetRequiredPaymentsString(nBlockHeight);
    }
}

void CServicenodePayments::FillBlockPayee(CMutableTransaction& txNew, int64_t /*nFees*/, bool fProofOfStake)
{
    CBlockIndex* pindexPrev = chainActive.Tip();
    if (!pindexPrev) return;

    bool hasPayment = true;
    CScript payee;

    //spork
    if (!servicenodePayments.GetBlockPayee(pindexPrev->nHeight + 1, payee)) {
        //no servicenode detected
        CServicenode* winningNode = mnodeman.GetCurrentServiceNode(1);
        if (winningNode) {
            payee = GetScriptForDestination(winningNode->pubKeyCollateralAddress.GetID());
        } else {
            LogPrintf("CreateNewBlock: Failed to detect servicenode to pay\n");
            hasPayment = false;
        }
    }

    CAmount blockValue = GetBlockValue(pindexPrev->nHeight);
    CAmount servicenodePayment = GetServicenodePayment(pindexPrev->nHeight, blockValue);

    if (hasPayment) {
        if (fProofOfStake) {
            /**For Proof Of Stake vout[0] must be null
             * Stake reward can be split into many different outputs, so we must
             * use vout.size() to align with several different cases.
             * An additional output is appended as the servicenode payment
             */
            unsigned int i = txNew.vout.size();
            txNew.vout.resize(i + 1);
            txNew.vout[i].scriptPubKey = payee;
            txNew.vout[i].nValue = servicenodePayment;

            //subtract mn payment from the stake reward
            txNew.vout[i - 1].nValue -= servicenodePayment;
        } else {
            txNew.vout.resize(2);
            txNew.vout[1].scriptPubKey = payee;
            txNew.vout[1].nValue = servicenodePayment;
            txNew.vout[0].nValue = blockValue - servicenodePayment;
        }

        CTxDestination address1;
        ExtractDestination(payee, address1);
        CBitcoinAddress address2(address1);

        LogPrintf("Servicenode payment of %s to %s\n", FormatMoney(servicenodePayment).c_str(), address2.ToString().c_str());
    }
}

int CServicenodePayments::GetMinServicenodePaymentsProto()
{    return ActiveProtocol();
}

void CServicenodePayments::ProcessMessageServicenodePayments(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    if (!servicenodeSync.IsBlockchainSynced()) return;

    if (fLiteMode) return; //disable all Obfuscation/Servicenode related functionality


    if (strCommand == "mnget") { //Servicenode Payments Request Sync
        if (fLiteMode) return;   //disable all Obfuscation/Servicenode related functionality

        int nCountNeeded;
        vRecv >> nCountNeeded;

        if (Params().NetworkID() == CBaseChainParams::MAIN) {
            if (pfrom->HasFulfilledRequest("mnget")) {
                LogPrintf("mnget - peer already asked me for the list\n");
                Misbehaving(pfrom->GetId(), 20);
                return;
            }
        }

        pfrom->FulfilledRequest("mnget");
        servicenodePayments.Sync(pfrom, nCountNeeded);
        LogPrint("mnpayments", "mnget - Sent Servicenode winners to peer %i\n", pfrom->GetId());
    } else if (strCommand == "mnw") { //Servicenode Payments Declare Winner
        //this is required in litemodef
        CServicenodePaymentWinner winner;
        vRecv >> winner;

        if (pfrom->nVersion < ActiveProtocol()) return;

        int nHeight;
        {
            TRY_LOCK(cs_main, locked);
            if (!locked || chainActive.Tip() == NULL) return;
            nHeight = chainActive.Tip()->nHeight;
        }

        if (servicenodePayments.mapServicenodePayeeVotes.count(winner.GetHash())) {
            LogPrint("mnpayments", "mnw - Already seen - %s bestHeight %d\n", winner.GetHash().ToString().c_str(), nHeight);
            servicenodeSync.AddedServicenodeWinner(winner.GetHash());
            return;
        }

        int nFirstBlock = nHeight - (mnodeman.CountEnabled() * 1.25);
        if (winner.nBlockHeight < nFirstBlock || winner.nBlockHeight > nHeight + 20) {
            LogPrint("mnpayments", "mnw - winner out of range - FirstBlock %d Height %d bestHeight %d\n", nFirstBlock, winner.nBlockHeight, nHeight);
            return;
        }

        std::string strError = "";
        if (!winner.IsValid(pfrom, strError)) {
            // if(strError != "") LogPrintf("mnw - invalid message - %s\n", strError);
            return;
        }

        if (!servicenodePayments.CanVote(winner.vinServicenode.prevout, winner.nBlockHeight)) {
            //  LogPrintf("mnw - servicenode already voted - %s\n", winner.vinServicenode.prevout.ToStringShort());
            return;
        }

        if (!winner.SignatureValid()) {
            // LogPrintf("mnw - invalid signature\n");
            if (servicenodeSync.IsSynced()) Misbehaving(pfrom->GetId(), 20);
            // it could just be a non-synced servicenode
            mnodeman.AskForMN(pfrom, winner.vinServicenode);
            return;
        }

        CTxDestination address1;
        ExtractDestination(winner.payee, address1);
        CBitcoinAddress address2(address1);

        //   LogPrint("mnpayments", "mnw - winning vote - Addr %s Height %d bestHeight %d - %s\n", address2.ToString().c_str(), winner.nBlockHeight, nHeight, winner.vinServicenode.prevout.ToStringShort());

        if (servicenodePayments.AddWinningServicenode(winner)) {
            winner.Relay();
            servicenodeSync.AddedServicenodeWinner(winner.GetHash());
        }
    }
}

bool CServicenodePaymentWinner::Sign(CKey& keyServicenode, CPubKey& pubKeyServicenode)
{
    std::string errorMessage;
    std::string strServiceNodeSignMessage;

    std::string strMessage = vinServicenode.prevout.ToStringShort() +
                             boost::lexical_cast<std::string>(nBlockHeight) +
                             payee.ToString();

    if (!obfuScationSigner.SignMessage(strMessage, errorMessage, vchSig, keyServicenode)) {
        LogPrintf("CServicenodePing::Sign() - Error: %s\n", errorMessage.c_str());
        return false;
    }

    if (!obfuScationSigner.VerifyMessage(pubKeyServicenode, vchSig, strMessage, errorMessage)) {
        LogPrintf("CServicenodePing::Sign() - Error: %s\n", errorMessage.c_str());
        return false;
    }

    return true;
}

bool CServicenodePayments::GetBlockPayee(int nBlockHeight, CScript& payee)
{
    if (mapServicenodeBlocks.count(nBlockHeight)) {
        return mapServicenodeBlocks[nBlockHeight].GetPayee(payee);
    }

    return false;
}

// Is this servicenode scheduled to get paid soon?
// -- Only look ahead up to 8 blocks to allow for propagation of the latest 2 winners
bool CServicenodePayments::IsScheduled(CServicenode& mn, int nNotBlockHeight)
{
    LOCK(cs_mapServicenodeBlocks);

    int nHeight;
    {
        TRY_LOCK(cs_main, locked);
        if (!locked || chainActive.Tip() == NULL) return false;
        nHeight = chainActive.Tip()->nHeight;
    }

    CScript mnpayee;
    mnpayee = GetScriptForDestination(mn.pubKeyCollateralAddress.GetID());

    CScript payee;
    for (int64_t h = nHeight; h <= nHeight + 8; h++) {
        if (h == nNotBlockHeight) continue;
        if (mapServicenodeBlocks.count(h)) {
            if (mapServicenodeBlocks[h].GetPayee(payee)) {
                if (mnpayee == payee) {
                    return true;
                }
            }
        }
    }

    return false;
}

bool CServicenodePayments::AddWinningServicenode(CServicenodePaymentWinner& winnerIn)
{
    uint256 blockHash = 0;
    if (!GetBlockHash(blockHash, winnerIn.nBlockHeight - 100)) {
        return false;
    }

    {
        LOCK2(cs_mapServicenodePayeeVotes, cs_mapServicenodeBlocks);

        if (mapServicenodePayeeVotes.count(winnerIn.GetHash())) {
            return false;
        }

        mapServicenodePayeeVotes[winnerIn.GetHash()] = winnerIn;

        if (!mapServicenodeBlocks.count(winnerIn.nBlockHeight)) {
            CServicenodeBlockPayees blockPayees(winnerIn.nBlockHeight);
            mapServicenodeBlocks[winnerIn.nBlockHeight] = blockPayees;
        }
    }

    mapServicenodeBlocks[winnerIn.nBlockHeight].AddPayee(winnerIn.payee, 1);

    return true;
}

bool CServicenodeBlockPayees::IsTransactionValid(const CTransaction& txNew)
{
    LOCK(cs_vecPayments);

    int nMaxSignatures = 0;
    std::string strPayeesPossible = "";

    CAmount nReward = GetBlockValue(nBlockHeight);

    //account for the fact that all peers do not see the same servicenode count. A allowance of being off our servicenode count is given
    //we only need to look at an increased servicenode count because as count increases, the reward decreases. This code only checks
    //for mnPayment >= required, so it only makes sense to check the max node count allowed.
    CAmount requiredServicenodePayment = GetServicenodePayment(nBlockHeight, nReward, mnodeman.size() + Params().ServicenodeCountDrift());

    //require at least 6 signatures
    BOOST_FOREACH (CServicenodePayee& payee, vecPayments)
        if (payee.nVotes >= nMaxSignatures && payee.nVotes >= MNPAYMENTS_SIGNATURES_REQUIRED)
            nMaxSignatures = payee.nVotes;

    // if we don't have at least 6 signatures on a payee, approve whichever is the longest chain
    if (nMaxSignatures < MNPAYMENTS_SIGNATURES_REQUIRED) return true;

    BOOST_FOREACH (CServicenodePayee& payee, vecPayments) {
        bool found = false;
        BOOST_FOREACH (CTxOut out, txNew.vout) {
            if (payee.scriptPubKey == out.scriptPubKey) {
                if(out.nValue >= requiredServicenodePayment)
                    found = true;
                else
                    LogPrintf("Servicenode payment is out of drift range. Paid=%s Min=%s\n", FormatMoney(out.nValue).c_str(), FormatMoney(requiredServicenodePayment).c_str());
            }
        }

        if (payee.nVotes >= MNPAYMENTS_SIGNATURES_REQUIRED) {
            if (found) return true;

            CTxDestination address1;
            ExtractDestination(payee.scriptPubKey, address1);
            CBitcoinAddress address2(address1);

            if (strPayeesPossible == "") {
                strPayeesPossible += address2.ToString();
            } else {
                strPayeesPossible += "," + address2.ToString();
            }
        }
    }

    LogPrintf("CServicenodePayments::IsTransactionValid - Missing required payment of %s to %s\n", FormatMoney(requiredServicenodePayment).c_str(), strPayeesPossible.c_str());
    return false;
}

std::string CServicenodeBlockPayees::GetRequiredPaymentsString()
{
    LOCK(cs_vecPayments);

    std::string ret = "Unknown";

    BOOST_FOREACH (CServicenodePayee& payee, vecPayments) {
        CTxDestination address1;
        ExtractDestination(payee.scriptPubKey, address1);
        CBitcoinAddress address2(address1);

        if (ret != "Unknown") {
            ret += ", " + address2.ToString() + ":" + boost::lexical_cast<std::string>(payee.nVotes);
        } else {
            ret = address2.ToString() + ":" + boost::lexical_cast<std::string>(payee.nVotes);
        }
    }

    return ret;
}

std::string CServicenodePayments::GetRequiredPaymentsString(int nBlockHeight)
{
    LOCK(cs_mapServicenodeBlocks);

    if (mapServicenodeBlocks.count(nBlockHeight)) {
        return mapServicenodeBlocks[nBlockHeight].GetRequiredPaymentsString();
    }

    return "Unknown";
}

bool CServicenodePayments::IsTransactionValid(const CTransaction& txNew, int nBlockHeight)
{
    LOCK(cs_mapServicenodeBlocks);

    if (mapServicenodeBlocks.count(nBlockHeight)) {
        return mapServicenodeBlocks[nBlockHeight].IsTransactionValid(txNew);
    }

    return true;
}

void CServicenodePayments::CleanPaymentList()
{
    LOCK2(cs_mapServicenodePayeeVotes, cs_mapServicenodeBlocks);

    int nHeight;
    {
        TRY_LOCK(cs_main, locked);
        if (!locked || chainActive.Tip() == NULL) return;
        nHeight = chainActive.Tip()->nHeight;
    }

    //keep up to five cycles for historical sake
    int nLimit = std::max(int(mnodeman.size() * 1.25), 1000);

    std::map<uint256, CServicenodePaymentWinner>::iterator it = mapServicenodePayeeVotes.begin();
    while (it != mapServicenodePayeeVotes.end()) {
        CServicenodePaymentWinner winner = (*it).second;

        if (nHeight - winner.nBlockHeight > nLimit) {
            LogPrint("mnpayments", "CServicenodePayments::CleanPaymentList - Removing old Servicenode payment - block %d\n", winner.nBlockHeight);
            servicenodeSync.mapSeenSyncMNW.erase((*it).first);
            mapServicenodePayeeVotes.erase(it++);
            mapServicenodeBlocks.erase(winner.nBlockHeight);
        } else {
            ++it;
        }
    }
}

bool CServicenodePaymentWinner::IsValid(CNode* pnode, std::string& strError)
{
    CServicenode* pmn = mnodeman.Find(vinServicenode);

    if (!pmn) {
        strError = strprintf("Unknown Servicenode %s", vinServicenode.prevout.hash.ToString());
        LogPrintf("CServicenodePaymentWinner::IsValid - %s\n", strError);
        mnodeman.AskForMN(pnode, vinServicenode);
        return false;
    }

    if (pmn->protocolVersion < ActiveProtocol()) {
        strError = strprintf("Servicenode protocol too old %d - req %d", pmn->protocolVersion, ActiveProtocol());
        LogPrintf("CServicenodePaymentWinner::IsValid - %s\n", strError);
        return false;
    }

    int n = mnodeman.GetServicenodeRank(vinServicenode, nBlockHeight - 100, ActiveProtocol());

    if (n > MNPAYMENTS_SIGNATURES_TOTAL) {
        //It's common to have servicenodes mistakenly think they are in the top 10
        // We don't want to print all of these messages, or punish them unless they're way off
        if (n > MNPAYMENTS_SIGNATURES_TOTAL * 2) {
            strError = strprintf("Servicenode not in the top %d (%d)", MNPAYMENTS_SIGNATURES_TOTAL * 2, n);
            LogPrintf("CServicenodePaymentWinner::IsValid - %s\n", strError);
            if (servicenodeSync.IsSynced()) Misbehaving(pnode->GetId(), 20);
        }
        return false;
    }

    return true;
}

bool CServicenodePayments::ProcessBlock(int nBlockHeight)
{
    if (!fServiceNode) return false;

    //reference node - hybrid mode

    int n = mnodeman.GetServicenodeRank(activeServicenode.vin, nBlockHeight - 100, ActiveProtocol());

    if (n == -1) {
        LogPrint("mnpayments", "CServicenodePayments::ProcessBlock - Unknown Servicenode\n");
        return false;
    }

    if (n > MNPAYMENTS_SIGNATURES_TOTAL) {
        LogPrint("mnpayments", "CServicenodePayments::ProcessBlock - Servicenode not in the top %d (%d)\n", MNPAYMENTS_SIGNATURES_TOTAL, n);
        return false;
    }

    if (nBlockHeight <= nLastBlockHeight) return false;

    CServicenodePaymentWinner newWinner(activeServicenode.vin);

    if (budget.IsBudgetPaymentBlock(nBlockHeight)) {
        //is budget payment block -- handled by the budgeting software
    } else {
        LogPrintf("CServicenodePayments::ProcessBlock() Start nHeight %d - vin %s. \n", nBlockHeight, activeServicenode.vin.prevout.hash.ToString());

        // pay to the oldest MN that still had no payment but its input is old enough and it was active long enough
        int nCount = 0;
        CServicenode* pmn = mnodeman.GetNextServicenodeInQueueForPayment(nBlockHeight, true, nCount);

        if (pmn != NULL) {
            LogPrintf("CServicenodePayments::ProcessBlock() Found by FindOldestNotInVec \n");

            newWinner.nBlockHeight = nBlockHeight;

            CScript payee = GetScriptForDestination(pmn->pubKeyCollateralAddress.GetID());
            newWinner.AddPayee(payee);

            CTxDestination address1;
            ExtractDestination(payee, address1);
            CBitcoinAddress address2(address1);

            LogPrintf("CServicenodePayments::ProcessBlock() Winner payee %s nHeight %d. \n", address2.ToString().c_str(), newWinner.nBlockHeight);
        } else {
            LogPrintf("CServicenodePayments::ProcessBlock() Failed to find servicenode to pay\n");
        }
    }

    std::string errorMessage;
    CPubKey pubKeyServicenode;
    CKey keyServicenode;

    if (!obfuScationSigner.SetKey(strServiceNodePrivKey, errorMessage, keyServicenode, pubKeyServicenode)) {
        LogPrintf("CServicenodePayments::ProcessBlock() - Error upon calling SetKey: %s\n", errorMessage.c_str());
        return false;
    }

    LogPrintf("CServicenodePayments::ProcessBlock() - Signing Winner\n");
    if (newWinner.Sign(keyServicenode, pubKeyServicenode)) {
        LogPrintf("CServicenodePayments::ProcessBlock() - AddWinningServicenode\n");

        if (AddWinningServicenode(newWinner)) {
            newWinner.Relay();
            nLastBlockHeight = nBlockHeight;
            return true;
        }
    }

    return false;
}

void CServicenodePaymentWinner::Relay()
{
    CInv inv(MSG_SERVICENODE_WINNER, GetHash());
    RelayInv(inv);
}

bool CServicenodePaymentWinner::SignatureValid()
{
    CServicenode* pmn = mnodeman.Find(vinServicenode);

    if (pmn != NULL) {
        std::string strMessage = vinServicenode.prevout.ToStringShort() +
                                 boost::lexical_cast<std::string>(nBlockHeight) +
                                 payee.ToString();

        std::string errorMessage = "";
        if (!obfuScationSigner.VerifyMessage(pmn->pubKeyServicenode, vchSig, strMessage, errorMessage)) {
            return error("CServicenodePaymentWinner::SignatureValid() - Got bad Servicenode address signature %s\n", vinServicenode.prevout.hash.ToString());
        }

        return true;
    }

    return false;
}

void CServicenodePayments::Sync(CNode* node, int nCountNeeded)
{
    LOCK(cs_mapServicenodePayeeVotes);

    int nHeight;
    {
        TRY_LOCK(cs_main, locked);
        if (!locked || chainActive.Tip() == NULL) return;
        nHeight = chainActive.Tip()->nHeight;
    }

    int nCount = (mnodeman.CountEnabled() * 1.25);
    if (nCountNeeded > nCount) nCountNeeded = nCount;

    int nInvCount = 0;
    std::map<uint256, CServicenodePaymentWinner>::iterator it = mapServicenodePayeeVotes.begin();
    while (it != mapServicenodePayeeVotes.end()) {
        CServicenodePaymentWinner winner = (*it).second;
        if (winner.nBlockHeight >= nHeight - nCountNeeded && winner.nBlockHeight <= nHeight + 20) {
            node->PushInventory(CInv(MSG_SERVICENODE_WINNER, winner.GetHash()));
            nInvCount++;
        }
        ++it;
    }
    node->PushMessage("ssc", SERVICENODE_SYNC_MNW, nInvCount);
}

std::string CServicenodePayments::ToString() const
{
    std::ostringstream info;

    info << "Votes: " << (int)mapServicenodePayeeVotes.size() << ", Blocks: " << (int)mapServicenodeBlocks.size();

    return info.str();
}


int CServicenodePayments::GetOldestBlock()
{
    LOCK(cs_mapServicenodeBlocks);

    int nOldestBlock = std::numeric_limits<int>::max();

    std::map<int, CServicenodeBlockPayees>::iterator it = mapServicenodeBlocks.begin();
    while (it != mapServicenodeBlocks.end()) {
        if ((*it).first < nOldestBlock) {
            nOldestBlock = (*it).first;
        }
        it++;
    }

    return nOldestBlock;
}


int CServicenodePayments::GetNewestBlock()
{
    LOCK(cs_mapServicenodeBlocks);

    int nNewestBlock = 0;

    std::map<int, CServicenodeBlockPayees>::iterator it = mapServicenodeBlocks.begin();
    while (it != mapServicenodeBlocks.end()) {
        if ((*it).first > nNewestBlock) {
            nNewestBlock = (*it).first;
        }
        it++;
    }

    return nNewestBlock;
}
