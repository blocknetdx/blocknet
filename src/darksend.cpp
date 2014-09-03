

#include "darksend.h"
#include "main.h"
#include "init.h"

#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/lexical_cast.hpp>

#include <algorithm>
#include <boost/assign/list_of.hpp>

using namespace std;
using namespace boost;


// create DarkSend pools
CDarkSendPool darkSendPool;
CDarkSendSigner darkSendSigner;
std::vector<CMasterNode> darkSendMasterNodes;
std::vector<int64> darkSendDenominations;
int64 enforceMasternodePaymentsTime = 4085657524;
std::vector<CTxIn> vecMasternodeAskedFor;
std::vector<CDarksendQueue> vecDarksendQueue;
std::string strUseMasternode = "";

/* *** BEGIN DARKSEND MAGIC - DARKCOIN **********
    Copyright 2014, Darkcoin Developers 
        eduffield - evan@darkcoin.io
*/


struct CompareValueOnly
{
    bool operator()(const pair<int64, CTxIn>& t1,
                    const pair<int64, CTxIn>& t2) const
    {
        return t1.first < t2.first;
    }
};

struct CompareValueOnly2
{
    bool operator()(const pair<int64, int>& t1,
                    const pair<int64, int>& t2) const
    {
        return t1.first < t2.first;
    }
};



int randomizeList (int i) { return std::rand()%i;}

void CDarkSendPool::SetNull(bool clearEverything){
    finalTransaction.vin.clear();
    finalTransaction.vout.clear();

    entries.clear();

    state = POOL_STATUS_ACCEPTING_ENTRIES;

    lastTimeChanged = GetTimeMillis();

    entriesCount = 0;
    lastEntryAccepted = 0;
    countEntriesAccepted = 0;

    sessionUsers = 0;
    sessionAmount = 0;
    sessionFoundMasternode = false;
    sessionTries = 0;

    if(clearEverything){
        myEntries.clear();

        if(fMasterNode){
            sessionID = 1 + (rand() % 999999);
        } else {
            sessionID = 0;
        }
    }
}

bool CDarkSendPool::SetCollateralAddress(std::string strAddress){
    CBitcoinAddress address;
    if (!address.SetString(strAddress))
    {
        LogPrintf("CDarkSendPool::SetCollateralAddress - Invalid DarkSend collateral address\n");
        return false;
    }
    collateralPubKey.SetDestination(address.Get());
    return true;
}

void CDarkSendPool::UnlockCoins(){
    BOOST_FOREACH(CTxIn v, lockedCoins)
        pwalletMain->UnlockCoin(v.prevout);

    lockedCoins.clear();
}

void CDarkSendPool::Check()
{
    if(fDebug) LogPrintf("CDarkSendPool::Check()\n");
    if(fDebug) LogPrintf("CDarkSendPool::Check() - entries count %lu\n", entries.size());
    
    // move on to next phase
    if(state == POOL_STATUS_ACCEPTING_ENTRIES && entries.size() >= POOL_MAX_TRANSACTIONS)
    {
        if(fDebug) LogPrintf("CDarkSendPool::Check() -- ACCEPTING OUTPUTS\n");
        UpdateState(POOL_STATUS_FINALIZE_TRANSACTION);
    }

    if(state == POOL_STATUS_FINALIZE_TRANSACTION && finalTransaction.vin.empty() && finalTransaction.vout.empty()) {
        if(fDebug) LogPrintf("CDarkSendPool::Check() -- FINALIZE TRANSACTIONS\n");
        UpdateState(POOL_STATUS_SIGNING);

        if (fMasterNode) { 
            CTransaction txNew;

            for(unsigned int i = 0; i < entries.size(); i++){
                BOOST_FOREACH(const CTxOut v, entries[i].vout)
                    txNew.vout.push_back(v);

                BOOST_FOREACH(const CDarkSendEntryVin s, entries[i].sev)
                    txNew.vin.push_back(s.vin);
            }
            std::random_shuffle ( txNew.vout.begin(), txNew.vout.end(), randomizeList);

            LogPrintf("Transaction 1: %s\n", txNew.ToString().c_str());

            SignFinalTransaction(txNew, NULL);
            RelayDarkSendFinalTransaction(sessionID, txNew);
        }
    }

    // move on to next phase
    if(state == POOL_STATUS_SIGNING && SignaturesComplete()) { 
        if(fDebug) LogPrintf("CDarkSendPool::Check() -- SIGNING\n");            
        UpdateState(POOL_STATUS_TRANSMISSION);

        CWalletTx txNew = CWalletTx(pwalletMain, finalTransaction);

        LOCK2(cs_main, pwalletMain->cs_wallet);
        {
            if (fMasterNode) { //only the main node is master atm                
                LogPrintf("Transaction 2: %s\n", txNew.ToString().c_str());

                // Broadcast
                if (!txNew.AcceptToMemoryPool(true, false))
                {
                    LogPrintf("CDarkSendPool::Check() - CommitTransaction : Error: Transaction not valid\n");
                    SetNull();
                    pwalletMain->Lock();
                    UpdateState(POOL_STATUS_ACCEPTING_ENTRIES);
                    RelayDarkSendCompletedTransaction(sessionID, true, "Transaction not valid, please try again");
                    return;
                }

                int i = 0;
                BOOST_FOREACH(const CTxIn& txin, txNew.vin)
                {
                    BOOST_FOREACH(const CDarkSendEntry e, myEntries)
                    {
                        BOOST_FOREACH(const CDarkSendEntryVin s, e.sev) {
                            //find my pending transaction that matches
                            if(txin == s.vin)
                            {
                                LogPrintf("CDarkSendPool::Check() - marking vin spent %i", i);
                                CWalletTx &coin = pwalletMain->mapWallet[txin.prevout.hash];
                                coin.BindWallet(pwalletMain);
                                coin.MarkSpent(txin.prevout.n);
                                coin.WriteToDisk();
                            }
                        }
                    }
                    i++;
                }

                if(myEntries.size() > 0) {
                    // add to my wallet if it's mine
                    pwalletMain->AddToWallet(txNew);
                }
                txNew.AddSupportingTransactions();
                txNew.fTimeReceivedIsTxTime = true;
                
                txNew.RelayWalletTransaction();
                
                RelayDarkSendCompletedTransaction(sessionID, false, "Transaction Created Successfully");
                LogPrintf("CDarkSendPool::Check() -- IS MASTER -- TRANSMITTING DARKSEND\n");
            }
        }
    }

    // move on to next phase, allow 3 seconds incase the masternode wants to send us anything else
    if((state == POOL_STATUS_TRANSMISSION && fMasterNode) || (state == POOL_STATUS_SIGNING && completedTransaction) ) {
        LogPrintf("CDarkSendPool::Check() -- COMPLETED -- RESETTING \n");
        SetNull(true);
        UnlockCoins();
        if(fMasterNode) RelayDarkSendStatus(darkSendPool.sessionID, darkSendPool.GetState(), darkSendPool.GetEntriesCount(), -1);    
        pwalletMain->Lock();
    }

    if((state == POOL_STATUS_ERROR || state == POOL_STATUS_SUCCESS) && GetTimeMillis()-lastTimeChanged >= 10000) {
        LogPrintf("CDarkSendPool::Check() -- RESETTING MESSAGE \n");
        SetNull(true);
        if(fMasterNode) RelayDarkSendStatus(darkSendPool.sessionID, darkSendPool.GetState(), darkSendPool.GetEntriesCount(), -1);    
        UnlockCoins();
    }
}

