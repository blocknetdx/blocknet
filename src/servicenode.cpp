// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The PIVX developers
// Copyright (c) 2015-2018 The Blocknet developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "servicenode.h"
#include "addrman.h"
#include "servicenodeman.h"
#include "obfuscation.h"
#include "sync.h"
#include "util.h"

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <xbridge/xbridgeapp.h>

// keep track of the scanning errors I've seen
map<uint256, int> mapSeenServicenodeScanningErrors;
// cache block hashes as we calculate them
std::map<int64_t, uint256> mapCacheBlockHashes;

//Get the last hash that matches the modulus given. Processed in reverse order
bool GetBlockHash(uint256& hash, int nBlockHeight)
{
    if (chainActive.Tip() == NULL) return false;

    if (nBlockHeight == 0)
        nBlockHeight = chainActive.Tip()->nHeight;

    if (mapCacheBlockHashes.count(nBlockHeight)) {
        hash = mapCacheBlockHashes[nBlockHeight];
        return true;
    }

    const CBlockIndex* BlockLastSolved = chainActive.Tip();
    const CBlockIndex* BlockReading = chainActive.Tip();

    if (BlockLastSolved == NULL || BlockLastSolved->nHeight == 0 || chainActive.Tip()->nHeight + 1 < nBlockHeight) return false;

    int nBlocksAgo = 0;
    if (nBlockHeight > 0) nBlocksAgo = (chainActive.Tip()->nHeight + 1) - nBlockHeight;
    assert(nBlocksAgo >= 0);

    int n = 0;
    for (unsigned int i = 1; BlockReading && BlockReading->nHeight > 0; i++) {
        if (n >= nBlocksAgo) {
            hash = BlockReading->GetBlockHash();
            mapCacheBlockHashes[nBlockHeight] = hash;
            return true;
        }
        n++;

        if (BlockReading->pprev == NULL) {
            assert(BlockReading);
            break;
        }
        BlockReading = BlockReading->pprev;
    }

    return false;
}

CServicenode::CServicenode()
{
    LOCK(cs);

    vin                           = CTxIn();
    addr                          = CService();
    pubKeyCollateralAddress       = CPubKey();
    pubKeyServicenode             = CPubKey();
    sig                           = std::vector<unsigned char>();
    activeState                   = SERVICENODE_ENABLED;
    sigTime                       = GetAdjustedTime();
    lastPing                      = CServicenodePing();
    cacheInputAge                 = 0;
    cacheInputAgeBlock            = 0;
    unitTest                      = false;
    allowFreeTx                   = true;
    nActiveState                  = SERVICENODE_ENABLED,
    protocolVersion               = PROTOCOL_VERSION;
    nLastDsq                      = 0;
    nScanningErrorCount           = 0;
    nLastScanningErrorBlockHeight = 0;
    lastTimeChecked               = 0;
    nLastDsee                     = 0; // temporary, do not save. Remove after migration to v12
    nLastDseep                    = 0; // temporary, do not save. Remove after migration to v12
}

CServicenode::CServicenode(const CServicenode& other)
{
    LOCK(cs);

    vin                           = other.vin;
    addr                          = other.addr;
    pubKeyCollateralAddress       = other.pubKeyCollateralAddress;
    pubKeyServicenode             = other.pubKeyServicenode;
    sig                           = other.sig;
    activeState                   = other.activeState;
    sigTime                       = other.sigTime;
    lastPing                      = other.lastPing;
    cacheInputAge                 = other.cacheInputAge;
    cacheInputAgeBlock            = other.cacheInputAgeBlock;
    unitTest                      = other.unitTest;
    allowFreeTx                   = other.allowFreeTx;
    nActiveState                  = SERVICENODE_ENABLED,
    protocolVersion               = other.protocolVersion;
    nLastDsq                      = other.nLastDsq;
    nScanningErrorCount           = other.nScanningErrorCount;
    nLastScanningErrorBlockHeight = other.nLastScanningErrorBlockHeight;
    lastTimeChecked               = 0;
    nLastDsee                     = other.nLastDsee;  // temporary, do not save. Remove after migration to v12
    nLastDseep                    = other.nLastDseep; // temporary, do not save. Remove after migration to v12
    connectedWallets              = other.connectedWallets;
}

