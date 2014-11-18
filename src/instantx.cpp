


#include "bignum.h"
#include "sync.h"
#include "net.h"
#include "key.h"
#include "util.h"
#include "script.h"
#include "hashblock.h"
#include "base58.h"
#include "instantx.h"
#include "masternode.h"
#include "activemasternode.h"
#include "darksend.h"

using namespace std;
using namespace boost;

std::vector<CTransactionLock> vecTxLocks;

std::map<uint256, CTransaction> mapTxLockReq;
std::map<uint256, CTransactionLock> mapTxLocks;

#define INSTANTX_SIGNATURES_REQUIRED           2

//txlock - Locks transaction
//
//step 1.) Broadcast intention to lock transaction inputs, "txlreg", CTransaction
//step 2.) Top 10 masternodes, open connect to top 1 masternode. Send "txvote", CTransaction, Signature, Approve
//step 3.) Top 1 masternode, waits for 10 messages. Upon success, sends "txlock'

void ProcessMessageInstantX(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    if (strCommand == "txlreq")
    {
        printf("ProcessMessageInstantX::txlreq\n");
        CDataStream vMsg(vRecv);
        CTransaction tx;
        int nBlockHeight;
        vRecv >> tx >> nBlockHeight;

        CInv inv(MSG_TXLOCK_REQUEST, tx.GetHash());
        pfrom->AddInventoryKnown(inv);

        if(mapTxLockReq.count(inv.hash)){
            printf("ProcessMessageInstantX::txlreq - Already Have Transaction Lock Request: %s %s : accepted %s\n",
                pfrom->addr.ToString().c_str(), pfrom->cleanSubVer.c_str(),
                tx.GetHash().ToString().c_str()
            );
            return;
        }

        bool fMissingInputs = false;
        CValidationState state;
        if (tx.AcceptToMemoryPool(state, true, true, &fMissingInputs))
        {
            RelayTransactionLockReq(tx, inv.hash);
            DoConsensusVote(tx, true, nBlockHeight); 

            mapTxLockReq.insert(make_pair(inv.hash, tx));

            printf("ProcessMessageInstantX::txlreq - Transaction Lock Request: %s %s : accepted %s\n",
                pfrom->addr.ToString().c_str(), pfrom->cleanSubVer.c_str(),
                tx.GetHash().ToString().c_str()
            );

            return;

        } else {


            // can we get the conflicting transaction as proof?

            RelayTransactionLockReq(tx, inv.hash);
            DoConsensusVote(tx, false, nBlockHeight); 

            printf("ProcessMessageInstantX::txlreq - Transaction Lock Request: %s %s : rejected %s\n",
                pfrom->addr.ToString().c_str(), pfrom->cleanSubVer.c_str(),
                tx.GetHash().ToString().c_str()
            );

            //record prevout, increment the amount of times seen. Ban if over 100

            return;
        }
    } 
    else if (strCommand == "txlvote") //InstantX Lock Consensus Votes
    {
        CConsensusVote ctx;
        vRecv >> ctx;

        ProcessConsensusVote(ctx);
        
        return;
    }
    else if (strCommand == "txlock") //InstantX Lock Transaction Inputs
    {
        printf("ProcessMessageInstantX::txlock\n");

        CDataStream vMsg(vRecv);
        CTransactionLock ctxl;
        vRecv >> ctxl;

        CInv inv(MSG_TXLOCK, ctxl.GetHash());
        pfrom->AddInventoryKnown(inv);

        printf(" -- ProcessMessageInstantX::txlock %d  %s\n", mapTxLocks.count(inv.hash), inv.hash.ToString().c_str());

        if(!mapTxLocks.count(inv.hash)){
            /*if(ctxl.CountSignatures() < INSTANTX_SIGNATURES_REQUIRED){
                printf("InstantX::txlock - not enough signatures\n");
                return;
            }
            if(!ctxl.SignaturesValid()){
                printf("InstantX::txlock - got invalid TransactionLock, rejected\n");
                return;
            }
            if(!ctxl.AllInFavor()){
                printf("InstantX::txlock - not all in favor of lock, rejected\n");
                return;
            }*/

            mapTxLocks.insert(make_pair(inv.hash, ctxl));

            //broadcast the new lock
/*            LOCK(cs_vNodes);
            BOOST_FOREACH(CNode* pnode, vNodes)
            {
                if(!pnode->fRelayTxes)
                    continue;

                pnode->PushMessage("txlock", ctxl);
            }
*/            
            //pwalletMain->UpdatedConfirmations();

            printf("InstantX :: Got Transaction Lock: %s %s : accepted %s\n",
                pfrom->addr.ToString().c_str(), pfrom->cleanSubVer.c_str(),
                ctxl.GetHash().ToString().c_str()
            );
        }
    }
}

