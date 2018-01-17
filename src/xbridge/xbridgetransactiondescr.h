#ifndef XBRIDGETRANSACTIONDESCR
#define XBRIDGETRANSACTIONDESCR

// #include "uint256.h"
#include "base58.h"
#include "xbridgepacket.h"
#include "xkey.h"
#include "xbitcoinsecret.h"
#include "xbridgewalletconnector.h"

#include <string>
#include <boost/cstdint.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

//******************************************************************************
//******************************************************************************
namespace xbridge
{

//******************************************************************************
//******************************************************************************
struct TransactionDescr
{
    enum
    {
        COIN = 1000000
    };

    enum State
    {
        trExpired = -1,
        trNew = 0,
        trOffline,
        trPending,
        trAccepting,
        trHold,
        trInitialized,
        trCreated,
        trSigned,
        trCommited,
        trFinished,
        trRollback,
        trRollbackFailed,
        trDropped,
        trCancelled,
        trInvalid
    };

    uint256                    id;

    char                       role;

    std::vector<unsigned char> hubAddress;
    std::vector<unsigned char> confirmAddress;

    std::vector<unsigned char> from;
    std::string                fromCurrency;
    uint64_t                   fromAmount;
    std::vector<unsigned char> to;
    std::string                toCurrency;
    uint64_t                   toAmount;

    uint32_t                   lockTimeTx1;
    uint32_t                   lockTimeTx2;

    State                      state;
    uint32_t                   reason;

    boost::posix_time::ptime   created;
    boost::posix_time::ptime   txtime;

    // raw bitcoin transactions
    std::string                binTxId;
    std::string                binTx;
    std::string                payTxId;
    std::string                payTx;
    std::string                refTxId;
    std::string                refTx;

    // multisig address and redeem script
    std::string                depositP2SH;
    std::vector<unsigned char> innerScript;

    // prevtxs for signrawtransaction
    // std::string                prevtxs;

    XBridgePacketPtr           packet;

    // local created key (for exchange)
    std::vector<unsigned char>    mPubKey;
    std::vector<unsigned char>    mPrivKey;

    // X key (secret data)
    std::vector<unsigned char>    xPubKey;
    std::vector<unsigned char>    xPrivKey;

    // used coins in transaction
    std::vector<xbridge::wallet::UtxoEntry> usedCoins;

    TransactionDescr()
        : role(0)
        , lockTimeTx1(0)
        , lockTimeTx2(0)
        , state(trNew)
        , reason(0)
        , created(boost::posix_time::second_clock::universal_time())
        , txtime(boost::posix_time::second_clock::universal_time())
    {}

//    bool operator == (const XBridgeTransactionDescr & d) const
//    {
//        return id == d.id;
//    }

    bool operator < (const TransactionDescr & d) const
    {
        return created < d.created;
    }

    bool operator > (const TransactionDescr & d) const
    {
        return created > d.created;
    }

    TransactionDescr & operator = (const TransactionDescr & d)
    {
        if (this == &d)
        {
            return *this;
        }

        copyFrom(d);

        return *this;
    }

    TransactionDescr(const TransactionDescr & d)
    {
        state   = trNew;
        created = boost::posix_time::second_clock::universal_time();
        txtime  = boost::posix_time::second_clock::universal_time();

        copyFrom(d);
    }

    void updateTimestamp()
    {
        txtime       = boost::posix_time::second_clock::universal_time();
    }

    void updateTimestamp(const TransactionDescr & d)
    {
        txtime       = boost::posix_time::second_clock::universal_time();
        if (created > d.created)
        {
            created = d.created;
        }
    }

    bool isLocal() const
    {
        return from.size() != 0 && to.size() != 0;
    }

    std::string strState() const
    {
        switch (state)
        {
            case trInvalid:   return std::string("Invalid");
            case trNew:       return std::string("New");
            case trPending:   return std::string("Open");
            case trAccepting: return std::string("Accepting");
            case trHold:      return std::string("Hold");
            case trCreated:   return std::string("Created");
            case trSigned:    return std::string("Signed");
            case trCommited:  return std::string("Commited");
            case trFinished:  return std::string("Finished");
            case trCancelled: return std::string("Cancelled");
            case trRollback:  return std::string("Rolled Back");
            case trDropped:   return std::string("Dropped");
            case trExpired:   return std::string("Expired");
            case trOffline:   return std::string("Offline");
            default:          return std::string("Unknown");
        }
    }

private:
    void copyFrom(const TransactionDescr & d)
    {
        id           = d.id;
        role         = d.role;
        from         = d.from;
        fromCurrency = d.fromCurrency;
        fromAmount   = d.fromAmount;
        to           = d.to;
        toCurrency   = d.toCurrency;
        toAmount     = d.toAmount;
        lockTimeTx1  = d.lockTimeTx1;
        lockTimeTx2  = d.lockTimeTx2;
        state        = d.state;
        reason       = d.reason;
        payTx        = d.payTx;
        refTx        = d.refTx;

        binTxId      = d.binTxId;
        binTx        = d.binTx;
        payTxId      = d.payTxId;
        payTx        = d.payTx;
        refTxId      = d.refTxId;
        refTx        = d.refTx;

        // multisig address and redeem script
        depositP2SH     = d.depositP2SH;
        innerScript       = d.innerScript;

        // prevtxs for signrawtransaction
        // prevtxs      = d.prevtxs;

        // multisig key
        mPubKey      = d.mPubKey;
        mPrivKey      = d.mPrivKey;

        // X key
        xPubKey      = d.xPubKey;
        xPrivKey      = d.xPrivKey;

        hubAddress     = d.hubAddress;
        confirmAddress = d.confirmAddress;

        updateTimestamp(d);
    }
};

} // namespace xbridge

#endif // XBRIDGETRANSACTIONDESCR

