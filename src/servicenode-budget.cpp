// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The PIVX developers
// Copyright (c) 2015-2018 The Blocknet developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "init.h"
#include "main.h"

#include "addrman.h"
#include "servicenode-budget.h"
#include "servicenode-sync.h"
#include "servicenode.h"
#include "servicenodeman.h"
#include "obfuscation.h"
#include "util.h"
#include "utilmoneystr.h"
#include "spork.h"
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

CBudgetManager budget;
CCriticalSection cs_budget;

std::map<uint256, int64_t> askedForSourceProposalOrBudget;
std::vector<CBudgetProposalBroadcast> vecImmatureBudgetProposals;
std::vector<CFinalizedBudgetBroadcast> vecImmatureFinalizedBudgets;

int GetBudgetPaymentCycleBlocks()
{
    // Amount of blocks in a months period of time (using 1 minutes per) = (60*24*30)
    if (Params().NetworkID() == CBaseChainParams::MAIN)
        return 43200;
    //for testing purposes
    return 144; // 10 times per day
}

/**
 * Proposal fee. Sporked to allow the community to change the amount more easily.
 * @return
 */
CAmount GetProposalFee() {
    if (IsSporkActive(SPORK_18_PROPOSAL_FEE))
        return static_cast<CAmount>(GetSporkValue(SPORK_18_PROPOSAL_FEE_AMOUNT) * COIN);
    return 50 * COIN;
}

/**
 * Budget fee is the same as the proposal fee.
 * @return
 */
CAmount GetBudgetFee() {
    return GetProposalFee();
}

bool IsBudgetCollateralValid(uint256 nTxCollateralHash, uint256 nExpectedHash, std::string& strError, int64_t& nTime, int& nConf)
{
    CTransaction txCollateral;
    uint256 nBlockHash;
    if (!GetTransaction(nTxCollateralHash, txCollateral, nBlockHash, true)) {
        strError = strprintf("Can't find collateral tx %s", txCollateral.ToString());
        LogPrintf("CBudgetProposalBroadcast::IsBudgetCollateralValid - %s\n", strError);
        return false;
    }

    if (txCollateral.vout.size() < 1) return false;
    if (txCollateral.nLockTime != 0) return false;

    CScript findScript;
    findScript << OP_RETURN << ToByteVector(nExpectedHash);

    bool foundOpReturn = false;
    BOOST_FOREACH (const CTxOut o, txCollateral.vout) {
        if (!o.scriptPubKey.IsNormalPaymentScript() && !o.scriptPubKey.IsUnspendable()) {
            strError = strprintf("Invalid Script %s", txCollateral.ToString());
            LogPrintf("CBudgetProposalBroadcast::IsBudgetCollateralValid - %s\n", strError);
            return false;
        }
        if (o.scriptPubKey == findScript && o.nValue >= GetProposalFee()) foundOpReturn = true;
    }
    if (!foundOpReturn) {
        strError = strprintf("Couldn't find opReturn %s in %s", nExpectedHash.ToString(), txCollateral.ToString());
        LogPrintf("CBudgetProposalBroadcast::IsBudgetCollateralValid - %s\n", strError);
        return false;
    }

    // RETRIEVE CONFIRMATIONS AND NTIME
    /*
        - nTime starts as zero and is passed-by-reference out of this function and stored in the external proposal
        - nTime is never validated via the hashing mechanism and comes from a full-validated source (the blockchain)
    */

    int conf = GetIXConfirmations(nTxCollateralHash);
    if (nBlockHash != uint256()) {
        BlockMap::iterator mi = mapBlockIndex.find(nBlockHash);
        if (mi != mapBlockIndex.end() && (*mi).second) {
            CBlockIndex* pindex = (*mi).second;
            if (chainActive.Contains(pindex)) {
                conf += chainActive.Height() - pindex->nHeight + 1;
                nTime = pindex->nTime;
            }
        }
    }

    nConf = conf;

    //if we're syncing we won't have swiftTX information, so accept 1 confirmation
    if (conf >= BUDGET_FEE_CONFIRMATIONS) {
        return true;
    } else {
        strError = strprintf("Collateral requires at least %d confirmations - %d confirmations", BUDGET_FEE_CONFIRMATIONS, conf);
        LogPrintf("CBudgetProposalBroadcast::IsBudgetCollateralValid - %s - %d confirmations\n", strError, conf);
        return false;
    }
}

void CBudgetManager::CheckOrphanVotes()
{
    LOCK(cs);


    std::string strError = "";
    std::map<uint256, CBudgetVote>::iterator it1 = mapOrphanServicenodeBudgetVotes.begin();
    while (it1 != mapOrphanServicenodeBudgetVotes.end()) {
        if (budget.UpdateProposal(((*it1).second), NULL, strError)) {
            LogPrintf("CBudgetManager::CheckOrphanVotes - Proposal/Budget is known, activating and removing orphan vote\n");
            mapOrphanServicenodeBudgetVotes.erase(it1++);
        } else {
            ++it1;
        }
    }
    std::map<uint256, CFinalizedBudgetVote>::iterator it2 = mapOrphanFinalizedBudgetVotes.begin();
    while (it2 != mapOrphanFinalizedBudgetVotes.end()) {
        if (budget.UpdateFinalizedBudget(((*it2).second), NULL, strError)) {
            LogPrintf("CBudgetManager::CheckOrphanVotes - Proposal/Budget is known, activating and removing orphan vote\n");
            mapOrphanFinalizedBudgetVotes.erase(it2++);
        } else {
            ++it2;
        }
    }
}

void CBudgetManager::SubmitFinalBudget()
{
    static int nSubmittedHeight = 0; // height at which final budget was submitted last time
    int nCurrentHeight;
    {
        TRY_LOCK(cs_main, locked);
        if (!locked) return;
        if (!chainActive.Tip()) return;
        nCurrentHeight = chainActive.Height();
    }

    int nBlockStart = nCurrentHeight - nCurrentHeight % GetBudgetPaymentCycleBlocks() + GetBudgetPaymentCycleBlocks();
    if (nSubmittedHeight >= nBlockStart) return;
    // Submit mainnet budget 2880 blocks before superblock ~2 days
    if (Params().NetworkID() == CBaseChainParams::MAIN && nBlockStart - nCurrentHeight > ((GetBudgetPaymentCycleBlocks() / 30) * 2))
        return;
    // Submit testnet budget 20 blocks before superblock ~20 minutes
    if (Params().NetworkID() == CBaseChainParams::TESTNET && nBlockStart - nCurrentHeight > 20)
        return;
    
    std::vector<CBudgetProposal*> vBudgetProposals = budget.GetBudget();
    std::string strBudgetName = "superblock-" + std::to_string(nBlockStart);
    std::vector<CTxBudgetPayment> vecTxBudgetPayments;
    
    for (auto vBudgetProposal : vBudgetProposals) {
        CTxBudgetPayment txBudgetPayment;
        txBudgetPayment.nProposalHash = vBudgetProposal->GetHash();
        txBudgetPayment.payee = vBudgetProposal->GetPayee();
        txBudgetPayment.nAmount = vBudgetProposal->GetAllotted();
        vecTxBudgetPayments.push_back(txBudgetPayment);
    }

    if (vecTxBudgetPayments.empty()) {
        LogPrintf("CBudgetManager::SubmitFinalBudget - Found No Proposals For Period\n");
        return;
    }

    CFinalizedBudgetBroadcast tempBudget(strBudgetName, nBlockStart, vecTxBudgetPayments, 0);
    {
        LOCK(cs);
        if (mapSeenFinalizedBudgets.count(tempBudget.GetHash())) {
            LogPrintf("CBudgetManager::SubmitFinalBudget - Budget already exists - %s\n", tempBudget.GetHash().ToString());
            nSubmittedHeight = nCurrentHeight;
            return; //already exists
        }
    }

    //create fee tx
    CTransaction tx;
    uint256 txidCollateral;
    {
        LOCK(cs);
        if (!mapCollateralTxids.count(tempBudget.GetHash())) {
            CWalletTx wtx;
            if (!pwalletMain->GetBudgetSystemCollateralTX(wtx, tempBudget.GetHash(), false)) {
                LogPrintf("CBudgetManager::SubmitFinalBudget - Can't make collateral transaction\n");
                return;
            }
            
            // make our change address
            CReserveKey reservekey(pwalletMain);
            //send the tx to the network
            pwalletMain->CommitTransaction(wtx, reservekey, "tx"); // use normal tx for final budget collateral
            tx = (CTransaction)wtx;
            txidCollateral = tx.GetHash();
            mapCollateralTxids.insert(make_pair(tempBudget.GetHash(), txidCollateral));
        } else {
            txidCollateral = mapCollateralTxids[tempBudget.GetHash()];
        }
    }

    int conf = GetIXConfirmations(txidCollateral);
    CTransaction txCollateral;
    uint256 nBlockHash;

    if (!GetTransaction(txidCollateral, txCollateral, nBlockHash, true)) {
        LogPrintf("CBudgetManager::SubmitFinalBudget - Can't find collateral tx %s", txidCollateral.ToString());
        return;
    }

    if (nBlockHash != uint256()) {
        BlockMap::iterator mi = mapBlockIndex.find(nBlockHash);
        if (mi != mapBlockIndex.end() && (*mi).second) {
            CBlockIndex* pindex = (*mi).second;
            if (chainActive.Contains(pindex)) {
                conf += chainActive.Height() - pindex->nHeight + 1;
            }
        }
    }

    /*
     Wait will we have 1 extra confirmation, otherwise some clients might reject this feeTX
     -- This function is tied to NewBlock, so we will propagate this budget while the block is also propagating
     */
    if (conf < BUDGET_FEE_CONFIRMATIONS + 1) {
        LogPrintf("CBudgetManager::SubmitFinalBudget - Collateral requires at least %d confirmations - %s - %d confirmations\n", BUDGET_FEE_CONFIRMATIONS + 1, txidCollateral.ToString(), conf);
        return;
    }

    //create the proposal incase we're the first to make it
    CFinalizedBudgetBroadcast finalizedBudgetBroadcast(strBudgetName, nBlockStart, vecTxBudgetPayments, txidCollateral);

    std::string strError = "";
    if (!finalizedBudgetBroadcast.IsValid(strError)) {
        LogPrintf("CBudgetManager::SubmitFinalBudget - Invalid finalized budget - %s \n", strError);
        return;
    }

    LOCK(cs);
    mapSeenFinalizedBudgets.insert(make_pair(finalizedBudgetBroadcast.GetHash(), finalizedBudgetBroadcast));
    finalizedBudgetBroadcast.Relay();
    budget.AddFinalizedBudget(finalizedBudgetBroadcast);
    nSubmittedHeight = nCurrentHeight;
    LogPrintf("CBudgetManager::SubmitFinalBudget - Done! %s\n", finalizedBudgetBroadcast.GetHash().ToString());
}

