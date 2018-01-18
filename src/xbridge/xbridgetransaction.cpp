//*****************************************************************************
//*****************************************************************************

#include "xbridgetransaction.h"
#include "util/logger.h"
#include "util/xutil.h"
#include "utilstrencodings.h"

#include <boost/date_time/posix_time/conversion.hpp>

//******************************************************************************
//******************************************************************************
namespace xbridge
{

//*****************************************************************************
//*****************************************************************************
Transaction::Transaction()
    : m_state(trInvalid)
    // , m_stateCounter(0)
    , m_a_stateChanged(false)
    , m_b_stateChanged(false)
    , m_confirmationCounter(0)
{

}

//*****************************************************************************
//*****************************************************************************
Transaction::Transaction(const uint256                    & id,
                         const std::vector<unsigned char> & sourceAddr,
                         const std::string                & sourceCurrency,
                         const uint64_t                   & sourceAmount,
                         const std::vector<unsigned char> & destAddr,
                         const std::string                & destCurrency,
                         const uint64_t                   & destAmount,
                         const time_t                     & created)
    : m_id(id)
    , m_created(boost::posix_time::from_time_t(created))
    , m_last(boost::posix_time::from_time_t(created))
    , m_state(trNew)
    // , m_stateCounter(0)
    , m_a_stateChanged(false)
    , m_b_stateChanged(false)
    , m_confirmationCounter(0)
    , m_sourceCurrency(sourceCurrency)
    , m_destCurrency(destCurrency)
    , m_sourceAmount(sourceAmount)
    , m_destAmount(destAmount)
    , m_a(id)
{
    m_a.setSource(sourceAddr);
    m_a.setDest(destAddr);
}

//*****************************************************************************
//*****************************************************************************
Transaction::~Transaction()
{
}

//*****************************************************************************
//*****************************************************************************
uint256 Transaction::id() const
{
    return m_id;
}

//*****************************************************************************
// state of transaction
//*****************************************************************************
Transaction::State Transaction::state() const
{
    return m_state;
}

//*****************************************************************************
//*****************************************************************************
Transaction::State Transaction::increaseStateCounter(const Transaction::State state,
                                                                   const std::vector<unsigned char> & from)
{
    LOG() << "confirm transaction state <" << strState(state)
          << "> from " << util::to_str(from);

    if (state == trJoined && m_state == state)
    {
        if (from == m_a.source())
        {
            m_a_stateChanged = true;
        }
        else if (from == m_b.source())
        {
            m_b_stateChanged = true;
        }
        if (m_a_stateChanged && m_b_stateChanged)
        {
            m_state = trHold;
            m_a_stateChanged = m_b_stateChanged = false;
        }
        return m_state;
    }
    else if (state == trHold && m_state == state)
    {
        if (from == m_a.dest())
        {
            m_a_stateChanged = true;
        }
        else if (from == m_b.dest())
        {
            m_b_stateChanged = true;
        }
        if (m_a_stateChanged && m_b_stateChanged)
        {
            m_state = trInitialized;
            m_a_stateChanged = m_b_stateChanged = false;
        }
        return m_state;
    }
    else if (state == trInitialized && m_state == state)
    {
        if (from == m_a.source())
        {
            m_a_stateChanged = true;
        }
        else if (from == m_b.source())
        {
            m_b_stateChanged = true;
        }
        if (m_a_stateChanged && m_b_stateChanged)
        {
            m_state = trCreated;
            m_a_stateChanged = m_b_stateChanged = false;
        }
        return m_state;
    }
    else if (state == trCreated && m_state == state)
    {
        if (from == m_a.dest())
        {
            m_a_stateChanged = true;
        }
        else if (from == m_b.dest())
        {
            m_b_stateChanged = true;
        }
        if (m_a_stateChanged && m_b_stateChanged)
        {
            m_state = trFinished;
            m_a_stateChanged = m_b_stateChanged = false;
        }
        return m_state;
    }
//    else if (state == trSigned && m_state == state)
//    {
//        if (from == m_a.source())
//        {
//            m_a_stateChanged = true;
//        }
//        else if (from == m_b.source())
//        {
//            m_b_stateChanged = true;
//        }
//        if (m_a_stateChanged && m_b_stateChanged)
//        {
//            m_state = trCommited;
//            m_a_stateChanged = m_b_stateChanged = false;
//        }
//        return m_state;
//    }
//    else if (state == trCommited && m_state == state)
//    {
//        if (from == m_a.dest())
//        {
//            m_a_stateChanged = true;
//        }
//        else if (from == m_b.dest())
//        {
//            m_b_stateChanged = true;
//        }

//        if (m_a_stateChanged && m_b_stateChanged)
//        {
//            m_state = trFinished;
//            m_a_stateChanged = m_b_stateChanged = false;
//        }
//        return m_state;
//    }

    return trInvalid;
}

//*****************************************************************************
//*****************************************************************************
// static
std::string Transaction::strState(const State state)
{
    static std::string states[] = {
        "trInvalid", "trNew", "trJoined",
        "trHold", "trInitialized", "trCreated",
        "trSigned", "trCommited", "trConfirmed",
        "trFinished", "trCancelled", "trDropped"
    };

    return states[state];
}

//*****************************************************************************
//*****************************************************************************
std::string Transaction::strState() const
{
    return strState(m_state);
}

//*****************************************************************************
//*****************************************************************************
void Transaction::updateTimestamp()
{
    m_last = boost::posix_time::second_clock::universal_time();
}

//*****************************************************************************
//*****************************************************************************
boost::posix_time::ptime Transaction::createdTime() const
{
    return m_created;
}

//*****************************************************************************
//*****************************************************************************
bool Transaction::isFinished() const
{
    return m_state == trCancelled ||
           m_state == trFinished ||
           m_state == trDropped;
}

//*****************************************************************************
//*****************************************************************************
bool Transaction::isValid() const
{
    return m_state != trInvalid;
}

//*****************************************************************************
//*****************************************************************************
bool Transaction::isExpired() const
{
    boost::posix_time::time_duration td = boost::posix_time::second_clock::universal_time() - m_last;
    if (m_state == trNew && td.total_seconds() > pendingTTL)
    {
        return true;
    }
    if (m_state > trNew && td.total_seconds() > TTL)
    {
        return true;
    }
    return false;
}

//*****************************************************************************
//*****************************************************************************
void Transaction::cancel()
{
    LOG() << "cancel transaction <" << m_id.GetHex() << ">";
    m_state = trCancelled;
}

//*****************************************************************************
//*****************************************************************************
void Transaction::drop()
{
    LOG() << "drop transaction <" << m_id.GetHex() << ">";
    m_state = trDropped;
}

//*****************************************************************************
//*****************************************************************************
void Transaction::finish()
{
    LOG() << "finish transaction <" << m_id.GetHex() << ">";
    m_state = trFinished;
}

//*****************************************************************************
//*****************************************************************************
bool Transaction::confirm(const std::string & id)
{
    if (m_bintxid1 == id || m_bintxid2 == id)
    {
        if (++m_confirmationCounter >= 2)
        {
            m_state = trConfirmed;
            return true;
        }
    }

    return false;
}

//*****************************************************************************
//*****************************************************************************
std::vector<unsigned char> Transaction::a_address() const
{
    return m_a.source();
}

//*****************************************************************************
//*****************************************************************************
std::vector<unsigned char> Transaction::a_destination() const
{
    return m_a.dest();
}

//*****************************************************************************
//*****************************************************************************
std::string Transaction::a_currency() const
{
    return m_sourceCurrency;
}

//*****************************************************************************
//*****************************************************************************
uint64_t Transaction::a_amount() const
{
    return m_sourceAmount;
}

//*****************************************************************************
//*****************************************************************************
std::string Transaction::a_bintxid() const
{
    return m_bintxid1;
}

//*****************************************************************************
//*****************************************************************************
std::vector<unsigned char> Transaction::a_innerScript() const
{
    return m_innerScript1;
}

//*****************************************************************************
//*****************************************************************************
uint256 Transaction::a_datatxid() const
{
    return m_a_datatxid;
}

//*****************************************************************************
//*****************************************************************************
std::vector<unsigned char> Transaction::a_pk1() const
{
    return m_a_pk1;
}

//*****************************************************************************
//*****************************************************************************
//uint256 XBridgeTransaction::secondId() const
//{
//    return m_second.id();
//}

//*****************************************************************************
//*****************************************************************************
std::vector<unsigned char> Transaction::b_address() const
{
    return m_b.source();
}

//*****************************************************************************
//*****************************************************************************
std::vector<unsigned char> Transaction::b_destination() const
{
    return m_b.dest();
}

//*****************************************************************************
//*****************************************************************************
std::string Transaction::b_currency() const
{
    return m_destCurrency;
}

//*****************************************************************************
//*****************************************************************************
uint64_t Transaction::b_amount() const
{
    return m_destAmount;
}

//*****************************************************************************
//*****************************************************************************
std::string Transaction::b_bintxid() const
{
    return m_bintxid2;
}

//*****************************************************************************
//*****************************************************************************
std::vector<unsigned char> Transaction::b_innerScript() const
{
    return m_innerScript2;
}

//*****************************************************************************
//*****************************************************************************
// uint256 XBridgeTransaction::b_datatxid() const
//{
//    return m_b_datatxid;
//}

//*****************************************************************************
//*****************************************************************************
std::vector<unsigned char> Transaction::b_pk1() const
{
    return m_b_pk1;
}

//*****************************************************************************
//*****************************************************************************
bool Transaction::tryJoin(const TransactionPtr other)
{
    DEBUG_TRACE();

    if (m_state != trNew || other->state() != trNew)
    {
        // can be joined only new transactions
        return false;
    }

    if (m_sourceCurrency != other->m_destCurrency ||
        m_destCurrency != other->m_sourceCurrency)
    {
        // not same currencies
        ERR() << "not same currencies. transaction not joined" << __FUNCTION__;
        return false;
    }

    if (m_sourceAmount != other->m_destAmount ||
        m_destAmount != other->m_sourceAmount)
    {
        // not same currencies
        ERR() << "not same amount. transaction not joined" << __FUNCTION__;
        return false;
    }

    // join second member
    m_b = other->m_a;

    m_state = trJoined;

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Transaction::setKeys(const std::vector<unsigned char> & addr,
                                 const uint256 & datatxid,
                                 const std::vector<unsigned char> & pk)
{
    if (m_b.dest() == addr)
    {
        m_b_datatxid = datatxid;
        m_b_pk1      = pk;
        return true;
    }
    else if (m_a.dest() == addr)
    {
        m_a_datatxid = datatxid;
        m_a_pk1      = pk;
        return true;
    }
    return false;
}

//*****************************************************************************
//*****************************************************************************
bool Transaction::setBinTxId(const std::vector<unsigned char> & addr,
                                    const std::string & id,
                                    const std::vector<unsigned char> & innerScript)
{
    if (m_b.source() == addr)
    {
        m_bintxid2     = id;
        m_innerScript2 = innerScript;
        return true;
    }
    else if (m_a.source() == addr)
    {
        m_bintxid1     = id;
        m_innerScript1 = innerScript;
        return true;
    }
    return false;
}

} // namespace xbridge
