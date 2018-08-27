
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The PIVX developers
// Copyright (c) 2015-2018 The Blocknet developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef SERVICENODE_H
#define SERVICENODE_H

#include "base58.h"
#include "key.h"
#include "main.h"
#include "net.h"
#include "sync.h"
#include "timedata.h"
#include "util.h"

#define SERVICENODE_MIN_CONFIRMATIONS 15
#define SERVICENODE_MIN_MNP_SECONDS (10 * 60)
#define SERVICENODE_MIN_MNB_SECONDS (5 * 60)
#define SERVICENODE_PING_SECONDS (5 * 60)
#define SERVICENODE_EXPIRATION_SECONDS (120 * 60)
#define SERVICENODE_REMOVAL_SECONDS (130 * 60)
#define SERVICENODE_CHECK_SECONDS 5

using namespace std;

class CServicenode;
class CServicenodeBroadcast;
class CServicenodePing;
extern map<int64_t, uint256> mapCacheBlockHashes;

bool GetBlockHash(uint256& hash, int nBlockHeight);


//
// The Servicenode Ping Class : Contains a different serialize method for sending pings from servicenodes throughout the network
//

class CServicenodePing
{
public:
    CTxIn vin;
    uint256 blockHash;
    int64_t sigTime; //mnb message times
    std::vector<unsigned char> vchSig;
    //removed stop

    CServicenodePing();
    CServicenodePing(const CTxIn & newVin);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(vin);
        READWRITE(blockHash);
        READWRITE(sigTime);
        READWRITE(vchSig);
    }

    bool CheckAndUpdate(int& nDos, bool fRequireEnabled = true);
    bool Sign(const CKey & keyServicenode, const CPubKey & pubKeyServicenode);
    void Relay();

    uint256 GetHash()
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << vin;
        ss << sigTime;
        return ss.GetHash();
    }

    void swap(CServicenodePing& first, CServicenodePing& second) // nothrow
    {
        // enable ADL (not necessary in our case, but good practice)
        using std::swap;

        // by swapping the members of two classes,
        // the two classes are effectively swapped
        swap(first.vin, second.vin);
        swap(first.blockHash, second.blockHash);
        swap(first.sigTime, second.sigTime);
        swap(first.vchSig, second.vchSig);
    }

    CServicenodePing& operator=(CServicenodePing from)
    {
        swap(*this, from);
        return *this;
    }
    friend bool operator==(const CServicenodePing& a, const CServicenodePing& b)
    {
        return a.vin == b.vin && a.blockHash == b.blockHash;
    }
    friend bool operator!=(const CServicenodePing& a, const CServicenodePing& b)
    {
        return !(a == b);
    }
};

class CServicenodeXWallet
{
public:
    explicit CServicenodeXWallet() {}
    explicit CServicenodeXWallet(const std::string & walletName) : strWalletName(walletName) {}

    std::string strWalletName;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(LIMITED_STRING(strWalletName, 8));
    }
};

//
// The Servicenode Class. For managing the Obfuscation process. It contains the input of the 5000 BLOCK, signature to prove
// it's the one who own that ip address and code for calculating the payment election.
//
class CServicenode
{
private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;
    int64_t lastTimeChecked;

public:
    enum state {
        SERVICENODE_PRE_ENABLED,
        SERVICENODE_ENABLED,
        SERVICENODE_EXPIRED,
        SERVICENODE_OUTPOINT_SPENT,
        SERVICENODE_REMOVE,
        SERVICENODE_WATCHDOG_EXPIRED,
        SERVICENODE_POSE_BAN,
        SERVICENODE_VIN_SPENT,
        SERVICENODE_POS_ERROR
    };

    CTxIn vin;
    CService addr;
    CPubKey pubKeyCollateralAddress;
    CPubKey pubKeyServicenode;
    CPubKey pubKeyCollateralAddress1;
    CPubKey pubKeyServicenode1;
    std::vector<unsigned char> sig;
    int activeState;
    int64_t sigTime; //mnb message time
    int cacheInputAge;
    int cacheInputAgeBlock;
    bool unitTest;
    bool allowFreeTx;
    int protocolVersion;
    int nActiveState;
    int64_t nLastDsq; //the dsq count from the last dsq broadcast of this node
    int nScanningErrorCount;
    int nLastScanningErrorBlockHeight;
    CServicenodePing lastPing;

    // xbridge wallets list, connected to service node
    std::vector<CServicenodeXWallet> connectedWallets;

    int64_t nLastDsee;  // temporary, do not save. Remove after migration to v12
    int64_t nLastDseep; // temporary, do not save. Remove after migration to v12

    CServicenode();
    CServicenode(const CServicenode& other);
    CServicenode(const CServicenodeBroadcast& mnb);


    void swap(CServicenode& first, CServicenode& second) // nothrow
    {
        // enable ADL (not necessary in our case, but good practice)
        using std::swap;

        // by swapping the members of two classes,
        // the two classes are effectively swapped
        swap(first.vin, second.vin);
        swap(first.addr, second.addr);
        swap(first.pubKeyCollateralAddress, second.pubKeyCollateralAddress);
        swap(first.pubKeyServicenode, second.pubKeyServicenode);
        swap(first.sig, second.sig);
        swap(first.activeState, second.activeState);
        swap(first.sigTime, second.sigTime);
        swap(first.lastPing, second.lastPing);
        swap(first.cacheInputAge, second.cacheInputAge);
        swap(first.cacheInputAgeBlock, second.cacheInputAgeBlock);
        swap(first.unitTest, second.unitTest);
        swap(first.allowFreeTx, second.allowFreeTx);
        swap(first.protocolVersion, second.protocolVersion);
        swap(first.nLastDsq, second.nLastDsq);
        swap(first.nScanningErrorCount, second.nScanningErrorCount);
        swap(first.nLastScanningErrorBlockHeight, second.nLastScanningErrorBlockHeight);
        swap(first.connectedWallets, second.connectedWallets);
    }