//
// CBudgetDB
//

CBudgetDB::CBudgetDB()
{
    pathDB = GetDataDir() / "budget.dat";
    strMagicMessage = "ServicenodeBudget";
}

bool CBudgetDB::Write(const CBudgetManager& objToSave)
{
    LOCK(objToSave.cs);

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

    LogPrintf("Written info to budget.dat  %dms\n", GetTimeMillis() - nStart);

    return true;
}

CBudgetDB::ReadResult CBudgetDB::Read(CBudgetManager& objToLoad, bool fDryRun)
{
    LOCK(objToLoad.cs);

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
            error("%s : Invalid servicenode cache magic message", __func__);
            return IncorrectMagicMessage;
        }


        // de-serialize file header (network specific magic number) and ..
        ssObj >> FLATDATA(pchMsgTmp);

        // ... verify the network matches ours
        if (memcmp(pchMsgTmp, Params().MessageStart(), sizeof(pchMsgTmp))) {
            error("%s : Invalid network magic number", __func__);
            return IncorrectMagicNumber;
        }

        // de-serialize data into CBudgetManager object
        ssObj >> objToLoad;
    } catch (std::exception& e) {
        objToLoad.Clear();
        error("%s : Deserialize or I/O error - %s", __func__, e.what());
        return IncorrectFormat;
    }

    LogPrintf("Loaded info from budget.dat  %dms\n", GetTimeMillis() - nStart);
    LogPrintf("  %s\n", objToLoad.ToString());
    if (!fDryRun) {
        LogPrintf("Budget manager - cleaning....\n");
        objToLoad.CheckAndRemove();
        LogPrintf("Budget manager - result:\n");
        LogPrintf("  %s\n", objToLoad.ToString());
    }

    return Ok;
}

void DumpBudgets()
{
    int64_t nStart = GetTimeMillis();

    CBudgetDB budgetdb;
    CBudgetManager tempBudget;

    LogPrintf("Verifying budget.dat format...\n");
    CBudgetDB::ReadResult readResult = budgetdb.Read(tempBudget, true);
    // there was an error and it was not an error on file opening => do not proceed
    if (readResult == CBudgetDB::FileError)
        LogPrintf("Missing budgets file - budget.dat, will try to recreate\n");
    else if (readResult != CBudgetDB::Ok) {
        LogPrintf("Error reading budget.dat: ");
        if (readResult == CBudgetDB::IncorrectFormat)
            LogPrintf("magic is ok but data has invalid format, will try to recreate\n");
        else {
            LogPrintf("file format is unknown or invalid, please fix it manually\n");
            return;
        }
    }
    LogPrintf("Writting info to budget.dat...\n");
    budgetdb.Write(budget);

    LogPrintf("Budget dump finished  %dms\n", GetTimeMillis() - nStart);
}

bool CBudgetManager::AddFinalizedBudget(CFinalizedBudget& finalizedBudget)
{
    LOCK(cs);
    std::string strError = "";
    if (!finalizedBudget.IsValid(strError)) return false;

    if (mapFinalizedBudgets.count(finalizedBudget.GetHash())) {
        return false;
    }

    mapFinalizedBudgets.insert(make_pair(finalizedBudget.GetHash(), finalizedBudget));
    return true;
}

bool CBudgetManager::AddProposal(CBudgetProposal& budgetProposal)
{
    LOCK(cs);
    std::string strError = "";
    if (!budgetProposal.IsValid(strError)) {
        LogPrintf("CBudgetManager::AddProposal - invalid budget proposal - %s\n", strError);
        return false;
    }

    if (mapProposals.count(budgetProposal.GetHash())) {
        return false;
    }

    mapProposals.insert(make_pair(budgetProposal.GetHash(), budgetProposal));
    LogPrintf("CBudgetManager::AddProposal - proposal %s added\n", budgetProposal.GetName ().c_str ());
    return true;
}

void CBudgetManager::CheckAndRemove()
{
    LOCK(cs);
    LogPrint("mnbudget", "CBudgetManager::CheckAndRemove\n");

    std::string strError = "";

    LogPrint("mnbudget", "CBudgetManager::CheckAndRemove - mapFinalizedBudgets cleanup - size: %d\n", mapFinalizedBudgets.size());
    std::map<uint256, CFinalizedBudget>::iterator it = mapFinalizedBudgets.begin();
    while (it != mapFinalizedBudgets.end()) {
        CFinalizedBudget* pfinalizedBudget = &((*it).second);

        pfinalizedBudget->fValid = pfinalizedBudget->IsValid(strError);
        LogPrintf("CBudgetManager::CheckAndRemove - pfinalizedBudget->IsValid - strError: %s\n", strError);
        if (pfinalizedBudget->fValid) {
            pfinalizedBudget->AutoCheck();
        }

        ++it;
    }

    LogPrint("mnbudget", "CBudgetManager::CheckAndRemove - mapProposals cleanup - size: %d\n", mapProposals.size());
    std::map<uint256, CBudgetProposal>::iterator it2 = mapProposals.begin();
    while (it2 != mapProposals.end()) {
        CBudgetProposal* pbudgetProposal = &((*it2).second);
        pbudgetProposal->fValid = pbudgetProposal->IsValid(strError);
        if (!strError.empty ()) {
            LogPrintf("CBudgetManager::CheckAndRemove - invalid budget proposal %s - %s\n", pbudgetProposal->GetName().c_str (), strError);
            strError = "";
        }
        ++it2;
    }
    
    LogPrintf("CBudgetManager::CheckAndRemove - PASSED\n");
}

/**
 * Adds all valid finalized budget payees to a single transaction.
 * @param txNew
 * @param superblock
 * @return
 */
bool CBudgetManager::FillBlockPayees(CMutableTransaction &txNew, int superblock) {
    LOCK(cs);
    
    // Add all the payees for this superblock
    std::vector<CTxBudgetPayment> approvedPayees;
    if (!allValidFinalPayees(approvedPayees, superblock))
        return false;
    
    // Handle all the budget payments in a single tx
    auto i = static_cast<unsigned int>(txNew.vout.size());
    txNew.vout.resize(i + approvedPayees.size());
    
    // Add payees to tx
    int j = i;
    for (auto &item : approvedPayees) {
        txNew.vout[j].scriptPubKey = item.payee;
        txNew.vout[j].nValue = item.nAmount;
        
        CTxDestination address1;
        ExtractDestination(item.payee, address1);
        CBitcoinAddress address2(address1);
        LogPrintf("CBudgetManager::FillBlockPayees - Budget payment to %s for %s\n", address2.ToString(), FormatMoney(item.nAmount));
        
        j++;
    }
    
    // success
    return true;
}

/**
 * Finds all the payees approved by Governance for this superblock. This method filters out any payees who's proposals
 * didn't receive enough votes, including filtering out any final budgets and subsequent proposals that didn't receive
 * enough votes. Vote consensus requirement is 10% of the servicenode network.
 * @param approvedPayees
 * @param superblock
 * @return
 */
bool CBudgetManager::allValidFinalPayees(std::vector<CTxBudgetPayment> &approvedPayees, int superblock) {
    // If not superblock do not proceed
    if (superblock > 0 && superblock % GetBudgetPaymentCycleBlocks() != 0)
        return false;

    // We want to ensure highest voted budgets make it in the superblock (sort them descending by votes)
    std::vector<CFinalizedBudget*> sorted;
    for (auto &item : mapFinalizedBudgets) {
        sorted.push_back(&item.second);
    }
    sort(sorted.begin(), sorted.end(), [](CFinalizedBudget *a, CFinalizedBudget *b) {
        return a->GetVoteCount() > b->GetVoteCount();
    });

    // Consolidate budgets that meet criteria, pay as many proposals within valid budgets
    // up to the max allotted by the superblock. This may result in at least one approved
    // final budget being partial paid out if the superblock runs out of funds
    CAmount maxPayout = CBudgetManager::GetTotalBudget(superblock);
    CAmount runningTotal = 0;

    // Unique payees
    std::set<CTxBudgetPayment> uniquePayees;

    // Consolidate budget payees
    for (auto finalizedBudget : sorted) {
        // Must have votes and valid start and end blocks and have enough votes (10% consensus)
        if (finalizedBudget->GetVoteCount() > 0 &&
            finalizedBudget->GetVoteCount() > (double)mnodeman.CountEnabled(ActiveProtocol()) / 10 &&
            superblock >= finalizedBudget->GetBlockStart() &&
            superblock <= finalizedBudget->GetBlockEnd()) {
            // Get finalized budget payees (these are sorted by highest votes first)
            std::vector<CTxBudgetPayment> payees;
            finalizedBudget->GetPayees(payees);
            // Add payees that fit in the budget
            // It's possible some higher voted proposals could be excluded if they are too expensive
            for (auto &payee : payees) {
                // Skip existing payees (key comparator is proposal hash)
                if (!uniquePayees.count(payee) && runningTotal + payee.nAmount <= maxPayout) {
                    runningTotal += payee.nAmount;
                    uniquePayees.insert(payee);
                }
            }
        }
        if (runningTotal >= maxPayout)
            break;
    }

    // Copy unique payees into approved list
    approvedPayees.clear();
    for (const auto &payee : uniquePayees)
        approvedPayees.push_back(payee);

    // Must have payees to succeed
    return !approvedPayees.empty();
}

