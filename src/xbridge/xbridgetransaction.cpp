//*****************************************************************************
//*****************************************************************************

#include "xbridgetransaction.h"
#include "util/logger.h"
#include "util/xutil.h"
#include "bitcoinrpcconnector.h"
#include "utilstrencodings.h"


//*****************************************************************************
//*****************************************************************************
XBridgeTransaction::XBridgeTransaction()
    : m_state(trInvalid)
    // , m_stateCounter(0)
    , m_a_stateChanged(false)
    , m_b_stateChanged(false)
    , m_confirmationCounter(0)
{

}

//*****************************************************************************
//*****************************************************************************
XBridgeTransaction::XBridgeTransaction(const uint256     & id,
                                       const std::string & sourceAddr,
                                       const std::string & sourceCurrency,
                                       const uint64_t    & sourceAmount,
                                       const std::string & destAddr,
                                       const std::string & destCurrency,
                                       const uint64_t    & destAmount)
    : m_id(id)
    , m_created(boost::posix_time::second_clock::universal_time())
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
XBridgeTransaction::~XBridgeTransaction()
{
}

//*****************************************************************************
//*****************************************************************************
uint256 XBridgeTransaction::id() const
{
    return m_id;
}

//*****************************************************************************
// state of transaction
//*****************************************************************************
XBridgeTransaction::State XBridgeTransaction::state() const
{
    return m_state;
}

//*****************************************************************************
//*****************************************************************************
XBridgeTransaction::State XBridgeTransaction::increaseStateCounter(XBridgeTransaction::State state,
                                                                   const std::string & from)
{
    LOG() << "confirm transaction state <" << strState(state)
          << "> from " << from;

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
std::string XBridgeTransaction::strState(const State state)
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
std::string XBridgeTransaction::strState() const
{
    return strState(m_state);
}

//*****************************************************************************
//*****************************************************************************
void XBridgeTransaction::updateTimestamp()
{
    m_created = boost::posix_time::second_clock::universal_time();
}

boost::posix_time::ptime XBridgeTransaction::createdTime() const
{
    return m_created;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeTransaction::isFinished() const
{
    return m_state == trCancelled ||
           m_state == trFinished ||
           m_state == trDropped;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeTransaction::isValid() const
{
    return m_state != trInvalid;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeTransaction::isExpired() const
{
    boost::posix_time::time_duration td = boost::posix_time::second_clock::universal_time() - m_created;
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
void XBridgeTransaction::cancel()
{
    LOG() << "cancel transaction <" << m_id.GetHex() << ">";
    m_state = trCancelled;
}

//*****************************************************************************
//*****************************************************************************
void XBridgeTransaction::drop()
{
    LOG() << "drop transaction <" << m_id.GetHex() << ">";
    m_state = trDropped;
}

//*****************************************************************************
//*****************************************************************************
void XBridgeTransaction::finish()
{
    LOG() << "finish transaction <" << m_id.GetHex() << ">";
    m_state = trFinished;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeTransaction::confirm(const std::string & id)
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
uint256 XBridgeTransaction::hash1() const
{
    return Hash(m_sourceCurrency.begin(), m_sourceCurrency.end(),
                BEGIN(m_sourceAmount), END(m_sourceAmount),
                m_destCurrency.begin(), m_destCurrency.end(),
                BEGIN(m_destAmount), END(m_destAmount));
}

//*****************************************************************************
//*****************************************************************************
uint256 XBridgeTransaction::hash2() const
{
    return Hash(m_destCurrency.begin(), m_destCurrency.end(),
                BEGIN(m_destAmount), END(m_destAmount),
                m_sourceCurrency.begin(), m_sourceCurrency.end(),
                BEGIN(m_sourceAmount), END(m_sourceAmount));
}

//*****************************************************************************
//*****************************************************************************
//uint256 XBridgeTransaction::firstId() const
//{
//    return m_first.id();
//}

//*****************************************************************************
//*****************************************************************************
std::string XBridgeTransaction::a_address() const
{
    return m_a.source();
}

//*****************************************************************************
//*****************************************************************************
std::string XBridgeTransaction::a_destination() const
{
    return m_a.dest();
}

//*****************************************************************************
//*****************************************************************************
std::string XBridgeTransaction::a_currency() const
{
    return m_sourceCurrency;
}

//*****************************************************************************
//*****************************************************************************
uint64_t XBridgeTransaction::a_amount() const
{
    return m_sourceAmount;
}

//*****************************************************************************
//*****************************************************************************
std::string XBridgeTransaction::a_bintxid() const
{
    return m_bintxid1;
}

//*****************************************************************************
//*****************************************************************************
std::string XBridgeTransaction::a_innerScript() const
{
    return m_innerScript1;
}

//*****************************************************************************
//*****************************************************************************
uint256 XBridgeTransaction::a_datatxid() const
{
    return m_a_datatxid;
}

//*****************************************************************************
//*****************************************************************************
xbridge::CPubKey XBridgeTransaction::a_pk1() const
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
std::string XBridgeTransaction::b_address() const
{
    return m_b.source();
}

//*****************************************************************************
//*****************************************************************************
std::string XBridgeTransaction::b_destination() const
{
    return m_b.dest();
}

//*****************************************************************************
//*****************************************************************************
std::string XBridgeTransaction::b_currency() const
{
    return m_destCurrency;
}

//*****************************************************************************
//*****************************************************************************
uint64_t XBridgeTransaction::b_amount() const
{
    return m_destAmount;
}

//*****************************************************************************
//*****************************************************************************
std::string XBridgeTransaction::b_bintxid() const
{
    return m_bintxid2;
}

//*****************************************************************************
//*****************************************************************************
std::string XBridgeTransaction::b_innerScript() const
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
xbridge::CPubKey XBridgeTransaction::b_pk1() const
{
    return m_b_pk1;
}

//*****************************************************************************
//*****************************************************************************
std::string XBridgeTransaction::fromXAddr(const std::vector<unsigned char> & xaddr) const
{
    if (rpc::toXAddr(m_a.source()) == xaddr)
    {
        return m_a.source();
    }
    else if (rpc::toXAddr(m_b.source()) == xaddr)
    {
        return m_b.source();
    }
    else if (rpc::toXAddr(m_a.dest()) == xaddr)
    {
        return m_a.dest();
    }
    else if (rpc::toXAddr(m_b.dest()) == xaddr)
    {
        return m_b.dest();
    }
    return std::string();
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeTransaction::tryJoin(const XBridgeTransactionPtr other)
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
bool XBridgeTransaction::setKeys(const std::string & addr,
                                 const uint256 & datatxid,
                                 const xbridge::CPubKey & pk)
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
bool XBridgeTransaction::setBinTxId(const std::string & addr,
                                    const std::string & id,
                                    const std::string & innerScript)
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
