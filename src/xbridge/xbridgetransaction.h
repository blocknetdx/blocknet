//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGETRANSACTION_H
#define XBRIDGETRANSACTION_H

#include "uint256.h"
#include "xbridgetransactionmember.h"
#include "xbridgedef.h"
#include "sync.h"

#include <vector>
#include <string>

#include <boost/cstdint.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/date_time/posix_time/ptime.hpp>

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
                const std::vector<unsigned char> & mpubkey);

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
     * @brief createdTime
     * @return time of creation transaction
     */
    boost::posix_time::ptime createdTime() const;

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

    // uint256                    firstId() const;
    std::vector<unsigned char> a_address() const;
    std::vector<unsigned char> a_destination() const;
    std::string                a_currency() const;
    uint64_t                   a_amount() const;
    std::string                a_payTx() const;
    std::string                a_refTx() const { return m_a.refTx(); }
    std::string                a_bintxid() const;
    uint32_t                   a_lockTime() const;
    std::string                a_payTxId() const;
    bool                       a_refunded() const { return m_a_refunded; }

    std::vector<unsigned char> a_pk1() const;

    // uint256                    secondId() const;
    std::vector<unsigned char> b_address() const;
    std::vector<unsigned char> b_destination() const;
    std::string                b_currency() const;
    uint64_t                   b_amount() const;
    std::string                b_payTx() const;
    std::string                b_refTx() const { return m_b.refTx(); }
    std::string                b_bintxid() const;
    uint32_t                   b_lockTime() const;
    std::string                b_payTxId() const;
    bool                       b_refunded() const { return m_b_refunded; }

    std::vector<unsigned char> b_pk1() const;

    bool tryJoin(const TransactionPtr other);

    bool                       setKeys(const std::vector<unsigned char> & addr,
                                       const std::vector<unsigned char> & pk);
    bool                       setBinTxId(const std::vector<unsigned char> &addr,
                                          const std::string & id);

    void a_setRefunded(const bool refunded) { m_a_refunded = refunded; }
    void b_setRefunded(const bool refunded) { m_b_refunded = refunded; }

    void a_setLockTime(const uint32_t lockTime) { m_a.setLockTime(lockTime); }
    void b_setLockTime(const uint32_t lockTime) { m_b.setLockTime(lockTime); }

    void a_setPayTxId(const std::string & payTxId) { m_a.setPayTxId(payTxId); }
    void b_setPayTxId(const std::string & payTxId) { m_b.setPayTxId(payTxId); }

    void a_setRefundTx(const std::string & refTxId, const std::string & refTx) {
        m_a.setRefTxId(refTxId); m_a.setRefTx(refTx);
    }
    void b_setRefundTx(const std::string & refTxId, const std::string & refTx) {
        m_b.setRefTxId(refTxId); m_b.setRefTx(refTx);
    }

    friend std::ostream & operator << (std::ostream & out, const TransactionPtr & tx);

public:
    CCriticalSection           m_lock;

private:
    uint256                    m_id;

    boost::posix_time::ptime   m_created;
    boost::posix_time::ptime   m_last;

    uint256                    m_blockHash; //hash of block when transaction created

    State                      m_state;

    bool                       m_a_stateChanged;
    bool                       m_b_stateChanged;
    bool                       m_a_refunded{false};
    bool                       m_b_refunded{false};

    unsigned int               m_confirmationCounter;

    std::string                m_sourceCurrency;
    std::string                m_destCurrency;

    uint64_t                   m_sourceAmount;
    uint64_t                   m_destAmount;

    std::string                m_bintxid1;
    std::string                m_bintxid2;

    XBridgeTransactionMember   m_a;
    XBridgeTransactionMember   m_b;
};

} // namespace xbridge

#endif // XBRIDGETRANSACTION_H