CFinalizedBudget* CBudgetManager::FindFinalizedBudget(uint256 nHash)
{
    LOCK(cs);
    if (mapFinalizedBudgets.count(nHash))
        return &mapFinalizedBudgets[nHash];

    return NULL;
}

CBudgetProposal* CBudgetManager::FindProposal(const std::string& strProposalName)
{
    LOCK(cs);
    //find the prop with the highest yes count

    int nYesCount = -99999;
    CBudgetProposal* pbudgetProposal = NULL;

    std::map<uint256, CBudgetProposal>::iterator it = mapProposals.begin();
    while (it != mapProposals.end()) {
        if ((*it).second.strProposalName == strProposalName && (*it).second.GetYeas() > nYesCount) {
            pbudgetProposal = &((*it).second);
            nYesCount = pbudgetProposal->GetYeas();
        }
        ++it;
    }

    if (nYesCount == -99999) return NULL;

    return pbudgetProposal;
}

CBudgetProposal* CBudgetManager::FindProposal(uint256 nHash)
{
    LOCK(cs);

    if (mapProposals.count(nHash))
        return &mapProposals[nHash];

    return NULL;
}

/**
 * Checks that the specified block is the superblock. Also checks that at least 1 final budget exists with
 * votes that meet the required network consensus.
 * @param nBlockHeight
 * @return
 */
bool CBudgetManager::IsBudgetPaymentBlock(int nBlockHeight)
{
    LOCK(cs);
    
    // If not the superblock do not proceed
    if (nBlockHeight > 0 && nBlockHeight % GetBudgetPaymentCycleBlocks() != 0)
        return false;
    
    int highestVotes = 0;
    
    std::map<uint256, CFinalizedBudget>::iterator it = mapFinalizedBudgets.begin();
    while (it != mapFinalizedBudgets.end()) {
        CFinalizedBudget* pfinalizedBudget = &((*it).second);
        if (pfinalizedBudget->GetVoteCount() > highestVotes &&
            nBlockHeight >= pfinalizedBudget->GetBlockStart() &&
            nBlockHeight <= pfinalizedBudget->GetBlockEnd()) {
            highestVotes = pfinalizedBudget->GetVoteCount();
        }

        ++it;
    }
    
    // 10% consensus
    return highestVotes > (double)mnodeman.CountEnabled(ActiveProtocol()) / 10;
}

/**
 * Checks that all the approved proposals are in the budget payment transaction.
 * @param txNew
 * @param nBlockHeight
 * @return
 */
bool CBudgetManager::IsTransactionValid(const CTransaction& txNew, int nBlockHeight)
{
    LOCK(cs);
    
    // The vote consensus check happens in allValidFinalPayees
    std::vector<CTxBudgetPayment> approvedPayees;
    if (!allValidFinalPayees(approvedPayees, nBlockHeight))
        return false;
    
    // Ensure all the proposals from the approved finalized budgets are in the transaction
    for (auto i = static_cast<int>(approvedPayees.size() - 1); i >= 0; i--) {
        auto &payee = approvedPayees[i];
        for (auto &txOut : txNew.vout) {
            if (payee.payee == txOut.scriptPubKey && payee.nAmount == txOut.nValue)
                approvedPayees.pop_back(); // removing for truthiness gives us a state machine (all were found if "approvedPayees" is empty)
        }
    }
    
    // Empty approvedPayees here indicates success
    // If we didn't find all the proposals and respective payees the transaction is invalid
    if (!approvedPayees.empty()) {
        for (auto &payee : approvedPayees) {
            CTxDestination address1;
            ExtractDestination(payee.payee, address1);
            CBitcoinAddress address2(address1);
            LogPrintf("CBudgetManager::IsTransactionValid - Missing required payment - %s: %s\n", address2.ToString(), FormatMoney(payee.nAmount));
        }
        return false;
    }
    
    return true;
}

std::vector<CBudgetProposal*> CBudgetManager::GetAllProposals()
{
    LOCK(cs);

    std::vector<CBudgetProposal*> vBudgetProposalRet;

    std::map<uint256, CBudgetProposal>::iterator it = mapProposals.begin();
    while (it != mapProposals.end()) {
        (*it).second.CleanAndRemove(false);

        CBudgetProposal* pbudgetProposal = &((*it).second);
        vBudgetProposalRet.push_back(pbudgetProposal);

        ++it;
    }

    return vBudgetProposalRet;
}

//
// Sort by votes, if there's a tie sort by their feeHash TX
//
struct sortProposalsByVotes {
    bool operator()(const std::pair<CBudgetProposal*, int>& left, const std::pair<CBudgetProposal*, int>& right)
    {
        if (left.second != right.second)
            return (left.second > right.second);
        return (left.first->nFeeTXHash > right.first->nFeeTXHash);
    }
};

/**
 * Returns the budget proposals that meet the requirements for the next superblock. This method locks cs_main
 * critical section as it accesses chainActive.Tip
 * @return
 */
std::vector<CBudgetProposal*> CBudgetManager::GetBudget() {
    std::vector<CBudgetProposal*> vBudgetProposalsRet;
    int chainHeight = -1;
    {
        bool fail = false;
        TRY_LOCK(cs_main, locked);
        if (!locked) fail = true;
        if (!chainActive.Tip()) fail = true;
        if (fail) {
            LogPrintf("CBudgetManager::GetBudget - Failed to get chain tip\n");
            return vBudgetProposalsRet;
        }
        chainHeight = chainActive.Height();
    }
    if (chainHeight <= 0)
        return vBudgetProposalsRet;
    
    LOCK(cs);
    
    // Sort budgets by votes
    std::vector<std::pair<CBudgetProposal*, int>> vBudgetProposalsSort;
    for (auto &item : mapProposals) {
        CBudgetProposal *proposal = &(item.second);
        proposal->CleanAndRemove(false);
        vBudgetProposalsSort.emplace_back(proposal, proposal->Votes());
    }
    std::sort(vBudgetProposalsSort.begin(), vBudgetProposalsSort.end(), sortProposalsByVotes());
    
    // Next superblock start
    int nBlockStart = chainHeight - chainHeight % GetBudgetPaymentCycleBlocks() + GetBudgetPaymentCycleBlocks();
    int nNextSuperblock = nBlockStart + GetBudgetPaymentCycleBlocks() - 1;
    
    // Total budget allowed for the superblock
    CAmount nTotalBudget = CBudgetManager::GetTotalBudget(nBlockStart);
    CAmount nBudgetAllocated = 0;
    
    // Get valid proposals for the next superblock
    for (auto &item : vBudgetProposalsSort) {
        CBudgetProposal *pbudgetProposal = item.first;
        if (pbudgetProposal->fValid &&                                      // valid proposal
            pbudgetProposal->nBlockStart <= nBlockStart &&                  // valid start
            pbudgetProposal->nBlockEnd >= nNextSuperblock &&                // valid end must be at some point after the next superblock
            pbudgetProposal->Votes() > (double)mnodeman.CountEnabled(ActiveProtocol()) / 10 && // at least 10% consensus
            pbudgetProposal->IsEstablished()) {
            // If the proposal amount fits in the superblock budget proceed
            if (pbudgetProposal->GetAmount() + nBudgetAllocated <= nTotalBudget) {
                pbudgetProposal->SetAllotted(pbudgetProposal->GetAmount());
                nBudgetAllocated += pbudgetProposal->GetAmount();
                vBudgetProposalsRet.push_back(pbudgetProposal);
            } else { // exclude proposals that are too expensive
                pbudgetProposal->SetAllotted(0);
            }
        }
    }

    return vBudgetProposalsRet;
}

struct sortFinalizedBudgetsByVotes {
    bool operator()(const std::pair<CFinalizedBudget*, int>& left, const std::pair<CFinalizedBudget*, int>& right)
    {
        return left.second > right.second;
    }
};

std::vector<CFinalizedBudget*> CBudgetManager::GetFinalizedBudgets()
{
    LOCK(cs);

    std::vector<CFinalizedBudget*> vFinalizedBudgetsRet;
    std::vector<std::pair<CFinalizedBudget*, int> > vFinalizedBudgetsSort;

    // ------- Grab The Budgets In Order

    std::map<uint256, CFinalizedBudget>::iterator it = mapFinalizedBudgets.begin();
    while (it != mapFinalizedBudgets.end()) {
        CFinalizedBudget* pfinalizedBudget = &((*it).second);

        vFinalizedBudgetsSort.push_back(make_pair(pfinalizedBudget, pfinalizedBudget->GetVoteCount()));
        ++it;
    }
    std::sort(vFinalizedBudgetsSort.begin(), vFinalizedBudgetsSort.end(), sortFinalizedBudgetsByVotes());

    std::vector<std::pair<CFinalizedBudget*, int> >::iterator it2 = vFinalizedBudgetsSort.begin();
    while (it2 != vFinalizedBudgetsSort.end()) {
        vFinalizedBudgetsRet.push_back((*it2).first);
        ++it2;
    }

    return vFinalizedBudgetsRet;
}

std::string CBudgetManager::GetRequiredPaymentsString(int nBlockHeight)
{
    LOCK(cs);

    std::string ret = "unknown-budget";

    std::map<uint256, CFinalizedBudget>::iterator it = mapFinalizedBudgets.begin();
    while (it != mapFinalizedBudgets.end()) {
        CFinalizedBudget* pfinalizedBudget = &((*it).second);
        if (nBlockHeight >= pfinalizedBudget->GetBlockStart() && nBlockHeight <= pfinalizedBudget->GetBlockEnd()) {
            std::vector<CTxBudgetPayment> payments;
            if (pfinalizedBudget->GetBudgetPayments(nBlockHeight, payments)) {
                for (auto &payment : payments) {
                    if (ret == "unknown-budget") {
                        ret = payment.nProposalHash.ToString();
                    } else {
                        ret += ",";
                        ret += payment.nProposalHash.ToString();
                    }
                }
            } else {
                LogPrintf("CBudgetManager::GetRequiredPaymentsString - Couldn't find budget payment for block %d\n", nBlockHeight);
            }
        }

        ++it;
    }

    return ret;
}

