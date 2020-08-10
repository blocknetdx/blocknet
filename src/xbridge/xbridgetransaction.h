// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//*****************************************************************************
//*****************************************************************************

#ifndef BLOCKNET_XBRIDGE_XBRIDGETRANSACTION_H
#define BLOCKNET_XBRIDGE_XBRIDGETRANSACTION_H

#include <xbridge/xbridgedef.h>
#include <xbridge/xbridgetransactionmember.h>

#include <uint256.h>
#include <sync.h>

#include <vector>
#include <string>

#include <boost/cstdint.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>

//******************************************************************************
//******************************************************************************
namespace xbridge
{

//*****************************************************************************
//*****************************************************************************
class Transaction
{
public:
    // see strState when editing
    enum State
    {
        trInvalid = 0,
        trNew,
        trJoined,
        trHold,
        trInitialized,
        trCreated,
        trSigned,
        trCommited,

        trFinished,
        trCancelled,
        trDropped
    };

    enum
    {
        // transaction lock time base, in seconds, 60 sec * 10 min
        lockTime = 60 * 10,

        // pending transaction ttl in seconds, 6 min from last update
        pendingTTL = 60 * 6,

        // transaction ttl in seconds, 60 sec * 60 min
        TTL = 60 * 60,

        // order deadline ttl in seconds, 60 sec * 60 min * 24 hours * 7 days
        deadlineTTL = 60 * 60 * 24 * 7,

        // number of blocks ttl, 1440 blocks per day * 7 days
        blocksTTL = 1440 * 7
    };

public:
    Transaction();

    Transaction(const uint256                    & id,
                const std::vector<unsigned char> & sourceAddr,
                const std::string                & sourceCurrency,
                const uint64_t                   & sourceAmount,
                const std::vector<unsigned char> & destAddr,
                const std::string                & destCurrency,
                const uint64_t                   & destAmount,
                const uint64_t                   & created,
                const uint256                    & blockHash,
                const std::vector<unsigned char> & mpubkey,
                bool                               partialAllowed,
                uint64_t                           minFromAmount);

    ~Transaction();

    uint256 id() const;

    uint256 blockHash() const;

    //
    /**
     * @brief state
     * @return state of transaction
     */
    State state() const;
    //
    /**
     * @brief increaseStateCounter update state counter and update state
     * @param state
     * @param from
     * @return
     */
    State increaseStateCounter(const State state, const std::vector<unsigned char> & from);

    /**
     * @brief strState
     * @param state - transaction state
     * @return string name of state
     */
    static std::string strState(const State state);
    /**
     * @brief strState
     * @return  string name of state
     */
    std::string strState() const;

    /**
     * @brief updateTimestamp - update transaction time
     */
    void updateTimestamp();

    /**
     * @brief updateTooSoon - Returns true if the order update ping is too soon.
     * @return true if update is too soon
     */
    bool updateTooSoon();

    /**
     * @brief createdTime
     * @return time of creation transaction
     */
    boost::posix_time::ptime createdTime() const;

    /**
     * @brief lastUtxoCheckTime
     * @return time of the last check on maker utxos.
     */
    boost::posix_time::ptime utxoCheckTime() const {
        LOCK(m_lock);
        return m_lastUtxoCheck;
    }

    /**
     * @brief lastUtxoCheckTime
     * @return Set the time of the last check on maker utxos.
     */
    void updateUtxoCheckTime(const boost::posix_time::ptime & t) {
        LOCK(m_lock);
        m_lastUtxoCheck = t;
    }

    /**
     * @brief isFinished
     * @return true if transaction finished, canclelled or dropped
     */
    bool isFinished() const;
    /**
     * @brief isValid
     * @return true, if transaction not invalid
     */
    bool isValid() const;
    
    /**
     * @brief matches
     * @param id
     * @return true if the specified id matches
     */
    bool matches(uint256 & id) const;
    
    /**
     * @brief isExpired check time of last transaction update
     * @return true, if la
     */
    bool isExpired() const;
    bool isExpiredByBlockNumber() const;

    /**
     * @brief cancel - set transaction state to trCancelled
     */
    void cancel();
    /**
     * @brief drop - set transaction state to trDropped
     */
    void drop();
    /**
     * @brief finish - set transaction state to finished
     */
    void finish();

    /**
     * @brief Set the accepting state.
     */
    void setAccepting(bool flag) { LOCK(m_lock); m_accepting = flag; }
    /**
     * @brief Return the accepting state.
     * @return
     */
    bool accepting() { LOCK(m_lock); return m_accepting; }

