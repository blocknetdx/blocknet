// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//*****************************************************************************
//*****************************************************************************

#ifndef BLOCKNET_XBRIDGE_XBRIDGEWALLET_H
#define BLOCKNET_XBRIDGE_XBRIDGEWALLET_H

#include <primitives/transaction.h>
#include <serialize.h>
#include <sync.h>
#include <uint256.h>

#include <cstring>
#include <set>
#include <stdint.h>
#include <string>
#include <vector>

#include <boost/thread.hpp>

//*****************************************************************************
//*****************************************************************************
namespace xbridge
{
    CAmount xBridgeIntFromReal(double amount);

//*****************************************************************************
//*****************************************************************************
namespace wallet
{
typedef std::pair<std::string, std::vector<std::string> > AddressBookEntry;

struct UtxoEntry
{
    std::string txId;
    uint32_t    vout;
    double      amount{0};
    std::string address;
    std::string scriptPubKey;
    uint32_t    confirmations{0};

    std::vector<unsigned char> rawAddress;
    std::vector<unsigned char> signature;

    std::string toString() const;
    bool hasConfirmations{false};

    bool operator < (const UtxoEntry & r) const
    {
        return (txId < r.txId) || ((txId == r.txId) && (vout < r.vout));
    }

    bool operator == (const UtxoEntry & r) const
    {
        return (txId == r.txId) && (vout ==r.vout);
    }

    COutPoint outpoint() const {
        return { uint256S(txId), vout };
    }

    CAmount camount() const {
        return xBridgeIntFromReal(amount);
    }

    void setConfirmations(const uint32_t confs) {
        confirmations = confs;
        hasConfirmations = true;
    }

    static const int CURRENT_VERSION=1;
    int nVersion{CURRENT_VERSION};

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream & s, Operation ser_action) {
        READWRITE(nVersion);
        READWRITE(txId);
        READWRITE(vout);
        READWRITE(amount);
        READWRITE(address);
        READWRITE(scriptPubKey);
        READWRITE(confirmations);
        READWRITE(rawAddress);
        READWRITE(signature);
        READWRITE(hasConfirmations);
    }
};

} // namespace wallet

static constexpr int XMIN_LOCKTIME_BLOCKS = 6;
static constexpr int XMAX_LOCKTIME_DRIFT_BLOCKS = 4; // must be less than XMIN_LOCKTIME_BLOCKS
static constexpr int XMAKER_LOCKTIME_TARGET_SECONDS = 2*60*60; // 2 hours
static constexpr int XTAKER_LOCKTIME_TARGET_SECONDS = 30*60; // 30 mins, must be less than maker locktime target
static constexpr int XSLOW_TAKER_LOCKTIME_TARGET_SECONDS = 60*60; // 1 hour, must be less than maker locktime target
static constexpr int XSLOW_BLOCKTIME_SECONDS = 600;
static constexpr int XLOCKTIME_DRIFT_SECONDS = XTAKER_LOCKTIME_TARGET_SECONDS/2; // 15 minutes


//*****************************************************************************
//*****************************************************************************
class WalletParam
{
public:
    WalletParam()
        : txVersion(1)
        , COIN(0)
        , minTxFee(0)
        , feePerByte(0)
        , dustAmount(0)
        , blockTime(0)
        , blockSize(1024)
        , requiredConfirmations(0)
        , serviceNodeFee(.015)
        , txWithTimeField(false)
        , isLockCoinsSupported(false)
    {
        addrPrefix.resize(1, '\0');
        scriptPrefix.resize(1, '\0');
        secretPrefix.resize(1, '\0');
    }

    WalletParam & operator = (const WalletParam & other)
    {
        title                       = other.title;
        currency                    = other.currency;
        address                     = other.address;

        m_ip                        = other.m_ip;
        m_port                      = other.m_port;
        m_user                      = other.m_user;
        m_passwd                    = other.m_passwd;

        addrPrefix                  = other.addrPrefix;
        scriptPrefix                = other.scriptPrefix;
        secretPrefix                = other.secretPrefix;

        txVersion                   = other.txVersion;
        COIN                        = other.COIN;
        minTxFee                    = other.minTxFee;
        feePerByte                  = other.feePerByte;
        dustAmount                  = other.dustAmount;
        method                      = other.method;
        blockTime                   = other.blockTime;
        blockSize                   = other.blockSize;
        requiredConfirmations       = other.requiredConfirmations;
        txWithTimeField             = other.txWithTimeField;
        isLockCoinsSupported        = other.isLockCoinsSupported;
        jsonver                     = other.jsonver;
        contenttype                 = other.contenttype;
        cashAddrPrefix              = other.cashAddrPrefix;

        mediantime                  = other.mediantime; // useful for fork management

        return *this;
    }

// TODO temporary public
public:
    std::string                  title;
    std::string                  currency;

    std::string                  address;

    std::string                  m_ip;
    std::string                  m_port;
    std::string                  m_user;
    std::string                  m_passwd;

    std::string                  addrPrefix;
    std::string                  scriptPrefix;
    std::string                  secretPrefix;
    uint32_t                     txVersion;
    uint64_t                     COIN;
    uint64_t                     minTxFee;
    uint64_t                     feePerByte;
    uint64_t                     dustAmount;
    std::string                  method;

    // block time in seconds
    uint32_t                     blockTime;

    // block size in megabytes
    uint32_t                     blockSize;

    // required confirmations for tx
    uint32_t                     requiredConfirmations;

    //service node fee, see rpc::createFeeTransaction
    const double                 serviceNodeFee;

    // serialized transaction contains time field (default not)
    bool                         txWithTimeField;

    // support for lock/unlock coins (default off)
    bool                         isLockCoinsSupported;
    mutable CCriticalSection     lockedCoinsLocker;
    std::set<wallet::UtxoEntry>  lockedCoins;

    // json version for use with rpc
    std::string                  jsonver;
    // content type for rpc requests
    std::string                  contenttype;

    // median time
    int64_t                      mediantime{0};
    // cash address prefix
    std::string                  cashAddrPrefix;
};

} // namespace xbridge

#endif // BLOCKNET_XBRIDGE_XBRIDGEWALLET_H
