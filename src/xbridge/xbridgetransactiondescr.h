// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_XBRIDGE_XBRIDGETRANSACTIONDESCR_H
#define BLOCKNET_XBRIDGE_XBRIDGETRANSACTIONDESCR_H

#include <xbridge/xbridgedef.h>
#include <xbridge/xbridgepacket.h>
#include <xbridge/xbridgewalletconnector.h>

#include <base58.h>
#include <pubkey.h>
#include <sync.h>

#include <string>

#include <boost/cstdint.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

//******************************************************************************
//******************************************************************************
namespace xbridge
{

boost::uint64_t timeToInt(const boost::posix_time::ptime &time);
boost::posix_time::ptime intToTime(const uint64_t& number);

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

    static const int CURRENT_VERSION=1;
    int nVersion{CURRENT_VERSION};

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream & s, Operation ser_action) {
        LOCK(_lock);
        READWRITE(nVersion);
        READWRITE(id);
        READWRITE(role);
        READWRITE(hubAddress);
        READWRITE(confirmAddress);
        READWRITE(from);
        READWRITE(fromCurrency);
        READWRITE(fromAmount);
        READWRITE(fromAddr);
        READWRITE(to);
        READWRITE(toCurrency);
        READWRITE(toAmount);
        READWRITE(toAddr);
        READWRITE(origFromCurrency);
        READWRITE(origFromAmount);
        READWRITE(origToCurrency);
        READWRITE(origToAmount);
        READWRITE(lockTime);
        READWRITE(opponentLockTime);
        if (ser_action.ForRead()) {
            int stateT;
            int reasonT;
            READWRITE(stateT);
            READWRITE(reasonT);
            state = static_cast<State>(stateT);
            reason = static_cast<TxCancelReason>(stateT);
            uint64_t createdT;
            uint64_t txtimeT;
            READWRITE(createdT);
            READWRITE(txtimeT);
            created = intToTime(createdT);
            txtime = intToTime(txtimeT);
        } else {
            READWRITE(static_cast<int>(state));
            READWRITE(static_cast<int>(reason));
            uint64_t createdT = timeToInt(created);
            uint64_t txtimeT = timeToInt(txtime);
            READWRITE(createdT);
            READWRITE(txtimeT);
        }
        READWRITE(blockHash);
        READWRITE(binTxId);
        READWRITE(binTxVout);
        READWRITE(binTx);
        READWRITE(payTxId);
        READWRITE(payTx);
        READWRITE(refTxId);
        READWRITE(refTx);
        READWRITE(oBinTxId);
        READWRITE(oBinTxVout);
        READWRITE(oBinTxP2SHAmount);
        READWRITE(oHashedSecret);
        READWRITE(oPayTxId);
        READWRITE(oPayTxTries);
        READWRITE(oOverpayment);
        READWRITE(lockP2SHAddress);
        READWRITE(lockScript);
        READWRITE(unlockP2SHAddress);
        READWRITE(unlockScript);
        READWRITE(mPubKey);
        READWRITE(mPrivKey);
        READWRITE(oPubKey);
        READWRITE(xPubKey);
        READWRITE(xPrivKey);
        READWRITE(sPubKey);
        READWRITE(usedCoins);
        READWRITE(feeUtxos);
        READWRITE(rawFeeTx);
        READWRITE(watchStartBlock);
        READWRITE(watchCurrentBlock);
        READWRITE(watching);
        READWRITE(watchingForSpentDeposit);
        READWRITE(watchingDone);
        READWRITE(redeemedCounterpartyDeposit);
        READWRITE(depositSent);
        READWRITE(repostCoins);
        READWRITE(partialOrderPrepTx);
        READWRITE(partialOrdersAllowed);
        READWRITE(partialOrderPending);
        READWRITE(repostOrder);
        READWRITE(minFromAmount);
        READWRITE(historical);
        READWRITE(logPayTx1);
        READWRITE(logPayTx2);
        READWRITE(parentOrder);
    }

    void SetNull() {
        LOCK(_lock);
        id.SetNull();
        role = 0;
        hubAddress.clear();
        confirmAddress.clear();
        from.clear();
        fromCurrency.clear();
        fromAmount = 0;
        fromAddr.clear();
        to.clear();
        toCurrency.clear();
        toAmount = 0;
        toAddr.clear();
        origFromCurrency.clear();
        origFromAmount = 0;
        origToCurrency.clear();
        origToAmount = 0;
        lockTime = 0;
        opponentLockTime = 0;
        state = trNew;
        reason = crUnknown;
        created = intToTime(0);
        txtime = intToTime(0);
        blockHash.SetNull();
        binTxId.clear();
        binTxVout = 0;
        binTx.clear();
        payTxId.clear();
        payTx.clear();
        refTxId.clear();
        refTx.clear();
        oBinTxId.clear();
        oBinTxVout = 0;
        oBinTxP2SHAmount = 0;
        oHashedSecret.clear();
        oPayTxId.clear();
        oPayTxTries = 0;
        oOverpayment = 0;
        lockP2SHAddress.clear();
        lockScript.clear();
        unlockP2SHAddress.clear();
        unlockScript.clear();
        mPubKey.clear();
        mPrivKey.clear();
        oPubKey.clear();
        xPubKey.clear();
        xPrivKey.clear();
        sPubKey.clear();
        usedCoins.clear();
        feeUtxos.clear();
        rawFeeTx.clear();
        watchStartBlock = 0;
        watchCurrentBlock = 0;
        watching = false;
        watchingForSpentDeposit = false;
        watchingDone = false;
        redeemedCounterpartyDeposit = false;
        depositSent = false;
        repostCoins.clear();
        partialOrderPrepTx.clear();
        partialOrdersAllowed = false;
        partialOrderPending = false;
        repostOrder = false;
        minFromAmount = 0;
        historical = false;
        logPayTx1 = false;
        logPayTx2 = false;
        parentOrder.SetNull();
    }

    uint256                    id;

    char                       role;

    std::vector<unsigned char> hubAddress;
    std::vector<unsigned char> confirmAddress;

    std::vector<unsigned char> from;
    std::string                fromCurrency;
    uint64_t                   fromAmount;
    std::string                fromAddr;
    std::vector<unsigned char> to;
    std::string                toCurrency;
    uint64_t                   toAmount;
    std::string                toAddr;

    std::string                origFromCurrency;
    uint64_t                   origFromAmount{0};
    std::string                origToCurrency;
    uint64_t                   origToAmount{0};

    uint32_t                   lockTime;
    uint32_t                   opponentLockTime;

    State                      state{trNew};
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
    uint64_t                   oBinTxP2SHAmount{0};
    std::vector<unsigned char> oHashedSecret;
    std::string                oPayTxId;
    uint32_t                   oPayTxTries{0};
    double                     oOverpayment{0};

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
    std::set<xbridge::wallet::UtxoEntry> feeUtxos;
    std::string rawFeeTx;

    // pay tx verification watches
    uint32_t watchStartBlock{0};
    uint32_t watchCurrentBlock{0};
    bool     watching{false};
    bool     watchingForSpentDeposit{false};
    bool     watchingDone{false};
    bool     redeemedCounterpartyDeposit{false};
    bool     depositSent{false};

    // is partial order allowed
    std::vector<xbridge::wallet::UtxoEntry> repostCoins;
    std::string partialOrderPrepTx;
    bool     partialOrdersAllowed{false};
    bool     partialOrderPending{false};
    // repost partial order after completion
    bool     repostOrder{false};
    // partial order amounts
    uint64_t minFromAmount{0};

    // Track if tx is historical tx
    bool historical{false};

    // Track whether pay tx has been logged
    bool logPayTx1{false};
    bool logPayTx2{false};

    uint256 parentOrder; // Parent order id of a partial order

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

    std::string orderType() {
        if (isPartialOrderAllowed())
            return "partial";
        return "exact";
    }

    void allowPartialOrders() {
        LOCK(_lock);
        partialOrdersAllowed = true;
    }

    bool isPartialOrderAllowed() {
        LOCK(_lock);
        return partialOrdersAllowed;
    }

    bool isPartialRepost() {
        LOCK(_lock);
        return repostOrder;
    }

    void setPartialOrderPending(const bool flag) {
        LOCK(_lock);
        partialOrderPending = flag;
    }

    bool isPartialOrderPending() {
        LOCK(_lock);
        return partialOrderPending;
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

    void setWatchingForSpentDeposit(const bool flag) {
        LOCK(_lock);
        watchingForSpentDeposit = flag;
    }

    bool isWatchingForSpentDeposit() {
        LOCK(_lock);
        return watchingForSpentDeposit;
    }
    
    void sentDeposit() {
        LOCK(_lock);
        depositSent = true;
    }
    
    void failDeposit() {
        LOCK(_lock);
        depositSent = false;
    }
    
    bool didSendDeposit() {
        LOCK(_lock);
        return depositSent;
    }

    bool hasRedeemedCounterpartyDeposit() {
        LOCK(_lock);
        return redeemedCounterpartyDeposit;
    }

    void counterpartyDepositRedeemed() {
        LOCK(_lock);
        redeemedCounterpartyDeposit = true;
    }

    void moveToHistory() {
        LOCK(_lock);
        historical = true;
    }

    bool isHistorical() {
        LOCK(_lock);
        return historical;
    }

    void setLogPayTx1() {
        LOCK(_lock);
        logPayTx1 = true;
    }
    bool didLogPayTx1() {
        LOCK(_lock);
        return logPayTx1;
    }
    void setLogPayTx2() {
        LOCK(_lock);
        logPayTx2 = true;
    }
    bool didLogPayTx2() {
        LOCK(_lock);
        return logPayTx2;
    }

    void setUpdateTime(const boost::posix_time::ptime t) {
        LOCK(_lock);
        txtime = t;
    }

    void setParentOrder(const uint256 orderid) {
        LOCK(_lock);
        parentOrder = orderid;
    }

    uint256 getParentOrder() {
        LOCK(_lock);
        return parentOrder;
    }

    std::set<wallet::UtxoEntry> utxos() {
        LOCK(_lock);
        return {usedCoins.begin(), usedCoins.end()};
    }

    std::set<wallet::UtxoEntry> origUtxos() {
        LOCK(_lock);
        std::set<wallet::UtxoEntry> r{usedCoins.begin(), usedCoins.end()};
        r.insert(repostCoins.begin(), repostCoins.end());
        return std::move(r);
    }

    int utxoCount() {
        LOCK(_lock);
        return usedCoins.size() + repostCoins.size();
    }

    const std::string refundAddress() {
        // Find the largest utxo to use as redeem if from address is empty
        if (fromAddr.empty()) {
            if (usedCoins.empty())
                return ""; // empty if nothing found
            std::vector<wallet::UtxoEntry> utxos = usedCoins;
            // sort descending and pick first
            sort(utxos.begin(), utxos.end(),
                 [](const wallet::UtxoEntry & a, const wallet::UtxoEntry & b) {
                     return a.amount > b.amount;
                 });
            return utxos[0].address;
        }
        return fromAddr;
    }

    void clearUsedCoins() {
        LOCK(_lock);
        usedCoins.clear();
        feeUtxos.clear();
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
    void copyFrom(const TransactionDescr & d) {
        LOCK(_lock);
        nVersion                     = d.nVersion;
        id                           = d.id;
        role                         = d.role;
        hubAddress                   = d.hubAddress;
        confirmAddress               = d.confirmAddress;
        from                         = d.from;
        fromCurrency                 = d.fromCurrency;
        fromAmount                   = d.fromAmount;
        fromAddr                     = d.fromAddr;
        to                           = d.to;
        toCurrency                   = d.toCurrency;
        toAmount                     = d.toAmount;
        toAddr                       = d.toAddr;
        origFromCurrency             = d.origFromCurrency;
        origFromAmount               = d.origFromAmount;
        origToCurrency               = d.origToCurrency;
        origToAmount                 = d.origToAmount;
        lockTime                     = d.lockTime;
        opponentLockTime             = d.opponentLockTime;
        state                        = d.state;
        reason                       = d.reason;
        created                      = d.created;
        txtime                       = d.txtime;
        blockHash                    = d.blockHash;
        binTxId                      = d.binTxId;
        binTxVout                    = d.binTxVout;
        binTx                        = d.binTx;
        payTxId                      = d.payTxId;
        payTx                        = d.payTx;
        refTxId                      = d.refTxId;
        refTx                        = d.refTx;
        oBinTxId                     = d.oBinTxId;
        oBinTxVout                   = d.oBinTxVout;
        oBinTxP2SHAmount             = d.oBinTxP2SHAmount;
        oHashedSecret                = d.oHashedSecret;
        oPayTxId                     = d.oPayTxId;
        oPayTxTries                  = d.oPayTxTries;
        oOverpayment                 = d.oOverpayment;
        lockP2SHAddress              = d.lockP2SHAddress;
        lockScript                   = d.lockScript;
        unlockP2SHAddress            = d.unlockP2SHAddress;
        unlockScript                 = d.unlockScript;
        mPubKey                      = d.mPubKey;
        mPrivKey                     = d.mPrivKey;
        oPubKey                      = d.oPubKey;
        xPubKey                      = d.xPubKey;
        xPrivKey                     = d.xPrivKey;
        sPubKey                      = d.sPubKey;
        usedCoins                    = d.usedCoins;
        feeUtxos                     = d.feeUtxos;
        rawFeeTx                     = d.rawFeeTx;
        watchStartBlock              = d.watchStartBlock;
        watchCurrentBlock            = d.watchCurrentBlock;
        watching                     = d.watching;
        watchingForSpentDeposit      = d.watchingForSpentDeposit;
        watchingDone                 = d.watchingDone;
        redeemedCounterpartyDeposit  = d.redeemedCounterpartyDeposit;
        depositSent                  = d.depositSent;
        repostCoins                  = d.repostCoins;
        partialOrderPrepTx           = d.partialOrderPrepTx;
        partialOrdersAllowed         = d.partialOrdersAllowed;
        partialOrderPending          = d.partialOrderPending;
        repostOrder                  = d.repostOrder;
        minFromAmount                = d.minFromAmount;
        historical                   = d.historical;
        logPayTx1                    = d.logPayTx1;
        logPayTx2                    = d.logPayTx2;
        parentOrder                  = d.parentOrder;
    }
};

} // namespace xbridge

#endif // BLOCKNET_XBRIDGE_XBRIDGETRANSACTIONDESCR_H

