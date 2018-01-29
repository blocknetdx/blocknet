//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGETRANSACTION_H
#define XBRIDGETRANSACTION_H

#include "uint256.h"
#include "xbridgetransactionmember.h"
#include "xkey.h"
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

        trConfirmed,
        trFinished,
        trCancelled,
        trDropped
    };

    enum
    {
        // transaction lock time base, in seconds, 10 min
        lockTime = 600,

        // pending transaction ttl in seconds, 1 min from last update
        pendingTTL = 60,

        // transaction ttl in seconds, 60 min
        TTL = 3600
    };

public:
    /**
     * @brief Transaction - default constructor
     */
    Transaction();
    /**
     * @brief Transaction
     * @param id - id of transaction
     * @param sourceAddr - source address
     * @param sourceCurrency - source currency
     * @param sourceAmount - source amount
     * @param destAddr - destionation address
     * @param destCurrency - destiontion address
     * @param destAmount - destination amount
     * @param created - time of creation
     * @param blockHash - hash of block
     */
    Transaction(const uint256                    & id,
                const std::vector<unsigned char> & sourceAddr,
                const std::string                & sourceCurrency,
                const uint64_t                   & sourceAmount,
                const std::vector<unsigned char> & destAddr,
                const std::string                & destCurrency,
                const uint64_t                   & destAmount,
                const std::time_t                & created,
                const uint256                    & blockHash);
    ~Transaction();

    /**
     * @brief id
     * @return id of transaction
     */
    uint256 id() const;

    /**
     * @brief blockHash
     * @return hash of block
     */
    uint256 blockHash() const;

    // state of transaction
    /**
     * @brief state
     * @return  state of transaction
     */
    State state() const;
    //
    /**
     * @brief increaseStateCounter  update state counter and update state
     * @param state
     * @param from
     * @return new transaction state
     */
    State increaseStateCounter(const State state, const std::vector<unsigned char> & from);

    /**
     * @brief strState
     * @param state - state value
     * @return string description of transaction state
     */
    static std::string strState(const State state);
    /**
     * @brief strState
     * @return string description of this transaction state
     */
    std::string strState() const;

    /**
     * @brief updateTimestamp -update transaction time
     */
    void updateTimestamp();
    /**
     * @brief createdTime
     * @return time of creation transaction
     */
    boost::posix_time::ptime createdTime() const;

    /**
     * @brief isFinished
     * @return true, if transaction has been finished, canceled or dropped
     */
    bool isFinished() const;
    /**
     * @brief isValid
     * @return true, if transaction state != invalid
     */
    bool isValid() const;
    /**
     * @brief isExpired
     * @return true, if last update time more 1 min and transaction state is new or
     * if transaction pending and last update time more 1 hour
     */
    bool isExpired() const;
    /**
     * @brief isExpiredByBlockNumber
     * @return expired because we don't have this hash in blockchain
     */
    bool isExpiredByBlockNumber() const;

    /**
     * @brief cancel - set transaction state to canceled
     */
    void cancel();
    /**
     * @brief drop - sets transaction to dropped
     */
    void drop();
    /**
     * @brief finish - set transaction state to finished
     */
    void finish();

    /**
     * @brief confirm
     * @param idid - id of transaction
     * @return true, if transaction state change to confirmed
     */
    bool confirm(const std::string & id);

    // uint256                    firstId() const;
    /**
     * @brief a_address
     * @return Alice's address of source
     */
    std::vector<unsigned char> a_address() const;
    /**
     * @brief a_destination
     * @return  Alice's destination address
     */
    std::vector<unsigned char> a_destination() const;
    /**
     * @brief a_currency
     * @return Alice's source currecy
     */
    std::string                a_currency() const;
    /**
     * @brief a_amount
     * @return Alice's source amount
     */
    uint64_t                   a_amount() const;
    /**
     * @brief a_payTx
     * @return
     */
    std::string                a_payTx() const;
    /**
     * @brief a_refTx
     * @return
     */
    std::string                a_refTx() const;
    /**
     * @brief a_bintxid
     * @return
     */
    std::string                a_bintxid() const;

    // TODO remove script
    /**
     * @brief a_innerScript
     * @return
     */
    std::vector<unsigned char> a_innerScript() const;

    uint256                    a_datatxid() const;
    /**
     * @brief a_pk1
     * @return Alice's public key
     */
    std::vector<unsigned char> a_pk1() const;

    // uint256                    secondId() const;
    /**
     * @brief b_address
     * @return Bob's source address
     */
    std::vector<unsigned char> b_address() const;
    /**
     * @brief b_destination
     * @return Bob's destionation address
     */
    std::vector<unsigned char> b_destination() const;
    /**
     * @brief b_currency
     * @return Bob's source currency
     */
    std::string                b_currency() const;
    /**
     * @brief b_amount
     * @return Bob's amount
     */
    uint64_t                   b_amount() const;
    std::string                b_payTx() const;
    std::string                b_refTx() const;
    std::string                b_bintxid() const;

    // TODO remove script
    std::vector<unsigned char> b_innerScript() const;

    // uint256                    b_datatxid() const;
    /**
     * @brief b_pk1
     * @return Bob's public key
     */
    std::vector<unsigned char> b_pk1() const;

    bool tryJoin(const TransactionPtr other);

    /**
     * @brief setKeys - set public key for Alice or Bob transaction
     * @param addr - wallet address
     * @param datatxid
     * @param pk - public key
     * @return - true, if Alice or Bob public key value installed
     */
    bool                       setKeys(const std::vector<unsigned char> & addr,
                                       const uint256 & datatxid,
                                       const std::vector<unsigned char> & pk);
    bool                       setBinTxId(const std::vector<unsigned char> &addr,
                                          const std::string & id,
                                          const std::vector<unsigned char> & innerScript);

