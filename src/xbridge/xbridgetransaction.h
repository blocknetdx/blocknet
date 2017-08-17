//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGETRANSACTION_H
#define XBRIDGETRANSACTION_H

#include "uint256.h"
#include "xbridgetransactionmember.h"
#include "xkey.h"

#include <vector>
#include <string>

#include <boost/cstdint.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/date_time/posix_time/ptime.hpp>

//*****************************************************************************
//*****************************************************************************
class XBridgeTransaction;
typedef boost::shared_ptr<XBridgeTransaction> XBridgeTransactionPtr;

//*****************************************************************************
//*****************************************************************************
class XBridgeTransaction
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

        // pending transaction ttl in seconds, 72 hours
        pendingTTL = 259200,

        // transaction ttl in seconds, 60 min
        TTL = 3600
    };

public:
    XBridgeTransaction();
    XBridgeTransaction(const uint256     & id,
                       const std::string & sourceAddr,
                       const std::string & sourceCurrency,
                       const uint64_t    & sourceAmount,
                       const std::string & destAddr,
                       const std::string & destCurrency,
                       const uint64_t    & destAmount);
    ~XBridgeTransaction();

    uint256 id() const;

    // state of transaction
    State state() const;
    // update state counter and update state
    State increaseStateCounter(State state, const std::string & from);

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

    // hash of transaction
    uint256 hash1() const;
    uint256 hash2() const;

    // uint256                    firstId() const;
    std::string                a_address() const;
    std::string                a_destination() const;
    std::string                a_currency() const;
    uint64_t                   a_amount() const;
    std::string                a_payTx() const;
    std::string                a_refTx() const;
    std::string                a_bintxid() const;
    std::string                a_innerScript() const;

    uint256                    a_datatxid() const;
    xbridge::CPubKey           a_pk1() const;

    // uint256                    secondId() const;
    std::string                b_address() const;
    std::string                b_destination() const;
    std::string                b_currency() const;
    uint64_t                   b_amount() const;
    std::string                b_payTx() const;
    std::string                b_refTx() const;
    std::string                b_bintxid() const;
    std::string                b_innerScript() const;

    // uint256                    b_datatxid() const;
    xbridge::CPubKey           b_pk1() const;

    std::string                fromXAddr(const std::vector<unsigned char> & xaddr) const;

    bool tryJoin(const XBridgeTransactionPtr other);

    bool                       setKeys(const std::string & addr,
                                       const uint256 & datatxid,
                                       const xbridge::CPubKey & pk);
    bool                       setBinTxId(const std::string & addr,
                                          const std::string & id,
                                          const std::string & innerScript);

public:
    boost::mutex               m_lock;

private:
    uint256                    m_id;

    boost::posix_time::ptime   m_created;

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

    std::string                m_innerScript1;
    std::string                m_innerScript2;

    XBridgeTransactionMember   m_a;
    XBridgeTransactionMember   m_b;

    uint256                    m_a_datatxid;
    uint256                    m_b_datatxid;

    xbridge::CPubKey           m_a_pk1;
    xbridge::CPubKey           m_b_pk1;
};

#endif // XBRIDGETRANSACTION_H