void CDarkSendPool::ChargeFees(){
    if(fMasterNode) {
        int i = 0;
        // who didn't sign?
        BOOST_FOREACH(const CDarkSendEntry v, entries) {
            BOOST_FOREACH(const CDarkSendEntryVin s, v.sev) {
                if(!s.isSigSet){
                    LogPrintf("CDarkSendPool::ChargeFees -- found uncooperative node (didn't sign). charging fees. %u\n", i);

                    CWalletTx wtxCollateral = CWalletTx(pwalletMain, v.collateral);

                    // Broadcast
                    if (!wtxCollateral.AcceptToMemoryPool(true, false))
                    {
                        // This must not fail. The transaction has already been signed and recorded.
                        LogPrintf("CDarkSendPool::ChargeFees() : Error: Transaction not valid");
                    }
                    wtxCollateral.RelayWalletTransaction();
                }
                i++;
            }
        }
    }
}

void CDarkSendPool::CheckTimeout(){
    // catching hanging sessions
    if(!fMasterNode) {
        if(state == POOL_STATUS_TRANSMISSION) {
            if(fDebug) LogPrintf("CDarkSendPool::CheckTimeout() -- SESSION COMPLETED -- CHECKING\n");
            Check();
        }        
    }

    int c = 0;
    vector<CDarksendQueue>::iterator it;
    for(it=vecDarksendQueue.begin();it<vecDarksendQueue.end();it++){
        if((*it).IsExpired()){
            LogPrintf("CDarkSendPool::CheckTimeout() : REMOVING EXPIRED QUEUE ENTRY - %d\n", c);
            vecDarksendQueue.erase(it);
            break;
        }
        c++;
    }

    if(state == POOL_STATUS_ACCEPTING_ENTRIES){
        c = 0;

        std::vector<CDarkSendEntry> *vec = &myEntries;
        if(fMasterNode) vec = &entries; 

        vector<CDarkSendEntry>::iterator it2;
        for(it2=vec->begin();it2<vec->end();it2++){
            if((*it2).IsExpired()){
                LogPrintf("CDarkSendPool::CheckTimeout() : REMOVING EXPIRED ENTRY - %d\n", c);
                vec->erase(it2);
                if(entries.size() == 0 && myEntries.size() == 0){
                    SetNull(true);
                    UnlockCoins();
                }
                if(fMasterNode){
                    RelayDarkSendStatus(darkSendPool.sessionID, darkSendPool.GetState(), darkSendPool.GetEntriesCount(), -1);   
                }
                break;
            }
            c++;
        }

        if(GetTimeMillis()-lastTimeChanged >= 120000){
            lastTimeChanged = GetTimeMillis();

            sessionUsers = 0;
            sessionAmount = 0;
            sessionFoundMasternode = false;
            sessionTries = 0;            
        
        }

    } else if(GetTimeMillis()-lastTimeChanged >= 30000){
        if(fDebug) LogPrintf("CDarkSendPool::CheckTimeout() -- SESSION TIMED OUT (30) -- RESETTING\n");
        SetNull();
        UnlockCoins();

        UpdateState(POOL_STATUS_ERROR);
        lastMessage = "Session timed out (30), please resubmit";
    }


    if(state == POOL_STATUS_SIGNING && GetTimeMillis()-lastTimeChanged >= 10000 ) {
        if(fDebug) LogPrintf("CDarkSendPool::CheckTimeout() -- SESSION TIMED OUT -- RESETTING\n");
        ChargeFees();
        SetNull();
        UnlockCoins();
        //add my transactions to the new session

        UpdateState(POOL_STATUS_ERROR);
        lastMessage = "Signing timed out, please resubmit";
    }
}

bool CDarkSendPool::SignatureValid(const CScript& newSig, const CTxIn& newVin){
    CTransaction txNew;
    txNew.vin.clear();
    txNew.vout.clear();

    int found = -1;
    CScript sigPubKey = CScript();
    unsigned int i = 0;

    BOOST_FOREACH(CDarkSendEntry e, entries) {
        BOOST_FOREACH(const CTxOut out, e.vout)
            txNew.vout.push_back(out);

        BOOST_FOREACH(const CDarkSendEntryVin s, e.sev){
            txNew.vin.push_back(s.vin);

            if(s.vin == newVin){
                found = i;
                sigPubKey = s.vin.prevPubKey;
            }
            i++;
        }
    }

    if(found >= 0){ //might have to do this one input at a time?
        int n = found;
        txNew.vin[n].scriptSig = newSig;
        if(fDebug) LogPrintf("CDarkSendPool::SignatureValid() - Sign with sig %s\n", newSig.ToString().substr(0,24).c_str());
        if (!VerifyScript(txNew.vin[n].scriptSig, sigPubKey, txNew, n, SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_STRICTENC, 0)){
            if(fDebug) LogPrintf("CDarkSendPool::SignatureValid() - Signing - ERROR signing input %u\n", n);
            return false;
        }
    }

    if(fDebug) LogPrintf("CDarkSendPool::SignatureValid() - Signing - Succesfully signed input\n");
    return true;
}

bool CDarkSendPool::IsCollateralValid(const CTransaction& txCollateral){    
    if(txCollateral.vout[0].scriptPubKey != collateralPubKey || 
       txCollateral.vout[0].nValue != DARKSEND_COLLATERAL) {
        if(fDebug) LogPrintf ("CDarkSendPool::IsCollateralValid - not correct amount or addr (0)\n");
        return false;
    }

    LogPrintf("CDarkSendPool::IsCollateralValid %s\n", txCollateral.ToString().c_str());

    CWalletTx wtxCollateral = CWalletTx(pwalletMain, txCollateral);

    if (!wtxCollateral.IsAcceptable(true, false)){
        if(fDebug) LogPrintf ("CDarkSendPool::IsCollateralValid - didn't pass IsAcceptable\n");
        return false;
    }

    return true;
}

bool CDarkSendPool::AddEntry(const std::vector<CTxIn>& newInput, const int64& nAmount, const CTransaction& txCollateral, const std::vector<CTxOut>& newOutput, std::string& error){
    if (!fMasterNode) return false;

    BOOST_FOREACH(CTxIn in, newInput) {
        if (in.prevout.IsNull() || nAmount < 0) {
            if(fDebug) LogPrintf ("CDarkSendPool::AddEntry - input not valid!\n");
            error = "input not valid";
            sessionUsers--;
            return false;
        }
    }

    if (!IsCollateralValid(txCollateral)){
        if(fDebug) LogPrintf ("CDarkSendPool::AddEntry - collateral not valid!\n");
        error = "collateral not valid";
        sessionUsers--;
        return false;
    }

    if(entries.size() >= POOL_MAX_TRANSACTIONS){
        if(fDebug) LogPrintf ("CDarkSendPool::AddEntry - entries is full!\n");   
        error = "entries is full";
        sessionUsers--;
        return false;
    }

    BOOST_FOREACH(CTxIn in, newInput) {
        LogPrintf("looking for vin -- %s\n", in.ToString().c_str());
        BOOST_FOREACH(const CDarkSendEntry v, entries) {
            BOOST_FOREACH(const CDarkSendEntryVin s, v.sev){
                if(s.vin == in) {
                    if(fDebug) LogPrintf ("CDarkSendPool::AddEntry - found in vin\n"); 
                    error = "already have that vin";
                    sessionUsers--;
                    return false;
                }
            }
        }
    }

    if(state == POOL_STATUS_ACCEPTING_ENTRIES) {
        CDarkSendEntry v;
        v.Add(newInput, nAmount, txCollateral, newOutput);
        entries.push_back(v);

        LogPrintf("CDarkSendPool::AddEntry -- adding %s\n", newInput[0].ToString().c_str());
        error = "";

        if(entries.size() == 1) {
            //broadcast that I'm accepting entries
            CDarksendQueue dsq;
            dsq.nDenom = GetDenominations(newOutput);
            dsq.vin = vinMasterNode;
            dsq.time = GetTime();
            dsq.Sign();
            dsq.Relay();
        }

        return true;
    }

    if(fDebug) LogPrintf ("CDarkSendPool::AddEntry - can't accept new entry, wrong state!\n");
    error = "wrong state";
    sessionUsers--;
    return false;
}

