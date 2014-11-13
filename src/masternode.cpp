

#include "masternode.h"
#include "activemasternode.h"
#include "darksend.h"
#include "core.h"


/** The list of active masternodes */
std::vector<CMasterNode> darkSendMasterNodes;
/** Object for who's going to get paid on which blocks */
CMasternodePayments masternodePayments;
/** Which masternodes we're asked other clients for */
std::vector<CTxIn> vecMasternodeAskedFor;
// keep track of masternode votes I've seen
map<uint256, int> mapSeenMasternodeVotes;

// manage the masternode connections
void ProcessMasternodeConnections(){
    LOCK(cs_vNodes);
    
    BOOST_FOREACH(CNode* pnode, vNodes)
    {
        //if it's our masternode, let it be
        if(darkSendPool.submittedToMasternode == pnode->addr) continue;

        if(pnode->fDarkSendMaster){
            LogPrintf("Closing masternode connection %s \n", pnode->addr.ToString().c_str());
            pnode->CloseSocketDisconnect();
        }
    }
}

void ProcessMessageMasternode(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    if (strCommand == "dsee") { //DarkSend Election Entry   
        if (pfrom->nVersion != darkSendPool.MIN_PEER_PROTO_VERSION) {
            return;
        }

        bool fIsInitialDownload = IsInitialBlockDownload();
        if(fIsInitialDownload) return;

        CTxIn vin;
        CService addr;
        CPubKey pubkey;
        CPubKey pubkey2;
        vector<unsigned char> vchSig;
        int64 sigTime;
        int count;
        int current;
        int64 lastUpdated;
        vRecv >> vin >> addr >> vchSig >> sigTime >> pubkey >> pubkey2 >> count >> current >> lastUpdated;

        bool isLocal = false; // addr.IsRFC1918();
        std::string vchPubKey(pubkey.begin(), pubkey.end());
        std::string vchPubKey2(pubkey2.begin(), pubkey2.end());
        
        CScript pubkeyScript;
        pubkeyScript.SetDestination(pubkey.GetID());

        if(pubkeyScript.size() != 25) {
            LogPrintf("dsee - pubkey the wrong size\n");
            pfrom->Misbehaving(100);
            return;  
        }

        std::string strMessage = addr.ToString() + boost::lexical_cast<std::string>(sigTime) + vchPubKey + vchPubKey2; 

        CScript pubkeyScript2;
        pubkeyScript2.SetDestination(pubkey2.GetID());
        
        if(pubkeyScript2.size() != 25) {
            LogPrintf("dsee - pubkey the wrong size\n");
            pfrom->Misbehaving(100);
            return;  
        }

        std::string errorMessage = "";
        if(!darkSendSigner.VerifyMessage(pubkey, vchSig, strMessage, errorMessage)){
            LogPrintf("dsee - Got bad masternode address signature\n");
            pfrom->Misbehaving(100);
            return;
        }

        if((fTestNet && addr.GetPort() != 19999) || (!fTestNet && addr.GetPort() != 9999)) return;

        //LogPrintf("Searching existing masternodes : %s - %s\n", addr.ToString().c_str(),  vin.ToString().c_str());
        
        BOOST_FOREACH(CMasterNode& mn, darkSendMasterNodes) {
            //LogPrintf(" -- %s\n", mn.vin.ToString().c_str());

            if(mn.vin.prevout == vin.prevout) {
                if(!mn.UpdatedWithin(MASTERNODE_MIN_SECONDS)){

                    if(mn.now < sigTime){ //take the newest entry
                        LogPrintf("dsee - Got updated entry for %s\n", addr.ToString().c_str());
                        mn.pubkey2 = pubkey2;
                        mn.now = sigTime;
                        mn.sig = vchSig;
                        mn.UpdateLastSeen();
                    }

                    if(pubkey2 == activeMasternode.pubkeyMasterNode2){
                        activeMasternode.EnableHotColdMasterNode(vin, sigTime, addr);
                    }

                    if(count == -1) //count == -1 when it's a new entry
                        RelayDarkSendElectionEntry(vin, addr, vchSig, sigTime, pubkey, pubkey2, count, current, lastUpdated);
                }

                return;
            }
        }

        if(!darkSendSigner.IsVinAssociatedWithPubkey(vin, pubkey)) {
            LogPrintf("dsee - Got mismatched pubkey and vin\n");
            pfrom->Misbehaving(100);
            return;
        }

        LogPrintf("dsee - Got NEW masternode entry %s\n", addr.ToString().c_str());

        CValidationState state;
        CTransaction tx = CTransaction();
        CTxOut vout = CTxOut(999.99*COIN, darkSendPool.collateralPubKey);
        tx.vin.push_back(vin);
        tx.vout.push_back(vout);
        if(tx.AcceptableInputs(state, true)){            
            LogPrintf("dsee - Accepted masternode entry %i %i\n", count, current);

            if(GetInputAge(vin) < MASTERNODE_MIN_CONFIRMATIONS){
                LogPrintf("dsee - Input must have least %d confirmations\n", MASTERNODE_MIN_CONFIRMATIONS);
                pfrom->Misbehaving(20);
                return;
            }

            addrman.Add(CAddress(addr), pfrom->addr, 2*60*60);

            CMasterNode mn(addr, vin, pubkey, vchSig, sigTime, pubkey2);
            mn.UpdateLastSeen(lastUpdated);
            darkSendMasterNodes.push_back(mn);

            if(pubkey2 == activeMasternode.pubkeyMasterNode2){
                activeMasternode.EnableHotColdMasterNode(vin, sigTime, addr);
            }

            if(count == -1 && !isLocal)
                RelayDarkSendElectionEntry(vin, addr, vchSig, sigTime, pubkey, pubkey2, count, current, lastUpdated); 

        } else {
            LogPrintf("dsee - Rejected masternode entry\n");

            int nDoS = 0;
            if (state.IsInvalid(nDoS))
            {
                LogPrintf("%s from %s %s was not accepted into the memory pool\n", tx.GetHash().ToString().c_str(),
                    pfrom->addr.ToString().c_str(), pfrom->cleanSubVer.c_str());
                if (nDoS > 0)
                    pfrom->Misbehaving(nDoS);
            }
        }
    }

    else if (strCommand == "dseep") { //DarkSend Election Entry Ping 
        if (pfrom->nVersion != darkSendPool.MIN_PEER_PROTO_VERSION) {
            return;
        }
        bool fIsInitialDownload = IsInitialBlockDownload();
        if(fIsInitialDownload) return;

        CTxIn vin;
        vector<unsigned char> vchSig;
        int64 sigTime;
        bool stop;
        vRecv >> vin >> vchSig >> sigTime >> stop;

        if (sigTime > GetAdjustedTime() + 60 * 60) {
            LogPrintf("dseep: Signature rejected, too far into the future %s\n", vin.ToString().c_str());
            return;
        }

        /*if (sigTime <= GetAdjustedTime() - 60 * 60) {
            LogPrintf("dseep: Signature rejected, too far into the past %s - %"PRI64u" %"PRI64u" \n", vin.ToString().c_str(), sigTime, GetAdjustedTime());
            return;
        }*/

        //LogPrintf("Searching existing masternodes : %s - %s\n", addr.ToString().c_str(),  vin.ToString().c_str());

        BOOST_FOREACH(CMasterNode& mn, darkSendMasterNodes) {
            if(mn.vin == vin) {
                if(mn.lastDseep < sigTime){ //take this only if it's newer
                    mn.lastDseep = sigTime;

                    std::string strMessage = mn.addr.ToString() + boost::lexical_cast<std::string>(sigTime) + boost::lexical_cast<std::string>(stop); 

                    std::string errorMessage = "";
                    if(!darkSendSigner.VerifyMessage(mn.pubkey2, vchSig, strMessage, errorMessage)){
                        LogPrintf("dseep: Got bad masternode address signature %s \n", vin.ToString().c_str());
                        //pfrom->Misbehaving(20);
                        return;
                    }

                    if(stop) {
                        if(mn.IsEnabled()){
                            mn.Disable();
                            mn.Check();
                            RelayDarkSendElectionEntryPing(vin, vchSig, sigTime, stop);
                        }
                    } else if(!mn.UpdatedWithin(MASTERNODE_MIN_SECONDS)){
                        mn.UpdateLastSeen();
                        RelayDarkSendElectionEntryPing(vin, vchSig, sigTime, stop);
                    }
                }
                return;
            }
        }

        // ask for the dsee info once from the node that sent dseep

        LogPrintf("dseep: Couldn't find masternode entry %s\n", vin.ToString().c_str());

        BOOST_FOREACH(CTxIn vinAsked, vecMasternodeAskedFor)
            if (vinAsked == vin) return;

        LogPrintf("dseep: Asking source node for missing entry %s\n", vin.ToString().c_str());

        vecMasternodeAskedFor.push_back(vin);
        pfrom->PushMessage("dseg", vin);

    } else if (strCommand == "dseg") { //Get masternode list or specific entry
        if (pfrom->nVersion != darkSendPool.MIN_PEER_PROTO_VERSION) {
            return;
        }

        CTxIn vin;
        vRecv >> vin;

        if(vin == CTxIn()) { //only should ask for this once
            if(pfrom->HasFulfilledRequest("dseg")) {
                LogPrintf("dseg -- peer already asked me for the list\n");
                pfrom->Misbehaving(20);
                return;
            }

            pfrom->FulfilledRequest("dseg");
        } //else, asking for a specific node which is ok

        int count = darkSendMasterNodes.size()-1;
        int i = 0;

        BOOST_FOREACH(CMasterNode mn, darkSendMasterNodes) {    
            LogPrintf("Sending master node entry - %s \n", mn.addr.ToString().c_str());

            if(mn.addr.IsRFC1918()) continue; //local network

            if(vin == CTxIn()){
                mn.Check();
                if(mn.IsEnabled()) {
                    pfrom->PushMessage("dsee", mn.vin, mn.addr, mn.sig, mn.now, mn.pubkey, mn.pubkey2, count, i, mn.lastTimeSeen);
                }
            } else if (vin == mn.vin) {
                pfrom->PushMessage("dsee", mn.vin, mn.addr, mn.sig, mn.now, mn.pubkey, mn.pubkey2, count, i, mn.lastTimeSeen);
            }
            i++;
        }
    }

    else if (strCommand == "mnget") { //Masternode Payments Request Sync
        
        if(pfrom->HasFulfilledRequest("mnget")) {
            LogPrintf("mnget -- peer already asked me for the list\n");
            pfrom->Misbehaving(20);
            return;
        }

        pfrom->FulfilledRequest("mnget");
        masternodePayments.Sync(pfrom);
    }
    else if (strCommand == "mnw") { //Masternode Payments Declare Winner
        CMasternodePaymentWinner winner;
        vRecv >> winner;

        if(pindexBest == NULL) return;
 
        uint256 hash = winner.GetHash();
        if(mapSeenMasternodeVotes.count(hash)) {
            if(fDebug) LogPrintf("mnw - seen vote %s Height %d bestHeight %d\n", hash.ToString().c_str(), winner.nBlockHeight, pindexBest->nHeight);
            return;
        }

        if(winner.nBlockHeight < pindexBest->nHeight - 10 || winner.nBlockHeight > pindexBest->nHeight+20){
            LogPrintf("mnw - winner out of range %s Height %d bestHeight %d\n", winner.vin.ToString().c_str(), winner.nBlockHeight, pindexBest->nHeight);
            return;
        }

        if(winner.vin.nSequence != std::numeric_limits<unsigned int>::max()){
            LogPrintf("mnw - invalid nSequence\n");
            pfrom->Misbehaving(100);
            return;
        }

        LogPrintf("mnw - winning vote  %s Height %d bestHeight %d\n", winner.vin.ToString().c_str(), winner.nBlockHeight, pindexBest->nHeight);

        if(!masternodePayments.CheckSignature(winner)){
            LogPrintf("mnw - invalid signature\n");
            pfrom->Misbehaving(100);
            return;
        }

        mapSeenMasternodeVotes.insert(make_pair(hash, 1));

        if(masternodePayments.AddWinningMasternode(winner)){
            masternodePayments.Relay(winner);
        }
    }
}

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


