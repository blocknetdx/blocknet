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
    Transaction();
    Transaction(const uint256                    & id,
                const std::vector<unsigned char> & sourceAddr,
                const std::string                & sourceCurrency,
                const uint64_t                   & sourceAmount,
                const std::vector<unsigned char> & destAddr,
                const std::string                & destCurrency,
                const uint64_t                   & destAmount,
                const std::time_t                & created);
    ~Transaction();

    uint256 id() const;

    // state of transaction
    State state() const;
    // update state counter and update state
    State increaseStateCounter(const State state, const std::vector<unsigned char> & from);

    static std::string strState(const State state);
    std::string strState() const;

    void updateTimestamp();
    boost::posix_time::ptime createdTime() const;

    bool isFinished() const;
    bool isValid() const;
    bool isExpired() const;

    void cancel();
    void drop();
    void finish();

    bool confirm(const std::string & id);

    // uint256                    firstId() const;
    std::vector<unsigned char> a_address() const;
    std::vector<unsigned char> a_destination() const;
    std::string                a_currency() const;
    uint64_t                   a_amount() const;
    std::string                a_payTx() const;
    std::string                a_refTx() const;
    std::string                a_bintxid() const;

    // TODO remove script
    std::vector<unsigned char> a_innerScript() const;

    uint256                    a_datatxid() const;
    std::vector<unsigned char> a_pk1() const;

    // uint256                    secondId() const;
    std::vector<unsigned char> b_address() const;
    std::vector<unsigned char> b_destination() const;
    std::string                b_currency() const;
    uint64_t                   b_amount() const;
    std::string                b_payTx() const;
    std::string                b_refTx() const;
    std::string                b_bintxid() const;

    // TODO remove script
    std::vector<unsigned char> b_innerScript() const;

    // uint256                    b_datatxid() const;
    std::vector<unsigned char> b_pk1() const;

    bool tryJoin(const TransactionPtr other);

    bool                       setKeys(const std::vector<unsigned char> & addr,
                                       const uint256 & datatxid,
                                       const std::vector<unsigned char> & pk);
    bool                       setBinTxId(const std::vector<unsigned char> &addr,
                                          const std::string & id,
                                          const std::vector<unsigned char> & innerScript);

public:
    boost::mutex               m_lock;

private:
    uint256                    m_id;

    boost::posix_time::ptime   m_created;
    boost::posix_time::ptime   m_last;

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

    std::vector<unsigned char> m_a_pk1;
    std::vector<unsigned char> m_b_pk1;
};

} // namespace xbridge

#endif // XBRIDGETRANSACTION_H