bool CDarkSendPool::AddScriptSig(const CTxIn& newVin){
    LogPrintf("CDarkSendPool::AddScriptSig -- new sig  %s\n", newVin.scriptSig.ToString().substr(0,24).c_str());
    
    BOOST_FOREACH(const CDarkSendEntry v, entries) {
        BOOST_FOREACH(const CDarkSendEntryVin s, v.sev){
            if(s.vin.scriptSig == newVin.scriptSig) {
                LogPrintf("CDarkSendPool::AddScriptSig - already exists \n");
                return false;
            }
        }
    }

    if(!SignatureValid(newVin.scriptSig, newVin)){
        LogPrintf("CDarkSendPool::AddScriptSig - Invalid Sig\n");
        return false;
    }

    LogPrintf(" --- POOL_STATUS %d\n", state);

    LogPrintf("CDarkSendPool::AddScriptSig -- sig %s\n", newVin.ToString().c_str());

    if(state == POOL_STATUS_SIGNING) {
        BOOST_FOREACH(CTxIn& vin, finalTransaction.vin){
            if(newVin.prevout == vin.prevout && vin.nSequence == newVin.nSequence){
                vin.scriptSig = newVin.scriptSig;
                vin.prevPubKey = newVin.prevPubKey;
                LogPrintf("CDarkSendPool::AddScriptSig -- adding to finalTransaction  %s\n", newVin.scriptSig.ToString().substr(0,24).c_str());
            }
        }
        for(unsigned int i = 0; i < entries.size(); i++){
            if(entries[i].AddSig(newVin)){
                LogPrintf("CDarkSendPool::AddScriptSig -- adding  %s\n", newVin.scriptSig.ToString().substr(0,24).c_str());
                return true;
            }
        }
    }

    LogPrintf("CDarkSendPool::AddScriptSig -- Couldn't set sig!\n" );
    return false;
}

bool CDarkSendPool::SignaturesComplete(){
    BOOST_FOREACH(const CDarkSendEntry v, entries) {
        BOOST_FOREACH(const CDarkSendEntryVin s, v.sev){
            if(!s.isSigSet) return false;
        }
    }
    return true;
}

void CDarkSendPool::SendMoney(const CTransaction& collateral, std::vector<CTxIn>& vin, std::vector<CTxOut>& vout, int64& fee, int64 amount){
    BOOST_FOREACH(CTxIn in, collateral.vin)
        lockedCoins.push_back(in);
    
    BOOST_FOREACH(CTxIn in, vin)
        lockedCoins.push_back(in);

    if(!sessionFoundMasternode){
        LogPrintf("CDarkSendPool::SendMoney() - No masternode has been selected yet.\n");
        UnlockCoins();
        SetNull(true);
        return;
    }

    if (!CheckDiskSpace())
        return;

/*    BOOST_FOREACH(CTxOut& out, vout)
        out.scriptPubKey.insert(0, OP_DARKSEND);
*/
    if(fMasterNode) {
        LogPrintf("CDarkSendPool::SendMoney() - DarkSend from a masternode is not supported currently.\n");
        return;
    }

    LogPrintf("CDarkSendPool::SendMoney() - Added transaction to pool.\n");

    ClearLastMessage();

    //check it like a transaction
    {
        int64 nValueOut = 0;

        CValidationState state;
        CTransaction tx;

        BOOST_FOREACH(const CTxOut o, vout){
            nValueOut += o.nValue;
            tx.vout.push_back(o);
        }

        BOOST_FOREACH(const CTxIn i, vin){
            tx.vin.push_back(i);

            LogPrintf("dsi -- tx in %s\n", i.ToString().c_str());                
        }


        bool missing = false;
        if (!tx.IsAcceptable(state, true, false, &missing, false)){ //AcceptableInputs(state, true)){
            LogPrintf("dsi -- transactione not valid! %s \n", tx.ToString().c_str());
            return;
        }
    }



    // store our entry for later use
    CDarkSendEntry e;
    e.Add(vin, amount, collateral, vout);
    myEntries.push_back(e);

    // relay our entry to the master node
    RelayDarkSendIn(vin, amount, collateral, vout);
    Check();
}

bool CDarkSendPool::StatusUpdate(int newState, int newEntriesCount, int newAccepted, std::string& error, int newSessionID){
    if(fMasterNode) return false;
    if(state == POOL_STATUS_ERROR || state == POOL_STATUS_SUCCESS) return false;

    UpdateState(newState);
    entriesCount = newEntriesCount;

    LogPrintf("DarkSendStatusUpdate - state: %i entriesCount: %i accepted: %i error: %s \n", newState, newEntriesCount, newAccepted, error.c_str());

    if(newAccepted != -1) {
        lastEntryAccepted = newAccepted;
        countEntriesAccepted += newAccepted;
        if(newAccepted == 0){
            UpdateState(POOL_STATUS_ERROR);
            lastMessage = error;
        }

        if(newAccepted == 1) {
            sessionID = newSessionID;
            LogPrintf("CDarkSendPool::StatusUpdate - set sessionID to %d\n", sessionID);
            sessionFoundMasternode = true;
        }
    }

    if(newState == POOL_STATUS_ACCEPTING_ENTRIES){
        if(newAccepted == 1){
            LogPrintf("CDarkSendPool::StatusUpdate - entry accepted! \n");
            sessionFoundMasternode = true;
            if(darkSendPool.GetMyTransactionCount() == 0) DoAutomaticDenominating();
        } else if (newAccepted == 0 && sessionID == 0 && !sessionFoundMasternode) {
            LogPrintf("CDarkSendPool::StatusUpdate - entry not accepted by masternode \n");
            UnlockCoins();
            DoAutomaticDenominating();
        }
        if(sessionFoundMasternode) return true;
    }

    return true;
}

bool CDarkSendPool::SignFinalTransaction(CTransaction& finalTransactionNew, CNode* node){
    if(fDebug) LogPrintf("CDarkSendPool::AddFinalTransaction - Got Finalized Transaction\n");

    if(!finalTransaction.vin.empty()){
        LogPrintf("CDarkSendPool::AddFinalTransaction - Rejected Final Transaction!\n");
        return false;
    }


    finalTransaction = finalTransactionNew;
    LogPrintf("CDarkSendPool::SignFinalTransaction %s\n", finalTransaction.ToString().c_str());
    
    //make sure node is master
    //make sure my inputs/outputs are present, otherwise refuse to sign

    vector<CTxIn> sigs;

    BOOST_FOREACH(const CDarkSendEntry e, myEntries) {
        BOOST_FOREACH(const CDarkSendEntryVin s, e.sev) {
            /* Sign my transaction and all outputs */
            int mine = -1;
            CScript prevPubKey = CScript();
            CTxIn vin = CTxIn();
            
            for(unsigned int i = 0; i < finalTransaction.vin.size(); i++){
                if(finalTransaction.vin[i] == s.vin){
                    mine = i;
                    prevPubKey = s.vin.prevPubKey;
                    vin = s.vin;
                }
            }

            if(mine >= 0){ //might have to do this one input at a time?
                int foundOutputs = 0;
                
                for(unsigned int i = 0; i < finalTransaction.vout.size(); i++){
                    BOOST_FOREACH(const CTxOut o, e.vout) {
                        if(finalTransaction.vout[i] == o){
                            foundOutputs++;
                        }
                    }
                }

                int targetOuputs = e.vout.size();
                if(foundOutputs < targetOuputs) {
                    LogPrintf("CDarkSendPool::Sign - My entries are not correct! Refusing to sign. %d entries %d target. \n", foundOutputs, targetOuputs);
                    return false;
                }

                if(fDebug) LogPrintf("CDarkSendPool::Sign - Signing my input %i\n", mine);
                if(!SignSignature(*pwalletMain, prevPubKey, finalTransaction, mine, int(SIGHASH_ALL|SIGHASH_ANYONECANPAY))) { // changes scriptSig
                    if(fDebug) LogPrintf("CDarkSendPool::Sign - Unable to sign my own transaction! \n");
                    // not sure what to do here, it will timeout...?
                }

                sigs.push_back(finalTransaction.vin[mine]);
                LogPrintf(" -- dss %d %d %s\n", mine, (int)sigs.size(), finalTransaction.vin[mine].scriptSig.ToString().c_str());
            }
            
        }
        
        if(fDebug) LogPrintf("CDarkSendPool::Sign - txNew:\n%s", finalTransaction.ToString().c_str());
    }

    if(sigs.size() > 0 && node != NULL)
        node->PushMessage("dss", sigs);

    return true;
}


