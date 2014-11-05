
// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef DARKSEND_H
#define DARKSEND_H

#include "main.h"

class CTxIn;
class CDarkSendPool;
class CDarkSendSigner;
class CMasterNode;
class CMasterNodeVote;
class CBitcoinAddress;
class CDarksendQueue;
class CMasternodePayments;

extern CDarkSendPool darkSendPool;
extern CDarkSendSigner darkSendSigner;
extern CMasternodePayments masternodePayments;
extern std::vector<CMasterNode> darkSendMasterNodes;
extern std::vector<int64> darkSendDenominations;
extern std::string strMasterNodePrivKey;
extern std::vector<CDarksendQueue> vecDarksendQueue;
extern std::vector<CTxIn> vecMasternodeAskedFor;

static const int64 DARKSEND_COLLATERAL = (0.1*COIN);
static const int64 DARKSEND_FEE = 0.0125*COIN;


// 
// The Masternode Class. For managing the darksend process. It contains the input of the 1000DRK, signature to prove
// it's the one who own that ip address and code for calculating the payment election.
//  
class CMasterNode
{
public:
    CService addr;
    CTxIn vin;
    int64 lastTimeSeen;
    CPubKey pubkey;
    CPubKey pubkey2;
    std::vector<unsigned char> sig;
    int64 now;
    int enabled;
    bool unitTest;

    CMasterNode(CService newAddr, CTxIn newVin, CPubKey newPubkey, std::vector<unsigned char> newSig, int64 newNow, CPubKey newPubkey2)
    {
        addr = newAddr;
        vin = newVin;
        pubkey = newPubkey;
        pubkey2 = newPubkey2;
        sig = newSig;
        now = newNow;
        enabled = 1;
        lastTimeSeen = 0;
        unitTest = false;    
    }

    uint256 CalculateScore(int mod=1, int64 nBlockHeight=0);

    void UpdateLastSeen(int64 override=0)
    {
        if(override == 0){
            lastTimeSeen = GetAdjustedTime();
        } else {
            lastTimeSeen = override;
        }
    }

    void Check();

    bool UpdatedWithin(int seconds)
    {
        //LogPrintf("UpdatedWithin %"PRI64u", %"PRI64u" --  %d \n", GetTimeMicros() , lastTimeSeen, (GetTimeMicros() - lastTimeSeen) < seconds);

        return (GetAdjustedTime() - lastTimeSeen) < seconds;
    }

    void Disable()
    {
        lastTimeSeen = 0;
    }

    bool IsEnabled()
    {
        return enabled == 1;
    }
};

// for storing the winning payments
class CMasternodePaymentWinner
{
public:
    int nBlockHeight;
    CTxIn vin;
    std::vector<unsigned char> vchSig;
    uint64 score;

    CMasternodePaymentWinner() {
        nBlockHeight = 0;
        score = 0;
        vin = CTxIn();
    }

    uint256 GetHash(){
        uint256 n2 = Hash9(BEGIN(nBlockHeight), END(nBlockHeight));
        uint256 n3 = vin.prevout.hash > n2 ? (vin.prevout.hash - n2) : (n2 - vin.prevout.hash);

        return n3;
    }

    IMPLEMENT_SERIALIZE(
        READWRITE(nBlockHeight);
        READWRITE(score);
        READWRITE(vin);
        READWRITE(vchSig);
    )
};

//
// Masternode Payments Class 
// Keeps track of who should get paid for which blocks
//


class CMasternodePayments
{
private:
    std::vector<CMasternodePaymentWinner> vWinning;
    int nSyncedFromPeer;
    std::string strMasterPrivKey;
    std::string strTestPubKey;
    std::string strMainPubKey;

public:

    CMasternodePayments() {
        strMainPubKey = "04549ac134f694c0243f503e8c8a9a986f5de6610049c40b07816809b0d1d06a21b07be27b9bb555931773f62ba6cf35a25fd52f694d4e1106ccd237a7bb899fdd";
        strTestPubKey = "046f78dcf911fbd61910136f7f0f8d90578f68d0b3ac973b5040fb7afb501b5939f39b108b0569dca71488f5bbf498d92e4d1194f6f941307ffd95f75e76869f0e";
    }