CServicenode::CServicenode(const CServicenodeBroadcast& mnb)
{
    LOCK(cs);

    vin                           = mnb.vin;
    addr                          = mnb.addr;
    pubKeyCollateralAddress       = mnb.pubKeyCollateralAddress;
    pubKeyServicenode             = mnb.pubKeyServicenode;
    sig                           = mnb.sig;
    activeState                   = SERVICENODE_ENABLED;
    sigTime                       = mnb.sigTime;
    lastPing                      = mnb.lastPing;
    cacheInputAge                 = 0;
    cacheInputAgeBlock            = 0;
    unitTest                      = false;
    allowFreeTx                   = true;
    nActiveState                  = SERVICENODE_ENABLED,
    protocolVersion               = mnb.protocolVersion;
    nLastDsq                      = mnb.nLastDsq;
    nScanningErrorCount           = 0;
    nLastScanningErrorBlockHeight = 0;
    lastTimeChecked               = 0;
    nLastDsee                     = 0; // temporary, do not save. Remove after migration to v12
    nLastDseep                    = 0; // temporary, do not save. Remove after migration to v12
    connectedWallets              = mnb.connectedWallets;
}

//
// When a new servicenode broadcast is sent, update our information
//
bool CServicenode::UpdateFromNewBroadcast(CServicenodeBroadcast& mnb)
{
    if (mnb.sigTime > sigTime) {
        pubKeyServicenode       = mnb.pubKeyServicenode;
        pubKeyCollateralAddress = mnb.pubKeyCollateralAddress;
        sigTime                 = mnb.sigTime;
        sig                     = mnb.sig;
        protocolVersion         = mnb.protocolVersion;
        addr                    = mnb.addr;
        lastTimeChecked         = 0;
        connectedWallets        = mnb.connectedWallets;
        int nDoS                = 0;
        if (mnb.lastPing == CServicenodePing() ||
            (mnb.lastPing != CServicenodePing() && mnb.lastPing.CheckAndUpdate(nDoS, false)))
        {
            lastPing = mnb.lastPing;
            mnodeman.mapSeenServicenodePing.insert(make_pair(lastPing.GetHash(), lastPing));
        }
        return true;
    }
    return false;
}

//
// Deterministically calculate a given "score" for a Servicenode depending on how close it's hash is to
// the proof of work for that block. The further away they are the better, the furthest will win the election
// and get paid this block
//
uint256 CServicenode::CalculateScore(int /*mod*/, int64_t nBlockHeight)
{
    if (chainActive.Tip() == NULL) return 0;

    uint256 hash = 0;
    uint256 aux = vin.prevout.hash + vin.prevout.n;

    if (!GetBlockHash(hash, nBlockHeight)) {
        LogPrintf("CalculateScore ERROR - nHeight %d - Returned 0\n", nBlockHeight);
        return 0;
    }

    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << hash;
    uint256 hash2 = ss.GetHash();

    CHashWriter ss2(SER_GETHASH, PROTOCOL_VERSION);
    ss2 << hash;
    ss2 << aux;
    uint256 hash3 = ss2.GetHash();

    uint256 r = (hash3 > hash2 ? hash3 - hash2 : hash2 - hash3);

    return r;
}

void CServicenode::Check(bool forceCheck)
{
    if (ShutdownRequested()) return;

    if (!forceCheck && (GetTime() - lastTimeChecked < SERVICENODE_CHECK_SECONDS)) return;
    lastTimeChecked = GetTime();


    //once spent, stop doing the checks
    if (activeState == SERVICENODE_VIN_SPENT) return;


    if (!IsPingedWithin(SERVICENODE_REMOVAL_SECONDS)) {
        activeState = SERVICENODE_REMOVE;
        return;
    }

    if (!IsPingedWithin(SERVICENODE_EXPIRATION_SECONDS)) {
        activeState = SERVICENODE_EXPIRED;
        return;
    }

    if (!unitTest) {
        CValidationState state;
        CMutableTransaction tx = CMutableTransaction();
        CTxOut vout = CTxOut(SERVICENODE_ACCEPTABLE_INPUTS_CHECK_AMOUNT * COIN, obfuScationPool.collateralPubKey);
        tx.vin.push_back(vin);
        tx.vout.push_back(vout);

        {
            TRY_LOCK(cs_main, lockMain);
            if (!lockMain) return;

            if (!AcceptableInputs(mempool, state, CTransaction(tx), false, NULL)) {
                activeState = SERVICENODE_VIN_SPENT;
                return;
            }
        }
    }

    activeState = SERVICENODE_ENABLED; // OK
}