CAmount CBudgetManager::GetTotalBudget(int /*nHeight*/)
{
    if (Params().NetworkID() == CBaseChainParams::TESTNET) {
        CAmount nSubsidy = 500 * COIN;
        return ((nSubsidy / 100) * 10) * 146;
    }

    //get block value and calculate from that
    CAmount nSubsidy = 1 * COIN;
    /*
    if (nHeight <= Params().LAST_POW_BLOCK() && nHeight >= 151200) {
        nSubsidy = 50 * COIN;
    } else if (nHeight <= 302399 && nHeight > Params().LAST_POW_BLOCK()) {
        nSubsidy = 50 * COIN;
    } else if (nHeight <= 345599 && nHeight >= 302400) {
        nSubsidy = 45 * COIN;
    } else if (nHeight <= 388799 && nHeight >= 345600) {
        nSubsidy = 40 * COIN;
    } else if (nHeight <= 431999 && nHeight >= 388800) {
        nSubsidy = 35 * COIN;
    } else if (nHeight <= 475199 && nHeight >= 432000) {
        nSubsidy = 30 * COIN;
    } else if (nHeight <= 518399 && nHeight >= 475200) {
        nSubsidy = 25 * COIN;
    } else if (nHeight <= 561599 && nHeight >= 518400) {
        nSubsidy = 20 * COIN;
    } else if (nHeight <= 604799 && nHeight >= 561600) {
        nSubsidy = 15 * COIN;
    } else if (nHeight <= 647999 && nHeight >= 604800) {
        nSubsidy = 10 * COIN;
    } else if (nHeight >= 648000) {
        nSubsidy = 5 * COIN;
    } else {
        nSubsidy = 0 * COIN;
    }
    */
    // Amount of blocks in a months period of time (using 1 minutes per) = (60*24*30)

    return ((nSubsidy / 100) * 10) * 1440 * 30;
}

void CBudgetManager::NewBlock()
{
    int chainHeight = -1;
    {
        TRY_LOCK(cs_main, locked);
        if (!locked) return;
        if (!chainActive.Tip()) return;
        chainHeight = chainActive.Height();
    }
    if (chainHeight <= 0)
        return;
    
    TRY_LOCK(cs, fBudgetNewBlock);
    if (!fBudgetNewBlock) return;
    
    // Make sure budgets are synced
    if (servicenodeSync.RequestedServicenodeAssets <= SERVICENODE_SYNC_BUDGET)
        return;
    
    //suggest the budget we see
    if (strBudgetMode == "suggest") {
        SubmitFinalBudget();
    }

    //this function should be called 1/14 blocks, allowing up to 100 votes per day on all proposals
    if (Params().NetworkID() == CBaseChainParams::MAIN && chainHeight % 14 != 0)
        return;
    // testnet call once every 5 blocks
    if (Params().NetworkID() == CBaseChainParams::TESTNET && chainHeight % 5 != 0)
        return;
    
    // incremental sync with our peers
    if (servicenodeSync.IsSynced()) {
        LogPrintf("CBudgetManager::NewBlock - incremental sync started\n");
        if (chainHeight % 1440 == rand() % 1440) {
            ClearSeen();
            ResetSync();
        }

        LOCK(cs_vNodes);
        BOOST_FOREACH (CNode* pnode, vNodes)
            if (pnode->nVersion >= ActiveProtocol())
                Sync(pnode, 0, true);

        MarkSynced();
    }


    CheckAndRemove();

    //remove invalid votes once in a while (we have to check the signatures and validity of every vote, somewhat CPU intensive)

    LogPrintf("CBudgetManager::NewBlock - askedForSourceProposalOrBudget cleanup - size: %d\n", askedForSourceProposalOrBudget.size());
    std::map<uint256, int64_t>::iterator it = askedForSourceProposalOrBudget.begin();
    while (it != askedForSourceProposalOrBudget.end()) {
        if ((*it).second > GetTime() - (60 * 60 * 24)) {
            ++it;
        } else {
            askedForSourceProposalOrBudget.erase(it++);
        }
    }

    LogPrintf("CBudgetManager::NewBlock - mapProposals cleanup - size: %d\n", mapProposals.size());
    std::map<uint256, CBudgetProposal>::iterator it2 = mapProposals.begin();
    while (it2 != mapProposals.end()) {
        (*it2).second.CleanAndRemove(false);
        ++it2;
    }

    LogPrintf("CBudgetManager::NewBlock - mapFinalizedBudgets cleanup - size: %d\n", mapFinalizedBudgets.size());
    std::map<uint256, CFinalizedBudget>::iterator it3 = mapFinalizedBudgets.begin();
    while (it3 != mapFinalizedBudgets.end()) {
        (*it3).second.CleanAndRemove(false);
        ++it3;
    }

    LogPrintf("CBudgetManager::NewBlock - vecImmatureBudgetProposals cleanup - size: %d\n", vecImmatureBudgetProposals.size());
    std::vector<CBudgetProposalBroadcast>::iterator it4 = vecImmatureBudgetProposals.begin();
    while (it4 != vecImmatureBudgetProposals.end()) {
        std::string strError = "";
        int nConf = 0;
        if (!IsBudgetCollateralValid((*it4).nFeeTXHash, (*it4).GetHash(), strError, (*it4).nTime, nConf)) {
            ++it4;
            continue;
        }

        if (!(*it4).IsValid(strError)) {
            LogPrintf("mprop (immature) - invalid budget proposal - %s\n", strError);
            it4 = vecImmatureBudgetProposals.erase(it4);
            continue;
        }

        CBudgetProposal budgetProposal((*it4));
        if (AddProposal(budgetProposal)) {
            (*it4).Relay();
        }

        LogPrintf("mprop (immature) - new budget - %s\n", (*it4).GetHash().ToString());
        it4 = vecImmatureBudgetProposals.erase(it4);
    }

    LogPrintf("CBudgetManager::NewBlock - vecImmatureFinalizedBudgets cleanup - size: %d\n", vecImmatureFinalizedBudgets.size());
    std::vector<CFinalizedBudgetBroadcast>::iterator it5 = vecImmatureFinalizedBudgets.begin();
    while (it5 != vecImmatureFinalizedBudgets.end()) {
        std::string strError = "";
        int nConf = 0;
        if (!IsBudgetCollateralValid((*it5).nFeeTXHash, (*it5).GetHash(), strError, (*it5).nTime, nConf)) {
            ++it5;
            continue;
        }

        if (!(*it5).IsValid(strError)) {
            LogPrintf("fbs (immature) - invalid finalized budget - %s\n", strError);
            it5 = vecImmatureFinalizedBudgets.erase(it5);
            continue;
        }

        LogPrintf("fbs (immature) - new finalized budget - %s\n", (*it5).GetHash().ToString());

        CFinalizedBudget finalizedBudget((*it5));
        if (AddFinalizedBudget(finalizedBudget)) {
            (*it5).Relay();
        }

        it5 = vecImmatureFinalizedBudgets.erase(it5);
    }
    LogPrintf("CBudgetManager::NewBlock - PASSED\n");
}