    bool SetPrivKey(std::string strPrivKey);
    bool CheckSignature(CMasternodePaymentWinner& winner);
    bool Sign(CMasternodePaymentWinner& winner);
    
    // Deterministically calculate a given "score" for a masternode depending on how close it's hash is 
    // to the blockHeight. The further away they are the better, the furthest will win the election 
    // and get paid this block
    // 

    uint64 CalculateScore(uint256 blockHash, CTxIn& vin);
    bool GetWinningMasternode(int nBlockHeight, CTxIn& vinOut);
    bool AddWinningMasternode(CMasternodePaymentWinner& winner);
    bool ProcessBlock(int nBlockHeight);
    void Relay(CMasternodePaymentWinner& winner);
    void Sync(CNode* node);
    void CleanPaymentList();
    int LastPayment(CTxIn& vin);

    //slow
    bool GetBlockPayee(int nBlockHeight, CScript& payee);
};


// An input in the darksend pool
class CDarkSendEntryVin
{
public:
    bool isSigSet;
    CTxIn vin;

    CDarkSendEntryVin()
    {
        isSigSet = false;
        vin = CTxIn();
    }
};

// A clients transaction in the darksend pool
class CDarkSendEntry
{
public:
    bool isSet;
    std::vector<CDarkSendEntryVin> sev;
    int64 amount;
    CTransaction collateral;
    std::vector<CTxOut> vout;
    CTransaction txSupporting;
    int64 addedTime;

    CDarkSendEntry()
    {
        isSet = false;
        collateral = CTransaction();
        amount = 0;
    }

    bool Add(const std::vector<CTxIn> vinIn, int64 amountIn, const CTransaction collateralIn, const std::vector<CTxOut> voutIn)
    {
        if(isSet){return false;}

        BOOST_FOREACH(const CTxIn v, vinIn) {
            CDarkSendEntryVin s = CDarkSendEntryVin();
            s.vin = v;
            sev.push_back(s);
        }
        vout = voutIn;
        amount = amountIn;
        collateral = collateralIn;
        isSet = true;
        addedTime = GetTime();
        
        return true;
    }

    bool AddSig(const CTxIn& vin)
    {
        BOOST_FOREACH(CDarkSendEntryVin& s, sev) {
            if(s.vin.prevout == vin.prevout && s.vin.nSequence == vin.nSequence){
                if(s.isSigSet){return false;}
                s.vin.scriptSig = vin.scriptSig;
                s.vin.prevPubKey = vin.prevPubKey;
                s.isSigSet = true;
                
                return true;
            }
        }

        return false;
    }

    bool IsExpired()
    {
        return (GetTime() - addedTime) > 120;// 120 seconds
    }
};

// 
// A currently inprogress darksend merge and denomination information
//
class CDarksendQueue
{
public:
    CTxIn vin;
    int64 time;
    int nDenom;
    bool ready; //ready for submit
    std::vector<unsigned char> vchSig;

    CDarksendQueue()
    {
        nDenom = 0;
        vin = CTxIn();
        time = 0;   
        vchSig.clear();
        ready = false;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(nDenom);
        READWRITE(vin);
        READWRITE(time);
        READWRITE(ready);
        READWRITE(vchSig);
    )

    bool GetAddress(CService &addr)
    {
        BOOST_FOREACH(CMasterNode mn, darkSendMasterNodes) {
            if(mn.vin == vin){
                addr = mn.addr;
                return true;
            }
        }
        return false;
    }

    bool Sign();
    bool Relay();

    bool IsExpired()
    {
        return (GetTime() - time) > 120;// 120 seconds
    }

    bool CheckSignature();

};

//
// Helper object for signing and checking signatures
//
class CDarkSendSigner
{
public:
    bool IsVinAssociatedWithPubkey(CTxIn& vin, CPubKey& pubkey);
    bool SetKey(std::string strSecret, std::string& errorMessage, CKey& key, CPubKey& pubkey);
    bool SignMessage(std::string strMessage, std::string& errorMessage, std::vector<unsigned char>& vchSig, CKey key);
    bool VerifyMessage(CPubKey pubkey, std::vector<unsigned char>& vchSig, std::string strMessage, std::string& errorMessage);
};