int64_t CServicenode::SecondsSincePayment()
{
    CScript pubkeyScript;
    pubkeyScript = GetScriptForDestination(pubKeyCollateralAddress.GetID());

    int64_t sec = (GetAdjustedTime() - GetLastPaid());
    int64_t month = 60 * 60 * 24 * 30;
    if (sec < month) return sec; //if it's less than 30 days, give seconds

    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << vin;
    ss << sigTime;
    uint256 hash = ss.GetHash();

    // return some deterministic value for unknown/unpaid but force it to be more than 30 days old
    return month + hash.GetCompact(false);
}

int64_t CServicenode::GetLastPaid()
{
    CBlockIndex* pindexPrev = chainActive.Tip();
    if (pindexPrev == NULL) return false;

    CScript mnpayee;
    mnpayee = GetScriptForDestination(pubKeyCollateralAddress.GetID());

    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << vin;
    ss << sigTime;
    uint256 hash = ss.GetHash();

    // use a deterministic offset to break a tie -- 2.5 minutes
    int64_t nOffset = hash.GetCompact(false) % 150;

    if (chainActive.Tip() == NULL) return false;

    const CBlockIndex* BlockReading = chainActive.Tip();

    int nMnCount = mnodeman.CountEnabled() * 1.25;
    int n = 0;
    for (unsigned int i = 1; BlockReading && BlockReading->nHeight > 0; i++) {
        if (n >= nMnCount) {
            return 0;
        }
        n++;

        if (servicenodePayments.mapServicenodeBlocks.count(BlockReading->nHeight)) {
            /*
                Search for this payee, with at least 2 votes. This will aid in consensus allowing the network
                to converge on the same payees quickly, then keep the same schedule.
            */
            if (servicenodePayments.mapServicenodeBlocks[BlockReading->nHeight].HasPayeeWithVotes(mnpayee, 2)) {
                return BlockReading->nTime + nOffset;
            }
        }

        if (BlockReading->pprev == NULL) {
            assert(BlockReading);
            break;
        }
        BlockReading = BlockReading->pprev;
    }

    return 0;
}

std::string CServicenode::GetStatus()
{
    switch (nActiveState) {
    case CServicenode::SERVICENODE_PRE_ENABLED:
        return "PRE_ENABLED";
    case CServicenode::SERVICENODE_ENABLED:
        return "ENABLED";
    case CServicenode::SERVICENODE_EXPIRED:
        return "EXPIRED";
    case CServicenode::SERVICENODE_OUTPOINT_SPENT:
        return "OUTPOINT_SPENT";
    case CServicenode::SERVICENODE_REMOVE:
        return "REMOVE";
    case CServicenode::SERVICENODE_WATCHDOG_EXPIRED:
        return "WATCHDOG_EXPIRED";
    case CServicenode::SERVICENODE_POSE_BAN:
        return "POSE_BAN";
    default:
        return "UNKNOWN";
    }
}

bool CServicenode::IsValidNetAddr()
{
    // TODO: regtest is fine with any addresses for now,
    // should probably be a bit smarter if one day we start to implement tests for this
    return Params().NetworkID() == CBaseChainParams::REGTEST ||
            (IsReachable(addr) && addr.IsRoutable());
}

/**
 * @brief Returns the string representation of this servicenode's supported services.
 * @return
 */
std::string CServicenode::GetServices() const
{
    auto services = xbridge::App::instance().nodeServices(pubKeyServicenode);
    if (services.empty())
        return "";

    std::string result;
    for (const auto &service : services)
        result += (result.length() > 0 ? "," : "") + service;

    return result;
}

/**
 * @brief Returns true if this servicenode has the specified service.
 * @param service
 * @return
 */
bool CServicenode::HasService(const std::string &service) const
{
    return xbridge::App::instance().hasNodeService(pubKeyServicenode, service);
}