public:
    boost::mutex               m_lock;

private:
    /**
     * @brief m_id - id of transaction
     */
    uint256                    m_id;

    /**
     * @brief m_created - time of creation transaction
     */
    boost::posix_time::ptime   m_created;
    /**
     * @brief m_last - time of last update transaction
     */
    boost::posix_time::ptime   m_last;

    /**
     * @brief m_blockHash hash of block when transaction created
     */
    uint256                    m_blockHash; //

    /**
     * @brief m_state - state of transaction
     */
    State                      m_state;

    /**
     * @brief m_a_stateChanged - flag of Alice's transaction status changed
     */
    bool                       m_a_stateChanged;
    /**
     * @brief m_b_stateChanged - flag of Bob's transaction changed
     */
    bool                       m_b_stateChanged;

    /**
     * @brief m_confirmationCounter - counter of confirmations the transactions
     */
    unsigned int               m_confirmationCounter;

    /**
     * @brief m_sourceCurrency - source (Alice's) currency
     */
    std::string                m_sourceCurrency;
    /**
     * @brief m_destCurrency - destination (Bob's) currency
     */
    std::string                m_destCurrency;

    /**
     * @brief m_sourceAmount - source (Alice's) amount
     */
    uint64_t                   m_sourceAmount;
    /**
     * @brief m_destAmount destination (Bob's) amount
     */
    uint64_t                   m_destAmount;

    std::string                m_bintxid1;
    std::string                m_bintxid2;

    std::vector<unsigned char> m_innerScript1;
    std::vector<unsigned char> m_innerScript2;

    /**
     * @brief m_a - Alice's transaction
     */
    XBridgeTransactionMember   m_a;
    /**
     * @brief m_b - Bob's transaction
     */
    XBridgeTransactionMember   m_b;


    uint256                    m_a_datatxid;
    uint256                    m_b_datatxid;

    /**
     * @brief m_a_pk1 - Alice's public key
     */
    std::vector<unsigned char> m_a_pk1;
    /**
     * @brief m_b_pk1 - Bob's public key
     */
    std::vector<unsigned char> m_b_pk1;
};

} // namespace xbridge

#endif // XBRIDGETRANSACTION_H