    // uint256                    firstId() const;
    std::vector<unsigned char> a_address() const;
    std::vector<unsigned char> a_destination() const;
    std::string                a_currency() const;
    uint64_t                   a_amount() const;
    std::string                a_payTx() const;
    std::string                a_refTx() const { LOCK(m_lock); return m_a.refTx(); }
    std::string                a_bintxid() const;
    uint32_t                   a_lockTime() const;
    std::string                a_payTxId() const;
    bool                       a_refunded() const { LOCK(m_lock); return m_a_refunded; }
    const std::vector<wallet::UtxoEntry> a_utxos() const { LOCK(m_lock); return m_a.utxos(); }
    uint64_t                   a_initial_amount() const { LOCK(m_lock); return m_sourceInitialAmount; }

    std::vector<unsigned char> a_pk1() const;

    // uint256                    secondId() const;
    std::vector<unsigned char> b_address() const;
    std::vector<unsigned char> b_destination() const;
    std::string                b_currency() const;
    uint64_t                   b_amount() const;
    std::string                b_payTx() const;
    std::string                b_refTx() const { LOCK(m_lock); return m_b.refTx(); }
    std::string                b_bintxid() const;
    uint32_t                   b_lockTime() const;
    std::string                b_payTxId() const;
    bool                       b_refunded() const { LOCK(m_lock); return m_b_refunded; }
    const std::vector<wallet::UtxoEntry> b_utxos() const { LOCK(m_lock); return m_b.utxos(); }
    uint64_t                   b_initial_amount() const { LOCK(m_lock); return m_destInitialAmount; }

    uint64_t                   min_partial_amount() const { LOCK(m_lock); return m_minPartialAmount; }

    std::vector<unsigned char> b_pk1() const;

    bool tryJoin(const TransactionPtr other);

    bool                       setKeys(const std::vector<unsigned char> & addr,
                                       const std::vector<unsigned char> & pk);
    bool                       setBinTxId(const std::vector<unsigned char> &addr,
                                          const std::string & id);

    void a_setRefunded(const bool refunded) { LOCK(m_lock); m_a_refunded = refunded; }
    void b_setRefunded(const bool refunded) { LOCK(m_lock); m_b_refunded = refunded; }

    void a_setUtxos(const std::vector<wallet::UtxoEntry> & utxos) { LOCK(m_lock); m_a.setUtxos(utxos); }
    void b_setUtxos(const std::vector<wallet::UtxoEntry> & utxos) { LOCK(m_lock); m_b.setUtxos(utxos); }

    void a_setLockTime(const uint32_t lockTime) { LOCK(m_lock); m_a.setLockTime(lockTime); }
    void b_setLockTime(const uint32_t lockTime) { LOCK(m_lock); m_b.setLockTime(lockTime); }

    void a_setPayTxId(const std::string & payTxId) { LOCK(m_lock); m_a.setPayTxId(payTxId); }
    void b_setPayTxId(const std::string & payTxId) { LOCK(m_lock); m_b.setPayTxId(payTxId); }

    void a_setRefundTx(const std::string & refTxId, const std::string & refTx) {
        LOCK(m_lock);
        m_a.setRefTxId(refTxId); m_a.setRefTx(refTx);
    }
    void b_setRefundTx(const std::string & refTxId, const std::string & refTx) {
        LOCK(m_lock);
        m_b.setRefTxId(refTxId); m_b.setRefTx(refTx);
    }

    bool orderType();

    /**
     * Joins the partial amounts with the maker's order. Assumes validation on these
     * amounts has already taken place.
     * @param takerPartialSource
     * @param takerPartialDest
     */
    void joinPartialAmounts(const uint64_t takerPartialSource, const uint64_t takerPartialDest) {
        LOCK(m_lock);
        m_sourceAmount = takerPartialDest; // maker matches taker's size
        m_destAmount = takerPartialSource; // maker matches taker's size
    }

    /**
     * @brief isPartialAllowed
     * @return true, if partial txs are allowed
     */
    bool isPartialAllowed();

    friend std::ostream & operator << (std::ostream & out, const TransactionPtr & tx);

public:
    mutable CCriticalSection   m_lock;

private:
    uint256                    m_id;

    boost::posix_time::ptime   m_created;
    boost::posix_time::ptime   m_last;
    boost::posix_time::ptime   m_lastUtxoCheck;

    uint256                    m_blockHash; //hash of block when transaction created

    State                      m_state;
    bool                       m_accepting{false};

    bool                       m_a_stateChanged;
    bool                       m_b_stateChanged;
    bool                       m_a_refunded{false};
    bool                       m_b_refunded{false};

    bool                       m_partialAllowed{false};

    unsigned int               m_confirmationCounter;

    std::string                m_sourceCurrency;
    std::string                m_destCurrency;

    uint64_t                   m_sourceAmount;
    uint64_t                   m_destAmount;
    uint64_t                   m_sourceInitialAmount;
    uint64_t                   m_destInitialAmount;

    uint64_t                   m_minPartialAmount;

    std::string                m_bintxid1;
    std::string                m_bintxid2;

    XBridgeTransactionMember   m_a;
    XBridgeTransactionMember   m_b;
};

} // namespace xbridge

#endif // BLOCKNET_XBRIDGE_XBRIDGETRANSACTION_H
