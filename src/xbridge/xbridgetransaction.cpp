//*****************************************************************************
//*****************************************************************************

#include "xbridgetransaction.h"
#include "xbridgeapp.h"
#include "xbridgeexchange.h"
#include "util/logger.h"
#include "util/xutil.h"
#include "util/settings.h"
#include "utilstrencodings.h"
#include "main.h"
#include "sync.h"

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
                         const uint64_t                   & created,
                         const uint256                    & blockHash,
                         const std::vector<unsigned char> & mpubkey)
    : m_id(id)
    , m_created(util::intToTime(created))
    , m_last(boost::posix_time::microsec_clock::universal_time())
    , m_blockHash(blockHash)
    , m_state(trNew)
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
    m_a.setMPubkey(mpubkey);
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

uint256 Transaction::blockHash() const
{
    return m_blockHash;
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
          << "> from " << HexStr(from);

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
        "trSigned", "trCommited",
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
    m_last = boost::posix_time::microsec_clock::universal_time();
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
    boost::posix_time::time_duration tdLast = boost::posix_time::microsec_clock::universal_time() - m_last;
    boost::posix_time::time_duration tdCreated = boost::posix_time::microsec_clock::universal_time() - m_created;

    if (m_state == trNew && tdCreated.total_seconds() > deadlineTTL)
        return true;

    if (m_state == trNew && tdLast.total_seconds() > pendingTTL)
        return true;

    if (m_state > trNew && tdLast.total_seconds() > TTL)
        return true;

    return false;
}

//*****************************************************************************
//*****************************************************************************
bool Transaction::isExpiredByBlockNumber() const
{
    LOCK(cs_main);

    if (mapBlockIndex.count(m_blockHash) == 0)
        return true; //expired because we don't have this hash in blockchain

    if(m_state > trNew && !isFinished())
        return false;

    CBlockIndex* blockindex = mapBlockIndex[m_blockHash];

    int trBlockHeight = blockindex->nHeight;
    int lastBlockHeight = chainActive.Height();

    if(lastBlockHeight - trBlockHeight > blocksTTL)
        return true;

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
uint32_t Transaction::a_lockTime() const
{
    return m_a.lockTime();
}

//*****************************************************************************
//*****************************************************************************
std::vector<unsigned char> Transaction::a_pk1() const
{
    return m_a.mpubkey();
}

//*****************************************************************************
//*****************************************************************************
std::string Transaction::a_payTxId() const
{
    return m_a.payTxId();
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
uint32_t Transaction::b_lockTime() const
{
    return m_b.lockTime();
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
    return m_b.mpubkey();
}

//*****************************************************************************
//*****************************************************************************
std::string Transaction::b_payTxId() const
{
    return m_b.payTxId();
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
                          const std::vector<unsigned char> & pk)
{
    if (m_b.dest() == addr)
    {
        m_b.setMPubkey(pk);
        return true;
    }
    else if (m_a.dest() == addr)
    {
        m_a.setMPubkey(pk);
        return true;
    }
    return false;
}

//*****************************************************************************
//*****************************************************************************
bool Transaction::setBinTxId(const std::vector<unsigned char> & addr,
                             const std::string & id)
{
    if (m_b.source() == addr)
    {
        m_bintxid2     = id;
        return true;
    }
    else if (m_a.source() == addr)
    {
        m_bintxid1     = id;
        return true;
    }
    return false;
}

//*****************************************************************************
//*****************************************************************************
std::ostream & operator << (std::ostream & out, const TransactionPtr & tx)
{
    if(!settings().isFullLog())
    {
        out << std::endl << "ORDER ID: " << tx->id().GetHex() << std::endl;

        return out;
    }

    xbridge::WalletConnectorPtr connFrom = xbridge::App::instance().connectorByCurrency(tx->a_currency());
    xbridge::WalletConnectorPtr connTo   = xbridge::App::instance().connectorByCurrency(tx->b_currency());

    if (!connFrom || !connTo)
        out << "MISSING SOME CONNECTOR, NOT ALL ORDER INFO WILL BE LOGGED";

    xbridge::Exchange & e = xbridge::Exchange::instance();

    std::vector<xbridge::wallet::UtxoEntry> items;
    e.getUtxoItems(tx->id(), items);

    std::ostringstream inputsStream;
    uint32_t count = 0;
    for(const xbridge::wallet::UtxoEntry & entry : items)
    {
        inputsStream << "    INDEX: " << count << std::endl
                     << "    ID: " << entry.txId << std::endl
                     << "    VOUT: " << boost::lexical_cast<std::string>(entry.vout) << std::endl
                     << "    AMOUNT: " << entry.amount << std::endl
                     << "    ADDRESS: " << entry.address << std::endl;

        ++count;
    }

    out << std::endl
        << "ORDER BODY" << std::endl
        << "ID: " << tx->id().GetHex() << std::endl
        << "MAKER: " << tx->a_currency() << std::endl
        << "MAKER SIZE: " << util::xBridgeStringValueFromAmount(tx->a_amount()) << std::endl
        << "MAKER ADDR: " << (!tx->a_address().empty() && connFrom ? connFrom->fromXAddr(tx->a_address()) : "") << std::endl
        << "TAKER: " << tx->b_currency() << std::endl
        << "TAKER SIZE: " << util::xBridgeStringValueFromAmount(tx->b_amount()) << std::endl
        << "TAKER ADDR: " << (!tx->b_address().empty() && connTo ? connTo->fromXAddr(tx->b_address()) : "") << std::endl
        << "STATE: " << tx->strState() << std::endl
        << "BLOCK HASH: " << tx->blockHash().GetHex() << std::endl
        << "CREATED AT: " << util::iso8601(tx->createdTime()) << std::endl
        << "USED INPUTS: " << std::endl << inputsStream.str();

    return out;
}

} // namespace xbridge