void CDarkSendPool::ProcessMasternodeConnections(){
    LOCK(cs_vNodes);
    
    BOOST_FOREACH(CNode* pnode, vNodes)
    {
        if(darkSendPool.GetMyTransactionCount() == 0 && pnode->fDarkSendMaster){
            LogPrintf("Closing masternode connection %s \n", pnode->addr.ToString().c_str());
            pnode->CloseSocketDisconnect();
        }
    }
}

bool CDarkSendPool::ConnectToBestMasterNode(int depth){
    if(fMasterNode) return false;
    
    int winner = GetCurrentMasterNode();
    LogPrintf("winner %d\n", winner);

    if(winner >= 0) {
        LogPrintf("CDarkSendPool::ConnectToBestMasterNode - Connecting to masternode at %s\n", darkSendMasterNodes[winner].addr.ToString().c_str());
        if(ConnectNode((CAddress)darkSendMasterNodes[winner].addr, NULL, true)){
            masterNodeAddr = darkSendMasterNodes[winner].addr.ToString();
            UpdateState(POOL_STATUS_ACCEPTING_ENTRIES);
            GetLastValidBlockHash(masterNodeBlockHash);
            return true;
        } else {
            darkSendMasterNodes[winner].enabled = 0;
            if(depth < 5){
                UpdateState(POOL_STATUS_ERROR);
                lastMessage = "Trying MasterNode #" + to_string(depth);
                return ConnectToBestMasterNode(depth+1);
            } else {
                UpdateState(POOL_STATUS_ERROR);
                lastMessage = "No valid MasterNode";
                LogPrintf("CDarkSendPool::ConnectToBestMasterNode - ERROR: %s\n", lastMessage.c_str());
            }
        }
    } else {
        UpdateState(POOL_STATUS_ERROR);
        lastMessage = "No valid MasterNode";
        LogPrintf("CDarkSendPool::ConnectToBestMasterNode - ERROR: %s\n", lastMessage.c_str());
    }

    //failed, so unlock any coins.
    UnlockCoins();

    //if couldn't connect, disable that one and try next
    return false;
}

bool CDarkSendPool::GetMasterNodeVin(CTxIn& vin, CPubKey& pubkey, CKey& secretKey)
{
    int64 nValueIn = 0;
    CScript pubScript;

    // try once before we try to denominate
    if (!pwalletMain->SelectCoinsMasternode(vin, nValueIn, pubScript))
    {
        if(fDebug) LogPrintf("CDarkSendPool::GetMasterNodeVin - I'm not a capable masternode\n");
        return false;
    }

    CTxDestination address1;
    ExtractDestination(pubScript, address1);
    CBitcoinAddress address2(address1);

    CKeyID keyID;
    if (!address2.GetKeyID(keyID)) {
        LogPrintf("CDarkSendPool::GetMasterNodeVin - Address does not refer to a key");
        return false;
    }

    if (!pwalletMain->GetKey(keyID, secretKey)) {
        LogPrintf ("CDarkSendPool::GetMasterNodeVin - Private key for address is not known");
        return false;
    }

    pubkey = secretKey.GetPubKey();
    return true;
}

bool CDarkSendPool::EnableHotColdMasterNode(CTxIn& vin, int64 sigTime, CService& addr)
{
    if(!fMasterNode) return false;

    isCapableMasterNode = MASTERNODE_IS_CAPABLE; 

    vinMasterNode = vin;
    masterNodeSignatureTime = sigTime;
    masterNodeSignAddr = addr;

    LogPrintf("CDarkSendPool::EnableHotColdMasterNode() - Enabled! You may shut down the cold daemon.");

    return true;
}

