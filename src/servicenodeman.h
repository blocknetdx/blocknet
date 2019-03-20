// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The PIVX developers
// Copyright (c) 2015-2018 The Blocknet developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SERVICENODEMAN_H
#define SERVICENODEMAN_H

#include "base58.h"
#include "key.h"
#include "main.h"
#include "servicenode.h"
#include "net.h"
#include "sync.h"
#include "util.h"

#define SERVICENODES_DUMP_SECONDS (15 * 60)
#define SERVICENODES_DSEG_SECONDS (3 * 60 * 60)

using namespace std;

class CServicenodeMan;

extern CServicenodeMan mnodeman;
void DumpServicenodes();

/** Access to the MN database (mncache.dat)
 */
class CServicenodeDB
{
private:
    boost::filesystem::path pathMN;
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

    CServicenodeDB();
    bool Write(const CServicenodeMan& mnodemanToSave);
    ReadResult Read(CServicenodeMan& mnodemanToLoad, bool fDryRun = false);
};

class CServicenodeMan
{
private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

    // critical section to protect the inner data structures specifically on messaging
    mutable CCriticalSection cs_process_message;

    // map to hold all MNs
    std::vector<CServicenodePtr> vServicenodes;
    // who's asked for the Servicenode list and the last time
    std::map<CNetAddr, int64_t> mAskedUsForServicenodeList;
    // who we asked for the Servicenode list and the last time
    std::map<CNetAddr, int64_t> mWeAskedForServicenodeList;
    // which Servicenodes we've asked for
    std::map<COutPoint, int64_t> mWeAskedForServicenodeListEntry;
    // Keep track of all broadcasts I've seen
    map<uint256, CServicenodeBroadcast> mapSeenServicenodeBroadcast;
    // Keep track of all pings I've seen
    map<uint256, CServicenodePing> mapSeenServicenodePing;
    // keep track of dsq count to prevent servicenodes from gaming obfuscation queue
    int64_t nDsqCount;

public:

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        LOCK(cs);
        std::vector<CServicenode> snodes;
        if (ser_action.ForRead()) {
            READWRITE(snodes);
            for (const auto & mn : snodes)
                vServicenodes.push_back(std::make_shared<CServicenode>(mn));
        } else { // Write
            for (const auto & mn : vServicenodes)
                snodes.push_back(*mn);
            READWRITE(snodes);
        }
        READWRITE(mAskedUsForServicenodeList);
        READWRITE(mWeAskedForServicenodeList);
        READWRITE(mWeAskedForServicenodeListEntry);
        READWRITE(nDsqCount);

        READWRITE(mapSeenServicenodeBroadcast);
        READWRITE(mapSeenServicenodePing);
    }

    CServicenodeMan();
    CServicenodeMan(CServicenodeMan& other);

    /// Add an entry
    bool Add(CServicenodePtr mn);

    /// Ask (source) node for mnb
    void AskForMN(CNode* pnode, CTxIn& vin);

    /// Check all Servicenodes
    void Check();

    /// Check all Servicenodes and remove inactive
    void CheckAndRemove(bool forceExpiredRemoval = false);

    /// Clear Servicenode vector
    void Clear();

    int CountEnabled(int protocolVersion = -1);

    void DsegUpdate(CNode* pnode);

    /// Find an entry
    CServicenodePtr Find(const CScript& payee);
    CServicenodePtr Find(const CTxIn& vin);
    CServicenodePtr Find(const CPubKey& pubKeyServicenode);

    /// Find an entry in the servicenode list that is next to be paid
    CServicenodePtr GetNextServicenodeInQueueForPayment(int nBlockHeight, bool fFilterSigTime, int& nCount);

    /// Find a random entry
    CServicenodePtr FindRandomNotInVec(std::vector<CTxIn>& vecToExclude, int protocolVersion = -1);

    /// Get the current winner for this block
    CServicenodePtr GetCurrentServiceNode(int mod = 1, int64_t nBlockHeight = 0, int minProtocol = 0);

    std::vector<CServicenodePtr> CheckAndCopyServicenodes()
    {
        Check();
        LOCK(cs);
        return vServicenodes;
    }
    std::vector<CServicenodePtr> CopyServicenodes()
    {
        LOCK(cs);
        return vServicenodes;
    }

    std::vector<pair<int, CServicenode> > GetServicenodeRanks(int64_t nBlockHeight, int minProtocol = 0);
    int GetServicenodeRank(const CTxIn& vin, int64_t nBlockHeight, int minProtocol = 0, bool fOnlyActive = true);
    CServicenodePtr GetServicenodeByRank(int nRank, int64_t nBlockHeight, int minProtocol = 0, bool fOnlyActive = true);

    void ProcessServicenodeConnections();

    void ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);

    /// Return the number of (unique) Servicenodes
    int size() {
        LOCK(cs);
        return static_cast<int>(vServicenodes.size());
    }

    std::string ToString() const;

    void Remove(CTxIn vin);

    /// Update servicenode list and maps using provided CServicenodeBroadcast
    void UpdateServicenodeList(CServicenodeBroadcast mnb);

    CServicenodeBroadcast& GetServicenodeBroadcast(const uint256 & hash) {
        LOCK(cs);
        return mapSeenServicenodeBroadcast[hash];
    }
    void AddServicenodeBroadcast(const uint256 & hash, CServicenodeBroadcast & mnb) {
        LOCK(cs);
        mapSeenServicenodeBroadcast[hash] = mnb;
    }
    void RemoveServicenodeBroadcast(const uint256 & hash) {
        LOCK(cs);
        mapSeenServicenodeBroadcast.erase(hash);
    }
    void UpdateServicenodeBroadcastLastPing(const uint256 & hash, CServicenodePing & ping) {
        LOCK(cs);
        mapSeenServicenodeBroadcast[hash].lastPing = ping;
    }
    bool SeenServicenodeBroadcast(const uint256 & hash) {
        LOCK(cs);
        return mapSeenServicenodeBroadcast.count(hash);
    }

    CServicenodePing& GetServicenodePing(const uint256 & hash) {
        LOCK(cs);
        return mapSeenServicenodePing[hash];
    }
    void AddServicenodePing(const uint256 & hash, CServicenodePing & ping) {
        LOCK(cs);
        mapSeenServicenodePing[hash] = ping;
    }
    bool SeenServicenodePing(const uint256 & hash) {
        LOCK(cs);
        return mapSeenServicenodePing.count(hash);
    }

    int64_t GetDsqCount() {
        LOCK(cs);
        return nDsqCount;
    }
    void AddDsqCount() {
        LOCK(cs);
        ++nDsqCount;
    }
};

#endif
