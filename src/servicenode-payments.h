

// Copyright (c) 2014-2015 The Dash developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef SERVICENODE_PAYMENTS_H
#define SERVICENODE_PAYMENTS_H

#include "key.h"
#include "main.h"
#include "servicenode.h"
#include <boost/lexical_cast.hpp>

using namespace std;

class CServicenodePayments;
class CServicenodePaymentWinner;
class CServicenodeBlockPayees;

extern CServicenodePayments servicenodePayments;

#define MNPAYMENTS_SIGNATURES_REQUIRED 6
#define MNPAYMENTS_SIGNATURES_TOTAL 10

void ProcessMessageServicenodePayments(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);
bool IsBlockPayeeValid(const CBlock& block, int nBlockHeight);
std::string GetRequiredPaymentsString(int nBlockHeight);
bool IsBlockValueValid(const CBlock& block, CAmount nExpectedValue, CAmount nMinted);
void FillBlockPayee(CMutableTransaction& txNew, int64_t nFees, bool fProofOfStake);

void DumpServicenodePayments();

/** Save Servicenode Payment Data (mnpayments.dat)
 */
class CServicenodePaymentDB
{
private:
    boost::filesystem::path pathDB;
    std::string strMagicMessage;

public:
    enum ReadResult {
        Ok,
        FileError,
        HashReadError,
        IncorrectHash,
        IncorrectMagicMessage,
        IncorrectMagicNumber,
        IncorrectFormat
    };

    CServicenodePaymentDB();
    bool Write(const CServicenodePayments& objToSave);
    ReadResult Read(CServicenodePayments& objToLoad, bool fDryRun = false);
};

class CServicenodePayee
{
public:
    CScript scriptPubKey;
    int nVotes;

    CServicenodePayee()
    {
        scriptPubKey = CScript();
        nVotes = 0;
    }

    CServicenodePayee(CScript payee, int nVotesIn)
    {
        scriptPubKey = payee;
        nVotes = nVotesIn;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(scriptPubKey);
        READWRITE(nVotes);
    }
};

// Keep track of votes for payees from servicenodes
class CServicenodeBlockPayees
{
private:
    CCriticalSection cs_vecPayments;
    void swap(CServicenodeBlockPayees& first, CServicenodeBlockPayees& second) {
        std::swap(first.nBlockHeight, second.nBlockHeight);
        first.vecPayments.swap(second.vecPayments);
    }

public:
    int nBlockHeight;
    std::vector<CServicenodePayee> vecPayments;

    CServicenodeBlockPayees()
    {
        nBlockHeight = 0;
        vecPayments.clear();
    }
    CServicenodeBlockPayees(int nBlockHeightIn)
    {
        nBlockHeight = nBlockHeightIn;
        vecPayments.clear();
    }
    CServicenodeBlockPayees(const CServicenodeBlockPayees & c)
    {
        LOCK(cs_vecPayments);
        nBlockHeight = c.nBlockHeight;
        vecPayments = c.vecPayments;
    }

    CServicenodeBlockPayees& operator=(CServicenodeBlockPayees from)
    {
        swap(*this, from);
        return *this;
    }

    void AddPayee(CScript payeeIn, int nIncrement)
    {
        LOCK(cs_vecPayments);

        BOOST_FOREACH (CServicenodePayee& payee, vecPayments) {
            if (payee.scriptPubKey == payeeIn) {
                payee.nVotes += nIncrement;
                return;
            }
        }

        CServicenodePayee c(payeeIn, nIncrement);
        vecPayments.push_back(c);
    }

    bool GetPayee(CScript& payee)
    {
        LOCK(cs_vecPayments);

        int nVotes = -1;
        BOOST_FOREACH (CServicenodePayee& p, vecPayments) {
            if (p.nVotes > nVotes) {
                payee = p.scriptPubKey;
                nVotes = p.nVotes;
            }
        }

        return (nVotes > -1);
    }

    bool HasPayeeWithVotes(CScript payee, int nVotesReq)
    {
        LOCK(cs_vecPayments);

        BOOST_FOREACH (CServicenodePayee& p, vecPayments) {
            if (p.nVotes >= nVotesReq && p.scriptPubKey == payee) return true;
        }

        return false;
    }

    bool IsTransactionValid(const CTransaction& txNew);
    std::string GetRequiredPaymentsString();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(nBlockHeight);
        READWRITE(vecPayments);
    }
};

// for storing the winning payments
class CServicenodePaymentWinner
{
public:
    CTxIn vinServicenode;

    int nBlockHeight;
    CScript payee;
    std::vector<unsigned char> vchSig;

