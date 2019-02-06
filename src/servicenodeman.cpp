// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The PIVX developers
// Copyright (c) 2015-2018 The Blocknet developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "servicenodeman.h"
#include "activeservicenode.h"
#include "addrman.h"
#include "servicenode.h"
#include "obfuscation.h"
#include "spork.h"
#include "util.h"
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

/** Servicenode manager */
CServicenodeMan mnodeman;

struct CompareLastPaid {
    bool operator()(const pair<int64_t, CTxIn>& t1,
        const pair<int64_t, CTxIn>& t2) const
    {
        return t1.first < t2.first;
    }
};

struct CompareScoreTxIn {
    bool operator()(const pair<int64_t, CTxIn>& t1,
        const pair<int64_t, CTxIn>& t2) const
    {
        return t1.first < t2.first;
    }
};

struct CompareScoreMN {
    bool operator()(const pair<int64_t, CServicenode>& t1,
        const pair<int64_t, CServicenode>& t2) const
    {
        return t1.first < t2.first;
    }
};

//
// CServicenodeDB
//

CServicenodeDB::CServicenodeDB()
{
    pathMN = GetDataDir() / "mncache.dat";
    strMagicMessage = "ServicenodeCache";
}

bool CServicenodeDB::Write(const CServicenodeMan& mnodemanToSave)
{
    int64_t nStart = GetTimeMillis();

    // serialize, checksum data up to that point, then append checksum
    CDataStream ssServicenodes(SER_DISK, CLIENT_VERSION);
    ssServicenodes << strMagicMessage;                   // servicenode cache file specific magic message
    ssServicenodes << FLATDATA(Params().MessageStart()); // network specific magic number
    ssServicenodes << mnodemanToSave;
    uint256 hash = Hash(ssServicenodes.begin(), ssServicenodes.end());
    ssServicenodes << hash;

    // open output file, and associate with CAutoFile
    FILE* file = fopen(pathMN.string().c_str(), "wb");
    CAutoFile fileout(file, SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("%s : Failed to open file %s", __func__, pathMN.string());

    // Write and commit header, data
    try {
        fileout << ssServicenodes;
    } catch (std::exception& e) {
        return error("%s : Serialize or I/O error - %s", __func__, e.what());
    }
    //    FileCommit(fileout);
    fileout.fclose();

    LogPrintf("Written info to mncache.dat  %dms\n", GetTimeMillis() - nStart);
    LogPrintf("  %s\n", mnodemanToSave.ToString());

    return true;
}

CServicenodeDB::ReadResult CServicenodeDB::Read(CServicenodeMan& mnodemanToLoad, bool fDryRun)
{
    int64_t nStart = GetTimeMillis();
    // open input file, and associate with CAutoFile
    FILE* file = fopen(pathMN.string().c_str(), "rb");
    CAutoFile filein(file, SER_DISK, CLIENT_VERSION);
    if (filein.IsNull()) {
        error("%s : Failed to open file %s", __func__, pathMN.string());
        return FileError;
    }

    // use file size to size memory buffer
    int fileSize = boost::filesystem::file_size(pathMN);
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

    CDataStream ssServicenodes(vchData, SER_DISK, CLIENT_VERSION);

    // verify stored checksum matches input data
    uint256 hashTmp = Hash(ssServicenodes.begin(), ssServicenodes.end());
    if (hashIn != hashTmp) {
        error("%s : Checksum mismatch, data corrupted", __func__);
        return IncorrectHash;
    }

    unsigned char pchMsgTmp[4];
    std::string strMagicMessageTmp;
    try {
        // de-serialize file header (servicenode cache file specific magic message) and ..

        ssServicenodes >> strMagicMessageTmp;

        // ... verify the message matches predefined one
        if (strMagicMessage != strMagicMessageTmp) {
            error("%s : Invalid servicenode cache magic message", __func__);
            return IncorrectMagicMessage;
        }

        // de-serialize file header (network specific magic number) and ..
        ssServicenodes >> FLATDATA(pchMsgTmp);

        // ... verify the network matches ours
        if (memcmp(pchMsgTmp, Params().MessageStart(), sizeof(pchMsgTmp))) {
            error("%s : Invalid network magic number", __func__);
            return IncorrectMagicNumber;
        }
        // de-serialize data into CServicenodeMan object
        ssServicenodes >> mnodemanToLoad;
    } catch (std::exception& e) {
        mnodemanToLoad.Clear();
        error("%s : Deserialize or I/O error - %s", __func__, e.what());
        return IncorrectFormat;
    }

    LogPrintf("Loaded info from mncache.dat  %dms\n", GetTimeMillis() - nStart);
    LogPrintf("  %s\n", mnodemanToLoad.ToString());
    if (!fDryRun) {
        LogPrintf("Servicenode manager - cleaning....\n");
        mnodemanToLoad.CheckAndRemove(true);
        LogPrintf("Servicenode manager - result:\n");
        LogPrintf("  %s\n", mnodemanToLoad.ToString());
    }

    return Ok;
}

void DumpServicenodes()
{
    int64_t nStart = GetTimeMillis();

    CServicenodeDB mndb;
    CServicenodeMan tempMnodeman;

    LogPrintf("Verifying mncache.dat format...\n");
    CServicenodeDB::ReadResult readResult = mndb.Read(tempMnodeman, true);
    // there was an error and it was not an error on file opening => do not proceed
    if (readResult == CServicenodeDB::FileError)
        LogPrintf("Missing servicenode cache file - mncache.dat, will try to recreate\n");
    else if (readResult != CServicenodeDB::Ok) {
        LogPrintf("Error reading mncache.dat: ");
        if (readResult == CServicenodeDB::IncorrectFormat)
            LogPrintf("magic is ok but data has invalid format, will try to recreate\n");
        else {
            LogPrintf("file format is unknown or invalid, please fix it manually\n");
            return;
        }
    }
    LogPrintf("Writting info to mncache.dat...\n");
    mndb.Write(mnodeman);

    LogPrintf("Servicenode dump finished  %dms\n", GetTimeMillis() - nStart);
}

CServicenodeMan::CServicenodeMan()
{
    nDsqCount = 0;
}

bool CServicenodeMan::Add(CServicenode& mn)
{
    LOCK(cs);

    if (!mn.IsEnabled())
        return false;

    CServicenode* pmn = Find(mn.vin);
    if (pmn == NULL) {
        LogPrint("servicenode", "CServicenodeMan: Adding new Servicenode %s - %i now\n", mn.vin.prevout.hash.ToString(), size() + 1);
        vServicenodes.push_back(mn);
        return true;
    }

    return false;
}

void CServicenodeMan::AskForMN(CNode* pnode, CTxIn& vin)
{
    std::map<COutPoint, int64_t>::iterator i = mWeAskedForServicenodeListEntry.find(vin.prevout);
    if (i != mWeAskedForServicenodeListEntry.end()) {
        int64_t t = (*i).second;
        if (GetTime() < t) return; // we've asked recently
    }

    // ask for the mnb info once from the node that sent mnp

    LogPrint("servicenode", "CServicenodeMan::AskForMN - Asking node for missing entry, vin: %s\n", vin.prevout.hash.ToString());
    pnode->PushMessage("dseg", vin);
    int64_t askAgain = GetTime() + SERVICENODE_MIN_MNP_SECONDS;
    mWeAskedForServicenodeListEntry[vin.prevout] = askAgain;
}

void CServicenodeMan::Check()
{
    LOCK(cs);

    BOOST_FOREACH (CServicenode& mn, vServicenodes) {
        mn.Check();
    }
}

void CServicenodeMan::CheckAndRemove(bool forceExpiredRemoval)
{
    Check();

    LOCK(cs);

    //remove inactive and outdated
    vector<CServicenode>::iterator it = vServicenodes.begin();
    while (it != vServicenodes.end()) {
        if ((*it).activeState == CServicenode::SERVICENODE_REMOVE ||
            (*it).activeState == CServicenode::SERVICENODE_VIN_SPENT ||
            (forceExpiredRemoval && (*it).activeState == CServicenode::SERVICENODE_EXPIRED) ||
            (*it).protocolVersion < servicenodePayments.GetMinServicenodePaymentsProto()) {
            LogPrint("servicenode", "CServicenodeMan: Removing inactive Servicenode %s - %i now\n", (*it).vin.prevout.hash.ToString(), size() - 1);

            //erase all of the broadcasts we've seen from this vin
            // -- if we missed a few pings and the node was removed, this will allow is to get it back without them
            //    sending a brand new mnb
            map<uint256, CServicenodeBroadcast>::iterator it3 = mapSeenServicenodeBroadcast.begin();
            while (it3 != mapSeenServicenodeBroadcast.end()) {
                if ((*it3).second.vin == (*it).vin) {
                    servicenodeSync.mapSeenSyncMNB.erase((*it3).first);
                    mapSeenServicenodeBroadcast.erase(it3++);
                } else {
                    ++it3;
                }
            }

            // allow us to ask for this servicenode again if we see another ping
            map<COutPoint, int64_t>::iterator it2 = mWeAskedForServicenodeListEntry.begin();
            while (it2 != mWeAskedForServicenodeListEntry.end()) {
                if ((*it2).first == (*it).vin.prevout) {
                    mWeAskedForServicenodeListEntry.erase(it2++);
                } else {
                    ++it2;
                }
            }

            it = vServicenodes.erase(it);
        } else {
            ++it;
        }
    }

    // check who's asked for the Servicenode list
    map<CNetAddr, int64_t>::iterator it1 = mAskedUsForServicenodeList.begin();
    while (it1 != mAskedUsForServicenodeList.end()) {
        if ((*it1).second < GetTime()) {
            mAskedUsForServicenodeList.erase(it1++);
        } else {
            ++it1;
        }
    }

    // check who we asked for the Servicenode list
    it1 = mWeAskedForServicenodeList.begin();
    while (it1 != mWeAskedForServicenodeList.end()) {
        if ((*it1).second < GetTime()) {
            mWeAskedForServicenodeList.erase(it1++);
        } else {
            ++it1;
        }
    }

    // check which Servicenodes we've asked for
    map<COutPoint, int64_t>::iterator it2 = mWeAskedForServicenodeListEntry.begin();
    while (it2 != mWeAskedForServicenodeListEntry.end()) {
        if ((*it2).second < GetTime()) {
            mWeAskedForServicenodeListEntry.erase(it2++);
        } else {
            ++it2;
        }
    }

    // remove expired mapSeenServicenodeBroadcast
    map<uint256, CServicenodeBroadcast>::iterator it3 = mapSeenServicenodeBroadcast.begin();
    while (it3 != mapSeenServicenodeBroadcast.end()) {
        if ((*it3).second.lastPing.sigTime < GetTime() - (SERVICENODE_REMOVAL_SECONDS * 2)) {
            mapSeenServicenodeBroadcast.erase(it3++);
            servicenodeSync.mapSeenSyncMNB.erase((*it3).second.GetHash());
        } else {
            ++it3;
        }
    }

    // remove expired mapSeenServicenodePing
    map<uint256, CServicenodePing>::iterator it4 = mapSeenServicenodePing.begin();
    while (it4 != mapSeenServicenodePing.end()) {
        if ((*it4).second.sigTime < GetTime() - (SERVICENODE_REMOVAL_SECONDS * 2)) {
            mapSeenServicenodePing.erase(it4++);
        } else {
            ++it4;
        }
    }
}

void CServicenodeMan::Clear()
{
    LOCK(cs);
    vServicenodes.clear();
    mAskedUsForServicenodeList.clear();
    mWeAskedForServicenodeList.clear();
    mWeAskedForServicenodeListEntry.clear();
    mapSeenServicenodeBroadcast.clear();
    mapSeenServicenodePing.clear();
    nDsqCount = 0;
}

int CServicenodeMan::CountEnabled(int protocolVersion)
{
    int i = 0;
    protocolVersion = protocolVersion == -1 ? servicenodePayments.GetMinServicenodePaymentsProto() : protocolVersion;

    BOOST_FOREACH (CServicenode& mn, vServicenodes) {
        mn.Check();
        if (mn.protocolVersion < protocolVersion || !mn.IsEnabled()) continue;
        i++;
    }

    return i;
}

void CServicenodeMan::DsegUpdate(CNode* pnode)
{
    LOCK(cs);

    if (Params().NetworkID() == CBaseChainParams::MAIN) {
        if (!(pnode->addr.IsRFC1918() || pnode->addr.IsLocal())) {
            std::map<CNetAddr, int64_t>::iterator it = mWeAskedForServicenodeList.find(pnode->addr);
            if (it != mWeAskedForServicenodeList.end()) {
                if (GetTime() < (*it).second) {
                    LogPrint("servicenode", "dseg - we already asked peer %i for the list; skipping...\n", pnode->GetId());
                    return;
                }
            }
        }
    }

    pnode->PushMessage("dseg", CTxIn());
    int64_t askAgain = GetTime() + SERVICENODES_DSEG_SECONDS;
    mWeAskedForServicenodeList[pnode->addr] = askAgain;
}

CServicenode* CServicenodeMan::Find(const CScript& payee)
{
    LOCK(cs);
    CScript payee2;

    BOOST_FOREACH (CServicenode& mn, vServicenodes) {
        payee2 = GetScriptForDestination(mn.pubKeyCollateralAddress.GetID());
        if (payee2 == payee)
            return &mn;
    }
    return NULL;
}

CServicenode* CServicenodeMan::Find(const CTxIn& vin)
{
    LOCK(cs);

    BOOST_FOREACH (CServicenode& mn, vServicenodes) {
        if (mn.vin.prevout == vin.prevout)
            return &mn;
    }
    return NULL;
}


CServicenode* CServicenodeMan::Find(const CPubKey& pubKeyServicenode)
{
    LOCK(cs);

    BOOST_FOREACH (CServicenode& mn, vServicenodes) {
        if (mn.pubKeyServicenode == pubKeyServicenode)
            return &mn;
    }
    return NULL;
}

//
// Deterministically select the oldest/best servicenode to pay on the network
//
CServicenode* CServicenodeMan::GetNextServicenodeInQueueForPayment(int nBlockHeight, bool fFilterSigTime, int& nCount)
{
    LOCK(cs);

    CServicenode* pBestServicenode = NULL;
    std::vector<pair<int64_t, CTxIn> > vecServicenodeLastPaid;

    /*
        Make a vector with all of the last paid times
    */

    int nMnCount = CountEnabled();
    BOOST_FOREACH (CServicenode& mn, vServicenodes) {
        mn.Check();
        if (!mn.IsEnabled()) continue;

        // //check protocol version
        if (mn.protocolVersion < servicenodePayments.GetMinServicenodePaymentsProto()) continue;

        //it's in the list (up to 8 entries ahead of current block to allow propagation) -- so let's skip it
        if (servicenodePayments.IsScheduled(mn, nBlockHeight)) continue;

        if (IsSporkActive(SPORK_19_SNODE_LIST)) { // new snode delay
            if (fFilterSigTime && mn.sigTime + SERVICENODE_DELAY_SECONDS > GetAdjustedTime()) continue;
        } else {
            //it's too new, wait for a cycle
            if (fFilterSigTime && mn.sigTime + (nMnCount * 2.6 * 60) > GetAdjustedTime()) continue;
        }

        //make sure it has as many confirmations as there are servicenodes
        if (mn.GetServicenodeInputAge() < nMnCount) continue;

        vecServicenodeLastPaid.push_back(make_pair(mn.SecondsSincePayment(), mn.vin));
    }

    nCount = (int)vecServicenodeLastPaid.size();

    //when the network is in the process of upgrading, don't penalize nodes that recently restarted
    if (fFilterSigTime && nCount < nMnCount / 3) return GetNextServicenodeInQueueForPayment(nBlockHeight, false, nCount);

    // Sort them high to low
    sort(vecServicenodeLastPaid.rbegin(), vecServicenodeLastPaid.rend(), CompareLastPaid());

    // Look at 1/10 of the oldest nodes (by last payment), calculate their scores and pay the best one
    //  -- This doesn't look at who is being paid in the +8-10 blocks, allowing for double payments very rarely
    //  -- 1/100 payments should be a double payment on mainnet - (1/(3000/10))*2
    //  -- (chance per block * chances before IsScheduled will fire)
    int nTenthNetwork = CountEnabled() / 10;
    int nCountTenth = 0;
    uint256 nHigh = 0;
    BOOST_FOREACH (PAIRTYPE(int64_t, CTxIn) & s, vecServicenodeLastPaid) {
        CServicenode* pmn = Find(s.second);
        if (!pmn) break;

        uint256 n = pmn->CalculateScore(1, nBlockHeight - 100);
        if (n > nHigh) {
            nHigh = n;
            pBestServicenode = pmn;
        }
        nCountTenth++;
        if (nCountTenth >= nTenthNetwork) break;
    }
    return pBestServicenode;
}

CServicenode* CServicenodeMan::FindRandomNotInVec(std::vector<CTxIn>& vecToExclude, int protocolVersion)
{
    LOCK(cs);

    protocolVersion = protocolVersion == -1 ? servicenodePayments.GetMinServicenodePaymentsProto() : protocolVersion;

    int nCountEnabled = CountEnabled(protocolVersion);
    LogPrint("servicenode", "CServicenodeMan::FindRandomNotInVec - nCountEnabled - vecToExclude.size() %d\n", nCountEnabled - vecToExclude.size());
    if (nCountEnabled - vecToExclude.size() < 1) return NULL;

    int rand = GetRandInt(nCountEnabled - vecToExclude.size());
    LogPrint("servicenode", "CServicenodeMan::FindRandomNotInVec - rand %d\n", rand);
    bool found;

    BOOST_FOREACH (CServicenode& mn, vServicenodes) {
        if (mn.protocolVersion < protocolVersion || !mn.IsEnabled()) continue;
        found = false;
        BOOST_FOREACH (CTxIn& usedVin, vecToExclude) {
            if (mn.vin.prevout == usedVin.prevout) {
                found = true;
                break;
            }
        }
        if (found) continue;
        if (--rand < 1) {
            return &mn;
        }
    }

    return NULL;
}

CServicenode* CServicenodeMan::GetCurrentServiceNode(int mod, int64_t nBlockHeight, int minProtocol)
{
    int64_t score = 0;
    CServicenode* winner = NULL;

    // scan for winner
    BOOST_FOREACH (CServicenode& mn, vServicenodes) {
        mn.Check();
        if (mn.protocolVersion < minProtocol || !mn.IsEnabled()) continue;

        // calculate the score for each Servicenode
        uint256 n = mn.CalculateScore(mod, nBlockHeight);
        int64_t n2 = n.GetCompact(false);

        // determine the winner
        if (n2 > score) {
            score = n2;
            winner = &mn;
        }
    }

    return winner;
}

int CServicenodeMan::GetServicenodeRank(const CTxIn& vin, int64_t nBlockHeight, int minProtocol, bool fOnlyActive)
{
    std::vector<pair<int64_t, CTxIn> > vecServicenodeScores;

    //make sure we know about this block
    uint256 hash = 0;
    if (!GetBlockHash(hash, nBlockHeight)) return -1;

    // scan for winner
    BOOST_FOREACH (CServicenode& mn, vServicenodes) {
        if (mn.protocolVersion < minProtocol) {
            if (fDebug)
                LogPrintf("Skipping Servicenode %s with obsolete version: %d)\n", mn.vin.prevout.hash.ToString(), mn.protocolVersion);
            continue;
        }
        if (fOnlyActive) {
            mn.Check();
            if (IsSporkActive(SPORK_19_SNODE_LIST)) {
                if (mn.GetServicenodeInputAge() < vServicenodes.size())
                    continue;
                auto snodeAge = GetAdjustedTime() - mn.sigTime;
                if (snodeAge < SERVICENODE_DELAY_SECONDS) {
                    if (fDebug)
                        LogPrintf("Skipping recently activated Servicenode %s with age: %ld\n", mn.vin.prevout.hash.ToString(), snodeAge);
                    continue;
                }
            }
            if (!mn.IsEnabled()) continue;
        }
        uint256 n = mn.CalculateScore(1, nBlockHeight);
        int64_t n2 = n.GetCompact(false);

        vecServicenodeScores.push_back(make_pair(n2, mn.vin));
    }

    sort(vecServicenodeScores.rbegin(), vecServicenodeScores.rend(), CompareScoreTxIn());

    int rank = 0;
    BOOST_FOREACH (PAIRTYPE(int64_t, CTxIn) & s, vecServicenodeScores) {
        rank++;
        if (s.second.prevout == vin.prevout) {
            return rank;
        }
    }

    return -1;
}

std::vector<pair<int, CServicenode> > CServicenodeMan::GetServicenodeRanks(int64_t nBlockHeight, int minProtocol)
{
    std::vector<pair<int64_t, CServicenode> > vecServicenodeScores;
    std::vector<pair<int, CServicenode> > vecServicenodeRanks;

    //make sure we know about this block
    uint256 hash = 0;
    if (!GetBlockHash(hash, nBlockHeight)) return vecServicenodeRanks;

    // scan for winner
    BOOST_FOREACH (CServicenode& mn, vServicenodes) {
        mn.Check();

        if (mn.protocolVersion < minProtocol) continue;

        if (!mn.IsEnabled()) {
            vecServicenodeScores.push_back(make_pair(9999, mn));
            continue;
        }

        uint256 n = mn.CalculateScore(1, nBlockHeight);
        int64_t n2 = n.GetCompact(false);

        vecServicenodeScores.push_back(make_pair(n2, mn));
    }

    sort(vecServicenodeScores.rbegin(), vecServicenodeScores.rend(), CompareScoreMN());

    int rank = 0;
    BOOST_FOREACH (PAIRTYPE(int64_t, CServicenode) & s, vecServicenodeScores) {
        rank++;
        vecServicenodeRanks.push_back(make_pair(rank, s.second));
    }

    return vecServicenodeRanks;
}

CServicenode* CServicenodeMan::GetServicenodeByRank(int nRank, int64_t nBlockHeight, int minProtocol, bool fOnlyActive)
{
    std::vector<pair<int64_t, CTxIn> > vecServicenodeScores;

    // scan for winner
    BOOST_FOREACH (CServicenode& mn, vServicenodes) {
        if (mn.protocolVersion < minProtocol) continue;
        if (fOnlyActive) {
            mn.Check();
            if (!mn.IsEnabled()) continue;
        }

        uint256 n = mn.CalculateScore(1, nBlockHeight);
        int64_t n2 = n.GetCompact(false);

        vecServicenodeScores.push_back(make_pair(n2, mn.vin));
    }

    sort(vecServicenodeScores.rbegin(), vecServicenodeScores.rend(), CompareScoreTxIn());

    int rank = 0;
    BOOST_FOREACH (PAIRTYPE(int64_t, CTxIn) & s, vecServicenodeScores) {
        rank++;
        if (rank == nRank) {
            return Find(s.second);
        }
    }

    return NULL;
}

void CServicenodeMan::ProcessServicenodeConnections()
{
    //we don't care about this for regtest
    if (Params().NetworkID() == CBaseChainParams::REGTEST) return;

    LOCK(cs_vNodes);
    BOOST_FOREACH (CNode* pnode, vNodes) {
        if (pnode->fObfuScationMaster) {
            if (obfuScationPool.pSubmittedToServicenode != NULL && pnode->addr == obfuScationPool.pSubmittedToServicenode->addr) continue;
            LogPrintf("Closing Servicenode connection peer=%i \n", pnode->GetId());
            pnode->fObfuScationMaster = false;
            pnode->Release();
        }
    }
}

void CServicenodeMan::ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    if (fLiteMode) return; //disable all Obfuscation/Servicenode related functionality
    if (!servicenodeSync.IsBlockchainSynced()) return;

    LOCK(cs_process_message);

    if (strCommand == "mnb") { //Servicenode Broadcast
        CServicenodeBroadcast mnb;
        vRecv >> mnb;

        if (mapSeenServicenodeBroadcast.count(mnb.GetHash())) { //seen
            servicenodeSync.AddedServicenodeList(mnb.GetHash());
            return;
        }
        mapSeenServicenodeBroadcast.insert(make_pair(mnb.GetHash(), mnb));

        int nDoS = 0;
        if (!mnb.CheckAndUpdate(nDoS)) {
            if (nDoS > 0)
                Misbehaving(pfrom->GetId(), nDoS);

            //failed
            return;
        }

        // make sure the vout that was signed is related to the transaction that spawned the Servicenode
        //  - this is expensive, so it's only done once per Servicenode
        if (!obfuScationSigner.IsVinAssociatedWithPubkey(mnb.vin, mnb.pubKeyCollateralAddress)) {
            LogPrintf("mnb - Got mismatched pubkey and vin\n");
            Misbehaving(pfrom->GetId(), 33);
            return;
        }

        // make sure it's still unspent
        //  - this is checked later by .check() in many places and by ThreadCheckObfuScationPool()
        if (mnb.CheckInputsAndAdd(nDoS)) {
            // use this as a peer
            addrman.Add(CAddress(mnb.addr), pfrom->addr, 2 * 60 * 60);
            servicenodeSync.AddedServicenodeList(mnb.GetHash());
        } else {
            LogPrintf("mnb - Rejected Servicenode entry %s\n", mnb.vin.prevout.hash.ToString());

            if (nDoS > 0)
                Misbehaving(pfrom->GetId(), nDoS);
        }
    }

    else if (strCommand == "mnp") { //Servicenode Ping
        CServicenodePing mnp;
        vRecv >> mnp;

        LogPrint("servicenode", "mnp - Servicenode ping, vin: %s\n", mnp.vin.prevout.hash.ToString());

        if (mapSeenServicenodePing.count(mnp.GetHash())) return; //seen
        mapSeenServicenodePing.insert(make_pair(mnp.GetHash(), mnp));

        int nDoS = 0;
        if (mnp.CheckAndUpdate(nDoS)) return;

        if (nDoS > 0) {
            // if anything significant failed, mark that node
            Misbehaving(pfrom->GetId(), nDoS);
        } else {
            // if nothing significant failed, search existing Servicenode list
            CServicenode* pmn = Find(mnp.vin);
            // if it's known, don't ask for the mnb, just return
            if (pmn != NULL) return;
        }

        // something significant is broken or mn is unknown,
        // we might have to ask for a servicenode entry once
        AskForMN(pfrom, mnp.vin);

    } else if (strCommand == "dseg") { //Get Servicenode list or specific entry

        CTxIn vin;
        vRecv >> vin;

        if (vin == CTxIn()) { //only should ask for this once
            //local network
            bool isLocal = (pfrom->addr.IsRFC1918() || pfrom->addr.IsLocal());

            if (!isLocal && Params().NetworkID() == CBaseChainParams::MAIN) {
                std::map<CNetAddr, int64_t>::iterator i = mAskedUsForServicenodeList.find(pfrom->addr);
                if (i != mAskedUsForServicenodeList.end()) {
                    int64_t t = (*i).second;
                    if (GetTime() < t) {
                        Misbehaving(pfrom->GetId(), 34);
                        LogPrintf("dseg - peer already asked me for the list\n");
                        return;
                    }
                }
                int64_t askAgain = GetTime() + SERVICENODES_DSEG_SECONDS;
                mAskedUsForServicenodeList[pfrom->addr] = askAgain;
            }
        } //else, asking for a specific node which is ok


        int nInvCount = 0;

        BOOST_FOREACH (CServicenode& mn, vServicenodes) {
            if (mn.addr.IsRFC1918()) continue; //local network

            if (mn.IsEnabled()) {
                LogPrint("servicenode", "dseg - Sending Servicenode entry - %s \n", mn.vin.prevout.hash.ToString());
                if (vin == CTxIn() || vin == mn.vin) {
                    CServicenodeBroadcast mnb = CServicenodeBroadcast(mn);
                    uint256 hash = mnb.GetHash();
                    pfrom->PushInventory(CInv(MSG_SERVICENODE_ANNOUNCE, hash));
                    nInvCount++;

                    if (!mapSeenServicenodeBroadcast.count(hash)) mapSeenServicenodeBroadcast.insert(make_pair(hash, mnb));

                    if (vin == mn.vin) {
                        LogPrint("servicenode", "dseg - Sent 1 Servicenode entry to peer %i\n", pfrom->GetId());
                        return;
                    }
                }
            }
        }

        if (vin == CTxIn()) {
            pfrom->PushMessage("ssc", SERVICENODE_SYNC_LIST, nInvCount);
            LogPrint("servicenode", "dseg - Sent %d Servicenode entries to peer %i\n", nInvCount, pfrom->GetId());
        }
    }
}

