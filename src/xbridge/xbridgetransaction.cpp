// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//*****************************************************************************
//*****************************************************************************

#include <xbridge/xbridgetransaction.h>

#include <xbridge/util/logger.h>
#include <xbridge/util/settings.h>
#include <xbridge/util/xutil.h>
#include <xbridge/xbridgewalletconnector.h>

#include <sync.h>
#include <univalue.h>
#include <util/strencodings.h>
#include <validation.h>

#include <boost/date_time/posix_time/conversion.hpp>
#include <boost/lexical_cast.hpp>

//******************************************************************************
//******************************************************************************
namespace xbridge
{
// from xbridgeexchange.h
extern bool ExchangeUtxos(const uint256 & txid, std::vector<wallet::UtxoEntry> & items);
// from xbridgeapp.h
extern WalletConnectorPtr ConnectorByCurrency(const std::string & currency);

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
                         const std::vector<unsigned char> & mpubkey,
                         const bool                         partialAllowed = false,
                         const uint64_t                     minFromAmount = 0)
    : m_id(id)
    , m_created(xbridge::intToTime(created))
    , m_last(boost::posix_time::microsec_clock::universal_time())
    , m_lastUtxoCheck(boost::posix_time::microsec_clock::universal_time())
    , m_blockHash(blockHash)
    , m_state(trNew)
    , m_a_stateChanged(false)
    , m_b_stateChanged(false)
    , m_confirmationCounter(0)
    , m_sourceCurrency(sourceCurrency)
    , m_destCurrency(destCurrency)
    , m_sourceAmount(sourceAmount)
    , m_destAmount(destAmount)
    , m_sourceInitialAmount(sourceAmount)
    , m_destInitialAmount(destAmount)
    , m_a(id)
    , m_partialAllowed(partialAllowed)
    , m_minPartialAmount(minFromAmount)
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
    LOCK(m_lock);
    return m_id;
}

uint256 Transaction::blockHash() const
{
    LOCK(m_lock);
    return m_blockHash;
}

//*****************************************************************************
// state of transaction
//*****************************************************************************
Transaction::State Transaction::state() const
{
    LOCK(m_lock);
    return m_state;
}

//*****************************************************************************
//*****************************************************************************
Transaction::State Transaction::increaseStateCounter(const Transaction::State state,
                                                     const std::vector<unsigned char> & from)
{
    LOCK(m_lock);
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
    LOCK(m_lock);
    return strState(m_state);
}

//*****************************************************************************
//*****************************************************************************
void Transaction::updateTimestamp()
{
    LOCK(m_lock);
    m_last = boost::posix_time::microsec_clock::universal_time();
}

//*****************************************************************************
//*****************************************************************************
bool Transaction::updateTooSoon()
{
    LOCK(m_lock);
    auto current = boost::posix_time::microsec_clock::universal_time();
    return (current - m_last).total_seconds() < pendingTTL/2;
}

//*****************************************************************************
//*****************************************************************************
boost::posix_time::ptime Transaction::createdTime() const
{
    LOCK(m_lock);
    return m_created;
}

//*****************************************************************************
//*****************************************************************************
bool Transaction::isFinished() const
{
    LOCK(m_lock);
    return m_state == trCancelled ||
           m_state == trFinished ||
           m_state == trDropped;
}

//*****************************************************************************
//*****************************************************************************
bool Transaction::isValid() const
{
    LOCK(m_lock);
    return m_state != trInvalid;
}

//*****************************************************************************
//*****************************************************************************
bool Transaction::matches(uint256 & id) const
{
    LOCK(m_lock);
    return m_id == id;
}

//*****************************************************************************
//*****************************************************************************
bool Transaction::isExpired() const
{
    LOCK(m_lock);
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
    bool gtNew{false};
    {
        LOCK(m_lock);
        gtNew = m_state > trNew;
    }
    if (gtNew && !isFinished())
        return false;

    LOCK2(m_lock, cs_main);

    CBlockIndex* blockindex = LookupBlockIndex(m_blockHash);
    if (!blockindex)
        return true; //expired because we don't have this hash in blockchain

    int trBlockHeight = blockindex->nHeight;
    int lastBlockHeight = chainActive.Height();

    if (lastBlockHeight - trBlockHeight > blocksTTL)
        return true;

    return false;
}