    CServicenodePaymentWinner()
    {
        nBlockHeight = 0;
        vinServicenode = CTxIn();
        payee = CScript();
    }

    CServicenodePaymentWinner(CTxIn vinIn)
    {
        nBlockHeight = 0;
        vinServicenode = vinIn;
        payee = CScript();
    }

    uint256 GetHash()
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << payee;
        ss << nBlockHeight;
        ss << vinServicenode.prevout;

        return ss.GetHash();
    }

    bool Sign(CKey& keyServicenode, CPubKey& pubKeyServicenode);
    bool IsValid(CNode* pnode, std::string& strError);
    bool SignatureValid();
    void Relay();

    void AddPayee(CScript payeeIn)
    {
        payee = payeeIn;
    }


    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(vinServicenode);
        READWRITE(nBlockHeight);
        READWRITE(payee);
        READWRITE(vchSig);
    }

    std::string ToString()
    {
        std::string ret = "";
        ret += vinServicenode.ToString();
        ret += ", " + boost::lexical_cast<std::string>(nBlockHeight);
        ret += ", " + payee.ToString();
        ret += ", " + boost::lexical_cast<std::string>((int)vchSig.size());
        return ret;
    }
};

//
// Servicenode Payments Class
// Keeps track of who should get paid for which blocks
//

class CServicenodePayments
{
private:
    mutable CCriticalSection cs;
    int nSyncedFromPeer;
    int nLastBlockHeight;
    std::map<uint256, CServicenodePaymentWinner> mapServicenodePayeeVotes;
    std::map<int, CServicenodeBlockPayees> mapServicenodeBlocks;
    std::map<uint256, int> mapServicenodesLastVote; //prevout.hash + prevout.n, nBlockHeight

public:

    CServicenodePayments()
    {
        nSyncedFromPeer = 0;
        nLastBlockHeight = 0;
    }

    void Clear()
    {
        LOCK(cs);
        mapServicenodeBlocks.clear();
        mapServicenodePayeeVotes.clear();
    }

    bool AddWinningServicenode(CServicenodePaymentWinner& winner);
    bool ProcessBlock(int nBlockHeight);

    void Sync(CNode* node, int nCountNeeded);
    void CleanPaymentList();
    int LastPayment(CServicenodePtr mn);

    bool GetPayeeScript(int nBlockHeight, CScript& payee);
    bool IsTransactionValid(const CTransaction& txNew, int nBlockHeight);
    bool IsScheduled(CServicenodePtr mn, int nNotBlockHeight);

    bool CanVote(COutPoint outServicenode, int nBlockHeight)
    {
        LOCK(cs);

        if (mapServicenodesLastVote.count(outServicenode.hash + outServicenode.n)) {
            if (mapServicenodesLastVote[outServicenode.hash + outServicenode.n] == nBlockHeight) {
                return false;
            }
        }

        //record this servicenode voted
        mapServicenodesLastVote[outServicenode.hash + outServicenode.n] = nBlockHeight;
        return true;
    }

    int GetMinServicenodePaymentsProto();
    void ProcessMessageServicenodePayments(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);
    std::string GetRequiredPaymentsString(int nBlockHeight);
    void FillBlockPayee(CMutableTransaction& txNew, int64_t nFees, bool fProofOfStake);
    std::string ToString() const;
    int GetOldestBlock();
    int GetNewestBlock();

    void EligibleServicenodes(const bool fFilterSigTime, const int & nBlockHeight,
            std::vector<CServicenodePtr> & snodes, std::map<CScript, bool> & eligibleSnodes);
    bool ValidNode(CServicenodePtr mn, const bool & fFilterSigTime, const int & nMnCount);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        LOCK(cs);
        READWRITE(mapServicenodePayeeVotes);
        READWRITE(mapServicenodeBlocks);
    }

    CServicenodePaymentWinner& GetVote(const uint256 & hash) {
        LOCK(cs);
        return mapServicenodePayeeVotes[hash];
    }
    void AddVote(const uint256 & hash, CServicenodePaymentWinner & winner) {
        LOCK(cs);
        mapServicenodePayeeVotes[hash] = winner;
    }
    bool HasVote(const uint256 & hash) {
        LOCK(cs);
        return mapServicenodePayeeVotes.count(hash);
    }

    CServicenodeBlockPayees GetBlockPayee(const int & block) {
        LOCK(cs);
        return mapServicenodeBlocks[block];
    }
    bool HasBlockPayee(const int & block) {
        LOCK(cs);
        return mapServicenodeBlocks.count(block);
    }
};


#endif