void CBudgetManager::ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    // lite mode is not supported
    if (fLiteMode) return;
    if (!servicenodeSync.IsBlockchainSynced()) return;

    LOCK(cs_budget);

    if (strCommand == "mnvs") { //Servicenode vote sync
        uint256 nProp;
        vRecv >> nProp;

        if (Params().NetworkID() == CBaseChainParams::MAIN) {
            if (nProp == 0) {
                if (pfrom->HasFulfilledRequest("mnvs")) {
                    LogPrintf("mnvs - peer already asked me for the list\n");
                    Misbehaving(pfrom->GetId(), 20);
                    return;
                }
                pfrom->FulfilledRequest("mnvs");
            }
        }

        Sync(pfrom, nProp);
        LogPrint("mnbudget", "mnvs - Sent Servicenode votes to peer %i\n", pfrom->GetId());
    }

    if (strCommand == "mprop") { //Servicenode Proposal
        CBudgetProposalBroadcast budgetProposalBroadcast;
        vRecv >> budgetProposalBroadcast;

        if (mapSeenServicenodeBudgetProposals.count(budgetProposalBroadcast.GetHash())) {
            servicenodeSync.AddedBudgetItem(budgetProposalBroadcast.GetHash());
            return;
        }

        std::string strError = "";
        int nConf = 0;
        if (!IsBudgetCollateralValid(budgetProposalBroadcast.nFeeTXHash, budgetProposalBroadcast.GetHash(), strError, budgetProposalBroadcast.nTime, nConf)) {
            LogPrintf("Proposal FeeTX is not valid - %s - %s\n", budgetProposalBroadcast.nFeeTXHash.ToString(), strError);
            if (nConf >= 1) vecImmatureBudgetProposals.push_back(budgetProposalBroadcast);
            return;
        }

        mapSeenServicenodeBudgetProposals.insert(make_pair(budgetProposalBroadcast.GetHash(), budgetProposalBroadcast));

        if (!budgetProposalBroadcast.IsValid(strError)) {
            LogPrintf("mprop - invalid budget proposal - %s\n", strError);
            return;
        }

        CBudgetProposal budgetProposal(budgetProposalBroadcast);
        if (AddProposal(budgetProposal)) {
            budgetProposalBroadcast.Relay();
        }
        servicenodeSync.AddedBudgetItem(budgetProposalBroadcast.GetHash());

        LogPrintf("mprop - new budget - %s\n", budgetProposalBroadcast.GetHash().ToString());

        //We might have active votes for this proposal that are valid now
        CheckOrphanVotes();
    }

    if (strCommand == "mvote") { //Servicenode Vote
        CBudgetVote vote;
        vRecv >> vote;
        vote.fValid = true;

        if (mapSeenServicenodeBudgetVotes.count(vote.GetHash())) {
            servicenodeSync.AddedBudgetItem(vote.GetHash());
            return;
        }

        CServicenode* pmn = mnodeman.Find(vote.vin);
        if (pmn == NULL) {
            LogPrintf("mvote - unknown servicenode - vin: %s\n", vote.vin.prevout.hash.ToString());
            mnodeman.AskForMN(pfrom, vote.vin);
            return;
        }


        mapSeenServicenodeBudgetVotes.insert(make_pair(vote.GetHash(), vote));
        if (!vote.SignatureValid(true)) {
            LogPrintf("mvote - signature invalid\n");
            if (servicenodeSync.IsSynced()) Misbehaving(pfrom->GetId(), 20);
            // it could just be a non-synced servicenode
            mnodeman.AskForMN(pfrom, vote.vin);
            return;
        }

        std::string strError = "";
        if (UpdateProposal(vote, pfrom, strError)) {
            vote.Relay();
            servicenodeSync.AddedBudgetItem(vote.GetHash());
        }

        LogPrint("mnbudget", "mvote - new budget vote - %s\n", vote.GetHash().ToString());
    }

    if (strCommand == "fbs") { //Finalized Budget Suggestion
        CFinalizedBudgetBroadcast finalizedBudgetBroadcast;
        vRecv >> finalizedBudgetBroadcast;

        if (mapSeenFinalizedBudgets.count(finalizedBudgetBroadcast.GetHash())) {
            servicenodeSync.AddedBudgetItem(finalizedBudgetBroadcast.GetHash());
            return;
        }

        std::string strError = "";
        int nConf = 0;
        if (!IsBudgetCollateralValid(finalizedBudgetBroadcast.nFeeTXHash, finalizedBudgetBroadcast.GetHash(), strError, finalizedBudgetBroadcast.nTime, nConf)) {
            LogPrintf("Finalized Budget FeeTX is not valid - %s - %s\n", finalizedBudgetBroadcast.nFeeTXHash.ToString(), strError);

            if (nConf >= 1) vecImmatureFinalizedBudgets.push_back(finalizedBudgetBroadcast);
            return;
        }

        mapSeenFinalizedBudgets.insert(make_pair(finalizedBudgetBroadcast.GetHash(), finalizedBudgetBroadcast));

        if (!finalizedBudgetBroadcast.IsValid(strError)) {
            LogPrintf("fbs - invalid finalized budget - %s\n", strError);
            return;
        }

        LogPrintf("fbs - new finalized budget - %s\n", finalizedBudgetBroadcast.GetHash().ToString());

        CFinalizedBudget finalizedBudget(finalizedBudgetBroadcast);
        if (AddFinalizedBudget(finalizedBudget)) {
            finalizedBudgetBroadcast.Relay();
        }
        servicenodeSync.AddedBudgetItem(finalizedBudgetBroadcast.GetHash());

        //we might have active votes for this budget that are now valid
        CheckOrphanVotes();
    }

    if (strCommand == "fbvote") { //Finalized Budget Vote
        CFinalizedBudgetVote vote;
        vRecv >> vote;
        vote.fValid = true;

        if (mapSeenFinalizedBudgetVotes.count(vote.GetHash())) {
            servicenodeSync.AddedBudgetItem(vote.GetHash());
            return;
        }

        CServicenode* pmn = mnodeman.Find(vote.vin);
        if (pmn == NULL) {
            LogPrint("mnbudget", "fbvote - unknown servicenode - vin: %s\n", vote.vin.prevout.hash.ToString());
            mnodeman.AskForMN(pfrom, vote.vin);
            return;
        }

        mapSeenFinalizedBudgetVotes.insert(make_pair(vote.GetHash(), vote));
        if (!vote.SignatureValid(true)) {
            LogPrintf("fbvote - signature invalid\n");
            if (servicenodeSync.IsSynced()) Misbehaving(pfrom->GetId(), 20);
            // it could just be a non-synced servicenode
            mnodeman.AskForMN(pfrom, vote.vin);
            return;
        }

        std::string strError = "";
        if (UpdateFinalizedBudget(vote, pfrom, strError)) {
            vote.Relay();
            servicenodeSync.AddedBudgetItem(vote.GetHash());

            LogPrintf("fbvote - new finalized budget vote - %s\n", vote.GetHash().ToString());
        } else {
            LogPrintf("fbvote - rejected finalized budget vote - %s - %s\n", vote.GetHash().ToString(), strError);
        }
    }
}

bool CBudgetManager::PropExists(uint256 nHash)
{
    if (mapProposals.count(nHash)) return true;
    return false;
}

//mark that a full sync is needed
void CBudgetManager::ResetSync()
{
    LOCK(cs);


    std::map<uint256, CBudgetProposalBroadcast>::iterator it1 = mapSeenServicenodeBudgetProposals.begin();
    while (it1 != mapSeenServicenodeBudgetProposals.end()) {
        CBudgetProposal* pbudgetProposal = FindProposal((*it1).first);
        if (pbudgetProposal && pbudgetProposal->fValid) {
            //mark votes
            std::map<uint256, CBudgetVote>::iterator it2 = pbudgetProposal->mapVotes.begin();
            while (it2 != pbudgetProposal->mapVotes.end()) {
                (*it2).second.fSynced = false;
                ++it2;
            }
        }
        ++it1;
    }

    std::map<uint256, CFinalizedBudgetBroadcast>::iterator it3 = mapSeenFinalizedBudgets.begin();
    while (it3 != mapSeenFinalizedBudgets.end()) {
        CFinalizedBudget* pfinalizedBudget = FindFinalizedBudget((*it3).first);
        if (pfinalizedBudget && pfinalizedBudget->fValid) {
            //send votes
            std::map<uint256, CFinalizedBudgetVote>::iterator it4 = pfinalizedBudget->mapVotes.begin();
            while (it4 != pfinalizedBudget->mapVotes.end()) {
                (*it4).second.fSynced = false;
                ++it4;
            }
        }
        ++it3;
    }
}

void CBudgetManager::MarkSynced()
{
    LOCK(cs);

    /*
        Mark that we've sent all valid items
    */

    std::map<uint256, CBudgetProposalBroadcast>::iterator it1 = mapSeenServicenodeBudgetProposals.begin();
    while (it1 != mapSeenServicenodeBudgetProposals.end()) {
        CBudgetProposal* pbudgetProposal = FindProposal((*it1).first);
        if (pbudgetProposal && pbudgetProposal->fValid) {
            //mark votes
            std::map<uint256, CBudgetVote>::iterator it2 = pbudgetProposal->mapVotes.begin();
            while (it2 != pbudgetProposal->mapVotes.end()) {
                if ((*it2).second.fValid)
                    (*it2).second.fSynced = true;
                ++it2;
            }
        }
        ++it1;
    }

    std::map<uint256, CFinalizedBudgetBroadcast>::iterator it3 = mapSeenFinalizedBudgets.begin();
    while (it3 != mapSeenFinalizedBudgets.end()) {
        CFinalizedBudget* pfinalizedBudget = FindFinalizedBudget((*it3).first);
        if (pfinalizedBudget && pfinalizedBudget->fValid) {
            //mark votes
            std::map<uint256, CFinalizedBudgetVote>::iterator it4 = pfinalizedBudget->mapVotes.begin();
            while (it4 != pfinalizedBudget->mapVotes.end()) {
                if ((*it4).second.fValid)
                    (*it4).second.fSynced = true;
                ++it4;
            }
        }
        ++it3;
    }
}


void CBudgetManager::Sync(CNode* pfrom, uint256 nProp, bool fPartial)
{
    LOCK(cs);

    /*
        Sync with a client on the network

        --

        This code checks each of the hash maps for all known budget proposals and finalized budget proposals, then checks them against the
        budget object to see if they're OK. If all checks pass, we'll send it to the peer.

    */

    int nInvCount = 0;

    std::map<uint256, CBudgetProposalBroadcast>::iterator it1 = mapSeenServicenodeBudgetProposals.begin();
    while (it1 != mapSeenServicenodeBudgetProposals.end()) {
        CBudgetProposal* pbudgetProposal = FindProposal((*it1).first);
        if (pbudgetProposal && pbudgetProposal->fValid && (nProp == 0 || (*it1).first == nProp)) {
            pfrom->PushInventory(CInv(MSG_BUDGET_PROPOSAL, (*it1).second.GetHash()));
            nInvCount++;

            //send votes
            std::map<uint256, CBudgetVote>::iterator it2 = pbudgetProposal->mapVotes.begin();
            while (it2 != pbudgetProposal->mapVotes.end()) {
                if ((*it2).second.fValid) {
                    if ((fPartial && !(*it2).second.fSynced) || !fPartial) {
                        pfrom->PushInventory(CInv(MSG_BUDGET_VOTE, (*it2).second.GetHash()));
                        nInvCount++;
                    }
                }
                ++it2;
            }
        }
        ++it1;
    }

    pfrom->PushMessage("ssc", SERVICENODE_SYNC_BUDGET_PROP, nInvCount);

    LogPrint("mnbudget", "CBudgetManager::Sync - sent %d items\n", nInvCount);

    nInvCount = 0;

    std::map<uint256, CFinalizedBudgetBroadcast>::iterator it3 = mapSeenFinalizedBudgets.begin();
    while (it3 != mapSeenFinalizedBudgets.end()) {
        CFinalizedBudget* pfinalizedBudget = FindFinalizedBudget((*it3).first);
        if (pfinalizedBudget && pfinalizedBudget->fValid && (nProp == 0 || (*it3).first == nProp)) {
            pfrom->PushInventory(CInv(MSG_BUDGET_FINALIZED, (*it3).second.GetHash()));
            nInvCount++;

            //send votes
            std::map<uint256, CFinalizedBudgetVote>::iterator it4 = pfinalizedBudget->mapVotes.begin();
            while (it4 != pfinalizedBudget->mapVotes.end()) {
                if ((*it4).second.fValid) {
                    if ((fPartial && !(*it4).second.fSynced) || !fPartial) {
                        pfrom->PushInventory(CInv(MSG_BUDGET_FINALIZED_VOTE, (*it4).second.GetHash()));
                        nInvCount++;
                    }
                }
                ++it4;
            }
        }
        ++it3;
    }

    pfrom->PushMessage("ssc", SERVICENODE_SYNC_BUDGET_FIN, nInvCount);
    LogPrint("mnbudget", "CBudgetManager::Sync - sent %d items\n", nInvCount);
}