void CDarkSendPool::RegisterAsMasterNode(bool stop)
{
    if(!fMasterNode) return;

    std::string errorMessage;

    CKey key2;
    CPubKey pubkey2;

    if(!darkSendSigner.SetKey(strMasterNodePrivKey, errorMessage, key2, pubkey2))
    {
        LogPrintf("Invalid masternodeprivkey: '%s'\n", errorMessage.c_str());
        exit(0);
    }

    if(isCapableMasterNode == MASTERNODE_INPUT_TOO_NEW || isCapableMasterNode == MASTERNODE_NOT_CAPABLE){
        isCapableMasterNode = MASTERNODE_NOT_PROCESSED;
    }

    if(isCapableMasterNode == MASTERNODE_NOT_PROCESSED) {
        if(strMasterNodeAddr.empty()) {
            if(!GetLocal(masterNodeSignAddr)) {
                LogPrintf("CDarkSendPool::RegisterAsMasterNode() - Can't detect external address. Please use the masternodeaddr configuration option.");
                isCapableMasterNode = MASTERNODE_NOT_CAPABLE;
                return;
            }
        } else {
            masterNodeSignAddr = CService(strMasterNodeAddr);
        }

        if((fTestNet && masterNodeSignAddr.GetPort() != 19999) || (!fTestNet && masterNodeSignAddr.GetPort() != 9999)) {
            LogPrintf("CDarkSendPool::RegisterAsMasterNode() - Invalid port");
            isCapableMasterNode = MASTERNODE_NOT_CAPABLE;
            exit(0);
        }

        LogPrintf("CDarkSendPool::RegisterAsMasterNode() - Checking inbound connection to '%s'\n", masterNodeSignAddr.ToString().c_str());

        if(ConnectNode((CAddress)masterNodeSignAddr, masterNodeSignAddr.ToString().c_str())){
            darkSendPool.masternodePortOpen = MASTERNODE_PORT_OPEN;
        } else {
            darkSendPool.masternodePortOpen = MASTERNODE_PORT_NOT_OPEN;
            isCapableMasterNode = MASTERNODE_NOT_CAPABLE;
            return;
        }


        if(pwalletMain->IsLocked()){
            isCapableMasterNode = MASTERNODE_NOT_CAPABLE;
            return;
        }

        isCapableMasterNode = MASTERNODE_NOT_CAPABLE;

        CKey SecretKey;
        // Choose coins to use
        if(GetMasterNodeVin(vinMasterNode, pubkeyMasterNode, SecretKey)) {

            if(GetInputAge(vinMasterNode) < MASTERNODE_MIN_CONFIRMATIONS){
                LogPrintf("CDarkSendPool::RegisterAsMasterNode() - Input must have least %d confirmations - %d confirmations\n", MASTERNODE_MIN_CONFIRMATIONS, GetInputAge(vinMasterNode));
                isCapableMasterNode = MASTERNODE_INPUT_TOO_NEW;
                return;
            }

            masterNodeSignatureTime = GetTimeMicros();

            std::string vchPubKey(pubkeyMasterNode.begin(), pubkeyMasterNode.end());
            std::string vchPubKey2(pubkey2.begin(), pubkey2.end());
            std::string strMessage = masterNodeSignAddr.ToString() + boost::lexical_cast<std::string>(masterNodeSignatureTime) + vchPubKey + vchPubKey2;

            if(!darkSendSigner.SignMessage(strMessage, errorMessage, vchMasterNodeSignature, SecretKey)) {
                LogPrintf("CDarkSendPool::RegisterAsMasterNode() - Sign message failed");
                return;
            }

            if(!darkSendSigner.VerifyMessage(pubkeyMasterNode, vchMasterNodeSignature, strMessage, errorMessage)) {
                LogPrintf("CDarkSendPool::RegisterAsMasterNode() - Verify message failed");
                return;
            }

            LogPrintf("CDarkSendPool::RegisterAsMasterNode() - Is capable master node!\n");

            isCapableMasterNode = MASTERNODE_IS_CAPABLE; 

            pwalletMain->LockCoin(vinMasterNode.prevout);

            bool found = false;
            BOOST_FOREACH(CMasterNode& mn, darkSendMasterNodes)
                if(mn.vin == vinMasterNode)
                    found = true;

            if(!found) {                
                LogPrintf("CDarkSendPool::RegisterAsMasterNode() - Adding myself to masternode list %s - %s\n", masterNodeSignAddr.ToString().c_str(), vinMasterNode.ToString().c_str());
                CMasterNode mn(masterNodeSignAddr, vinMasterNode, pubkeyMasterNode, vchMasterNodeSignature, masterNodeSignatureTime, pubkey2);
                mn.UpdateLastSeen(masterNodeSignatureTime);
                darkSendMasterNodes.push_back(mn);
                LogPrintf("CDarkSendPool::RegisterAsMasterNode() - Masternode input = %s\n", vinMasterNode.ToString().c_str());
            }
        
            RelayDarkSendElectionEntry(vinMasterNode, masterNodeSignAddr, vchMasterNodeSignature, masterNodeSignatureTime, pubkeyMasterNode, pubkey2, -1, -1, masterNodeSignatureTime);

            return;
        }
    }

    if(isCapableMasterNode != MASTERNODE_IS_CAPABLE) return;

    masterNodeSignatureTime = GetTimeMicros();

    std::string strMessage = masterNodeSignAddr.ToString() + boost::lexical_cast<std::string>(masterNodeSignatureTime) + boost::lexical_cast<std::string>(stop);

    if(!darkSendSigner.SignMessage(strMessage, errorMessage, vchMasterNodeSignature, key2)) {
        LogPrintf("CDarkSendPool::RegisterAsMasterNode() - Sign message failed");
        return;
    }

    if(!darkSendSigner.VerifyMessage(pubkey2, vchMasterNodeSignature, strMessage, errorMessage)) {
        LogPrintf("CDarkSendPool::RegisterAsMasterNode() - Verify message failed");
        return;
    }

    bool found = false;
    BOOST_FOREACH(CMasterNode& mn, darkSendMasterNodes) {
        //LogPrintf(" -- %s\n", mn.vin.ToString().c_str());

        if(mn.vin == vinMasterNode) {
            found = true;
            mn.UpdateLastSeen();
        }
    }
    assert(found);

    LogPrintf("CDarkSendPool::RegisterAsMasterNode() - Masternode input = %s\n", vinMasterNode.ToString().c_str());

    if (stop) isCapableMasterNode = MASTERNODE_STOPPED;

    RelayDarkSendElectionEntryPing(vinMasterNode, vchMasterNodeSignature, masterNodeSignatureTime, stop);
}

//Get last block hash
bool CDarkSendPool::GetLastValidBlockHash(uint256& hash, int mod, int nBlockHeight)
{
    const CBlockIndex *BlockLastSolved = pindexBest;
    const CBlockIndex *BlockReading = pindexBest;

    if (BlockLastSolved == NULL || BlockLastSolved->nHeight == 0) { return false; }

    int nBlocksAgo = 0;
    if(nBlockHeight > 0) nBlocksAgo = nBlockHeight - (pindexBest->nHeight+1);
    assert(nBlocksAgo >= 0);
    
    int n = 0;
    for (unsigned int i = 1; BlockReading && BlockReading->nHeight > 0; i++) {
        if(BlockReading->nHeight % mod == 0) {
            if(n >= nBlocksAgo){
                hash = BlockReading->GetBlockHash();
                return true;
            }
            n++;
        }

        if (BlockReading->pprev == NULL) { assert(BlockReading); break; }
        BlockReading = BlockReading->pprev;
    }

    return false;    
}

void CDarkSendPool::NewBlock()
{
    if(fDebug) LogPrintf("CDarkSendPool::NewBlock \n");

    if(IsInitialBlockDownload()) return;
    
    if(fDisableDarksend) return;

    if(!fMasterNode){
        //denominate all non-denominated inputs every 25 minutes.
        if(pindexBest->nHeight % 10 == 0) UnlockCoins();
        ProcessMasternodeConnections();
    }

}

void CDarkSendPool::CompletedTransaction(bool error, std::string lastMessageNew)
{
    if(fMasterNode) return;

    if(error){
        LogPrintf("CompletedTransaction -- error \n");
        UpdateState(POOL_STATUS_ERROR);
    } else {
        LogPrintf("CompletedTransaction -- success \n");
        UpdateState(POOL_STATUS_SUCCESS);

        myEntries.clear();

        // To avoid race conditions, we'll only let DS run once per block
        cachedLastSuccess = nBestHeight;
    }
    lastMessage = lastMessageNew;

    completedTransaction = true;
    Check();
    UnlockCoins();
}

void CDarkSendPool::ClearLastMessage()
{
    lastMessage = "";
}

uint256 CMasterNode::CalculateScore(int mod, int64 nBlockHeight)
{
    if(pindexBest == NULL) return 0;

    uint256 n1 = 0;
    if(!darkSendPool.GetLastValidBlockHash(n1, mod, nBlockHeight)) return 0;

    uint256 n2 = Hash9(BEGIN(n1), END(n1));
    uint256 n3 = vin.prevout.hash > n2 ? (vin.prevout.hash - n2) : (n2 - vin.prevout.hash);
    
    /*
    LogPrintf(" -- MasterNode CalculateScore() n1 = %s \n", n1.ToString().c_str());
    LogPrintf(" -- MasterNode CalculateScore() n2 = %s \n", n2.ToString().c_str());
    LogPrintf(" -- MasterNode CalculateScore() vin = %s \n", vin.prevout.hash.ToString().c_str());
    LogPrintf(" -- MasterNode CalculateScore() n3 = %s \n", n3.ToString().c_str());*/

    return n3;
}

int CDarkSendPool::GetMasternodeByVin(CTxIn& vin)
{
    int i = 0;

    BOOST_FOREACH(CMasterNode mn, darkSendMasterNodes) {
        if (mn.vin == vin) return i;
        i++;
    }

    return -1;
}