CServicenodeBroadcast::CServicenodeBroadcast()
{
    vin = CTxIn();
    addr = CService();
    pubKeyCollateralAddress = CPubKey();
    pubKeyServicenode1 = CPubKey();
    sig = std::vector<unsigned char>();
    activeState = SERVICENODE_ENABLED;
    sigTime = GetAdjustedTime();
    lastPing = CServicenodePing();
    cacheInputAge = 0;
    cacheInputAgeBlock = 0;
    unitTest = false;
    allowFreeTx = true;
    protocolVersion = PROTOCOL_VERSION;
    nLastDsq = 0;
    nScanningErrorCount = 0;
    nLastScanningErrorBlockHeight = 0;
}

CServicenodeBroadcast::CServicenodeBroadcast(const CService & newAddr,
                                             const CTxIn & newVin,
                                             const CPubKey & pubKeyCollateralAddressNew,
                                             const CPubKey & pubKeyServicenodeNew,
                                             const int protocolVersionIn)
{
    vin = newVin;
    addr = newAddr;
    pubKeyCollateralAddress = pubKeyCollateralAddressNew;
    pubKeyServicenode = pubKeyServicenodeNew;
    sig = std::vector<unsigned char>();
    activeState = SERVICENODE_ENABLED;
    sigTime = GetAdjustedTime();
    lastPing = CServicenodePing();
    cacheInputAge = 0;
    cacheInputAgeBlock = 0;
    unitTest = false;
    allowFreeTx = true;
    protocolVersion = protocolVersionIn;
    nLastDsq = 0;
    nScanningErrorCount = 0;
    nLastScanningErrorBlockHeight = 0;
}

CServicenodeBroadcast::CServicenodeBroadcast(const CServicenode& mn)
{
    vin = mn.vin;
    addr = mn.addr;
    pubKeyCollateralAddress = mn.pubKeyCollateralAddress;
    pubKeyServicenode = mn.pubKeyServicenode;
    sig = mn.sig;
    activeState = mn.activeState;
    sigTime = mn.sigTime;
    lastPing = mn.lastPing;
    cacheInputAge = mn.cacheInputAge;
    cacheInputAgeBlock = mn.cacheInputAgeBlock;
    unitTest = mn.unitTest;
    allowFreeTx = mn.allowFreeTx;
    protocolVersion = mn.protocolVersion;
    nLastDsq = mn.nLastDsq;
    nScanningErrorCount = mn.nScanningErrorCount;
    nLastScanningErrorBlockHeight = mn.nLastScanningErrorBlockHeight;
    connectedWallets = mn.connectedWallets;
}