bool CBudgetManager::UpdateProposal(CBudgetVote& vote, CNode* pfrom, std::string& strError)
{
    LOCK(cs);

    if (!mapProposals.count(vote.nProposalHash)) {
        if (pfrom) {
            // only ask for missing items after our syncing process is complete --
            //   otherwise we'll think a full sync succeeded when they return a result
            if (!servicenodeSync.IsSynced()) return false;

            LogPrintf("CBudgetManager::UpdateProposal - Unknown proposal %s, asking for source proposal\n", vote.nProposalHash.ToString());
            mapOrphanServicenodeBudgetVotes[vote.nProposalHash] = vote;

            if (!askedForSourceProposalOrBudget.count(vote.nProposalHash)) {
                pfrom->PushMessage("mnvs", vote.nProposalHash);
                askedForSourceProposalOrBudget[vote.nProposalHash] = GetTime();
            }
        }

        strError = "Proposal not found!";
        return false;
    }

    return mapProposals[vote.nProposalHash].AddOrUpdateVote(vote, strError);
}

bool CBudgetManager::UpdateFinalizedBudget(CFinalizedBudgetVote& vote, CNode* pfrom, std::string& strError)
{
    LOCK(cs);

    if (!mapFinalizedBudgets.count(vote.nBudgetHash)) {
        if (pfrom) {
            // only ask for missing items after our syncing process is complete --
            //   otherwise we'll think a full sync succeeded when they return a result
            if (!servicenodeSync.IsSynced()) return false;

            LogPrintf("CBudgetManager::UpdateFinalizedBudget - Unknown Finalized Proposal %s, asking for source budget\n", vote.nBudgetHash.ToString());
            mapOrphanFinalizedBudgetVotes[vote.nBudgetHash] = vote;

            if (!askedForSourceProposalOrBudget.count(vote.nBudgetHash)) {
                pfrom->PushMessage("mnvs", vote.nBudgetHash);
                askedForSourceProposalOrBudget[vote.nBudgetHash] = GetTime();
            }
        }

        strError = "Finalized Budget not found!";
        return false;
    }

    return mapFinalizedBudgets[vote.nBudgetHash].AddOrUpdateVote(vote, strError);
}

CBudgetProposal::CBudgetProposal()
{
    strProposalName = "unknown";
    nBlockStart = 0;
    nBlockEnd = 0;
    nAmount = 0;
    nTime = 0;
    fValid = true;
}

CBudgetProposal::CBudgetProposal(std::string strProposalNameIn, std::string strURLIn, int nBlockStartIn, int nBlockEndIn, CScript addressIn, CAmount nAmountIn, uint256 nFeeTXHashIn)
{
    strProposalName = strProposalNameIn;
    strURL = strURLIn;
    nBlockStart = nBlockStartIn;
    nBlockEnd = nBlockEndIn;
    address = addressIn;
    nAmount = nAmountIn;
    nFeeTXHash = nFeeTXHashIn;
    fValid = true;
}

CBudgetProposal::CBudgetProposal(const CBudgetProposal& other)
{
    strProposalName = other.strProposalName;
    strURL = other.strURL;
    nBlockStart = other.nBlockStart;
    nBlockEnd = other.nBlockEnd;
    address = other.address;
    nAmount = other.nAmount;
    nTime = other.nTime;
    nFeeTXHash = other.nFeeTXHash;
    mapVotes = other.mapVotes;
    fValid = true;
}

bool CBudgetProposal::IsValid(std::string& strError, bool fCheckCollateral)
{
    if (nBlockStart < 0) {
        strError = "Invalid Proposal";
        return false;
    }

    if (nBlockEnd < nBlockStart) {
        strError = "Invalid nBlockEnd (end before start)";
        return false;
    }

    if (nAmount < 10 * COIN) {
        strError = "Invalid nAmount";
        return false;
    }

    if (address == CScript()) {
        strError = "Invalid Payment Address";
        return false;
    }

    if (fCheckCollateral) {
        int nConf = 0;
        if (!IsBudgetCollateralValid(nFeeTXHash, GetHash(), strError, nTime, nConf)) {
            strError = "Invalid collateral";
            return false;
        }
    }
    
    // Not supporting multisig
    if (address.IsPayToScriptHash()) {
        strError = "Multisig is not currently supported.";
        return false;
    }
    
    // Check payment amount
    if (nAmount > CBudgetManager::GetTotalBudget(nBlockStart)) {
        strError = "Payment more than max";
        return false;
    }
    
    // Check for valid block end
    {
        TRY_LOCK(cs_main, locked);
        if (locked && chainActive.Tip() && GetBlockEnd() < chainActive.Height() - GetBudgetPaymentCycleBlocks() / 2) {
            strError = "Invalid nBlockEnd (end too early)";
            return false;
        }
    }

    return true;
}

bool CBudgetProposal::AddOrUpdateVote(CBudgetVote& vote, std::string& strError)
{
    LOCK(cs);

    uint256 hash = vote.vin.prevout.GetHash();

    if (mapVotes.count(hash)) {
        if (mapVotes[hash].nTime > vote.nTime) {
            strError = strprintf("new vote older than existing vote - %s\n", vote.GetHash().ToString());
            LogPrint("mnbudget", "CBudgetProposal::AddOrUpdateVote - %s\n", strError);
            return false;
        }
        if (vote.nTime - mapVotes[hash].nTime < BUDGET_VOTE_UPDATE_MIN) {
            strError = strprintf("time between votes is too soon - %s - %lli\n", vote.GetHash().ToString(), vote.nTime - mapVotes[hash].nTime);
            LogPrint("mnbudget", "CBudgetProposal::AddOrUpdateVote - %s\n", strError);
            return false;
        }
    }

    if (vote.nTime > GetTime() + (60 * 60)) {
        strError = strprintf("new vote is too far ahead of current time - %s - nTime %lli - Max Time %lli\n", vote.GetHash().ToString(), vote.nTime, GetTime() + (60 * 60));
        LogPrint("mnbudget", "CBudgetProposal::AddOrUpdateVote - %s\n", strError);
        return false;
    }

    mapVotes[hash] = vote;
    return true;
}

// If servicenode voted for a proposal, but is now invalid -- remove the vote
void CBudgetProposal::CleanAndRemove(bool fSignatureCheck)
{
    std::map<uint256, CBudgetVote>::iterator it = mapVotes.begin();

    while (it != mapVotes.end()) {
        (*it).second.fValid = (*it).second.SignatureValid(fSignatureCheck);
        ++it;
    }
}

double CBudgetProposal::GetRatio()
{
    int yeas = 0;
    int nays = 0;

    std::map<uint256, CBudgetVote>::iterator it = mapVotes.begin();

    while (it != mapVotes.end()) {
        if ((*it).second.nVote == VOTE_YES) yeas++;
        if ((*it).second.nVote == VOTE_NO) nays++;
        ++it;
    }

    if (yeas + nays == 0) return 0.0f;

    return ((double)(yeas) / (double)(yeas + nays));
}

int CBudgetProposal::GetYeas()
{
    int ret = 0;

    std::map<uint256, CBudgetVote>::iterator it = mapVotes.begin();
    while (it != mapVotes.end()) {
        if ((*it).second.nVote == VOTE_YES && (*it).second.fValid) ret++;
        ++it;
    }

    return ret;
}

int CBudgetProposal::GetNays()
{
    int ret = 0;

    std::map<uint256, CBudgetVote>::iterator it = mapVotes.begin();
    while (it != mapVotes.end()) {
        if ((*it).second.nVote == VOTE_NO && (*it).second.fValid) ret++;
        ++it;
    }

    return ret;
}

int CBudgetProposal::GetAbstains()
{
    int ret = 0;

    std::map<uint256, CBudgetVote>::iterator it = mapVotes.begin();
    while (it != mapVotes.end()) {
        if ((*it).second.nVote == VOTE_ABSTAIN && (*it).second.fValid) ret++;
        ++it;
    }

    return ret;
}

int CBudgetProposal::GetBlockStartCycle()
{
    //end block is half way through the next cycle (so the proposal will be removed much after the payment is sent)

    return nBlockStart - nBlockStart % GetBudgetPaymentCycleBlocks();
}

int CBudgetProposal::GetBlockCurrentCycle()
{
    int chainHeight = 0;
    {
        TRY_LOCK(cs_main, locked);
        if (!locked) return -1;
        if (!chainActive.Tip()) return -1;
        chainHeight = chainActive.Height();
    }
    if (chainHeight >= GetBlockEndCycle() || chainHeight <= 0)
        return -1;

    return chainHeight - chainHeight % GetBudgetPaymentCycleBlocks();
}

int CBudgetProposal::GetBlockEndCycle()
{
    //end block is half way through the next cycle (so the proposal will be removed much after the payment is sent)

    return nBlockEnd - GetBudgetPaymentCycleBlocks() / 2;
}

int CBudgetProposal::GetTotalPaymentCount()
{
    return (GetBlockEndCycle() - GetBlockStartCycle()) / GetBudgetPaymentCycleBlocks();
}

int CBudgetProposal::GetRemainingPaymentCount()
{
    int currentCycle = GetBlockCurrentCycle();
    if (currentCycle < 0)
        return 0;

    // If this budget starts in the future, this value will be wrong
    int nPayments = (GetBlockEndCycle() - currentCycle) / GetBudgetPaymentCycleBlocks() - 1;
    // Take the lowest value
    return std::min(nPayments, GetTotalPaymentCount());
}