void CServicenodeMan::Remove(CTxIn vin)
{
    LOCK(cs);

    vector<CServicenode>::iterator it = vServicenodes.begin();
    while (it != vServicenodes.end()) {
        if ((*it).vin == vin) {
            LogPrint("servicenode", "CServicenodeMan: Removing Servicenode %s - %i now\n", (*it).vin.prevout.hash.ToString(), size() - 1);
            vServicenodes.erase(it);
            break;
        }
        ++it;
    }
}

void CServicenodeMan::UpdateServicenodeList(CServicenodeBroadcast mnb)
{
    LOCK(cs);
    mapSeenServicenodePing.insert(std::make_pair(mnb.lastPing.GetHash(), mnb.lastPing));
    mapSeenServicenodeBroadcast.insert(std::make_pair(mnb.GetHash(), mnb));

    LogPrintf("CServicenodeMan::UpdateServicenodeList -- servicenode=%s\n", mnb.vin.prevout.ToStringShort());

    CServicenode* pmn = Find(mnb.vin);
    if (pmn == NULL) {
        CServicenode mn(mnb);
        if (Add(mn)) {
            servicenodeSync.AddedServicenodeList(mnb.GetHash());
        }
    } else if (pmn->UpdateFromNewBroadcast(mnb)) {
        servicenodeSync.AddedServicenodeList(mnb.GetHash());
    }
}

std::string CServicenodeMan::ToString() const
{
    std::ostringstream info;

    info << "Servicenodes: " << (int)vServicenodes.size() << ", peers who asked us for Servicenode list: " << (int)mAskedUsForServicenodeList.size() << ", peers we asked for Servicenode list: " << (int)mWeAskedForServicenodeList.size() << ", entries in Servicenode list we asked for: " << (int)mWeAskedForServicenodeListEntry.size() << ", nDsqCount: " << (int)nDsqCount;

    return info.str();
}