bool CServicenodeBroadcast::Create(const string & strService,
                                   const string & strKeyServicenode,
                                   const string & strTxHash,
                                   const string & strOutputIndex,
                                   std::string & strErrorRet,
                                   CServicenodeBroadcast & mnbRet,
                                   const bool fOffline)
{
    CTxIn txin;
    CPubKey pubKeyCollateralAddressNew;
    CKey keyCollateralAddressNew;
    CPubKey pubKeyServicenodeNew;
    CKey keyServicenodeNew;

    //need correct blocks to send ping
    if (!fOffline && !servicenodeSync.IsBlockchainSynced()) {
        strErrorRet = "Sync in progress. Must wait until sync is complete to start Servicenode";
        LogPrintf("CServicenodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    if (!obfuScationSigner.GetKeysFromSecret(strKeyServicenode, keyServicenodeNew, pubKeyServicenodeNew)) {
        strErrorRet = strprintf("Invalid servicenode key %s", strKeyServicenode);
        LogPrintf("CServicenodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    if (!pwalletMain->GetServicenodeVinAndKeys(txin, pubKeyCollateralAddressNew, keyCollateralAddressNew, strTxHash, strOutputIndex)) {
        strErrorRet = strprintf("Could not allocate txin %s:%s for servicenode %s", strTxHash, strOutputIndex, strService);
        LogPrintf("CServicenodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    CService service = CService(strService);
    int mainnetDefaultPort = Params(CBaseChainParams::MAIN).GetDefaultPort();
    if (Params().NetworkID() == CBaseChainParams::MAIN) {
        if (service.GetPort() != mainnetDefaultPort) {
            strErrorRet = strprintf("Invalid port %u for servicenode %s, only %d is supported on mainnet.", service.GetPort(), strService, mainnetDefaultPort);
            LogPrintf("CServicenodeBroadcast::Create -- %s\n", strErrorRet);
            return false;
        }
    } else if (service.GetPort() == mainnetDefaultPort) {
        strErrorRet = strprintf("Invalid port %u for servicenode %s, %d is the only supported on mainnet.", service.GetPort(), strService, mainnetDefaultPort);
        LogPrintf("CServicenodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    return Create(txin, CService(strService), keyCollateralAddressNew,
                  pubKeyCollateralAddressNew, keyServicenodeNew,
                  pubKeyServicenodeNew,
                  strErrorRet, mnbRet);
}

bool CServicenodeBroadcast::Create(const CTxIn & txin,
                                   const CService & service,
                                   const CKey & keyCollateralAddressNew,
                                   const CPubKey &pubKeyCollateralAddressNew,
                                   const CKey & keyServicenodeNew,
                                   const CPubKey & pubKeyServicenodeNew,
                                   std::string & strErrorRet,
                                   CServicenodeBroadcast & mnbRet)
{
    // wait for reindex and/or import to finish
    if (fImporting || fReindex) return false;

    LogPrint("servicenode", "CServicenodeBroadcast::Create -- pubKeyCollateralAddressNew = %s, pubKeyServicenodeNew.GetID() = %s\n",
        CBitcoinAddress(pubKeyCollateralAddressNew.GetID()).ToString(),
        pubKeyServicenodeNew.GetID().ToString());


    CServicenodePing mnp(txin);
    if (!mnp.Sign(keyServicenodeNew, pubKeyServicenodeNew)) {
        strErrorRet = strprintf("Failed to sign ping, servicenode=%s", txin.prevout.hash.ToString());
        LogPrintf("CServicenodeBroadcast::Create -- %s\n", strErrorRet);
        mnbRet = CServicenodeBroadcast();
        return false;
    }

    mnbRet = CServicenodeBroadcast(service, txin, pubKeyCollateralAddressNew,
                                   pubKeyServicenodeNew, PROTOCOL_VERSION);

    if (!mnbRet.IsValidNetAddr()) {
        strErrorRet = strprintf("Invalid IP address, servicenode=%s", txin.prevout.hash.ToString());
        LogPrintf("CServicenodeBroadcast::Create -- %s\n", strErrorRet);
        mnbRet = CServicenodeBroadcast();
        return false;
    }

    mnbRet.lastPing = mnp;
    if (!mnbRet.Sign(keyCollateralAddressNew)) {
        strErrorRet = strprintf("Failed to sign broadcast, servicenode=%s", txin.prevout.hash.ToString());
        LogPrintf("CServicenodeBroadcast::Create -- %s\n", strErrorRet);
        mnbRet = CServicenodeBroadcast();
        return false;
    }

    return true;
}

bool CServicenodeBroadcast::CheckAndUpdate(int& nDos)
{
    // make sure signature isn't in the future (past is OK)
    if (sigTime > GetAdjustedTime() + 60 * 60) {
        LogPrintf("mnb - Signature rejected, too far into the future %s\n", vin.prevout.hash.ToString());
        nDos = 1;
        return false;
    }

    std::string vchPubKey(pubKeyCollateralAddress.begin(), pubKeyCollateralAddress.end());
    std::string vchPubKey2(pubKeyServicenode.begin(), pubKeyServicenode.end());
    std::string strMessage = addr.ToString() + boost::lexical_cast<std::string>(sigTime) + vchPubKey + vchPubKey2 + boost::lexical_cast<std::string>(protocolVersion);

    if (protocolVersion < servicenodePayments.GetMinServicenodePaymentsProto()) {
        LogPrintf("mnb - ignoring outdated Servicenode %s protocol version %d\n", vin.prevout.hash.ToString(), protocolVersion);
        return false;
    }

    CScript pubkeyScript;
    pubkeyScript = GetScriptForDestination(pubKeyCollateralAddress.GetID());

    if (pubkeyScript.size() != 25) {
        LogPrintf("mnb - pubkey the wrong size\n");
        nDos = 100;
        return false;
    }

    CScript pubkeyScript2;
    pubkeyScript2 = GetScriptForDestination(pubKeyServicenode.GetID());

    if (pubkeyScript2.size() != 25) {
        LogPrintf("mnb - pubkey2 the wrong size\n");
        nDos = 100;
        return false;
    }

    if (!vin.scriptSig.empty()) {
        LogPrintf("mnb - Ignore Not Empty ScriptSig %s\n", vin.prevout.hash.ToString());
        return false;
    }

    std::string errorMessage = "";
    if (!obfuScationSigner.VerifyMessage(pubKeyCollateralAddress, sig, strMessage, errorMessage)) {
        LogPrintf("mnb - Got bad Servicenode address signature\n");
        nDos = 100;
        return false;
    }

    if (Params().NetworkID() == CBaseChainParams::MAIN) {
        if (addr.GetPort() != 41412) return false;
    } else if (addr.GetPort() == 41412)
        return false;

    //search existing Servicenode list, this is where we update existing Servicenodes with new mnb broadcasts
    CServicenode* pmn = mnodeman.Find(vin);

    // no such servicenode, nothing to update
    if (pmn == NULL)
        return true;
    else {
        // this broadcast older than we have, it's bad.
        if (pmn->sigTime > sigTime) {
            LogPrintf("mnb - Bad sigTime %d for Servicenode %s (existing broadcast is at %d)\n",
                sigTime, vin.prevout.hash.ToString(), pmn->sigTime);
            return false;
        }
        // servicenode is not enabled yet/already, nothing to update
        if (!pmn->IsEnabled()) return true;
    }

    // mn.pubkey = pubkey, IsVinAssociatedWithPubkey is validated once below,
    //   after that they just need to match
    if (pmn->pubKeyCollateralAddress == pubKeyCollateralAddress && !pmn->IsBroadcastedWithin(SERVICENODE_MIN_MNB_SECONDS)) {
        //take the newest entry
        LogPrint("servicenode", "mnb - Got updated entry for %s\n", vin.prevout.hash.ToString());
        if (pmn->UpdateFromNewBroadcast((*this))) {
            pmn->Check();
            if (pmn->IsEnabled()) Relay();
        }
        servicenodeSync.AddedServicenodeList(GetHash());
    }

    return true;
}

bool CServicenodeBroadcast::CheckInputsAndAdd(int& nDoS)
{
    // we are a servicenode with the same vin (i.e. already activated) and this mnb is ours (matches our Servicenode privkey)
    // so nothing to do here for us
    if (fServiceNode && vin.prevout == activeServicenode.vin.prevout && pubKeyServicenode == activeServicenode.pubKeyServicenode)
        return true;

    // search existing Servicenode list
    CServicenode* pmn = mnodeman.Find(vin);

    if (pmn != NULL) {
        // nothing to do here if we already know about this servicenode and it's enabled
        if (pmn->IsEnabled()) return true;
        // if it's not enabled, remove old MN first and continue
        else
            mnodeman.Remove(pmn->vin);
    }

    CValidationState state;
    CMutableTransaction tx = CMutableTransaction();
    CTxOut vout = CTxOut(SERVICENODE_ACCEPTABLE_INPUTS_CHECK_AMOUNT * COIN, obfuScationPool.collateralPubKey);
    tx.vin.push_back(vin);
    tx.vout.push_back(vout);

    {
        TRY_LOCK(cs_main, lockMain);
        if (!lockMain) {
            // not mnb fault, let it to be checked again later
            mnodeman.mapSeenServicenodeBroadcast.erase(GetHash());
            servicenodeSync.mapSeenSyncMNB.erase(GetHash());
            return false;
        }

        if (!AcceptableInputs(mempool, state, CTransaction(tx), false, NULL)) {
            //set nDos
            state.IsInvalid(nDoS);
            return false;
        }
    }

    LogPrint("servicenode", "mnb - Accepted Servicenode entry\n");

    if (GetInputAge(vin) < SERVICENODE_MIN_CONFIRMATIONS) {
        LogPrintf("mnb - Input must have at least %d confirmations\n", SERVICENODE_MIN_CONFIRMATIONS);
        // maybe we miss few blocks, let this mnb to be checked again later
        mnodeman.mapSeenServicenodeBroadcast.erase(GetHash());
        servicenodeSync.mapSeenSyncMNB.erase(GetHash());
        return false;
    }

    // verify that sig time is legit in past
    // should be at least not earlier than block when 1000 BLOCK tx got SERVICENODE_MIN_CONFIRMATIONS
    uint256 hashBlock = 0;
    CTransaction tx2;
    GetTransaction(vin.prevout.hash, tx2, hashBlock, true);
    BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
    if (mi != mapBlockIndex.end() && (*mi).second) {
        CBlockIndex* pMNIndex = (*mi).second;                                                        // block for 1000 BlocknetDX tx -> 1 confirmation
        CBlockIndex* pConfIndex = chainActive[pMNIndex->nHeight + SERVICENODE_MIN_CONFIRMATIONS - 1]; // block where tx got SERVICENODE_MIN_CONFIRMATIONS
        if (pConfIndex->GetBlockTime() > sigTime) {
            LogPrintf("mnb - Bad sigTime %d for Servicenode %s (%i conf block is at %d)\n",
                sigTime, vin.prevout.hash.ToString(), SERVICENODE_MIN_CONFIRMATIONS, pConfIndex->GetBlockTime());
            return false;
        }
    }

    LogPrintf("mnb - Got NEW Servicenode entry - %s - %lli \n", vin.prevout.hash.ToString(), sigTime);
    CServicenode mn(*this);
    mnodeman.Add(mn);

    // if it matches our Servicenode privkey, then we've been remotely activated
    if (pubKeyServicenode == activeServicenode.pubKeyServicenode && protocolVersion == PROTOCOL_VERSION) {
        activeServicenode.EnableHotColdServiceNode(vin, addr);
    }

    bool isLocal = addr.IsRFC1918() || addr.IsLocal();
    if (Params().NetworkID() == CBaseChainParams::REGTEST) isLocal = false;

    if (!isLocal) Relay();

    return true;
}

void CServicenodeBroadcast::Relay()
{
    CInv inv(MSG_SERVICENODE_ANNOUNCE, GetHash());
    RelayInv(inv);
}

bool CServicenodeBroadcast::Sign(const CKey & keyCollateralAddress)
{
    std::string errorMessage;

    std::string vchPubKey(pubKeyCollateralAddress.begin(), pubKeyCollateralAddress.end());
    std::string vchPubKey2(pubKeyServicenode.begin(), pubKeyServicenode.end());

    sigTime = GetAdjustedTime();

    std::string strMessage = addr.ToString() + boost::lexical_cast<std::string>(sigTime) + vchPubKey + vchPubKey2 + boost::lexical_cast<std::string>(protocolVersion);

    if (!obfuScationSigner.SignMessage(strMessage, errorMessage, sig, keyCollateralAddress)) {
        LogPrintf("CServicenodeBroadcast::Sign() - Error: %s\n", errorMessage);
        return false;
    }

    if (!obfuScationSigner.VerifyMessage(pubKeyCollateralAddress, sig, strMessage, errorMessage)) {
        LogPrintf("CServicenodeBroadcast::Sign() - Error: %s\n", errorMessage);
        return false;
    }

    return true;
}

CServicenodePing::CServicenodePing() : sigTime(0)
{
}

CServicenodePing::CServicenodePing(const CTxIn & newVin)
{
    vin = newVin;
    blockHash = chainActive[chainActive.Height() - 12]->GetBlockHash();
    sigTime = GetAdjustedTime();
    vchSig = std::vector<unsigned char>();
}


bool CServicenodePing::Sign(const CKey& keyServicenode, const CPubKey & pubKeyServicenode)
{
    std::string errorMessage;
    // std::string strServiceNodeSignMessage;

    sigTime = GetAdjustedTime();
    std::string strMessage = vin.ToString() + blockHash.ToString() + boost::lexical_cast<std::string>(sigTime);

    if (!obfuScationSigner.SignMessage(strMessage, errorMessage, vchSig, keyServicenode)) {
        LogPrintf("CServicenodePing::Sign() - Error: %s\n", errorMessage);
        return false;
    }

    if (!obfuScationSigner.VerifyMessage(pubKeyServicenode, vchSig, strMessage, errorMessage)) {
        LogPrintf("CServicenodePing::Sign() - Error: %s\n", errorMessage);
        return false;
    }

    return true;
}

bool CServicenodePing::CheckAndUpdate(int& nDos, bool fRequireEnabled)
{
    if (sigTime > GetAdjustedTime() + 60 * 60) {
        LogPrintf("CServicenodePing::CheckAndUpdate - Signature rejected, too far into the future %s\n", vin.prevout.hash.ToString());
        nDos = 1;
        return false;
    }

    if (sigTime <= GetAdjustedTime() - 60 * 60) {
        LogPrintf("CServicenodePing::CheckAndUpdate - Signature rejected, too far into the past %s - %d %d \n", vin.prevout.hash.ToString(), sigTime, GetAdjustedTime());
        nDos = 1;
        return false;
    }

    LogPrint("servicenode", "CServicenodePing::CheckAndUpdate - New Ping - %s - %lli\n", blockHash.ToString(), sigTime);

    // see if we have this Servicenode
    CServicenode* pmn = mnodeman.Find(vin);
    if (pmn != NULL && pmn->protocolVersion >= servicenodePayments.GetMinServicenodePaymentsProto()) {
        if (fRequireEnabled && !pmn->IsEnabled()) return false;

        // LogPrintf("mnping - Found corresponding mn for vin: %s\n", vin.ToString());
        // update only if there is no known ping for this servicenode or
        // last ping was more then SERVICENODE_MIN_MNP_SECONDS-60 ago comparing to this one
        if (!pmn->IsPingedWithin(SERVICENODE_MIN_MNP_SECONDS - 60, sigTime)) {
            std::string strMessage = vin.ToString() + blockHash.ToString() + boost::lexical_cast<std::string>(sigTime);

            std::string errorMessage = "";
            if (!obfuScationSigner.VerifyMessage(pmn->pubKeyServicenode, vchSig, strMessage, errorMessage)) {
                LogPrintf("CServicenodePing::CheckAndUpdate - Got bad Servicenode address signature %s\n", vin.prevout.hash.ToString());
                nDos = 33;
                return false;
            }

            BlockMap::iterator mi = mapBlockIndex.find(blockHash);
            if (mi != mapBlockIndex.end() && (*mi).second) {
                if ((*mi).second->nHeight < chainActive.Height() - 24) {
                    LogPrintf("CServicenodePing::CheckAndUpdate - Servicenode %s block hash %s is too old\n", vin.prevout.hash.ToString(), blockHash.ToString());
                    // Do nothing here (no Servicenode update, no mnping relay)
                    // Let this node to be visible but fail to accept mnping

                    return false;
                }
            } else {
                if (fDebug) LogPrintf("CServicenodePing::CheckAndUpdate - Servicenode %s block hash %s is unknown\n", vin.prevout.hash.ToString(), blockHash.ToString());
                // maybe we stuck so we shouldn't ban this node, just fail to accept it
                // TODO: or should we also request this block?

                return false;
            }

            pmn->lastPing = *this;

            //mnodeman.mapSeenServicenodeBroadcast.lastPing is probably outdated, so we'll update it
            CServicenodeBroadcast mnb(*pmn);
            uint256 hash = mnb.GetHash();
            if (mnodeman.mapSeenServicenodeBroadcast.count(hash)) {
                mnodeman.mapSeenServicenodeBroadcast[hash].lastPing = *this;
            }

            pmn->Check(true);
            if (!pmn->IsEnabled()) return false;

            LogPrint("servicenode", "CServicenodePing::CheckAndUpdate - Servicenode ping accepted, vin: %s\n", vin.prevout.hash.ToString());

            Relay();
            return true;
        }
        LogPrint("servicenode", "CServicenodePing::CheckAndUpdate - Servicenode ping arrived too early, vin: %s\n", vin.prevout.hash.ToString());
        //nDos = 1; //disable, this is happening frequently and causing banned peers
        return false;
    }
    LogPrint("servicenode", "CServicenodePing::CheckAndUpdate - Couldn't find compatible Servicenode entry, vin: %s\n", vin.prevout.hash.ToString());

    return false;
}

void CServicenodePing::Relay()
{
    CInv inv(MSG_SERVICENODE_PING, GetHash());
    RelayInv(inv);
}