int CDarkSendPool::GetCurrentMasterNode(int mod, int64 nBlockHeight)
{
    int i = 0;
    unsigned int score = 0;
    int winner = -1;

    if(!strUseMasternode.empty()){
        CService overrideAddr = CService(strUseMasternode);

        BOOST_FOREACH(CMasterNode mn, darkSendMasterNodes) {
            if(mn.addr == overrideAddr){
                LogPrintf("CDarkSendPool::GetCurrentMasterNode() - override %s\n", mn.addr.ToString().c_str());
                return i;
            }
            i++;
        }

        return -1;
    }

    // --- scan for winner

    BOOST_FOREACH(CMasterNode mn, darkSendMasterNodes) {
        mn.Check();
        if(!mn.IsEnabled()) {
            i++;
            continue;
        }

        uint256 n = mn.CalculateScore(mod, nBlockHeight);
        unsigned int n2 = 0;
        memcpy(&n2, &n, sizeof(n2));

        //LogPrintf("GetCurrentMasterNode: %d : %s : %u > %u\n", i, mn.addr.ToString().c_str(), n2, score);
        if(n2 > score){
            score = n2;
            winner = i;
        }
        i++;
    }
    //LogPrintf("GetCurrentMasterNode: winner %d\n", winner);

    return winner;
}

bool CDarkSendPool::DoAutomaticDenominating(bool fDryRun)
{
    if(fMasterNode) return false;

    //randomly denom between 3 and 8 blocks
    if(nBestHeight == cachedLastSuccess) {
        LogPrintf("CDarkSendPool::DoAutomaticDenominating - Last successful ds+ was too recent\n");
        return false;
    }
    if(fDisableDarksend) {
        LogPrintf("CDarkSendPool::DoAutomaticDenominating - Darksend is disabled\n");
        return false; 
    }

    if (!fDryRun && pwalletMain->IsLocked()){
        return false;
    }

    if(darkSendPool.GetState() != POOL_STATUS_ERROR && darkSendPool.GetState() != POOL_STATUS_SUCCESS){
        if(darkSendPool.GetMyTransactionCount() > 0){
            return true;
        }
    }

    // ** find the coins we'll use
    std::vector<CTxIn> vCoins;
    int64 nValueMin = 0.01*COIN;
    int64 nValueMax = 999*COIN;
    int64 nValueIn = 0;
    int minRounds = -2;
    int maxAmount = 1000;
    bool hasFeeInput = false;

    // if we have more denominated rounds (of any maturity) than the nAnonymizeDarkcoinAmount, we should use use those
    if(pwalletMain->GetDenominatedBalance(true) >= nAnonymizeDarkcoinAmount*COIN) {
        minRounds = 0;
    }
    //if we're set to less than a thousand, don't submit for than that to the pool
    if(nAnonymizeDarkcoinAmount < 1000) maxAmount = nAnonymizeDarkcoinAmount;

    //choose a random amount to denom
    //if(minRounds == 0) maxAmount = rand()%(maxAmount-(maxAmount/3))+maxAmount/3;

    int64 balanceNeedsAnonymized = pwalletMain->GetBalance() - pwalletMain->GetAnonymizedBalance();
    if(balanceNeedsAnonymized > maxAmount*COIN) balanceNeedsAnonymized= maxAmount*COIN;
    if(balanceNeedsAnonymized < COIN*1.1){
        LogPrintf("DoAutomaticDenominating : No funds detected in need of denominating \n");
        return false;
    }
    if(balanceNeedsAnonymized > nValueMax) balanceNeedsAnonymized = nValueMax;

    if (!pwalletMain->SelectCoinsDark(nValueMin, maxAmount*COIN, vCoins, nValueIn, minRounds, nDarksendRounds, hasFeeInput))
    {
        nValueIn = 0;
        vCoins.clear();

        //simply look for non-denominated coins
        if (pwalletMain->SelectCoinsDark(maxAmount*COIN, 9999999*COIN, vCoins, nValueIn, minRounds, nDarksendRounds, hasFeeInput))
        {
            if(!fDryRun) SplitUpMoney();
            return true;
        }

        LogPrintf("DoAutomaticDenominating : No funds detected in need of denominating (2)\n");
        return false;
    }

    if(nValueIn < COIN*1.1){

        //simply look for non-denominated coins
        if (pwalletMain->SelectCoinsDark(maxAmount*COIN, 9999999*COIN, vCoins, nValueIn, minRounds, nDarksendRounds, hasFeeInput))
        {
            if(!fDryRun) SplitUpMoney();
            return true;
        }

        LogPrintf("DoAutomaticDenominating : Too little to denominate (must have 1.1DRK) \n");
        return false;
    }

    if(fDryRun) return true;

    // initial phase, find a masternode
    if(!sessionFoundMasternode){
        int64 nTotalValue = pwalletMain->GetTotalValue(vCoins) - DARKSEND_FEE;
        if(nTotalValue/COIN > maxAmount) nTotalValue = maxAmount*COIN;
        
        double fDarkcoinSubmitted = nTotalValue / COIN;
        LogPrintf("Submiting Darksend for %f DRK\n", fDarkcoinSubmitted);

        // if we have any pending merges
        BOOST_FOREACH(CDarksendQueue dsq, vecDarksendQueue){
            CService addr;
            if(dsq.time == 0) continue;
            if(!dsq.GetAddress(addr)) continue;
            if(dsq.nDenom != GetDenominationsByAmount(nTotalValue)) {
                LogPrintf(" dsq.nDenom != GetDenominationsByAmount %"PRI64d" %d \n", dsq.nDenom, GetDenominationsByAmount(nTotalValue));
                continue;
            }
            dsq.time = 0; //remove node

            if(ConnectNode((CAddress)addr, NULL, true)){
                submittedToMasternode = addr;
                LOCK(cs_vNodes);
                BOOST_FOREACH(CNode* pnode, vNodes)
                {
                    if(submittedToMasternode != pnode->addr) continue;
                    pnode->PushMessage("dsa", nTotalValue);
                    LogPrintf("DoAutomaticDenominating --- connected (from queue), sending dsa for %"PRI64d"\n", nTotalValue);
                    return true;
                }
            } else {
                LogPrintf("DoAutomaticDenominating --- error connecting \n");
                return DoAutomaticDenominating();
            }
        }

        // otherwise, try one randomly
        if(sessionTries++ < 10){
            //pick a random masternode to use
            int max_value = darkSendMasterNodes.size();
            if(max_value <= 0) return false;
            int i = (rand() % max_value);

            lastTimeChanged = GetTimeMillis();
            LogPrintf("DoAutomaticDenominating -- attempt %d connection to masternode %s\n", sessionTries, darkSendMasterNodes[i].addr.ToString().c_str());
            if(ConnectNode((CAddress)darkSendMasterNodes[i].addr, NULL, true)){
                submittedToMasternode = darkSendMasterNodes[i].addr;
                LOCK(cs_vNodes);
                BOOST_FOREACH(CNode* pnode, vNodes)
                {
                    if(darkSendMasterNodes[i].addr != pnode->addr) continue;
                    pnode->PushMessage("dsa", nTotalValue);
                    LogPrintf("DoAutomaticDenominating --- connected, sending dsa for %"PRI64d"\n", nTotalValue);
                    return true;
                }
            } else {
                LogPrintf("DoAutomaticDenominating --- error connecting \n");
                return DoAutomaticDenominating();
            }
        } else {
            return false;
        }
    }

    std::string strError = pwalletMain->DarkSendDenominate(minRounds, maxAmount);
    LogPrintf("DoAutomaticDenominating : Running darksend denominate. Return '%s'\n", strError.c_str());
    
    if(strError == "") return true;

    if(strError == "Insufficient funds") {
        if(!fDryRun) SplitUpMoney();
        return true;
    } else if(strError == "Error: Darksend requires a collateral transaction and could not locate an acceptable input!"){
        if(!fDryRun) SplitUpMoney(true);
        return true;
    } else {
        LogPrintf("DoAutomaticDenominating : Error running denominate, %s\n", strError.c_str());
    }
    return false;
}

