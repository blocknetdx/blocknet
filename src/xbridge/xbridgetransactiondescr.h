#ifndef XBRIDGETRANSACTIONDESCR
#define XBRIDGETRANSACTIONDESCR

// #include "uint256.h"
#include "base58.h"
#include "xbridgepacket.h"
#include "xkey.h"
#include "xbitcoinsecret.h"

#include <string>
#include <boost/cstdint.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

//*****************************************************************************
//*****************************************************************************
struct XBridgeTransactionDescr;
typedef boost::shared_ptr<XBridgeTransactionDescr> XBridgeTransactionDescrPtr;

//******************************************************************************
//******************************************************************************
struct XBridgeTransactionDescr
{
    enum
    {
        MIN_TX_FEE = 100,
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
        trCreated,
        trSigned,
        trCommited,
        trFinished,
        trRollback,
        trDropped,
        trCancelled,
        trInvalid
    };

    uint256                    id;

    char                       role;

    std::vector<unsigned char> hubAddress;
    std::vector<unsigned char> confirmAddress;

    std::string                from;
    std::string                fromCurrency;
    uint64_t                   fromAmount;
    std::string                to;
    std::string                toCurrency;
    uint64_t                   toAmount;

    uint32_t                   tax;

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
    std::string                multisig;
    std::string                innerScript;

    // prevtxs for signrawtransaction
    // std::string                prevtxs;

    XBridgePacketPtr           packet;

    // multisig key
    xbridge::CPubKey           mPubKey;
    xbridge::CBitcoinSecret    mSecret;

    // X key
    xbridge::CPubKey           xPubKey;
    xbridge::CBitcoinSecret    xSecret;

    XBridgeTransactionDescr()
        : role(0)
        , tax(0)
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

    bool operator < (const XBridgeTransactionDescr & d) const
    {
        return created < d.created;
    }

    bool operator > (const XBridgeTransactionDescr & d) const
    {
        return created > d.created;
    }

    XBridgeTransactionDescr & operator = (const XBridgeTransactionDescr & d)
    {
        if (this == &d)
        {
            return *this;
        }

        copyFrom(d);

        return *this;
    }

    XBridgeTransactionDescr(const XBridgeTransactionDescr & d)
    {
        state   = trNew;
        created = boost::posix_time::second_clock::universal_time();
        txtime  = boost::posix_time::second_clock::universal_time();

        copyFrom(d);
    }

    void updateTimestamp(const XBridgeTransactionDescr & d)
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
    void copyFrom(const XBridgeTransactionDescr & d)
    {
        id           = d.id;
        role         = d.role;
        from         = d.from;
        fromCurrency = d.fromCurrency;
        fromAmount   = d.fromAmount;
        to           = d.to;
        toCurrency   = d.toCurrency;
        toAmount     = d.toAmount;
        tax          = d.tax;
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
        multisig     = d.multisig;
        innerScript       = d.innerScript;

        // prevtxs for signrawtransaction
        // prevtxs      = d.prevtxs;

        // multisig key
        mPubKey      = d.mPubKey;
        mSecret      = d.mSecret;

        // X key
        xPubKey      = d.xPubKey;
        xSecret      = d.xSecret;

        hubAddress     = d.hubAddress;
        confirmAddress = d.confirmAddress;

        updateTimestamp(d);
    }
};

#endif // XBRIDGETRANSACTIONDESCR