//
// Used to keep track of current status of darksend pool
//
class CDarkSendPool
{
public:
    static const int MIN_PEER_PROTO_VERSION = 70043;

    // clients entries
    std::vector<CDarkSendEntry> myEntries;
    // masternode entries
    std::vector<CDarkSendEntry> entries;
    // the finalized transaction ready for signing
    CTransaction finalTransaction;

    int64 lastTimeChanged;
    int64 lastAutoDenomination;

    unsigned int state;
    unsigned int entriesCount;
    unsigned int lastEntryAccepted;
    unsigned int countEntriesAccepted;

    // where collateral should be made out to
    CScript collateralPubKey;

    std::vector<CTxIn> lockedCoins;
    
    CTxIn vinMasterNode;
    CPubKey pubkeyMasterNode;
    CPubKey pubkeyMasterNode2;

    std::string strMasterNodeSignMessage;
    std::vector<unsigned char> vchMasterNodeSignature;
     
    int isCapableMasterNode;
    uint256 masterNodeBlockHash;
    std::string masterNodeAddr;
    CService masterNodeSignAddr;
    int64 masterNodeSignatureTime;
    int masternodePortOpen;

    std::string lastMessage;
    bool completedTransaction;
    bool unitTest;
    CService submittedToMasternode;

    int sessionID;
    int64 sessionAmount; //Users must submit an amount compatible with this amount
    int sessionUsers; //N Users have said they'll join
    bool sessionFoundMasternode; //If we've found a compatible masternode
    int sessionTries;
    std::vector<CTransaction> vecSessionCollateral;

    int lastSplitUpBlock;
    int splitUpInARow; // how many splits we've done since a success?
    int cachedLastSuccess;
    int cachedNumBlocks; //used for the overview screen

    CTransaction txCollateral; //collateral tx used during process

    CDarkSendPool()
    {
        /* DarkSend uses collateral addresses to trust parties entering the pool
            to behave themselves. If they don't it takes their money. */

        std::string strAddress = "";  
        if(!fTestNet) {
            strAddress = "Xq19GqFvajRrEdDHYRKGYjTsQfpV5jyipF";
        } else {
            strAddress = "mxE2Rp3oYpSEFdsN5TdHWhZvEHm3PJQQVm";
        }
        
        isCapableMasterNode = MASTERNODE_NOT_PROCESSED;
        masternodePortOpen = 0;
        lastSplitUpBlock = 0;
        cachedLastSuccess = 0;
        cachedNumBlocks = 0;
        unitTest = false;
        splitUpInARow = 0;
        txCollateral = CTransaction();

        SetCollateralAddress(strAddress);
        SetNull();
    }

    bool SetCollateralAddress(std::string strAddress);
    void SetNull(bool clearEverything=false);

    void UnlockCoins();

    bool IsNull() const
    {   
        return (state == POOL_STATUS_ACCEPTING_ENTRIES && entries.empty() && myEntries.empty());
    }

    int GetState() const
    {
        return state;
    }

    int GetEntriesCount() const
    {
        if(fMasterNode){
            return entries.size(); 
        } else {
            return entriesCount;
        }
    }

    int GetLastEntryAccepted() const
    {
        return lastEntryAccepted;
    }

    int GetCountEntriesAccepted() const
    {
        return countEntriesAccepted;
    }

    int GetMyTransactionCount() const
    {
        return myEntries.size();
    }

    std::string GetMasterNodeAddr()
    {
        return masterNodeAddr;
    }

    void UpdateState(unsigned int newState)
    {
        if (fMasterNode && (newState == POOL_STATUS_ERROR || newState == POOL_STATUS_SUCCESS)){
            LogPrintf("CDarkSendPool::UpdateState() - Can't set state to ERROR or SUCCESS as a masternode. \n");
            return;
        }

        LogPrintf("CDarkSendPool::UpdateState() == %d | %d \n", state, newState);
        if(state != newState){
            lastTimeChanged = GetTimeMillis();
            if(fMasterNode) {
                RelayDarkSendStatus(darkSendPool.sessionID, darkSendPool.GetState(), darkSendPool.GetEntriesCount(), MASTERNODE_RESET);
            }
        }
        state = newState;
    }