int GetMasternodeByVin(CTxIn& vin)
{
    int i = 0;

    BOOST_FOREACH(CMasterNode mn, darkSendMasterNodes) {
        if (mn.vin == vin) return i;
        i++;
    }

    return -1;
}

int GetCurrentMasterNode(int mod, int64 nBlockHeight)
{
    int i = 0;
    unsigned int score = 0;
    int winner = -1;

    // scan for winner
    BOOST_FOREACH(CMasterNode mn, darkSendMasterNodes) {
        mn.Check();
        if(!mn.IsEnabled()) {
            i++;
            continue;
        }

        // calculate the score for each masternode
        uint256 n = mn.CalculateScore(mod, nBlockHeight);
        unsigned int n2 = 0;
        memcpy(&n2, &n, sizeof(n2));
        
        // determine the winner
        if(n2 > score){
            score = n2;
            winner = i; 
        }
        i++;
    }

    return winner;
}

int GetMasternodeByRank(int findRank, int64 nBlockHeight)
{
    int i = 0;
 
    std::vector<pair<unsigned int, int> > vecMasternodeScores;

    i = 0;
    BOOST_FOREACH(CMasterNode mn, darkSendMasterNodes) {
        mn.Check();
        if(!mn.IsEnabled()) {
            i++;
            continue;
        }

        uint256 n = mn.CalculateScore(1, nBlockHeight);
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

int GetMasternodeRank(CTxIn& vin, int64 nBlockHeight)
{
    std::vector<pair<unsigned int, CTxIn> > vecMasternodeScores;

    BOOST_FOREACH(CMasterNode mn, darkSendMasterNodes) {
        mn.Check();
        if(!mn.IsEnabled()) {
            continue;
        }

        uint256 n = mn.CalculateScore(1, nBlockHeight);
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

// 
// Deterministically calculate a given "score" for a masternode depending on how close it's hash is to 
// the proof of work for that block. The further away they are the better, the furthest will win the election 
// and get paid this block
// 
uint256 CMasterNode::CalculateScore(int mod, int64 nBlockHeight)
{
    if(pindexBest == NULL) return 0;

    uint256 n1 = 0;
    if(!darkSendPool.GetLastValidBlockHash(n1, mod, nBlockHeight)) return 0;

    uint256 n2 = Hash9(BEGIN(n1), END(n1));
    uint256 n3 = vin.prevout.hash > n2 ? (vin.prevout.hash - n2) : (n2 - vin.prevout.hash);

    /*
    LogPrintf(" -- MasterNode CalculateScore() n2 = %s \n", n2.ToString().c_str());
    LogPrintf(" -- MasterNode CalculateScore() vin = %s \n", vin.prevout.hash.GetHex().c_str());
    LogPrintf(" -- MasterNode CalculateScore() n3 = %s \n", n3.ToString().c_str());*/
    
    return n3;
}

void CMasterNode::Check()
{
    //once spent, stop doing the checks
    if(enabled==3) return;


    if(!UpdatedWithin(MASTERNODE_REMOVAL_SECONDS)){
        enabled = 4;
        return;
    }

    if(!UpdatedWithin(MASTERNODE_EXPIRATION_SECONDS)){
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

bool CMasternodePayments::CheckSignature(CMasternodePaymentWinner& winner)
{
    std::string strMessage = winner.vin.ToString().c_str() + boost::lexical_cast<std::string>(winner.nBlockHeight); 
    std::string strPubKey = fTestNet? strTestPubKey : strMainPubKey;
    CPubKey pubkey(ParseHex(strPubKey));

    std::string errorMessage = "";
    if(!darkSendSigner.VerifyMessage(pubkey, winner.vchSig, strMessage, errorMessage)){
        return false;
    }

    return true;
}

bool CMasternodePayments::Sign(CMasternodePaymentWinner& winner) 
{
    std::string strMessage = winner.vin.ToString().c_str() + boost::lexical_cast<std::string>(winner.nBlockHeight); 

    CKey key2;
    CPubKey pubkey2;
    std::string errorMessage = "";

    if(!darkSendSigner.SetKey(strMasterPrivKey, errorMessage, key2, pubkey2))
    {
        LogPrintf("Invalid masternodeprivkey: '%s'\n", errorMessage.c_str());
        exit(0);
    }

    if(!darkSendSigner.SignMessage(strMessage, errorMessage, winner.vchSig, key2)) {
        LogPrintf("CDarksendQueue():Relay - Sign message failed");
        return false;
    }

    if(!darkSendSigner.VerifyMessage(pubkey2, winner.vchSig, strMessage, errorMessage)) {
        LogPrintf("CDarksendQueue():Relay - Verify message failed");
        return false;
    }

    return true;
}

uint64 CMasternodePayments::CalculateScore(uint256 blockHash, CTxIn& vin)
{
    uint256 n1 = blockHash;
    uint256 n2 = Hash9(BEGIN(n1), END(n1));
    uint256 n3 = Hash9(BEGIN(vin.prevout.hash), END(vin.prevout.hash));
    uint256 n4 = n3 > n2 ? (n3 - n2) : (n2 - n3);

    //printf(" -- CMasternodePayments CalculateScore() n2 = %"PRI64u" \n", n2.Get64());
    //printf(" -- CMasternodePayments CalculateScore() n3 = %"PRI64u" \n", n3.Get64());
    //printf(" -- CMasternodePayments CalculateScore() n4 = %"PRI64u" \n", n4.Get64());

    return n4.Get64();
}

bool CMasternodePayments::GetBlockPayee(int nBlockHeight, CScript& payee)
{
    BOOST_FOREACH(CMasternodePaymentWinner& winner, vWinning){
        if(winner.nBlockHeight == nBlockHeight) {

            CTransaction tx;
            uint256 hash;
            if(GetTransaction(winner.vin.prevout.hash, tx, hash, true)){
                BOOST_FOREACH(CTxOut out, tx.vout){
                    if(out.nValue == 1000*COIN){
                        payee = out.scriptPubKey;
                        return true;
                    }
                }
            }

            return false;
        }
    }

    return false;
}

bool CMasternodePayments::GetWinningMasternode(int nBlockHeight, CTxIn& vinOut)
{
    BOOST_FOREACH(CMasternodePaymentWinner& winner, vWinning){
        if(winner.nBlockHeight == nBlockHeight) {
            vinOut = winner.vin;
            return true;
        }
    }

    return false;
}

bool CMasternodePayments::AddWinningMasternode(CMasternodePaymentWinner& winnerIn)
{
    uint256 blockHash = 0;
    if(!darkSendPool.GetBlockHash(blockHash, winnerIn.nBlockHeight-576)) {
        return false;
    }

    winnerIn.score = CalculateScore(blockHash, winnerIn.vin);

    bool foundBlock = false;
    BOOST_FOREACH(CMasternodePaymentWinner& winner, vWinning){
        if(winner.nBlockHeight == winnerIn.nBlockHeight) {
            foundBlock = true;
            if(winner.score < winnerIn.score){
                winner.score = winnerIn.score;
                winner.vin = winnerIn.vin;
                winner.vchSig = winnerIn.vchSig;
                return true;
            }
        }
    }

    // if it's not in the vector
    if(!foundBlock){
         vWinning.push_back(winnerIn);
         return true;
    }

    return false;
}

void CMasternodePayments::CleanPaymentList()
{
    if(pindexBest == NULL) return;

    vector<CMasternodePaymentWinner>::iterator it;
    for(it=vWinning.begin();it<vWinning.end();it++){
        if(pindexBest->nHeight - (*it).nBlockHeight > 1000){
            if(fDebug) LogPrintf("Removing old masternode payment - block %d\n", (*it).nBlockHeight);
            vWinning.erase(it);
            break;
        }
    }
}

int CMasternodePayments::LastPayment(CMasterNode& mn)
{
    if(pindexBest == NULL) return 0;

    int ret = mn.GetMasternodeInputAge();

    BOOST_FOREACH(CMasternodePaymentWinner& winner, vWinning){
        if(winner.vin == mn.vin && pindexBest->nHeight - winner.nBlockHeight < ret)
            ret = pindexBest->nHeight - winner.nBlockHeight;
    }

    return ret;
}

bool CMasternodePayments::ProcessBlock(int nBlockHeight)
{
    if(strMasterPrivKey.empty()) return false;
    CMasternodePaymentWinner winner;

    uint256 blockHash = 0;
    if(!darkSendPool.GetBlockHash(blockHash, nBlockHeight-576)) return false;

    BOOST_FOREACH(CMasterNode& mn, darkSendMasterNodes) {
        mn.Check();

        if(!mn.IsEnabled()) {
            continue;
        }

        if(LastPayment(mn) < darkSendMasterNodes.size()*.9) continue;

        uint64 score = CalculateScore(blockHash, mn.vin);
        if(score > winner.score){
            winner.score = score;
            winner.nBlockHeight = nBlockHeight;
            winner.vin = mn.vin;
        }
    }

    if(winner.nBlockHeight == 0) return false; //no masternodes available

    if(Sign(winner)){
        if(AddWinningMasternode(winner)){
            Relay(winner);
            return true;
        }
    }

    return false;
}

void CMasternodePayments::Relay(CMasternodePaymentWinner& winner)
{    
    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes){
        if(!pnode->fRelayTxes)
            continue;

        pnode->PushMessage("mnw", winner);
    }
}

void CMasternodePayments::Sync(CNode* node)
{
    BOOST_FOREACH(CMasternodePaymentWinner& winner, vWinning)
        if(winner.nBlockHeight >= pindexBest->nHeight-10 && winner.nBlockHeight <= pindexBest->nHeight + 20)
            node->PushMessage("mnw", winner);
}


bool CMasternodePayments::SetPrivKey(std::string strPrivKey)
{
    CMasternodePaymentWinner winner;

    // Test signing successful, proceed
    strMasterPrivKey = strPrivKey;

    Sign(winner);

    if(CheckSignature(winner)){
        LogPrintf("Successfully initialized as masternode payments master\n");
        return true;
    } else {
        return false;
    }
}