CBudgetProposalBroadcast::CBudgetProposalBroadcast(std::string strProposalNameIn, std::string strURLIn, int nPaymentCount, CScript addressIn, CAmount nAmountIn, int nBlockStartIn, uint256 nFeeTXHashIn)
{
    strProposalName = strProposalNameIn;
    strURL = strURLIn;

    nBlockStart = nBlockStartIn;

    int nCycleStart = nBlockStart - nBlockStart % GetBudgetPaymentCycleBlocks();
    //calculate the end of the cycle for this vote, add half a cycle (vote will be deleted after that block)
    nBlockEnd = nCycleStart + GetBudgetPaymentCycleBlocks() * nPaymentCount + GetBudgetPaymentCycleBlocks() / 2;

    address = addressIn;
    nAmount = nAmountIn;

    nFeeTXHash = nFeeTXHashIn;
}

void CBudgetProposalBroadcast::Relay()
{
    CInv inv(MSG_BUDGET_PROPOSAL, GetHash());
    RelayInv(inv);
}

CBudgetVote::CBudgetVote()
{
    vin = CTxIn();
    nProposalHash = 0;
    nVote = VOTE_ABSTAIN;
    nTime = 0;
    fValid = true;
    fSynced = false;
}

CBudgetVote::CBudgetVote(CTxIn vinIn, uint256 nProposalHashIn, int nVoteIn)
{
    vin = vinIn;
    nProposalHash = nProposalHashIn;
    nVote = nVoteIn;
    nTime = GetAdjustedTime();
    fValid = true;
    fSynced = false;
}

void CBudgetVote::Relay()
{
    CInv inv(MSG_BUDGET_VOTE, GetHash());
    RelayInv(inv);
}

bool CBudgetVote::Sign(CKey& keyServicenode, CPubKey& pubKeyServicenode)
{
    // Choose coins to use
    CPubKey pubKeyCollateralAddress;
    CKey keyCollateralAddress;

    std::string errorMessage;
    std::string strMessage = vin.prevout.ToStringShort() + nProposalHash.ToString() + boost::lexical_cast<std::string>(nVote) + boost::lexical_cast<std::string>(nTime);

    if (!obfuScationSigner.SignMessage(strMessage, errorMessage, vchSig, keyServicenode)) {
        LogPrintf("CBudgetVote::Sign - Error upon calling SignMessage");
        return false;
    }

    if (!obfuScationSigner.VerifyMessage(pubKeyServicenode, vchSig, strMessage, errorMessage)) {
        LogPrintf("CBudgetVote::Sign - Error upon calling VerifyMessage");
        return false;
    }

    return true;
}

bool CBudgetVote::SignatureValid(bool fSignatureCheck)
{
    std::string errorMessage;
    std::string strMessage = vin.prevout.ToStringShort() + nProposalHash.ToString() + boost::lexical_cast<std::string>(nVote) + boost::lexical_cast<std::string>(nTime);

    CServicenode* pmn = mnodeman.Find(vin);

    if (pmn == NULL) {
        LogPrintf("CBudgetVote::SignatureValid() - Unknown Servicenode - %s\n", vin.prevout.hash.ToString());
        return false;
    }

    if (!fSignatureCheck) return true;

    if (!obfuScationSigner.VerifyMessage(pmn->pubKeyServicenode, vchSig, strMessage, errorMessage)) {
        LogPrintf("CBudgetVote::SignatureValid() - Verify message failed\n");
        return false;
    }

    return true;
}

CFinalizedBudget::CFinalizedBudget()
{
    strBudgetName = "";
    nBlockStart = 0;
    vecBudgetPayments.clear();
    mapVotes.clear();
    nFeeTXHash = 0;
    nTime = 0;
    fValid = true;
    fAutoChecked = false;
}

CFinalizedBudget::CFinalizedBudget(const CFinalizedBudget& other)
{
    strBudgetName = other.strBudgetName;
    nBlockStart = other.nBlockStart;
    vecBudgetPayments = other.vecBudgetPayments;
    mapVotes = other.mapVotes;
    nFeeTXHash = other.nFeeTXHash;
    nTime = other.nTime;
    fValid = true;
    fAutoChecked = false;
}

bool CFinalizedBudget::AddOrUpdateVote(CFinalizedBudgetVote& vote, std::string& strError)
{
    LOCK(cs);

    uint256 hash = vote.vin.prevout.GetHash();
    if (mapVotes.count(hash)) {
        if (mapVotes[hash].nTime > vote.nTime) {
            strError = strprintf("new vote older than existing vote - %s\n", vote.GetHash().ToString());
            LogPrint("mnbudget", "CFinalizedBudget::AddOrUpdateVote - %s\n", strError);
            return false;
        }
        if (vote.nTime - mapVotes[hash].nTime < BUDGET_VOTE_UPDATE_MIN) {
            strError = strprintf("time between votes is too soon - %s - %lli\n", vote.GetHash().ToString(), vote.nTime - mapVotes[hash].nTime);
            LogPrint("mnbudget", "CFinalizedBudget::AddOrUpdateVote - %s\n", strError);
            return false;
        }
    }

    if (vote.nTime > GetTime() + (60 * 60)) {
        strError = strprintf("new vote is too far ahead of current time - %s - nTime %lli - Max Time %lli\n", vote.GetHash().ToString(), vote.nTime, GetTime() + (60 * 60));
        LogPrint("mnbudget", "CFinalizedBudget::AddOrUpdateVote - %s\n", strError);
        return false;
    }

    mapVotes[hash] = vote;
    return true;
}

//evaluate if we should vote for this. Servicenode only
void CFinalizedBudget::AutoCheck()
{
    int chainHeight = -1;
    {
        TRY_LOCK(cs_main, locked);
        if (!locked) return;
        if (!chainActive.Tip()) return;
        chainHeight = chainActive.Height();
    }
    if (chainHeight <= 0 || chainHeight % GetBudgetPaymentCycleBlocks() == 0) // do not autocheck superblock
        return;
    
    LOCK(cs);
    
    LogPrintf("CFinalizedBudget::AutoCheck - %lli - %d\n", chainHeight, fAutoChecked);
    
    // Ignore if not a servicenode
    if (!fServiceNode || fAutoChecked)
        return;
    
    //do this 1 in 4 blocks -- spread out the voting activity on mainnet
    // -- this function is only called every fourteenth block, so this is really 1 in 56 blocks
    if (Params().NetworkID() == CBaseChainParams::MAIN && rand() % 4 != 0) {
        LogPrintf("CFinalizedBudget::AutoCheck - waiting\n");
        return;
    }

    fAutoChecked = true; //we only need to check this once

    // Automatically submit a vote for this final budget if it's valid
    if (strBudgetMode == "auto") {
        std::vector<CBudgetProposal*> vBudgetProposals = budget.GetBudget();
        auto budgetPayments = vecBudgetPayments;
        // Process budgets that are found
        if (!vBudgetProposals.empty()) {
            for (auto i = static_cast<int>(budgetPayments.size()); i >= 0; i--) {
                CTxBudgetPayment &payment = budgetPayments[i];
                for (auto proposal : vBudgetProposals) {
                    if (payment.nProposalHash == proposal->GetHash() &&
                        payment.payee.ToString() == proposal->GetPayee().ToString() &&
                        payment.nAmount == proposal->GetAmount()) {
                        // Budget payment is valid, remove from list (using list as state machine, empty list means all were found)
                        budgetPayments.pop_back();
                        LogPrintf("CFinalizedBudget::AutoCheck - Valid Payment Found: %s | %s | %s for Proposal %s | %s | %s\n",
                                  payment.nProposalHash.ToString(), payment.payee.ToString(), FormatMoney(payment.nAmount),
                                  proposal->GetHash().ToString(), proposal->GetPayee().ToString(), FormatMoney(proposal->GetAmount()));
                    }
                }
            }
        }

        // Log orphaned payments
        for (auto &payment : budgetPayments) {
            LogPrintf("CFinalizedBudget::AutoCheck - Payment: %s | %s | %s doesn't match any proposals\n",
                      payment.nProposalHash.ToString(), payment.payee.ToString(), FormatMoney(payment.nAmount));
        }
        
        // Non-empty list indicates some payments have missing proposals, do not proceed to vote
        if (!budgetPayments.empty()) {
            LogPrintf("CFinalizedBudget::AutoCheck - Failed to find a valid final budget\n");
            return;
        }
        
        LogPrintf("CFinalizedBudget::AutoCheck - Finalized Budget Matches! Submitting Vote.\n");
        SubmitVote();
    }
}

// If servicenode voted for a proposal, but is now invalid -- remove the vote
void CFinalizedBudget::CleanAndRemove(bool fSignatureCheck)
{
    LOCK(cs);
    std::map<uint256, CFinalizedBudgetVote>::iterator it = mapVotes.begin();

    while (it != mapVotes.end()) {
        (*it).second.fValid = (*it).second.SignatureValid(fSignatureCheck);
        ++it;
    }
}


CAmount CFinalizedBudget::GetTotalPayout()
{
    LOCK(cs);
    CAmount ret = 0;

    for (unsigned int i = 0; i < vecBudgetPayments.size(); i++) {
        ret += vecBudgetPayments[i].nAmount;
    }

    return ret;
}

std::string CFinalizedBudget::GetProposals()
{
    LOCK(cs);
    std::string ret = "";

    BOOST_FOREACH (CTxBudgetPayment& budgetPayment, vecBudgetPayments) {
        CBudgetProposal* pbudgetProposal = budget.FindProposal(budgetPayment.nProposalHash);

        std::string token = budgetPayment.nProposalHash.ToString();

        if (pbudgetProposal) token = pbudgetProposal->GetName();
        if (ret == "") {
            ret = token;
        } else {
            ret += "," + token;
        }
    }
    return ret;
}

/**
 * Return the payees in this finalized budget sorted by vote count, highest voted payees first.
 * @param payees
 * @return
 */
