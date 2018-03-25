//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGETRANSACTION_H
#define XBRIDGETRANSACTION_H

#include "uint256.h"
#include "xbridgetransactionmember.h"
#include "xbridgedef.h"

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

        // order deadline ttl in seconds, 60 sec * 60 min * 24h
        deadlineTTL = 60 * 60 * 24,

        // number of blocks ttl
        blocksTTL = 1440
    };

public:
    /**
     * @brief Transaction
     */
    Transaction();
    /**
     * @brief Transaction
     * @param id
     * @param sourceAddr
     * @param sourceCurrency
     * @param sourceAmount
     * @param destAddr
     * @param destCurrency
     * @param destAmount
     * @param created
     * @param blockHash
     * @param mpubkey
     */
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

    /**
     * @brief id
     * @return
     */
    uint256 id() const;

    /**
     * @brief blockHash
     * @return
     */
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
    /**
     * @brief a_address
     * @return
     */
    std::vector<unsigned char> a_address() const;
    /**
     * @brief a_destination
     * @return
     */
    std::vector<unsigned char> a_destination() const;
    /**
     * @brief a_currency
     * @return
     */
    std::string a_currency() const;
    /**
     * @brief a_amount
     * @return
     */
    uint64_t a_amount() const;
    /**
     * @brief a_payTx
     * @return
     */
    std::string a_payTx() const;
    /**
     * @brief a_refTx
     * @return
     */
    std::string a_refTx() const;
    /**
     * @brief a_bintxid
     * @return
     */
    std::string a_bintxid() const;

    // TODO remove script
    /**
     * @brief a_innerScript
     * @return
     */
    std::vector<unsigned char> a_innerScript() const;

    /**
     * @brief a_datatxid
     * @return
     */
    uint256 a_datatxid() const;
    /**
     * @brief a_pk1
     * @return
     */
    std::vector<unsigned char> a_pk1() const;

    // uint256                    secondId() const;
    /**
     * @brief b_address
     * @return
     */
    std::vector<unsigned char> b_address() const;
    /**
     * @brief b_destination
     * @return
     */
    std::vector<unsigned char> b_destination() const;
    /**
     * @brief b_currency
     * @return
     */
    std::string b_currency() const;
    /**
     * @brief b_amount
     * @return
     */
    uint64_t b_amount() const;
    /**
     * @brief b_payTx
     * @return
     */
    std::string b_payTx() const;
    /**
     * @brief b_refTx
     * @return
     */
    std::string b_refTx() const;
    /**
     * @brief b_bintxid
     * @return
     */
    std::string b_bintxid() const;

    // TODO remove script
    /**
     * @brief b_innerScript
     * @return
     */
    std::vector<unsigned char> b_innerScript() const;

    // uint256                    b_datatxid() const;
    /**
     * @brief b_pk1
     * @return
     */
    std::vector<unsigned char> b_pk1() const;

    /**
     * @brief tryJoin
     * @param other
     * @return
     */
    bool tryJoin(const TransactionPtr other);

    /**
     * @brief setKeys
     * @param addr
     * @param datatxid
     * @param pk
     * @return
     */
    bool setKeys(const std::vector<unsigned char> & addr,
                 const uint256 & datatxid,
                 const std::vector<unsigned char> & pk);
    /**
     * @brief setBinTxId
     * @param addr
     * @param id
     * @param innerScript
     * @return
     */
    bool setBinTxId(const std::vector<unsigned char> &addr,
                    const std::string & id,
                    const std::vector<unsigned char> & innerScript);

    friend std::ostream & operator << (std::ostream & out, const TransactionPtr & tx);

public:
    boost::mutex               m_lock;

private:
    uint256                    m_id;

    boost::posix_time::ptime   m_created;
    boost::posix_time::ptime   m_last;

    uint256                    m_blockHash; //hash of block when transaction created

    State                      m_state;

    bool                       m_a_stateChanged;
    bool                       m_b_stateChanged;

    unsigned int               m_confirmationCounter;

    std::string                m_sourceCurrency;
    std::string                m_destCurrency;

    uint64_t                   m_sourceAmount;
    uint64_t                   m_destAmount;

    std::string                m_bintxid1;
    std::string                m_bintxid2;

    std::vector<unsigned char> m_innerScript1;
    std::vector<unsigned char> m_innerScript2;

    XBridgeTransactionMember   m_a;
    XBridgeTransactionMember   m_b;

    uint256                    m_a_datatxid;
    uint256                    m_b_datatxid;
};

} // namespace xbridge

#endif // XBRIDGETRANSACTION_H