// check if we need to vote on this transaction
void DoConsensusVote(CTransaction& tx, bool approved, int64 nBlockHeight)
{
    if(!fMasterNode) {
        printf("InstantX::DoConsensusVote - Not masternode\n");
        return;
    }

    int winner = GetCurrentMasterNode(1, nBlockHeight);
    int n = GetMasternodeRank(activeMasternode.vinMasternode, nBlockHeight);

    if(n == -1 || winner == -1) 
    {
        printf("InstantX::DoConsensusVote - Unknown Masternode\n");
        return;
    }

    if(n == 1)
    { // winner, I'll be keeping track of this
        printf("InstantX::DoConsensusVote - Managing Masternode\n");
        CTransactionLock newLock;
        newLock.nBlockHeight = nBlockHeight;
        newLock.tx = tx;
        vecTxLocks.push_back(newLock);
    }

    CConsensusVote ctx;
    ctx.vinMasternode = activeMasternode.vinMasternode;
    ctx.approved = approved;
    ctx.txHash = tx.GetHash();
    ctx.nBlockHeight = nBlockHeight; 
    if(!ctx.Sign()){
        printf("InstantX::DoConsensusVote - Failed to sign consensus vote\n");
        return;
    }
    if(!ctx.SignatureValid()) {
        printf("InstantX::ProcessConsensusVote - Signature invalid\n");
        return;
    }

    
    if(n == 1){ //I'm the winner
        ProcessConsensusVote(ctx);
    } else if(n <= 10){ // not winner, but in the top10
        if(ConnectNode((CAddress)darkSendMasterNodes[winner].addr, NULL, true)){
            LOCK(cs_vNodes);
            BOOST_FOREACH(CNode* pnode, vNodes)
            {
                if(darkSendMasterNodes[winner].addr != pnode->addr) continue;

                pnode->PushMessage("txlvote", ctx);
                printf("InstantX::DoConsensusVote --- connected, sending vote %s\n", pnode->addr.ToString().c_str());
                return;
            }
        } else {
            printf("InstantX::DoConsensusVote --- error connecting \n");
            return;
        }
    }
}

//received a consensus vote
void ProcessConsensusVote(CConsensusVote& ctx)
{
    if(!fMasterNode) {
        printf("InstantX::ProcessConsensusVote - Not masternode\n");
        return;
    }
    
    int winner = GetCurrentMasterNode(1, ctx.nBlockHeight);
    if(winner == -1) {
        printf("InstantX::ProcessConsensusVote - Can't detect winning masternode\n");
        return;
    }

    //We're not the winning masternode
    if(darkSendMasterNodes[winner].vin != activeMasternode.vinMasternode) {
        printf("InstantX::ProcessConsensusVote - I'm not the winning masternode\n");
        return;
    }

    int n = GetMasternodeRank(ctx.vinMasternode, ctx.nBlockHeight);

    if(n == -1) 
    {
        printf("InstantX::DoConsensusVote - Unknown Masternode\n");
        return;
    }

    if(n > 10) 
    {
        printf("InstantX::DoConsensusVote - Masternode not in the top 10\n");
        return;
    }

    if(!ctx.SignatureValid()) {
        printf("InstantX::ProcessConsensusVote - Signature invalid\n");
        //don't ban, it could just be a non-synced masternode
        return;
    }

    //compile consessus vote
    printf(" -- 1\n");
    BOOST_FOREACH(CTransactionLock& ctxl, vecTxLocks){
        printf(" -- 2\n");
        if(ctxl.nBlockHeight == ctx.nBlockHeight){
            ctxl.AddSignature(ctx);
            printf(" -- 3 - %d\n", ctxl.CountSignatures());
            if(ctxl.CountSignatures() >= INSTANTX_SIGNATURES_REQUIRED){
                printf("InstantX::ProcessConsensusVote - Transaction Lock Is Complelete, broadcasting!\n");

                CInv inv(MSG_TXLOCK, ctxl.GetHash());
                mapTxLocks.insert(make_pair(inv.hash, ctxl));

                printf(" -- 4 %d  %s\n", mapTxLocks.count(inv.hash), inv.hash.ToString().c_str());

                //broadcast the new lock
                LOCK(cs_vNodes);
                BOOST_FOREACH(CNode* pnode, vNodes){
                    pnode->PushMessage("txlock", ctxl);
                }
            }
            return;
        }
    }

    return;
}