    int GetMaxPoolTransactions()
    {
        //if we're on testnet, just use two transactions per merge
        if(fTestNet) return 2;

        //use the production amount
        return POOL_MAX_TRANSACTIONS;
    }

    //Do we have enough users to take entries? 
    bool IsSessionReady(){
        return sessionUsers >= GetMaxPoolTransactions();
    }


    // Are these outputs compatible with other client in the pool?
    bool IsCompatibleWithEntries(std::vector<CTxOut> vout);
    // Is this amount compatible with other client in the pool?
    bool IsCompatibleWithSession(int64 nAmount, CTransaction txCollateral, std::string& strReason);

    // Passively run Darksend in the background according to the configuration in settings (only for QT)
    bool DoAutomaticDenominating(bool fDryRun=false, bool ready=false);

    // Get the current winner for this block
    int GetCurrentMasterNode(int mod=1, int64 nBlockHeight=0);

    int GetMasternodeByVin(CTxIn& vin);
    int GetMasternodeRank(CTxIn& vin, int mod);
    int GetMasternodeByRank(int findRank);

    // check for process in Darksend
    void Check();
    // charge fees to bad actors
    void ChargeFees();
    void CheckTimeout();
    // check to make sure a signature matches an input in the pool
    bool SignatureValid(const CScript& newSig, const CTxIn& newVin);
    // if the collateral is valid given by a client
    bool IsCollateralValid(const CTransaction& txCollateral);
    // add a clients entry to the pool
    bool AddEntry(const std::vector<CTxIn>& newInput, const int64& nAmount, const CTransaction& txCollateral, const std::vector<CTxOut>& newOutput, std::string& error);
    // add signature to a vin
    bool AddScriptSig(const CTxIn& newVin);
    // are all inputs signed?
    bool SignaturesComplete();
    // as a client, send a transaction to a masternode to start the denomination process
    void SendDarksendDenominate(std::vector<CTxIn>& vin, std::vector<CTxOut>& vout, int64& fee, int64 amount);
    // get masternode updates about the progress of darksend
    bool StatusUpdate(int newState, int newEntriesCount, int newAccepted, std::string& error, int newSessionID=0);

    // as a client, check and sign the final transaction
    bool SignFinalTransaction(CTransaction& finalTransactionNew, CNode* node);

    // close old masternode connections
    void ProcessMasternodeConnections();
    bool ConnectToBestMasterNode(int depth=0);

    // Get compatible 1000DRK vin to start a masternode
    bool GetMasterNodeVin(CTxIn& vin, CPubKey& pubkey, CKey& secretKey);
    // enable hot wallet mode (run a masternode with no funds)
    bool EnableHotColdMasterNode(CTxIn& vin, int64 sigTime, CService& addr);
    // start the masternode and register with the network
    void RegisterAsMasterNode(bool stop);
    // get block hash by height
    bool GetBlockHash(uint256& hash, int nBlockHeight);
    // get the last valid block hash for a given modulus
    bool GetLastValidBlockHash(uint256& hash, int mod=1, int nBlockHeight=0);
    // process a new block
    void NewBlock();
    void CompletedTransaction(bool error, std::string lastMessageNew);
    void ClearLastMessage();
    // get the darksend chain depth for a given input
    int GetInputDarksendRounds(CTxIn in, int rounds=0);
    // split up large inputs or make fee sized inputs
    bool SplitUpMoney(bool justCollateral=false);
    // get the denominations for a list of outputs (returns a bitshifted integer)
    int GetDenominations(const std::vector<CTxOut>& vout);
    // get the denominations for a specific amount of darkcoin. 
    int GetDenominationsByAmount(int64 nAmount);
};

void ConnectToDarkSendMasterNodeWinner();

void ThreadCheckDarkSendPool();

#endif