bool Transaction::isPartialAllowed()
{
    LOCK(m_lock);
    return m_partialAllowed;
}

//*****************************************************************************
//*****************************************************************************
void Transaction::cancel()
{
    LOCK(m_lock);
    LOG() << "cancel transaction <" << m_id.GetHex() << ">";
    m_accepting = false;
    m_state = trCancelled;
}

//*****************************************************************************
//*****************************************************************************
void Transaction::drop()
{
    LOCK(m_lock);
    LOG() << "drop transaction <" << m_id.GetHex() << ">";
    m_state = trDropped;
}

//*****************************************************************************
//*****************************************************************************
void Transaction::finish()
{
    LOCK(m_lock);
    LOG() << "finish transaction <" << m_id.GetHex() << ">";
    m_state = trFinished;
}

//*****************************************************************************
//*****************************************************************************
std::vector<unsigned char> Transaction::a_address() const
{
    LOCK(m_lock);
    return m_a.source();
}

//*****************************************************************************
//*****************************************************************************
std::vector<unsigned char> Transaction::a_destination() const
{
    LOCK(m_lock);
    return m_a.dest();
}

//*****************************************************************************
//*****************************************************************************
std::string Transaction::a_currency() const
{
    LOCK(m_lock);
    return m_sourceCurrency;
}

//*****************************************************************************
//*****************************************************************************
uint64_t Transaction::a_amount() const
{
    LOCK(m_lock);
    return m_sourceAmount;
}

//*****************************************************************************
//*****************************************************************************
std::string Transaction::a_bintxid() const
{
    LOCK(m_lock);
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
    LOCK(m_lock);
    return m_a.mpubkey();
}

//*****************************************************************************
//*****************************************************************************
std::string Transaction::a_payTxId() const
{
    LOCK(m_lock);
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
    LOCK(m_lock);
    return m_b.source();
}

//*****************************************************************************
//*****************************************************************************
std::vector<unsigned char> Transaction::b_destination() const
{
    LOCK(m_lock);
    return m_b.dest();
}

//*****************************************************************************
//*****************************************************************************
std::string Transaction::b_currency() const
{
    LOCK(m_lock);
    return m_destCurrency;
}

//*****************************************************************************
//*****************************************************************************
uint64_t Transaction::b_amount() const
{
    LOCK(m_lock);
    return m_destAmount;
}

//*****************************************************************************
//*****************************************************************************
std::string Transaction::b_bintxid() const
{
    LOCK(m_lock);
    return m_bintxid2;
}

//*****************************************************************************
//*****************************************************************************
uint32_t Transaction::b_lockTime() const
{
    LOCK(m_lock);
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
    LOCK(m_lock);
    return m_b.mpubkey();
}

//*****************************************************************************
//*****************************************************************************
std::string Transaction::b_payTxId() const
{
    LOCK(m_lock);
    return m_b.payTxId();
}