bool CDarkSendPool::SplitUpMoney(bool justCollateral)
{
    if((nBestHeight - lastSplitUpBlock) < 10){
        LogPrintf("SplitUpMoney - Too soon to split up again\n");
        return false;
    }

    int64 nTotalBalance = pwalletMain->GetDenominatedBalance(false);
    if(justCollateral && nTotalBalance > 1*COIN) nTotalBalance = 1*COIN;
    int64 nTotalOut = 0;
    lastSplitUpBlock = nBestHeight;

    LogPrintf("DoAutomaticDenominating: Split up large input (justCollateral %d):\n", justCollateral);
    LogPrintf(" auto -- nTotalBalance %"PRI64d"\n", nTotalBalance);
    LogPrintf(" auto-- nTotalOut %"PRI64d"\n", nTotalOut);

    // make our change address
    CReserveKey reservekey(pwalletMain);

    CScript scriptChange;
    CPubKey vchPubKey;
    assert(reservekey.GetReservedKey(vchPubKey)); // should never fail, as we just unlocked
    scriptChange.SetDestination(vchPubKey.GetID());

    CWalletTx wtx;
    int64 nFeeRet = 0;
    std::string strFail = "";
    vector< pair<CScript, int64> > vecSend;

    int64 a = nTotalBalance/5;
    if(a > 900*COIN) a = 900*COIN;
    if(a > nAnonymizeDarkcoinAmount*COIN) a = nAnonymizeDarkcoinAmount*COIN;

    LogPrintf(" auto-- split amount %"PRI64d"\n", a);

    int64 addingEachRound = (DARKSEND_FEE*5);
    if(!justCollateral) addingEachRound += (a) + (a/5);

    bool addedFees = false;
    while(nTotalOut + addingEachRound < nTotalBalance-DARKSEND_FEE && (!justCollateral || !addedFees)){
        LogPrintf(" nTotalOut %"PRI64d"\n", nTotalOut);
        LogPrintf(" nTotalOut + ((nTotalBalance/5) + (nTotalBalance/5/5) + 0.01*COIN) %"PRI64d"\n", nTotalOut + ((a) + (a/5) + ((DARKSEND_FEE*4))));
        LogPrintf(" nTotalBalance-(DARKSEND_COLLATERAL) %"PRI64d"\n", (nTotalBalance-DARKSEND_COLLATERAL));
        if(!justCollateral){
            vecSend.push_back(make_pair(scriptChange, a));
            vecSend.push_back(make_pair(scriptChange, a/5));
            nTotalOut += (a) + (a/5);
        }
        if(!addedFees){
            vecSend.push_back(make_pair(scriptChange, DARKSEND_COLLATERAL*5));
            vecSend.push_back(make_pair(scriptChange, DARKSEND_FEE));
            vecSend.push_back(make_pair(scriptChange, DARKSEND_FEE));
            vecSend.push_back(make_pair(scriptChange, DARKSEND_FEE));
            vecSend.push_back(make_pair(scriptChange, DARKSEND_FEE));
            vecSend.push_back(make_pair(scriptChange, DARKSEND_FEE));
            addedFees = true;
            nTotalOut += (DARKSEND_COLLATERAL*5)+(DARKSEND_FEE*5); 
        }
    }

    if(!justCollateral){
        if(nTotalOut <= 1.1*COIN || vecSend.size() < 3) 
            return false;
    } else {
        if(nTotalOut <= 0.1*COIN || vecSend.size() < 1) 
            return false;
    }

    CCoinControl *coinControl=NULL;
    bool success = pwalletMain->CreateTransaction(vecSend, wtx, reservekey, nFeeRet, strFail, coinControl, ONLY_NONDENOMINATED);
    if(!success){
        LogPrintf("SplitUpMoney: Error - %s\n", strFail.c_str());
        return false;
    }

    pwalletMain->CommitTransaction(wtx, reservekey);

    LogPrintf("SplitUpMoney Success: tx %s\n", wtx.GetHash().GetHex().c_str());

    return true;
}

int CDarkSendPool::GetMasternodeByRank(int findRank)
{
    int i = 0;
 
    if(!strUseMasternode.empty()){
        CService overrideAddr = CService(strUseMasternode);

        BOOST_FOREACH(CMasterNode mn, darkSendMasterNodes) {
            if(mn.addr == overrideAddr){
                LogPrintf("CDarkSendPool::GetMasternodeByRank() - override %s\n", mn.addr.ToString().c_str());
                return i;
            }
            i++;
        }

        return -1;
    }

    std::vector<pair<unsigned int, int> > vecMasternodeScores;

    i = 0;
    BOOST_FOREACH(CMasterNode mn, darkSendMasterNodes) {
        mn.Check();
        if(!mn.IsEnabled()) {
            i++;
            continue;
        }

        uint256 n = mn.CalculateScore();
        unsigned int n2 = 0;
        memcpy(&n2, &n, sizeof(n2));

        vecMasternodeScores.push_back(make_pair(n2, i));
        i++;
    }

    sort(vecMasternodeScores.rbegin(), vecMasternodeScores.rend(), CompareValueOnly2());
    
    int rank = 0;
    BOOST_FOREACH (PAIRTYPE(unsigned int, int)& s, vecMasternodeScores){
        rank++;
        if(rank == findRank) return s.second;
    }

    return -1;
}

int CDarkSendPool::GetMasternodeRank(CTxIn& vin, int mod)
{
    std::vector<pair<unsigned int, CTxIn> > vecMasternodeScores;

    BOOST_FOREACH(CMasterNode mn, darkSendMasterNodes) {
        mn.Check();
        if(!mn.IsEnabled()) {
            continue;
        }

        uint256 n = mn.CalculateScore(mod);
        unsigned int n2 = 0;
        memcpy(&n2, &n, sizeof(n2));

        vecMasternodeScores.push_back(make_pair(n2, mn.vin));
    }

    sort(vecMasternodeScores.rbegin(), vecMasternodeScores.rend(), CompareValueOnly());
    
    unsigned int rank = 0;
    BOOST_FOREACH (PAIRTYPE(unsigned int, CTxIn)& s, vecMasternodeScores){
        rank++;
        if(s.second == vin) return rank;
    }

    return -1;
}

// recursively find how many transactions deep the darksending goes
int CDarkSendPool::GetInputDarksendRounds(CTxIn in, int rounds)
{
    if(rounds >= 9) return rounds;

    std::string padding = "";
    padding.insert(0, ((rounds+1)*5)+3, ' ');

    //LogPrintf(" - %s %d\n", in.prevout.hash.ToString().c_str(), rounds);


    CWalletTx tx;
    if(pwalletMain->GetTransaction(in.prevout.hash,tx)){
        if(tx.vout[in.prevout.n].nValue == DARKSEND_FEE) return -3;

        if(rounds == 0){ //make sure the final output is non-denominate
            bool found = false;
            BOOST_FOREACH(int64 d, darkSendDenominations)
                if(tx.vout[in.prevout.n].nValue == d) found = true;

            if(!found) {
                //LogPrintf(" - NOT DENOM\n");
                return -2;
            }
        }
        bool found = false;

        BOOST_FOREACH(CTxOut out, tx.vout){
            BOOST_FOREACH(int64 d, darkSendDenominations)
                if(out.nValue == d)
                    found = true;
        }
        
        if(!found) {
            //LogPrintf(" - NOT FOUND\n");
            return rounds;
        }

        // find my vin and look that up
        BOOST_FOREACH(CTxIn in2, tx.vin) {
            if(pwalletMain->IsMine(in2)){
                //LogPrintf("rounds :: %s %s %d NEXT\n", padding.c_str(), in.ToString().c_str(), rounds);  
                int n = GetInputDarksendRounds(in2, rounds+1);
                if(n != -3) return n;
            }
        }
    } else {
        //LogPrintf("rounds :: %s %s %d NOTFOUND\n", padding.c_str(), in.ToString().c_str(), rounds);
    }

    return rounds-1;
}