    CServicenode& operator=(CServicenode from)
    {
        swap(*this, from);
        return *this;
    }
    friend bool operator==(const CServicenode& a, const CServicenode& b)
    {
        return a.vin == b.vin;
    }
    friend bool operator!=(const CServicenode& a, const CServicenode& b)
    {
        return !(a.vin == b.vin);
    }

    uint256 CalculateScore(int mod = 1, int64_t nBlockHeight = 0);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        LOCK(cs);

        READWRITE(vin);
        READWRITE(addr);
        READWRITE(pubKeyCollateralAddress);
        READWRITE(pubKeyServicenode);
        READWRITE(sig);
        READWRITE(sigTime);
        READWRITE(protocolVersion);
        READWRITE(activeState);
        READWRITE(lastPing);
        READWRITE(cacheInputAge);
        READWRITE(cacheInputAgeBlock);
        READWRITE(unitTest);
        READWRITE(allowFreeTx);
        READWRITE(nLastDsq);
        READWRITE(nScanningErrorCount);
        READWRITE(nLastScanningErrorBlockHeight);
    }

    int64_t SecondsSincePayment();

    bool UpdateFromNewBroadcast(CServicenodeBroadcast& mnb);

    inline uint64_t SliceHash(uint256& hash, int slice)
    {
        uint64_t n = 0;
        memcpy(&n, &hash + slice * 64, 64);
        return n;
    }

    void Check(bool forceCheck = false);

    bool IsBroadcastedWithin(int seconds)
    {
        return (GetAdjustedTime() - sigTime) < seconds;
    }

    bool IsPingedWithin(int seconds, int64_t now = -1)
    {
        now == -1 ? now = GetAdjustedTime() : now;

        return (lastPing == CServicenodePing()) ? false : now - lastPing.sigTime < seconds;
    }

    void Disable()
    {
        sigTime = 0;
        lastPing = CServicenodePing();
    }

    bool IsEnabled()
    {
        return activeState == SERVICENODE_ENABLED;
    }

    int GetServicenodeInputAge()
    {
        if (chainActive.Tip() == NULL) return 0;

        if (cacheInputAge == 0) {
            cacheInputAge = GetInputAge(vin);
            cacheInputAgeBlock = chainActive.Tip()->nHeight;
        }

        return cacheInputAge + (chainActive.Tip()->nHeight - cacheInputAgeBlock);
    }

    std::string GetStatus();

    std::string Status()
    {
        std::string strStatus = "ACTIVE";

        if (activeState == CServicenode::SERVICENODE_ENABLED) strStatus = "ENABLED";
        if (activeState == CServicenode::SERVICENODE_EXPIRED) strStatus = "EXPIRED";
        if (activeState == CServicenode::SERVICENODE_VIN_SPENT) strStatus = "VIN_SPENT";
        if (activeState == CServicenode::SERVICENODE_REMOVE) strStatus = "REMOVE";
        if (activeState == CServicenode::SERVICENODE_POS_ERROR) strStatus = "POS_ERROR";

        return strStatus;
    }

    int64_t GetLastPaid();
    bool IsValidNetAddr();

    std::string GetConnectedWalletsStr() const;
};

//
// The Servicenode Broadcast Class : Contains a different serialize method for sending servicenodes through the network
//

class CServicenodeBroadcast : public CServicenode
{
public:
    CServicenodeBroadcast();
    CServicenodeBroadcast(const CService & newAddr,
                          const CTxIn & newVin,
                          const CPubKey & pubKeyCollateralAddressNew,
                          const CPubKey & pubKeyServicenodeNew,
                          const int protocolVersionIn,
                          const std::vector<std::string> & exchangeWallets);
    CServicenodeBroadcast(const CServicenode& mn);

    bool CheckAndUpdate(int& nDoS);
    bool CheckInputsAndAdd(int& nDos);
    bool Sign(const CKey & keyCollateralAddress);
    void Relay();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(vin);
        READWRITE(addr);
        READWRITE(pubKeyCollateralAddress);
        READWRITE(pubKeyServicenode);
        READWRITE(sig);
        READWRITE(sigTime);
        READWRITE(protocolVersion);
        READWRITE(lastPing);
        READWRITE(nLastDsq);
        if (nType == SER_NETWORK && nVersion >= SERVICENODE_WITH_XBRIDGE_INFO_PROTO_VERSION)
        {
            READWRITE(connectedWallets);
        }
        else if (nType != SER_NETWORK)
        {
            READWRITE(connectedWallets);
        }
    }

    uint256 GetHash()
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << sigTime;
        ss << pubKeyCollateralAddress;
        return ss.GetHash();
    }

    /// Create Servicenode broadcast, needs to be relayed manually after that
    static bool Create(const CTxIn & vin,
                       const CService & service,
                       const CKey & keyCollateralAddressNew,
                       const CPubKey & pubKeyCollateralAddressNew,
                       const CKey & keyServicenodeNew,
                       const CPubKey & pubKeyServicenodeNew,
                       const std::vector<string> & exchangeWallets,
                       std::string & strErrorRet,
                       CServicenodeBroadcast & mnbRet);
    static bool Create(const std::string & strService,
                       const std::string & strKey,
                       const std::string & strTxHash,
                       const std::string & strOutputIndex,
                       const std::vector<string> & exchangeWallets,
                       std::string & strErrorRet,
                       CServicenodeBroadcast & mnbRet,
                       const bool fOffline = false);
};

#endif
