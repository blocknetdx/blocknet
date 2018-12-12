#ifndef XBRIDGETRANSACTIONDESCR
#define XBRIDGETRANSACTIONDESCR

// #include "uint256.h"
#include "base58.h"
#include "util/xutil.h"
#include "sync.h"
#include "xbridgedef.h"
#include "xbridgepacket.h"
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
    CCriticalSection _lock;

    enum
    {
        COIN = 1000000,
        MAX_COIN = 100000000
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

    uint32_t                   lockTime;
    uint32_t                   opponentLockTime;

    State                      state;
    uint32_t                   reason;

    boost::posix_time::ptime   created;
    boost::posix_time::ptime   txtime;

    uint256                    blockHash;

    // raw bitcoin transactions
    std::string                binTxId;
    uint32_t                   binTxVout;
    std::string                binTx;
    std::string                payTxId;
    std::string                payTx;
    std::string                refTxId;
    std::string                refTx;

    // counterparty info
    std::string                oBinTxId;
    uint32_t                   oBinTxVout{0};
    std::vector<unsigned char> oHashedSecret;
    std::string                oPayTxId;
    uint32_t                   oPayTxTries{0};

    // multisig address and redeem script
    std::string                lockP2SHAddress;
    std::vector<unsigned char> lockScript;

    std::string                unlockP2SHAddress;
    std::vector<unsigned char> unlockScript;

    // local created key (for exchange)
    std::vector<unsigned char>    mPubKey;
    std::vector<unsigned char>    mPrivKey;

    // other node pubkey (set when ... )
    std::vector<unsigned char>    oPubKey;

    // X key (secret data)
    std::vector<unsigned char>    xPubKey;
    std::vector<unsigned char>    xPrivKey;

    // service node pub key
    std::vector<unsigned char>    sPubKey;

    // used coins in transaction
    std::vector<xbridge::wallet::UtxoEntry> usedCoins;

    // pay tx verification watches
    uint32_t watchStartBlock{0};
    uint32_t watchCurrentBlock{0};
    bool     watching{false};
    bool     watchingDone{false};
    bool     redeemedCounterpartyDeposit{false};

    // keep track of excluded servicenodes (snodes can be excluded if they fail to post)
    std::set<CPubKey> _excludedSnodes;
    void excludeNode(CPubKey &key) {
        LOCK(_lock);
        _excludedSnodes.insert(key);
    }
    std::set<CPubKey> excludedNodes() {
        LOCK(_lock);
        return _excludedSnodes;
    }

    void setSecret(const std::vector<unsigned char> & secret) {
        LOCK(_lock);
        xPubKey = secret;
    }

    std::vector<unsigned char> secret() {
        LOCK(_lock);
        return xPubKey;
    }

    bool hasSecret() {
        LOCK(_lock);
        return xPubKey.size() == 33;
    }

    void setOtherPayTxId(const std::string & payTxId) {
        LOCK(_lock);
        oPayTxId = payTxId;
    }

    std::string otherPayTxId() {
        LOCK(_lock);
        return oPayTxId;
    }

    uint32_t otherPayTxTries() {
        LOCK(_lock);
        return oPayTxTries;
    }

    uint32_t maxOtherPayTxTries() {
        return 2;
    }

    void tryOtherPayTx() {
        LOCK(_lock);
        ++oPayTxTries;
    }

    void doneWatching() {
        LOCK(_lock);
        watchingDone = true;
    }

    bool isDoneWatching() {
        LOCK(_lock);
        return watchingDone;
    }

    void setWatchBlock(const uint32_t block) {
        LOCK(_lock);
        if (watchStartBlock == 0)
            watchStartBlock = block;
        watchCurrentBlock = block;
    }

    uint32_t getWatchStartBlock() {
        LOCK(_lock);
        return watchStartBlock;
    }

    uint32_t getWatchCurrentBlock() {
        LOCK(_lock);
        return watchCurrentBlock;
    }

    void setWatching(const bool flag) {
        LOCK(_lock);
        watching = flag;
    }

    bool isWatching() {
        LOCK(_lock);
        return watching;
    }

    bool hasRedeemedCounterpartyDeposit() {
        LOCK(_lock);
        return redeemedCounterpartyDeposit;
    }

    void counterpartyDepositRedeemed() {
        LOCK(_lock);
        redeemedCounterpartyDeposit = true;
    }

    TransactionDescr()
        : role(0)
        , lockTime(0)
        , opponentLockTime(0)
        , state(trNew)
        , reason(0)
        , created(boost::posix_time::microsec_clock::universal_time())
        , txtime(boost::posix_time::microsec_clock::universal_time())
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

    friend std::ostream & operator << (std::ostream & out, const TransactionDescrPtr & tx);

    TransactionDescr(const TransactionDescr & d)
    {
        state   = trNew;
        created = boost::posix_time::microsec_clock::universal_time();
        txtime  = boost::posix_time::microsec_clock::universal_time();

        copyFrom(d);
    }

    void updateTimestamp()
    {
        LOCK(_lock);
        txtime       = boost::posix_time::microsec_clock::universal_time();
    }

    void updateTimestamp(const TransactionDescr & d)
    {
        LOCK(_lock);
        txtime       = boost::posix_time::microsec_clock::universal_time();
        if (created > d.created)
        {
            created = d.created;
        }
    }

    bool isLocal() const
    {
        // must have from and to addresses
        return from.size() != 0 && to.size() != 0;
    }

    /**
     * Assigns the servicenode to the order.
     * @param snode Servicenode pubkey
     */
    void assignServicenode(CPubKey & snode) {
        LOCK(_lock);
        CKeyID snodeID = snode.GetID();
        std::copy(snodeID.begin(), snodeID.end(), hubAddress.begin());
        if (!snode.IsCompressed())
            snode.Compress();
        sPubKey = std::vector<unsigned char>(snode.begin(), snode.end());
    }

    std::string strState() const
    {
        switch (state)
        {
            case trExpired:        return std::string("expired");
            case trNew:            return std::string("new");
            case trOffline:        return std::string("offline");
            case trPending:        return std::string("open");
            case trAccepting:      return std::string("accepting");
            case trHold:           return std::string("hold");
            case trInitialized:    return std::string("initialized");
            case trCreated:        return std::string("created");
            case trSigned:         return std::string("signed");
            case trCommited:       return std::string("commited");
            case trFinished:       return std::string("finished");
            case trRollback:       return std::string("rolled back");
            case trRollbackFailed: return std::string("rollback failed");
            case trDropped:        return std::string("dropped");
            case trCancelled:      return std::string("canceled");
            case trInvalid:        return std::string("invalid");

            default:               return std::string("unknown");
        }
    }

private:
    void copyFrom(const TransactionDescr & d)
    {
        {
        LOCK(_lock);
        id                = d.id;
        role              = d.role;
        from              = d.from;
        fromCurrency      = d.fromCurrency;
        fromAmount        = d.fromAmount;
        to                = d.to;
        toCurrency        = d.toCurrency;
        toAmount          = d.toAmount;
        lockTime          = d.lockTime;
        opponentLockTime  = d.opponentLockTime;
        state             = d.state;
        reason            = d.reason;
        payTx             = d.payTx;
        refTx             = d.refTx;

        binTxId           = d.binTxId;
        binTxVout         = d.binTxVout;
        binTx             = d.binTx;
        payTxId           = d.payTxId;
        payTx             = d.payTx;
        refTxId           = d.refTxId;
        refTx             = d.refTx;

        oBinTxId          = d.oBinTxId;
        oBinTxVout        = d.oBinTxVout;
        oHashedSecret     = d.oHashedSecret;
        oPayTxId          = d.oPayTxId;
        oPayTxTries       = d.oPayTxTries;

        // multisig address and redeem script
        lockP2SHAddress   = d.lockP2SHAddress;
        lockScript        = d.lockScript;
        unlockP2SHAddress = d.unlockP2SHAddress;
        unlockScript      = d.unlockScript;

        // prevtxs for signrawtransaction
        // prevtxs      = d.prevtxs;

        // multisig key
        mPubKey           = d.mPubKey;
        mPrivKey          = d.mPrivKey;

        // X key
        xPubKey           = d.xPubKey;
        xPrivKey          = d.xPrivKey;

        hubAddress     = d.hubAddress;
        confirmAddress = d.confirmAddress;

        watchStartBlock   = d.watchStartBlock;
        watchCurrentBlock = d.watchCurrentBlock;
        watching          = d.watching;
        watchingDone      = d.watchingDone;
        redeemedCounterpartyDeposit = d.redeemedCounterpartyDeposit;
        }
        updateTimestamp(d);
    }
};

} // namespace xbridge

#endif // XBRIDGETRANSACTIONDESCR