//*****************************************************************************
//*****************************************************************************
bool Transaction::tryJoin(const TransactionPtr other)
{
    LOCK(m_lock);
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

    if (m_partialAllowed != other->m_partialAllowed) {
        LogOrderMsg(id().GetHex(), "partial allowed states do not match. transaction not joined", __FUNCTION__);
        return false;
    }

    if (m_partialAllowed)
    {
        if (!xBridgePartialOrderDriftCheck(m_sourceAmount, m_destAmount, other->m_sourceAmount, other->m_destAmount)) {
            UniValue log_obj(UniValue::VOBJ);
            log_obj.pushKV("orderid", id().GetHex());
            log_obj.pushKV("received_price", xbridge::xBridgeStringValueFromPrice(xBridgeValueFromAmount(m_sourceAmount) / xBridgeValueFromAmount(m_destAmount)));
            log_obj.pushKV("expected_price", xbridge::xBridgeStringValueFromPrice(xBridgeValueFromAmount(other->m_destAmount) / xBridgeValueFromAmount(other->m_sourceAmount)));
            xbridge::LogOrderMsg(log_obj, "taker price doesn't match maker expected price (join)", __FUNCTION__);
            return false;
        }

        if (m_sourceAmount < other->m_destAmount || m_destAmount < other->m_sourceAmount) {
            LogOrderMsg(id().GetHex(), "partial order amount error, taker amounts are too big. transaction not joined", __FUNCTION__);
            return false;
        } else if (other->m_destAmount < m_minPartialAmount) {
            LogOrderMsg(id().GetHex(), "partial order taker amount is smaller than the minimum. transaction not joined", __FUNCTION__);
            return false;
        }
    }

    if (!m_partialAllowed && (m_sourceAmount != other->m_destAmount || m_destAmount != other->m_sourceAmount)) {
        // amounts do not match
        LogOrderMsg(id().GetHex(), "taker amounts to not match maker ask amounts. transaction not joined", __FUNCTION__);
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
    LOCK(m_lock);
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
    LOCK(m_lock);
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
bool Transaction::orderType() {
    LOCK(m_lock);
    if (m_partialAllowed)
        return "partial";
    return "exact";
}

//*****************************************************************************
//*****************************************************************************
std::ostream & operator << (std::ostream & out, const TransactionPtr & tx)
{
    UniValue log_obj(UniValue::VOBJ);
    std::string errMsg;

    log_obj.pushKV("orderid", tx->id().GetHex());

    if(!settings().isFullLog()) {
        out << log_obj.write();
        return out;
    }

    xbridge::WalletConnectorPtr connFrom = ConnectorByCurrency(tx->a_currency());
    xbridge::WalletConnectorPtr connTo   = ConnectorByCurrency(tx->b_currency());

    if (!connFrom || !connTo)
        errMsg = (!connFrom ? tx->a_currency() : tx->b_currency()) + " connector missing";

    std::vector<xbridge::wallet::UtxoEntry> items;
    ExchangeUtxos(tx->id(), items);

    UniValue log_utxos(UniValue::VARR);
    uint32_t count = 0;
    for(const xbridge::wallet::UtxoEntry & entry : items)
    {
        UniValue log_utxo(UniValue::VOBJ);
        log_utxo.pushKV("index", static_cast<int>(count));
        log_utxo.pushKV("txid", entry.txId);
        log_utxo.pushKV("vout", static_cast<int>(entry.vout));
        log_utxo.pushKV("amount", xBridgeStringValueFromPrice(entry.amount, COIN));
        log_utxo.pushKV("address", entry.address);
        log_utxos.push_back(log_utxo);
        ++count;
    }

    log_obj.pushKV("maker", tx->a_currency());
    log_obj.pushKV("maker_size", xbridge::xBridgeStringValueFromAmount(tx->a_amount()));
    log_obj.pushKV("maker_addr", (!tx->a_address().empty() && connFrom ? connFrom->fromXAddr(tx->a_address()) : ""));
    log_obj.pushKV("taker", tx->b_currency());
    log_obj.pushKV("taker_size", xbridge::xBridgeStringValueFromAmount(tx->b_amount()));
    log_obj.pushKV("taker_addr", (!tx->b_address().empty() && connTo ? connTo->fromXAddr(tx->b_address()) : ""));
    log_obj.pushKV("order_type", tx->orderType());
    log_obj.pushKV("partial_minimum", xbridge::xBridgeStringValueFromAmount(tx->min_partial_amount()));
    log_obj.pushKV("partial_orig_maker_size", xbridge::xBridgeStringValueFromAmount(tx->a_initial_amount()));
    log_obj.pushKV("partial_orig_taker_size", xbridge::xBridgeStringValueFromAmount(tx->b_initial_amount()));
    log_obj.pushKV("state", tx->strState());
    log_obj.pushKV("block_hash", tx->blockHash().GetHex());
    log_obj.pushKV("created_at", xbridge::iso8601(tx->createdTime()));
    log_obj.pushKV("err_msg", errMsg);
    log_obj.pushKV("utxos", log_utxos);

    out << log_obj.write();
    return out;
}

} // namespace xbridge