void CleanTransactionLocksList()
{
    if(pindexBest == NULL) return;

    std::map<uint256, CTransactionLock>::iterator it = mapTxLocks.begin();
    
    while(it != mapTxLocks.end()) {
        if(pindexBest->nHeight - it->second.nBlockHeight > 24){ //keep them for an hour
            LogPrintf("Removing old transaction lock %s\n", it->second.GetHash().ToString().c_str());
            mapTxLocks.erase(it);
        }
        ++it;
    }

}

bool CConsensusVote::SignatureValid()
{
    std::string errorMessage;
    std::string strMessage = txHash.ToString().c_str() + boost::lexical_cast<std::string>(nBlockHeight) + boost::lexical_cast<std::string>(approved);
    printf("verify strMessage %s \n", strMessage.c_str());
         
    int n = GetMasternodeByVin(vinMasternode);

    if(n == -1) 
    {
        printf("InstantX::CConsensusVote::SignatureValid() - Unknown Masternode\n");
        return false;
    }

    printf("verify addr %s \n", darkSendMasterNodes[0].addr.ToString().c_str());
    printf("verify addr %s \n", darkSendMasterNodes[1].addr.ToString().c_str());
    printf("verify addr %d %s \n", n, darkSendMasterNodes[n].addr.ToString().c_str());

    CScript pubkey;
    pubkey.SetDestination(darkSendMasterNodes[n].pubkey2.GetID());
    CTxDestination address1;
    ExtractDestination(pubkey, address1);
    CBitcoinAddress address2(address1);
    printf("verify pubkey2 %s \n", address2.ToString().c_str());

    if(!darkSendSigner.VerifyMessage(darkSendMasterNodes[n].pubkey2, vchMasterNodeSignature, strMessage, errorMessage)) {
        printf("InstantX::CConsensusVote::SignatureValid() - Verify message failed\n");
        return false;
    }

    return true;
}

bool CConsensusVote::Sign()
{
    std::string errorMessage;

    CKey key2;
    CPubKey pubkey2;
    std::string strMessage = txHash.ToString().c_str() + boost::lexical_cast<std::string>(nBlockHeight) + boost::lexical_cast<std::string>(approved);
    printf("signing strMessage %s \n", strMessage.c_str());
    printf("signing privkey %s \n", strMasterNodePrivKey.c_str());

    if(!darkSendSigner.SetKey(strMasterNodePrivKey, errorMessage, key2, pubkey2))
    {
        printf("Invalid masternodeprivkey: '%s'\n", errorMessage.c_str());
        exit(0);
    }

    CScript pubkey;
    pubkey.SetDestination(pubkey2.GetID());
    CTxDestination address1;
    ExtractDestination(pubkey, address1);
    CBitcoinAddress address2(address1);
    printf("signing pubkey2 %s \n", address2.ToString().c_str());

    if(!darkSendSigner.SignMessage(strMessage, errorMessage, vchMasterNodeSignature, key2)) {
        printf("CActiveMasternode::RegisterAsMasterNode() - Sign message failed");
        return false;
    }

    if(!darkSendSigner.VerifyMessage(pubkey2, vchMasterNodeSignature, strMessage, errorMessage)) {
        printf("CActiveMasternode::RegisterAsMasterNode() - Verify message failed");
        return false;
    }

    return true;
}


bool CTransactionLock::SignaturesValid()
{

    BOOST_FOREACH(CConsensusVote vote, vecConsensusVotes)
    {
        int n = GetMasternodeRank(vote.vinMasternode, vote.nBlockHeight);

        if(n == -1) 
        {
            printf("InstantX::DoConsensusVote - Unknown Masternode\n");
            return false;
        }

        if(n > 10) 
        {
            printf("InstantX::DoConsensusVote - Masternode not in the top 10\n");
            return false;
        }

        if(!vote.SignatureValid()){
            printf("InstantX::CTransactionLock::SignaturesValid - Signature not valid\n");
            return false;
        }
    }

    return true;
}

bool CTransactionLock::AllInFavor()
{

    BOOST_FOREACH(CConsensusVote vote, vecConsensusVotes)
        if(vote.approved == false) return false;

    return true;
}

void CTransactionLock::AddSignature(CConsensusVote cv)
{
    vecConsensusVotes.push_back(cv);
}

int CTransactionLock::CountSignatures()
{
    return vecConsensusVotes.size();
}