void CMasterNode::Check()
{
    //once spent, stop doing the checks
    if(enabled==3) return;

    if(!UpdatedWithin(MASTERNODE_REMOVAL_MICROSECONDS)){
        enabled = 4;
        return;
    }

    if(!UpdatedWithin(MASTERNODE_EXPIRATION_MICROSECONDS)){
        enabled = 2;
        return;
    }

    if(!unitTest){
        CValidationState state;
        CTransaction tx = CTransaction();
        CTxOut vout = CTxOut(999.99*COIN, darkSendPool.collateralPubKey);
        tx.vin.push_back(vin);
        tx.vout.push_back(vout);

        if(!tx.AcceptableInputs(state, true)) {
            enabled = 3;
            return; 
        }
    }

    enabled = 1; // OK
}


bool CDarkSendPool::IsCompatibleWithEntries(std::vector<CTxOut> vout)
{
    BOOST_FOREACH(const CDarkSendEntry v, entries) {
        LogPrintf(" IsCompatibleWithEntries %d %d\n", GetDenominations(vout), GetDenominations(v.vout));
        if(GetDenominations(vout) != GetDenominations(v.vout)) return false;
    }

    return true;
}

bool CDarkSendPool::IsCompatibleWithSession(int64 nAmount)
{
    LogPrintf("CDarkSendPool::IsCompatibleWithSession - sessionAmount %"PRI64d" sessionUsers %d\n", sessionAmount, sessionUsers);

    if(sessionUsers < 0) sessionUsers = 0;
    
    if(sessionAmount == 0) {
        sessionAmount = nAmount;
        sessionUsers++;
        lastTimeChanged = GetTimeMillis();
        return true;
    }

    if(state != POOL_STATUS_ACCEPTING_ENTRIES || sessionUsers >= POOL_MAX_TRANSACTIONS){
        LogPrintf("CDarkSendPool::IsCompatibleWithSession - incompatible mode, return false %d %d\n", state != POOL_STATUS_ACCEPTING_ENTRIES, sessionUsers >= POOL_MAX_TRANSACTIONS);
        return false;
    }


    if(GetDenominationsByAmount(nAmount) != GetDenominationsByAmount(sessionAmount)) return false;
    LogPrintf("CDarkSendPool::IsCompatibleWithSession - compatible\n");

    sessionUsers++;
    lastTimeChanged = GetTimeMillis();
    return true;
}

int CDarkSendPool::GetDenominations(const std::vector<CTxOut>& vout){
    std::vector<pair<int64, int> > denomUsed;

    BOOST_FOREACH(int64 d, darkSendDenominations)
        denomUsed.push_back(make_pair(d, 0));

    BOOST_FOREACH(CTxOut out, vout)
        BOOST_FOREACH (PAIRTYPE(int64, int)& s, denomUsed)
            if (out.nValue == s.first)
                s.second = 1;

    int denom = 0;
    int c = 0;
    BOOST_FOREACH (PAIRTYPE(int64, int)& s, denomUsed)
        denom |= s.second << c++;

    return denom;
}

int CDarkSendPool::GetDenominationsByAmount(int64 nAmount){
    CScript e = CScript();
    int64 nValueLeft = nAmount;

    std::vector<CTxOut> vout1;
    BOOST_FOREACH(int64 v, darkSendDenominations){
        int nOutputs = 0;
        while(nValueLeft - v >= 0 && nOutputs <= 10) {
            CTxOut o(v, e);
            vout1.push_back(o);
            nValueLeft -= v;
            nOutputs++;
        }
    }

    return GetDenominations(vout1);
}

bool CDarkSendSigner::SetKey(std::string strSecret, std::string& errorMessage, CKey& key, CPubKey& pubkey){
    CBitcoinSecret vchSecret;
    bool fGood = vchSecret.SetString(strSecret);

    if (!fGood) {
        errorMessage = "Invalid private key";
        return false;
    }     

    key = vchSecret.GetKey();
    pubkey = key.GetPubKey();

    return true;
}

bool CDarkSendSigner::SignMessage(std::string strMessage, std::string& errorMessage, vector<unsigned char>& vchSig, CKey key)
{
    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    if (!key.SignCompact(ss.GetHash(), vchSig)) {
        errorMessage = "Sign failed";
        return false;
    }

    return true;
}

bool CDarkSendSigner::VerifyMessage(CPubKey pubkey, vector<unsigned char>& vchSig, std::string strMessage, std::string& errorMessage)
{
    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    CPubKey pubkey2;
    if (!pubkey2.RecoverCompact(ss.GetHash(), vchSig)) {
        errorMessage = "Error recovering pubkey";
        return false;
    }

    return (pubkey2.GetID() == pubkey.GetID());
}

bool CDarksendQueue::Sign()
{
    std::string strMessage = vin.ToString() + boost::lexical_cast<std::string>(nDenom) + boost::lexical_cast<std::string>(time); 

    CKey key2;
    CPubKey pubkey2;
    std::string errorMessage = "";

    if(!darkSendSigner.SetKey(strMasterNodePrivKey, errorMessage, key2, pubkey2))
    {
        LogPrintf("Invalid masternodeprivkey: '%s'\n", errorMessage.c_str());
        exit(0);
    }

    if(!darkSendSigner.SignMessage(strMessage, errorMessage, vchSig, key2)) {
        LogPrintf("CDarksendQueue():Relay - Sign message failed");
        return false;
    }

    if(!darkSendSigner.VerifyMessage(pubkey2, vchSig, strMessage, errorMessage)) {
        LogPrintf("CDarksendQueue():Relay - Verify message failed");
        return false;
    }
}

bool CDarksendQueue::Relay()
{

    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes)
        pnode->PushMessage("dsq", (*this));

    return true;
}

bool CDarksendQueue::CheckSignature()
{
    BOOST_FOREACH(CMasterNode& mn, darkSendMasterNodes) {

        if(mn.vin == vin) {
            std::string strMessage = vin.ToString() + boost::lexical_cast<std::string>(nDenom) + boost::lexical_cast<std::string>(time); 

            std::string errorMessage = "";
            if(!darkSendSigner.VerifyMessage(mn.pubkey2, vchSig, strMessage, errorMessage)){
                return error("Got bad masternode address signature %s \n", vin.ToString().c_str());
            }

            return true;
        }
    }

    return false;
}



void ThreadCheckDarkSendPool()
{
    // Make this thread recognisable as the wallet flushing thread
    RenameThread("bitcoin-darksend");

    unsigned int c = 0;
    while (true)
    {
        MilliSleep(1000);
        //LogPrintf("ThreadCheckDarkSendPool::check timeout\n");
        darkSendPool.CheckTimeout();
        
        if(c % 600 == 0){
            vector<CMasterNode>::iterator it;
            for(it=darkSendMasterNodes.begin();it<darkSendMasterNodes.end();it++){
                (*it).Check();
                if((*it).enabled == 4){
                    LogPrintf("REMOVING INACTIVE MASTERNODE %s\n", (*it).addr.ToString().c_str());
                    darkSendMasterNodes.erase(it);
                    break;
                }
            }
        }

        if(c == MASTERNODE_PING_SECONDS){
            darkSendPool.RegisterAsMasterNode(false);
            c = 0;
        }

        //auto denom every 2.5 minutes
        if(c % 60 == 0){
            darkSendPool.DoAutomaticDenominating();
        }
        c++;
    }
}