bool CFinalizedBudget::GetPayees(std::vector<CTxBudgetPayment> &payees) {
    LOCK(cs);
    
    if (vecBudgetPayments.empty())
        return false;
    
    // Rank budget payments by votes (highest first)
    payees = vecBudgetPayments;
    sort(payees.begin(), payees.end(), [](const CTxBudgetPayment &a, const CTxBudgetPayment &b) {
        CBudgetProposal *proposalA = budget.FindProposal(a.nProposalHash);
        CBudgetProposal *proposalB = budget.FindProposal(b.nProposalHash);
        return proposalA->Votes() > proposalB->Votes();
    });
    
    return true;
}

/**
 * Get the status of all budget proposals.
 * @return
 */
std::string CFinalizedBudget::GetStatus()
{
    LOCK(cs);
    
    std::string retBadHashes = "";
    std::string retBadPayeeOrAmount = "";
    int nBlockHeight = GetBlockStart();
    
    std::vector<CTxBudgetPayment> budgetPayments;
    if (!GetBudgetPayments(nBlockHeight, budgetPayments)) {
        LogPrintf("CFinalizedBudget::GetStatus - Couldn't find budget payment for block %lld\n", nBlockHeight);
    }
    
    for (auto &budgetPayment : budgetPayments) {
        CBudgetProposal *pbudgetProposal = budget.FindProposal(budgetPayment.nProposalHash);
        if (!pbudgetProposal) {
            if (retBadHashes.empty()) {
                retBadHashes = "Unknown proposal hash! Check this proposal before voting" + budgetPayment.nProposalHash.ToString();
            } else {
                retBadHashes += "," + budgetPayment.nProposalHash.ToString();
            }
        } else {
            if (pbudgetProposal->GetPayee() != budgetPayment.payee || pbudgetProposal->GetAmount() != budgetPayment.nAmount) {
                if (retBadPayeeOrAmount.empty()) {
                    retBadPayeeOrAmount = "Budget payee/nAmount doesn't match our proposal! " + budgetPayment.nProposalHash.ToString();
                } else {
                    retBadPayeeOrAmount += "," + budgetPayment.nProposalHash.ToString();
                }
            }
        }
    }
    
    if (retBadHashes.empty() && retBadPayeeOrAmount.empty())
        return "OK";
    
    return retBadHashes + retBadPayeeOrAmount;
}

bool CFinalizedBudget::IsValid(std::string& strError, bool fCheckCollateral)
{
    LOCK(cs);
    
    // Must be the superblock
    if (nBlockStart % GetBudgetPaymentCycleBlocks() != 0) {
        strError = "Invalid BlockStart";
        return false;
    }
    if (GetBlockEnd() - nBlockStart != 0) { // finalized budget must be paid on the superblock in it's entirety
        strError = "Invalid BlockEnd";
        return false;
    }
    if ((int)vecBudgetPayments.size() > 500) {
        strError = "Invalid budget payments count (too many, max is 500)";
        return false;
    }
    if (strBudgetName.empty()) {
        strError = "Invalid Budget Name";
        return false;
    }
    if (nBlockStart == 0) {
        strError = "Invalid BlockStart == 0";
        return false;
    }
    if (nFeeTXHash == 0) {
        strError = "Invalid FeeTx == 0";
        return false;
    }

    //can only pay out 10% of the possible coins (min value of coins)
    if (GetTotalPayout() > CBudgetManager::GetTotalBudget(nBlockStart)) {
        strError = "Invalid Payout (more than max)";
        return false;
    }

    std::string strError2 = "";
    if (fCheckCollateral) {
        int nConf = 0;
        if (!IsBudgetCollateralValid(nFeeTXHash, GetHash(), strError2, nTime, nConf)) {
            {
                strError = "Invalid Collateral : " + strError2;
                return false;
            }
        }
    }
    
    // Final budget start block must not be in the past
    {
        TRY_LOCK(cs_main, locked);
        if (locked && chainActive.Tip() && nBlockStart < chainActive.Height()) {
            strError = "This budget has expired";
            return false;
        }
    }

    return true;
}

void CFinalizedBudget::SubmitVote()
{
    LOCK(cs);
    CPubKey pubKeyServicenode;
    CKey keyServicenode;
    std::string errorMessage;

    if (!obfuScationSigner.SetKey(strServiceNodePrivKey, errorMessage, keyServicenode, pubKeyServicenode)) {
        LogPrintf("CFinalizedBudget::SubmitVote - Error upon calling SetKey\n");
        return;
    }

    CFinalizedBudgetVote vote(activeServicenode.vin, GetHash());
    if (!vote.Sign(keyServicenode, pubKeyServicenode)) {
        LogPrintf("CFinalizedBudget::SubmitVote - Failure to sign.");
        return;
    }

    std::string strError = "";
    if (budget.UpdateFinalizedBudget(vote, NULL, strError)) {
        LogPrintf("CFinalizedBudget::SubmitVote  - new finalized budget vote - %s\n", vote.GetHash().ToString());

        budget.mapSeenFinalizedBudgetVotes.insert(make_pair(vote.GetHash(), vote));
        vote.Relay();
    } else {
        LogPrintf("CFinalizedBudget::SubmitVote : Error submitting vote - %s\n", strError);
    }
}

bool CFinalizedBudget::GetBudgetPayments(int64_t nBlockHeight, std::vector<CTxBudgetPayment> &payments) {
    LOCK(cs);
    
    if (nBlockHeight - GetBlockStart() < 0)
        return false;
    
    for (const auto &payment : vecBudgetPayments)
        payments.push_back(payment);
    
    return true;
}

CFinalizedBudgetBroadcast::CFinalizedBudgetBroadcast()
{
    strBudgetName = "";
    nBlockStart = 0;
    vecBudgetPayments.clear();
    mapVotes.clear();
    vchSig.clear();
    nFeeTXHash = 0;
}

CFinalizedBudgetBroadcast::CFinalizedBudgetBroadcast(const CFinalizedBudget& other)
{
    strBudgetName = other.strBudgetName;
    nBlockStart = other.nBlockStart;
    BOOST_FOREACH (CTxBudgetPayment out, other.vecBudgetPayments)
    vecBudgetPayments.push_back(out);
    mapVotes = other.mapVotes;
    nFeeTXHash = other.nFeeTXHash;
}

CFinalizedBudgetBroadcast::CFinalizedBudgetBroadcast(std::string strBudgetNameIn, int nBlockStartIn, std::vector<CTxBudgetPayment> vecBudgetPaymentsIn, uint256 nFeeTXHashIn)
{
    strBudgetName = strBudgetNameIn;
    nBlockStart = nBlockStartIn;
    BOOST_FOREACH (CTxBudgetPayment out, vecBudgetPaymentsIn)
    vecBudgetPayments.push_back(out);
    mapVotes.clear();
    nFeeTXHash = nFeeTXHashIn;
}

void CFinalizedBudgetBroadcast::Relay()
{
    CInv inv(MSG_BUDGET_FINALIZED, GetHash());
    RelayInv(inv);
}

CFinalizedBudgetVote::CFinalizedBudgetVote()
{
    vin = CTxIn();
    nBudgetHash = 0;
    nTime = 0;
    vchSig.clear();
    fValid = true;
    fSynced = false;
}

CFinalizedBudgetVote::CFinalizedBudgetVote(CTxIn vinIn, uint256 nBudgetHashIn)
{
    vin = vinIn;
    nBudgetHash = nBudgetHashIn;
    nTime = GetAdjustedTime();
    vchSig.clear();
    fValid = true;
    fSynced = false;
}

void CFinalizedBudgetVote::Relay()
{
    CInv inv(MSG_BUDGET_FINALIZED_VOTE, GetHash());
    RelayInv(inv);
}

bool CFinalizedBudgetVote::Sign(CKey& keyServicenode, CPubKey& pubKeyServicenode)
{
    // Choose coins to use
    CPubKey pubKeyCollateralAddress;
    CKey keyCollateralAddress;

    std::string errorMessage;
    std::string strMessage = vin.prevout.ToStringShort() + nBudgetHash.ToString() + boost::lexical_cast<std::string>(nTime);

    if (!obfuScationSigner.SignMessage(strMessage, errorMessage, vchSig, keyServicenode)) {
        LogPrintf("CFinalizedBudgetVote::Sign - Error upon calling SignMessage");
        return false;
    }

    if (!obfuScationSigner.VerifyMessage(pubKeyServicenode, vchSig, strMessage, errorMessage)) {
        LogPrintf("CFinalizedBudgetVote::Sign - Error upon calling VerifyMessage");
        return false;
    }

    return true;
}

bool CFinalizedBudgetVote::SignatureValid(bool fSignatureCheck)
{
    std::string errorMessage;

    std::string strMessage = vin.prevout.ToStringShort() + nBudgetHash.ToString() + boost::lexical_cast<std::string>(nTime);

    CServicenode* pmn = mnodeman.Find(vin);

    if (pmn == NULL) {
        LogPrintf("CFinalizedBudgetVote::SignatureValid() - Unknown Servicenode\n");
        return false;
    }

    if (!fSignatureCheck) return true;

    if (!obfuScationSigner.VerifyMessage(pmn->pubKeyServicenode, vchSig, strMessage, errorMessage)) {
        LogPrintf("CFinalizedBudgetVote::SignatureValid() - Verify message failed\n");
        return false;
    }

    return true;
}

std::string CBudgetManager::ToString() const
{
    LOCK(cs);
    std::ostringstream info;

    info << "Proposals: " << (int)mapProposals.size() << ", Budgets: " << (int)mapFinalizedBudgets.size() << ", Seen Budgets: " << (int)mapSeenServicenodeBudgetProposals.size() << ", Seen Budget Votes: " << (int)mapSeenServicenodeBudgetVotes.size() << ", Seen Final Budgets: " << (int)mapSeenFinalizedBudgets.size() << ", Seen Final Budget Votes: " << (int)mapSeenFinalizedBudgetVotes.size();

    return info.str();
}
