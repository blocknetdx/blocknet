// Copyright (c) 2017-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//*****************************************************************************
//*****************************************************************************

#include <xbridge/xbridgesession.h>

#include <xbridge/bitcoinrpcconnector.h>
#include <xbridge/util/fastdelegate.h>
#include <xbridge/util/posixtimeconversion.h>
#include <xbridge/util/xutil.h>
#include <xbridge/util/logger.h>
#include <xbridge/util/settings.h>
#include <xbridge/util/txlog.h>
#include <xbridge/util/xassert.h>
#include <xbridge/xbitcointransaction.h>
#include <xbridge/xbridgeapp.h>
#include <xbridge/xbridgeexchange.h>
#include <xbridge/xbridgepacket.h>
#include <xbridge/xuiconnector.h>

#include <base58.h>
#include <core_io.h>
#include <consensus/validation.h>
#include <node/transaction.h>
#include <random.h>
#include <rpc/protocol.h>
#include <script/script.h>
#include <servicenode/servicenodemgr.h>
#include <sync.h>

#include <json/json_spirit.h>
#include <json/json_spirit_reader_template.h>
#include <json/json_spirit_writer_template.h>
#include <json/json_spirit_utils.h>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time/posix_time/conversion.hpp>

using namespace json_spirit;

//*****************************************************************************
//*****************************************************************************
namespace xbridge
{

//******************************************************************************
//******************************************************************************
// Threshold for nLockTime: below this value it is interpreted as block number,
// otherwise as UNIX timestamp.
// Tue Nov  5 00:53:20 1985 UTC
// const unsigned int LOCKTIME_THRESHOLD = 500000000;

//******************************************************************************
//******************************************************************************
struct PrintErrorCode
{
    const boost::system::error_code & error;

    explicit PrintErrorCode(const boost::system::error_code & e) : error(e) {}

    friend std::ostream & operator<<(std::ostream & out, const PrintErrorCode & e)
    {
        return out << " ERROR <" << e.error.value() << "> " << e.error.message();
    }
};

//*****************************************************************************
//*****************************************************************************
class Session::Impl
{
    friend class Session;

protected:
    void init();

protected:
    void sendPacket(const std::vector<unsigned char> & to, const XBridgePacketPtr & packet) const;
    void sendPacketBroadcast(XBridgePacketPtr packet) const;

    // return true if packet not for me, relayed
    bool checkPacketAddress(XBridgePacketPtr packet) const;

    // fn search xaddress in transaction and restore full 'coin' address as string
    bool isAddressInTransaction(const std::vector<unsigned char> & address,
                                const TransactionPtr & tx) const;

protected:
    bool encryptPacket(XBridgePacketPtr packet) const;
    bool decryptPacket(XBridgePacketPtr packet) const;

protected:
    bool processInvalid(XBridgePacketPtr packet) const;
    bool processZero(XBridgePacketPtr packet) const;
    bool processXChatMessage(XBridgePacketPtr packet) const;
    bool processServicesPing(XBridgePacketPtr packet) const;

    bool processTransaction(XBridgePacketPtr packet) const;
    bool processPendingTransaction(XBridgePacketPtr packet) const;
    bool processTransactionAccepting(XBridgePacketPtr packet) const;

    bool processTransactionHold(XBridgePacketPtr packet) const;
    bool processTransactionHoldApply(XBridgePacketPtr packet) const;

    bool processTransactionInit(XBridgePacketPtr packet) const;
    bool processTransactionInitialized(XBridgePacketPtr packet) const;

    bool processTransactionCreateA(XBridgePacketPtr packet) const;
    bool processTransactionCreateB(XBridgePacketPtr packet) const;
    bool processTransactionCreatedA(XBridgePacketPtr packet) const;
    bool processTransactionCreatedB(XBridgePacketPtr packet) const;

    bool processTransactionConfirmA(XBridgePacketPtr packet) const;
    bool processTransactionConfirmedA(XBridgePacketPtr packet) const;

    bool processTransactionConfirmB(XBridgePacketPtr packet) const;
    bool processTransactionConfirmedB(XBridgePacketPtr packet) const;

    bool finishTransaction(TransactionPtr tr) const;

    bool sendCancelTransaction(const TransactionPtr & tx,
                               const TxCancelReason & reason) const;
    bool sendCancelTransaction(const TransactionDescrPtr & tx,
                               const TxCancelReason & reason) const;
    bool sendRejectTransaction(const uint256 & id, const TxCancelReason & reason) const;

    bool processTransactionCancel(XBridgePacketPtr packet) const;
    bool processTransactionReject(XBridgePacketPtr packet) const;

    bool processTransactionFinished(XBridgePacketPtr packet) const;

protected:
    bool redeemOrderDeposit(const TransactionDescrPtr & xtx, int32_t & errCode) const;
    bool redeemOrderCounterpartyDeposit(const TransactionDescrPtr & xtx, int32_t & errCode) const;
    bool refundTraderDeposit(const std::string & orderId, const std::string & currency, const uint32_t & lockTime,
                             const std::string & refTx, int32_t & errCode) const;
    void sendTransaction(uint256 & id) const;

protected:
    std::vector<unsigned char> m_myid;

    typedef fastdelegate::FastDelegate1<XBridgePacketPtr, bool> PacketHandler;
    typedef std::map<const int, PacketHandler> PacketHandlersMap;
    PacketHandlersMap m_handlers;
};

//*****************************************************************************
//*****************************************************************************
Session::Session()
    : m_p(new Impl)
    , m_isWorking(false)
{
    m_p->init();
}

//*****************************************************************************
//*****************************************************************************
Session::~Session()
{
}

//*****************************************************************************
//*****************************************************************************
const std::vector<unsigned char> & Session::sessionAddr() const
{
    return m_p->m_myid;
}

//*****************************************************************************
//*****************************************************************************
void Session::Impl::init()
{
    if (m_handlers.size())
    {
        LOG() << "packet handlers map must be empty" << __FUNCTION__;
        return;
    }

    m_myid.resize(20);
    GetStrongRandBytes(&m_myid[0], 20);

    // process invalid
    m_handlers[xbcInvalid]                   .bind(this, &Impl::processInvalid);

    if (gArgs.GetBoolArg("-enableexchange", false) && sn::ServiceNodeMgr::instance().hasActiveSn())
    {
        // server side
        m_handlers[xbcTransaction]           .bind(this, &Impl::processTransaction);
        m_handlers[xbcTransactionAccepting]  .bind(this, &Impl::processTransactionAccepting);
        m_handlers[xbcTransactionHoldApply]  .bind(this, &Impl::processTransactionHoldApply);
        m_handlers[xbcTransactionInitialized].bind(this, &Impl::processTransactionInitialized);
        m_handlers[xbcTransactionCreatedA]   .bind(this, &Impl::processTransactionCreatedA);
        m_handlers[xbcTransactionCreatedB]   .bind(this, &Impl::processTransactionCreatedB);
        m_handlers[xbcTransactionConfirmedA] .bind(this, &Impl::processTransactionConfirmedA);
        m_handlers[xbcTransactionConfirmedB] .bind(this, &Impl::processTransactionConfirmedB);
    }
    else
    {
        // client side
        m_handlers[xbcPendingTransaction]    .bind(this, &Impl::processPendingTransaction);
        m_handlers[xbcTransactionHold]       .bind(this, &Impl::processTransactionHold);
        m_handlers[xbcTransactionInit]       .bind(this, &Impl::processTransactionInit);
        m_handlers[xbcTransactionCreateA]    .bind(this, &Impl::processTransactionCreateA);
        m_handlers[xbcTransactionCreateB]    .bind(this, &Impl::processTransactionCreateB);
        m_handlers[xbcTransactionConfirmA]   .bind(this, &Impl::processTransactionConfirmA);
        m_handlers[xbcTransactionConfirmB]   .bind(this, &Impl::processTransactionConfirmB);
    }

    {
        // common handlers
        m_handlers[xbcTransactionCancel]     .bind(this, &Impl::processTransactionCancel);
        m_handlers[xbcTransactionReject]     .bind(this, &Impl::processTransactionReject);
        m_handlers[xbcTransactionFinished]   .bind(this, &Impl::processTransactionFinished);
    }

    // xchat ()
    m_handlers[xbcXChatMessage].bind(this, &Impl::processXChatMessage);
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::encryptPacket(XBridgePacketPtr /*packet*/) const
{
    // DEBUG_TRACE();
    // TODO implement this
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::decryptPacket(XBridgePacketPtr /*packet*/) const
{
    // DEBUG_TRACE();
    // TODO implement this
    return true;
}

//*****************************************************************************
//*****************************************************************************
void Session::Impl::sendPacket(const std::vector<unsigned char> & to,
                               const XBridgePacketPtr & packet) const
{
    xbridge::App & app = xbridge::App::instance();
    app.sendPacket(to, packet);
}

//*****************************************************************************
//*****************************************************************************
void Session::Impl::sendPacketBroadcast(XBridgePacketPtr packet) const
{
    // DEBUG_TRACE();

    xbridge::App & app = xbridge::App::instance();
    app.sendPacket(packet);
}

//*****************************************************************************
// return true if packet for me and need to process
//*****************************************************************************
bool Session::Impl::checkPacketAddress(XBridgePacketPtr packet) const
{
    if (packet->size() < 20)
    {
        return false;
    }

    // check address
    if (memcmp(packet->data(), &m_myid[0], 20) == 0)
    {
        // this session address, need to process
        return true;
    }

    // not for me
    return false;
}

//*****************************************************************************
//*****************************************************************************
bool Session::processPacket(XBridgePacketPtr packet, CValidationState * state)
{
    // DEBUG_TRACE();

    setWorking();

    if (!m_p->decryptPacket(packet))
    {
        ERR() << "packet decoding error " << __FUNCTION__;
        setNotWorking();
        return false;
    }

    XBridgeCommand c = packet->command();

    if (m_p->m_handlers.count(c) == 0)
    {
        ERR() << "unknown command code <" << c << "> " << __FUNCTION__;
        m_p->m_handlers.at(xbcInvalid)(packet);
        setNotWorking();
        return false;
    }

    TRACE() << "received packet, command code <" << c << ">";

    if (!m_p->m_handlers.at(c)(packet))
    {
        if (state)
        {
            state->DoS(0, error("Xbridge packet processing error"), REJECT_INVALID, "bad-xbridge-packet");
        }

        ERR() << "packet processing error <" << c << "> " << __FUNCTION__;
        setNotWorking();
        return false;
    }

    setNotWorking();
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::processInvalid(XBridgePacketPtr /*packet*/) const
{
    // DEBUG_TRACE();
    // LOG() << "xbcInvalid instead of " << packet->command();
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::processZero(XBridgePacketPtr /*packet*/) const
{
    return true;
}

//*****************************************************************************
//*****************************************************************************
// static
bool Session::checkXBridgePacketVersion(const std::vector<unsigned char> & message)
{
    const uint32_t version = *reinterpret_cast<const uint32_t *>(&message[0]);

    if (version != static_cast<boost::uint32_t>(XBRIDGE_PROTOCOL_VERSION))
    {
        // ERR() << "incorrect protocol version <" << version << "> " << __FUNCTION__;
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
// static
bool Session::checkXBridgePacketVersion(XBridgePacketPtr packet)
{
    if (packet->version() != static_cast<boost::uint32_t>(XBRIDGE_PROTOCOL_VERSION))
    {
        // ERR() << "incorrect protocol version <" << packet->version() << "> " << __FUNCTION__;
        return false;
    }

    return true;
}

//*****************************************************************************
// retranslate packets from wallet to xbridge network
//*****************************************************************************
bool Session::Impl::processXChatMessage(XBridgePacketPtr /*packet*/) const
{
    LOG() << "Session::Impl::processXChatMessage not implemented";
    return true;

//    DEBUG_TRACE();

//    // size must be > 20 bytes (160bit)
//    if (packet->size() <= 20)
//    {
//        ERR() << "invalid packet size for xbcXChatMessage "
//              << "need more than 20 received " << packet->size() << " "
//              << __FUNCTION__;
//        return false;
//    }

//    // read dest address
//    std::vector<unsigned char> daddr(packet->data(), packet->data() + 20);

//    XBridgeApp & app = XBridgeApp::instance();
//    app.onSend(daddr,
//               std::vector<unsigned char>(packet->header(), packet->header()+packet->allSize()));

//    return true;
}

//*****************************************************************************
// broadcast
//*****************************************************************************
bool Session::Impl::processTransaction(XBridgePacketPtr packet) const
{
    // check and process packet if bridge is exchange
    Exchange & e = Exchange::instance();
    if (!e.isStarted())
    {
        return true;
    }

    DEBUG_TRACE();

    // size must be > 152 bytes
    if (packet->size() < 152)
    {
        ERR() << "invalid packet size for xbcTransaction "
              << "need min 152 bytes, received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    // read packet data
    std::vector<unsigned char> sid(packet->data(), packet->data()+XBridgePacket::hashSize);
    uint256 id(sid);
    uint32_t offset = XBridgePacket::hashSize;

    // Check if order already exists, if it does ignore processing
    TransactionPtr t = e.pendingTransaction(id);
    if (t->matches(id)) {
        // Update the transaction timestamp
        if (e.updateTimestampOrRemoveExpired(t)) {
            if (!e.makerUtxosAreStillValid(t)) { // if the maker utxos are no longer valid, cancel the order
                sendCancelTransaction(t, crBadAUtxo);
                return false;
            }
            xbridge::LogOrderMsg(id.GetHex(), "order already received, updating timestamp", __FUNCTION__);
            // relay order to network
            sendTransaction(id);
        }
        return true;
    }

    // source
    std::vector<unsigned char> saddr(packet->data()+offset, packet->data()+offset+XBridgePacket::addressSize);
    offset += XBridgePacket::addressSize;
    std::string scurrency((const char *)packet->data()+offset);
    offset += 8;
    uint64_t samount = *static_cast<boost::uint64_t *>(static_cast<void *>(packet->data()+offset));
    offset += sizeof(uint64_t);

    // destination
    std::vector<unsigned char> daddr(packet->data()+offset, packet->data()+offset+XBridgePacket::addressSize);
    offset += XBridgePacket::addressSize;
    std::string dcurrency((const char *)packet->data()+offset);
    offset += 8;
    uint64_t damount = *static_cast<uint64_t *>(static_cast<void *>(packet->data()+offset));
    offset += sizeof(uint64_t);

    uint64_t timestamp = *static_cast<uint64_t *>(static_cast<void *>(packet->data()+offset));
    offset += sizeof(uint64_t);

    std::vector<unsigned char> sblockhash(packet->data()+offset, packet->data()+offset+XBridgePacket::hashSize);
    uint256 blockHash(sblockhash);
    offset += XBridgePacket::hashSize;

    std::vector<unsigned char> mpubkey(packet->pubkey(), packet->pubkey()+XBridgePacket::pubkeySize);

    if (!packet->verify(mpubkey))
    {
        xbridge::LogOrderMsg(id.GetHex(), "bad counterparty packet signature", __FUNCTION__);
        return true;
    }

    bool isPartialOrder = *static_cast<uint16_t *>(static_cast<void *>(packet->data()+offset)) == 1;
    offset += sizeof(uint16_t);

    uint64_t minFromAmount = *static_cast<uint64_t *>(static_cast<void *>(packet->data()+offset));
    offset += sizeof(uint64_t);

    // utxos count
    uint32_t utxoItemsCount = *static_cast<uint32_t *>(static_cast<void *>(packet->data()+offset));
    offset += sizeof(uint32_t);

    if (isPartialOrder && utxoItemsCount > xBridgePartialOrderMaxUtxos) {
        xbridge::LogOrderMsg(id.GetHex(), "rejecting order, partial order has too many utxos", __FUNCTION__);
        return true;
    }

    xbridge::App & xapp = xbridge::App::instance();
    WalletConnectorPtr sconn = xapp.connectorByCurrency(scurrency);
    WalletConnectorPtr dconn = xapp.connectorByCurrency(dcurrency);
    if (!sconn || !dconn)
    {
        xbridge::LogOrderMsg(id.GetHex(), "no connector for " + (!sconn ? scurrency : dcurrency), __FUNCTION__);
        return true;
    }

    double commonAmount = 0;

    // utxo items
    std::vector<wallet::UtxoEntry> utxoItems;
    {
        // items
        for (uint32_t i = 0; i < utxoItemsCount; ++i)
        {
            const static uint32_t utxoItemSize = XBridgePacket::hashSize + sizeof(uint32_t) +
                                                 XBridgePacket::addressSize + XBridgePacket::signatureSize;
            if (packet->size() < offset+utxoItemSize)
            {
                xbridge::LogOrderMsg(id.GetHex(), "bad packet size while reading utxo items, packet dropped", __FUNCTION__);
                return true;
            }

            wallet::UtxoEntry entry;

            std::vector<unsigned char> stxid(packet->data()+offset, packet->data()+offset+XBridgePacket::hashSize);
            uint256 txid(stxid);
            offset += XBridgePacket::hashSize;

            entry.txId = txid.ToString();

            entry.vout = *static_cast<uint32_t *>(static_cast<void *>(packet->data()+offset));
            offset += sizeof(uint32_t);

            entry.rawAddress = std::vector<unsigned char>(packet->data()+offset, packet->data()+offset+20);
            offset += XBridgePacket::addressSize;

            entry.address = sconn->fromXAddr(entry.rawAddress);

            entry.signature = std::vector<unsigned char>(packet->data()+offset, packet->data()+offset+XBridgePacket::signatureSize);
            offset += XBridgePacket::signatureSize;

            if (!sconn->getTxOut(entry))
            {
                UniValue log_obj(UniValue::VOBJ);
                log_obj.pushKV("orderid", id.GetHex());
                log_obj.pushKV("utxo_txid", entry.txId);
                log_obj.pushKV("utxo_vout", static_cast<int>(entry.vout));
                xbridge::LogOrderMsg(log_obj, "bad utxo entry", __FUNCTION__);
                continue;
            }

            // check signature
            std::string signature = EncodeBase64(&entry.signature[0], entry.signature.size());
            if (!sconn->verifyMessage(entry.address, entry.toString(), signature))
            {
                UniValue log_obj(UniValue::VOBJ);
                log_obj.pushKV("orderid", id.GetHex());
                log_obj.pushKV("utxo_txid", entry.txId);
                log_obj.pushKV("utxo_vout", static_cast<int>(entry.vout));
                xbridge::LogOrderMsg(log_obj, "bad utxo signature", __FUNCTION__);
                continue;
            }

            commonAmount += entry.amount;

            utxoItems.push_back(entry);
        }
    }

    if (utxoItems.empty())
    {
        xbridge::LogOrderMsg(id.GetHex(), "order rejected, utxo items are empty <", __FUNCTION__);
        return true;
    }

    if (xBridgeAmountFromReal(commonAmount) < samount)
    {
        xbridge::LogOrderMsg(id.GetHex(), "order rejected, amount from utxo items <" + std::to_string(commonAmount) + "> "
                                          "less than required <" + std::to_string(static_cast<double>(samount)/static_cast<double>(TransactionDescr::COIN)) + "> ", __FUNCTION__);
        return true;
    }

    // check dust amount
    if (sconn->isDustAmount(static_cast<double>(samount) / TransactionDescr::COIN) ||
        sconn->isDustAmount(commonAmount - (static_cast<double>(samount) / TransactionDescr::COIN)) ||
        dconn->isDustAmount(static_cast<double>(damount) / TransactionDescr::COIN))
    {
        xbridge::LogOrderMsg(id.GetHex(), "order rejected, order amount is dust", __FUNCTION__);
        return true;
    }

    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", id.GetHex());
        log_obj.pushKV("from_addr", HexStr(saddr));
        log_obj.pushKV("from_currency", scurrency);
        log_obj.pushKV("from_amount", std::to_string(samount));
        log_obj.pushKV("to_addr", HexStr(daddr));
        log_obj.pushKV("to_currency", dcurrency);
        log_obj.pushKV("to_amount", std::to_string(damount));
        xbridge::LogOrderMsg(log_obj, "received order", __FUNCTION__);
    }

    CHashWriter ss(SER_GETHASH, 0);
    ss << saddr
       << scurrency
       << samount
       << daddr
       << dcurrency
       << damount
       << timestamp
       << blockHash
       << utxoItems.at(0).signature;
    uint256 checkId = ss.GetHash();
    if(checkId != id)
    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", id.GetHex());
        log_obj.pushKV("expected_orderid", checkId.GetHex());
        xbridge::LogOrderMsg(log_obj, "orderid from packet differs from hash of order body", __FUNCTION__);
        return true;
    }

    // check utxo items
    if (!e.checkUtxoItems(id, utxoItems))
    {
        xbridge::LogOrderMsg(id.GetHex(), "order rejected, utxo check failed", __FUNCTION__);
        return true;
    }

    {
        bool isCreated = false;
        if (!e.createTransaction(id,
                                 saddr, scurrency, samount,
                                 daddr, dcurrency, damount,
                                 timestamp, mpubkey, utxoItems,
                                 blockHash, isCreated, isPartialOrder, minFromAmount))
        {
            // not created
            xbridge::LogOrderMsg(id.GetHex(), "failed to create order", __FUNCTION__);
            return true;
        }
        
        TransactionPtr tr = e.pendingTransaction(id);

        if (isCreated)
        {
            {
                TransactionDescrPtr d(new TransactionDescr);
                d->id           = id;
                d->fromCurrency = scurrency;
                d->fromAmount   = samount;
                d->toCurrency   = dcurrency;
                d->toAmount     = damount;
                d->state        = TransactionDescr::trPending;
                d->blockHash    = blockHash;

                if (isPartialOrder) {
                    d->allowPartialOrders();
                    d->minFromAmount = minFromAmount;
                }

                xbridge::LogOrderMsg(d, __FUNCTION__);

                // Set role 'A' utxos used in the order
                tr->a_setUtxos(utxoItems);

                xbridge::LogOrderMsg(tr, __FUNCTION__);

                xuiConnector.NotifyXBridgeTransactionReceived(d);
            }
        }

        if (!tr->matches(id)) { // couldn't find order
            xbridge::LogOrderMsg(id.GetHex(), "failed to find order after it was created", __FUNCTION__);
            return true;
        }

        sendTransaction(id);
    }

    return true;
}

//******************************************************************************
// broadcast
//******************************************************************************
bool Session::Impl::processPendingTransaction(XBridgePacketPtr packet) const
{
    Exchange & e = Exchange::instance();
    if (e.isEnabled())
    {
        return true;
    }

    DEBUG_TRACE();

    if (packet->size() != 134)
    {
        ERR() << "incorrect packet size for xbcPendingTransaction "
              << "need 134 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    std::vector<unsigned char> stxid(packet->data(), packet->data()+XBridgePacket::hashSize);
    uint256 txid(stxid);
    uint32_t offset = XBridgePacket::hashSize;

    std::string scurrency = std::string(reinterpret_cast<const char *>(packet->data()+offset));
    offset += 8;
    uint64_t samount = *reinterpret_cast<boost::uint64_t *>(packet->data()+offset);
    offset += sizeof(uint64_t);

    std::string dcurrency = std::string(reinterpret_cast<const char *>(packet->data()+offset));
    offset += 8;
    uint64_t damount = *reinterpret_cast<boost::uint64_t *>(packet->data()+offset);
    offset += sizeof(uint64_t);

    auto hubAddress = std::vector<unsigned char>(packet->data()+offset, packet->data()+offset+XBridgePacket::addressSize);

    xbridge::App & xapp = App::instance();
    TransactionDescrPtr ptr = xapp.transaction(txid);

    // Servicenode pubkey assigned to order
    std::vector<unsigned char> spubkey(packet->pubkey(), packet->pubkey()+XBridgePacket::pubkeySize);

    // Reject if snode key doesn't match original (prevent order manipulation)
    if (ptr && !packet->verify(ptr->sPubKey)) {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", txid.GetHex());
        log_obj.pushKV("expected_snode", HexStr(ptr->sPubKey));
        log_obj.pushKV("received_snode", HexStr(spubkey));
        log_obj.pushKV("hubaddress", HexStr(hubAddress));
        xbridge::LogOrderMsg(log_obj, "wrong servicenode handling order", __FUNCTION__);
        return true;
    }

    // All traders verify sig, important when tx ptr is not known yet
    if (!packet->verify(spubkey))
    {
        xbridge::LogOrderMsg(txid.GetHex(), "bad snode packet signature on order", __FUNCTION__);
        return true;
    }

    WalletConnectorPtr sconn = xapp.connectorByCurrency(scurrency);
    WalletConnectorPtr dconn = xapp.connectorByCurrency(dcurrency);
    bool nowalletswitch = gArgs.GetBoolArg("-dxnowallets", settings().showAllOrders());
    if ((!sconn || !dconn) && !nowalletswitch)
    {
        xbridge::LogOrderMsg(txid.GetHex(), "no connector for <" + (!sconn ? scurrency : dcurrency) + ">", __FUNCTION__);
        return true;
    }

    // If the order state is canceled and the snode rebroadcasts, allow the
    // client the opportunity to re-accept the order.
    if (ptr && ptr->state != TransactionDescr::trCancelled)
    {
        if (ptr->state > TransactionDescr::trPending)
        {
            xbridge::LogOrderMsg(ptr->id.GetHex(), "already received order", __FUNCTION__);
            return true;
        }

        if (ptr->state == TransactionDescr::trNew)
        {
            xbridge::LogOrderMsg(ptr->id.GetHex(), "received confirmed order from snode, setting status to pending", __FUNCTION__);
            ptr->state = TransactionDescr::trPending;
        }

        offset += XBridgePacket::addressSize; // hub address
        offset += sizeof(uint64_t);           // created time
        offset += XBridgePacket::hashSize;    // block hash

        bool isPartialOrderAllowed = *reinterpret_cast<boost::uint16_t *>(packet->data()+offset) == 1;
        if (isPartialOrderAllowed)
            ptr->allowPartialOrders();
        offset += sizeof(uint16_t);

        uint64_t minFromAmount = *reinterpret_cast<boost::uint64_t *>(packet->data()+offset);
        ptr->minFromAmount = minFromAmount;
        offset += sizeof(uint64_t);

        // update timestamp
        ptr->updateTimestamp();

        xbridge::LogOrderMsg(ptr, __FUNCTION__);

        xuiConnector.NotifyXBridgeTransactionChanged(ptr->id);

        return true;
    }

    // create tx item
    ptr.reset(new TransactionDescr);
    ptr->id           = txid;

    ptr->fromCurrency = scurrency;
    ptr->fromAmount   = samount;
    ptr->origFromCurrency = scurrency;
    ptr->origFromAmount = samount;

    ptr->toCurrency   = dcurrency;
    ptr->toAmount     = damount;
    ptr->origToCurrency = dcurrency;
    ptr->origToAmount = damount;

    ptr->hubAddress   = hubAddress;
    offset += XBridgePacket::addressSize;

    ptr->created      = xbridge::intToTime(*reinterpret_cast<boost::uint64_t *>(packet->data()+offset));
    offset += sizeof(uint64_t);

    ptr->state        = TransactionDescr::trPending;
    ptr->sPubKey      = spubkey;

    std::vector<unsigned char> sblockhash(packet->data()+offset, packet->data()+offset+XBridgePacket::hashSize);
    ptr->blockHash    = uint256(sblockhash);
    offset += XBridgePacket::hashSize;

    bool isPartialOrderAllowed = *reinterpret_cast<boost::uint16_t *>(packet->data()+offset) == 1;
    if (isPartialOrderAllowed)
        ptr->allowPartialOrders();
    offset += sizeof(uint16_t);

    uint64_t minFromAmount = *reinterpret_cast<boost::uint64_t *>(packet->data()+offset);
    ptr->minFromAmount = minFromAmount;
    offset += sizeof(uint64_t);

    xapp.appendTransaction(ptr);

    xbridge::LogOrderMsg(ptr->id.GetHex(), "received pending order", __FUNCTION__);
    xbridge::LogOrderMsg(ptr, __FUNCTION__);

    xuiConnector.NotifyXBridgeTransactionReceived(ptr);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::processTransactionAccepting(XBridgePacketPtr packet) const
{
    // check and process packet if bridge is exchange
    Exchange & e = Exchange::instance();
    if (!e.isStarted())
    {
        return true;
    }

    DEBUG_TRACE();

    if (!checkPacketAddress(packet))
    {
        return true;
    }

    // size must be >= 188 bytes
    if (packet->size() < 188)
    {
        ERR() << "invalid packet size for xbcTransactionAccepting "
              << "need min 188 bytes, received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    uint32_t offset = XBridgePacket::addressSize;

    // read packet data
    std::vector<unsigned char> sid(packet->data()+offset, packet->data()+offset+XBridgePacket::hashSize);
    uint256 id(sid);
    offset += XBridgePacket::hashSize;

    // snode fee hex
    uint32_t feeTxSize = *static_cast<uint32_t *>(static_cast<void *>(packet->data()+offset));
    offset += sizeof(uint32_t);
    std::vector<unsigned char> feeTx(packet->data()+offset, packet->data()+offset+feeTxSize);
    offset += feeTxSize;

    // Process the service node fee transaction before proceeding
    CTransactionRef feeTxRef = nullptr;
    WalletParam wp;
    auto & smgr = sn::ServiceNodeMgr::instance();
    auto snodeEntry = smgr.getActiveSn();
    if (!snodeEntry.isNull()) {
        CMutableTransaction mtx;
        if (!DecodeHexTx(mtx, HexStr(feeTx), true)) {
            UniValue o(UniValue::VOBJ);
            o.pushKV("orderid", id.GetHex());
            o.pushKV("fee_tx", HexStr(feeTx));
            xbridge::LogOrderMsg(o, "taker submitted bad BLOCK fee (1)", __FUNCTION__);
            sendRejectTransaction(id, crBadFeeTx);
            return false;
        }
        feeTxRef = MakeTransactionRef(std::move(mtx));
        auto snode = smgr.getSn(snodeEntry.key.GetPubKey());
        bool hasAddr{false};
        for (const auto & o : feeTxRef->vout) {
            if (o.nValue < wp.serviceNodeFee * COIN)
                continue;
            auto addr1 = o.scriptPubKey == GetScriptForDestination(snodeEntry.address);
            auto addr2 = !snode.isNull() && o.scriptPubKey == GetScriptForDestination(CTxDestination(snode.getPaymentAddress()));
            if (addr1 || addr2) {
                hasAddr = true;
                break;
            }
        }
        if (!hasAddr) {
            UniValue o(UniValue::VOBJ);
            o.pushKV("orderid", id.GetHex());
            o.pushKV("fee_tx", HexStr(feeTx));
            xbridge::LogOrderMsg(o, "taker submitted bad BLOCK fee (2)", __FUNCTION__);
            sendRejectTransaction(id, crBadFeeTx);
            return false;
        }
    }

    // source
    std::vector<unsigned char> saddr(packet->data()+offset, packet->data()+offset+XBridgePacket::addressSize);
    offset += XBridgePacket::addressSize;
    std::string scurrency((const char *)packet->data()+offset);
    offset += 8;
    uint64_t samount = *static_cast<uint64_t *>(static_cast<void *>(packet->data()+offset));
    offset += sizeof(uint64_t);
    uint32_t sourceHeight = *static_cast<uint32_t *>(static_cast<void *>(packet->data() + offset));
    offset += sizeof(uint32_t);
    std::vector<unsigned char> sourceHashRaw(packet->data() + offset, packet->data() + offset + 8);
    std::string sourceHashTruncated(sourceHashRaw.begin(), sourceHashRaw.end());
    offset += 8;

    // destination
    std::vector<unsigned char> daddr(packet->data()+offset, packet->data()+offset+XBridgePacket::addressSize);
    offset += XBridgePacket::addressSize;
    std::string dcurrency((const char *)packet->data()+offset);
    offset += 8;
    uint64_t damount = *static_cast<uint64_t *>(static_cast<void *>(packet->data()+offset));
    offset += sizeof(uint64_t);
    uint32_t destinationHeight = *static_cast<uint32_t *>(static_cast<void *>(packet->data() + offset));
    offset += sizeof(uint32_t);
    std::vector<unsigned char> destinationHashRaw(packet->data() + offset, packet->data() + offset + 8);
    std::string destinationHashTruncated(destinationHashRaw.begin(), destinationHashRaw.end());
    offset += 8;

    std::vector<unsigned char> mpubkey(packet->pubkey(), packet->pubkey()+XBridgePacket::pubkeySize);

    // If order already accepted, ignore further attempts
    TransactionPtr trExists = e.transaction(id);
    if (trExists->matches(id)) {
        xbridge::LogOrderMsg(id.GetHex(), "order already accepted", __FUNCTION__);
        return true;
    }

    if (!packet->verify(mpubkey))
    {
        xbridge::LogOrderMsg(id.GetHex(), "invalid counterparty packet signature for order", __FUNCTION__);
        return true;
    }

    xbridge::App & xapp = xbridge::App::instance();
    WalletConnectorPtr conn = xapp.connectorByCurrency(scurrency);
    WalletConnectorPtr connDest = xapp.connectorByCurrency(dcurrency);
    if (!conn || !connDest) {
        xbridge::LogOrderMsg(id.GetHex(), "no connector for <" + (!conn ? scurrency : dcurrency) + ">", __FUNCTION__);
        sendRejectTransaction(id, crRpcError);
        return true;
    }

    // Obtain the block heights and hashes from both tokens involved in the order
    // and compare with those reported by the taker. If there are any discrepancies
    // reject the taker's request.
    //
    // Criteria:
    // 1) Taker's block heights must be within 2 blocks of snode's
    // 2) Taker's block hash must match the snode's block hash for the reported
    //    token heights. This lowers risk of pairing counterparies on forks
    {
        uint32_t sourceBlockHeightSnode;
        std::string sourceBlockHashSnode;
        std::string sourceBlockHashCounterparty;
        uint32_t destinationBlockHeightSnode;
        std::string destinationBlockHashSnode;
        std::string destinationBlockHashCounterparty;
        if (conn->getBlockCount(sourceBlockHeightSnode) && conn->getBlockHash(sourceBlockHeightSnode, sourceBlockHashSnode)
           && connDest->getBlockCount(destinationBlockHeightSnode) && connDest->getBlockHash(destinationBlockHeightSnode, destinationBlockHashSnode)
           && conn->getBlockHash(sourceHeight, sourceBlockHashCounterparty) && connDest->getBlockHash(destinationHeight, destinationBlockHashCounterparty))
        {
            // make sure source height is within 2 blocks
            if (abs((int)sourceBlockHeightSnode - (int)sourceHeight) > 1) {
                xbridge::LogOrderMsg(id.GetHex(), "order accept rejected, taker counterparty has out of bounds block height for <" + scurrency + ">", __FUNCTION__);
                sendRejectTransaction(id, crNotAccepted);
                return true;
            }
            // make sure destination height is within 2 blocks
            else if (abs((int)destinationBlockHeightSnode - (int)destinationHeight) > 1) {
                xbridge::LogOrderMsg(id.GetHex(), "order accept rejected, taker counterparty has out of bounds block height for <" + dcurrency + ">", __FUNCTION__);
                sendRejectTransaction(id, crNotAccepted);
                return true;
            }
            // if taker's source height equals snode's source height make sure the truncated hash matches
            else if (sourceBlockHeightSnode == sourceHeight && sourceBlockHashSnode.substr(0, sourceHashTruncated.size()) != sourceHashTruncated) {
                xbridge::LogOrderMsg(id.GetHex(), "order accept rejected, taker counterparty has bad block hash for <" + scurrency + ">", __FUNCTION__);
                sendRejectTransaction(id, crNotAccepted);
                return true;
            }
            // if taker's destination height equals snode's destination height make sure the truncated hash matches
            else if (destinationBlockHeightSnode == destinationHeight && destinationBlockHashSnode.substr(0, destinationHashTruncated.size()) != destinationHashTruncated) {
                xbridge::LogOrderMsg(id.GetHex(), "order accept rejected, taker counterparty has bad block hash for <" + dcurrency + ">", __FUNCTION__);
                sendRejectTransaction(id, crNotAccepted);
                return true;
            }
            // if taker's source height is different than snode, make sure the snode's block hash on reported taker source height matches
            else if (sourceBlockHashCounterparty.substr(0, sourceHashTruncated.size()) != sourceHashTruncated) {
                xbridge::LogOrderMsg(id.GetHex(), "order accept rejected, taker counterparty might be on fork, it has bad block hash for <" + scurrency + ">", __FUNCTION__);
                sendRejectTransaction(id, crNotAccepted);
                return true;
            }
            // if taker's destination height is different than snode, make sure the snode's block hash on reported taker destination height matches
            else if (destinationBlockHashCounterparty.substr(0, destinationHashTruncated.size()) != destinationHashTruncated) {
                xbridge::LogOrderMsg(id.GetHex(), "order accept rejected, taker counterparty might be on fork, it has bad block hash for <" + dcurrency + ">", __FUNCTION__);
                sendRejectTransaction(id, crNotAccepted);
                return true;
            }
        } else {
            xbridge::LogOrderMsg(id.GetHex(), "order accept rejected, snode rpc error", __FUNCTION__);
            sendRejectTransaction(id, crRpcError);
            return true;
        }

    }

    TransactionPtr trPending = e.pendingTransaction(id);
    if (!trPending->matches(id)) {
        xbridge::LogOrderMsg(id.GetHex(), "order not found", __FUNCTION__);
        return true;
    }
    if (trPending->accepting()) {
        xbridge::LogOrderMsg(id.GetHex(), "order already accepted", __FUNCTION__);
        return true;
    }
    trPending->setAccepting(true);

    //
    // Check if maker utxos are still valid
    //

    WalletConnectorPtr makerConn = xapp.connectorByCurrency(trPending->a_currency());
    if (!makerConn) {
        trPending->setAccepting(false);
        xbridge::LogOrderMsg(id.GetHex(), "no maker connector for <" + trPending->a_currency() + ">", __FUNCTION__);
        sendRejectTransaction(id, crRpcError);
        return true;
    }

    auto & makerUtxos = trPending->a_utxos();
    for (auto entry : makerUtxos) {
        if (!makerConn->getTxOut(entry)) {
            // Invalid utxos cancel order
            UniValue log_obj(UniValue::VOBJ);
            log_obj.pushKV("orderid", id.GetHex());
            log_obj.pushKV("utxo_txid", entry.txId);
            log_obj.pushKV("utxo_vout", static_cast<int>(entry.vout));
            xbridge::LogOrderMsg(log_obj, "order rejected, bad maker utxo in order, canceling", __FUNCTION__);
            sendCancelTransaction(trPending, crBadAUtxo);
            return false;
        }
    }

    //
    // END Check if maker utxos are still valid
    //

    double commonAmount = 0;

    // utxo items
    std::vector<wallet::UtxoEntry> utxoItems;
    {
        // array size
        uint32_t utxoItemsCount = *static_cast<uint32_t *>(static_cast<void *>(packet->data()+offset));
        offset += sizeof(uint32_t);

        // items
        for (uint32_t i = 0; i < utxoItemsCount; ++i)
        {
            const static uint32_t utxoItemSize = XBridgePacket::hashSize + sizeof(uint32_t) +
                                                 XBridgePacket::addressSize + XBridgePacket::signatureSize;
            if (packet->size() < offset+utxoItemSize)
            {
                trPending->setAccepting(false);
                xbridge::LogOrderMsg(id.GetHex(), "taker order rejected, bad packet size while reading utxo items, packet dropped", __FUNCTION__);
                sendRejectTransaction(id, crBadBUtxo);
                return true;
            }

            wallet::UtxoEntry entry;

            std::vector<unsigned char> stxid(packet->data()+offset, packet->data()+offset+XBridgePacket::hashSize);
            uint256 txid(stxid);
            offset += XBridgePacket::hashSize;

            entry.txId = txid.ToString();

            entry.vout = *static_cast<uint32_t *>(static_cast<void *>(packet->data()+offset));
            offset += sizeof(uint32_t);

            entry.rawAddress = std::vector<unsigned char>(packet->data()+offset,
                                                          packet->data()+offset+XBridgePacket::addressSize);
            offset += XBridgePacket::addressSize;

            entry.address = conn->fromXAddr(entry.rawAddress);

            entry.signature = std::vector<unsigned char>(packet->data()+offset,
                                                         packet->data()+offset+XBridgePacket::signatureSize);
            offset += XBridgePacket::signatureSize;

            if (!conn->getTxOut(entry))
            {
                UniValue log_obj(UniValue::VOBJ);
                log_obj.pushKV("orderid", id.GetHex());
                log_obj.pushKV("utxo_txid", entry.txId);
                log_obj.pushKV("utxo_vout", static_cast<int>(entry.vout));
                xbridge::LogOrderMsg(log_obj, "bad utxo entry", __FUNCTION__);
                continue;
            }

            // check signature
            std::string signature = EncodeBase64(&entry.signature[0], entry.signature.size());
            if (!conn->verifyMessage(entry.address, entry.toString(), signature))
            {
                UniValue log_obj(UniValue::VOBJ);
                log_obj.pushKV("orderid", id.GetHex());
                log_obj.pushKV("utxo_txid", entry.txId);
                log_obj.pushKV("utxo_vout", static_cast<int>(entry.vout));
                xbridge::LogOrderMsg(log_obj, "bad utxo signature", __FUNCTION__);
                continue;
            }

            commonAmount += entry.amount;

            utxoItems.push_back(entry);
        }
    }

    // Total amount included in taker utxos
    const auto camount = xBridgeAmountFromReal(commonAmount);

    // Check that supplied utxos covers taker's reported source amount
    if (camount < samount)
    {
        trPending->setAccepting(false);
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", id.GetHex());
        log_obj.pushKV("expecting_amount", xbridge::xBridgeValueFromAmount(samount));
        log_obj.pushKV("received_amount", commonAmount);
        xbridge::LogOrderMsg(log_obj, "taker order rejected, insufficient funds", __FUNCTION__);
        sendRejectTransaction(id, crNoMoney);
        return true;
    }

    // Reject orders that do not have enough coin
    if (!trPending->isPartialAllowed() && samount < trPending->b_amount())
    {
        trPending->setAccepting(false);
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", id.GetHex());
        log_obj.pushKV("expecting_amount", xbridge::xBridgeValueFromAmount(trPending->b_amount()));
        log_obj.pushKV("received_amount", samount);
        xbridge::LogOrderMsg(log_obj, "taker order rejected, insufficient funds", __FUNCTION__);
        sendRejectTransaction(id, crNoMoney);
        return true;
    }

    // Reject partial orders on non-partial order type (requested amount is too small)
    if (!trPending->isPartialAllowed() && damount < trPending->a_amount())
    {
        trPending->setAccepting(false);
        xbridge::LogOrderMsg(id.GetHex(), "taker order rejected, partial takes are not allowed for this order", __FUNCTION__);
        sendRejectTransaction(id, crNotAccepted);
        return true;
    }

    // Reject partial orders where the requested amount is smaller than the minimum
    if (trPending->isPartialAllowed() && damount < trPending->min_partial_amount())
    {
        trPending->setAccepting(false);
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", id.GetHex());
        log_obj.pushKV("expecting_amount", xbridge::xBridgeValueFromAmount(trPending->min_partial_amount()));
        log_obj.pushKV("received_amount", xbridge::xBridgeValueFromAmount(damount));
        xbridge::LogOrderMsg(log_obj, "taker order rejected, take amount must be greater than minimum", __FUNCTION__);
        sendRejectTransaction(id, crNoMoney);
        return true;
    }

    // Reject orders where the requested amount is larger than the maximum
    if (damount > trPending->a_amount())
    {
        trPending->setAccepting(false);
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", id.GetHex());
        log_obj.pushKV("expecting_amount", xbridge::xBridgeValueFromAmount(trPending->a_amount()));
        log_obj.pushKV("received_amount", xbridge::xBridgeValueFromAmount(damount));
        xbridge::LogOrderMsg(log_obj, "taker order rejected, take amount must be smaller than maximum", __FUNCTION__);
        sendRejectTransaction(id, crNotAccepted);
        return true;
    }

    // Reject orders where the taker bid amount is larger than the maker ask amount
    if (samount > trPending->b_amount())
    {
        trPending->setAccepting(false);
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", id.GetHex());
        log_obj.pushKV("expecting_amount", xbridge::xBridgeValueFromAmount(trPending->b_amount()));
        log_obj.pushKV("received_amount", xbridge::xBridgeValueFromAmount(samount));
        xbridge::LogOrderMsg(log_obj, "taker order rejected, offered amount must be less than the ask amount", __FUNCTION__);
        sendRejectTransaction(id, crNotAccepted);
        return true;
    }

    // check dust amount
    const double sourceAmountD = xBridgeValueFromAmount(samount);
    if (conn->isDustAmount(sourceAmountD) || conn->isDustAmount(commonAmount - sourceAmountD))
    {
        trPending->setAccepting(false);
        xbridge::LogOrderMsg(id.GetHex(), "taker order rejected, amount is dust", __FUNCTION__);
        sendRejectTransaction(id, crDust);
        return true;
    }

    if (!e.checkUtxoItems(id, utxoItems))
    {
        trPending->setAccepting(false);
        xbridge::LogOrderMsg(id.GetHex(), "taker order rejected, bad utxos", __FUNCTION__);
        sendRejectTransaction(id, crBadBUtxo);
        return true;
    }

    {
        if (e.acceptTransaction(id, saddr, scurrency, samount, daddr, dcurrency, damount, mpubkey, utxoItems,
                trPending->isPartialAllowed()))
        {
            // check transaction state, if trNew - do nothing,
            // if trJoined = send hold to client
            TransactionPtr tr = e.transaction(id);
            if (!tr->matches(id)) { // ignore no matching orders
                trPending->setAccepting(false);
                xbridge::LogOrderMsg(id.GetHex(), "rejecting taker order request, no order found with id", __FUNCTION__);
                sendRejectTransaction(id, crNotAccepted);
                return true;
            }

            if (tr->state() != xbridge::Transaction::trJoined)
            {
                trPending->setAccepting(false);
                UniValue log_obj(UniValue::VOBJ);
                log_obj.pushKV("orderid", tr->id().GetHex());
                log_obj.pushKV("state", tr->state());
                xbridge::LogOrderMsg(log_obj, "wrong tx state, expecting joined state", __FUNCTION__);
                sendRejectTransaction(tr->id(), crNotAccepted);
                return true;
            }

            // Process snode feetx
            {
                const CAmount highfee{::maxTxFee};
                uint256 txhash;
                std::string errstr;
                const TransactionError err = BroadcastTransaction(feeTxRef, txhash, errstr, highfee);
                if (TransactionError::OK != err) {
                    trPending->setAccepting(false);
                    UniValue o(UniValue::VOBJ);
                    o.pushKV("orderid", id.GetHex());
                    o.pushKV("fee_tx", HexStr(feeTx));
                    xbridge::LogOrderMsg(o, strprintf("unable to process taker BLOCK fee: %s", errstr), __FUNCTION__);
                    sendRejectTransaction(id, crBadFeeTx);
                    return false;
                }
                UniValue msg(UniValue::VOBJ);
                msg.pushKV("orderid", id.GetHex());
                msg.pushKV("fee_txid", txhash.GetHex());
                msg.pushKV("fee_amount", wp.serviceNodeFee);
                xbridge::LogOrderMsg(msg, "taker fee processed for order", __FUNCTION__);
            }

            // Set role 'B' utxos used in the order
            tr->b_setUtxos(utxoItems);

            // Join the partial amounts
            tr->joinPartialAmounts(samount, damount);
            xbridge::LogOrderMsg(tr, __FUNCTION__);

            XBridgePacketPtr reply1(new XBridgePacket(xbcTransactionHold));
            reply1->append(m_myid);
            reply1->append(tr->id().begin(), XBridgePacket::hashSize);
            reply1->append(samount); // taker from amount
            reply1->append(damount); // taker to amount
            reply1->sign(e.pubKey(), e.privKey());

            sendPacketBroadcast(reply1);
        } else {
            trPending->setAccepting(false);
            sendRejectTransaction(id, crNotAccepted);
        }
    }

    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", id.GetHex());
        log_obj.pushKV("from_addr", HexStr(saddr));
        log_obj.pushKV("from_currency", scurrency);
        log_obj.pushKV("from_amount", xbridge::xBridgeStringValueFromAmount(samount));
        log_obj.pushKV("to_addr", HexStr(daddr));
        log_obj.pushKV("to_currency", dcurrency);
        log_obj.pushKV("to_amount", xbridge::xBridgeStringValueFromAmount(damount));
        log_obj.pushKV("partial_minimum", xbridge::xBridgeStringValueFromAmount(trPending->min_partial_amount()));
        log_obj.pushKV("partial_orig_maker_size", xbridge::xBridgeStringValueFromAmount(trPending->a_initial_amount()));
        log_obj.pushKV("partial_orig_taker_size", xbridge::xBridgeStringValueFromAmount(trPending->b_initial_amount()));
        xbridge::LogOrderMsg(log_obj, "accepting taker order request", __FUNCTION__);
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool Session::Impl::processTransactionHold(XBridgePacketPtr packet) const
{

    DEBUG_TRACE();

    if (packet->size() != 68)
    {
        ERR() << "incorrect packet size for xbcTransactionHold "
              << "need 68 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    xbridge::App & xapp = xbridge::App::instance();
    uint32_t offset = 0;

    // servicenode addr
    std::vector<unsigned char> hubAddress(packet->data()+offset,
                                          packet->data()+offset+XBridgePacket::addressSize);
    offset += XBridgePacket::addressSize;

    // read packet data
    std::vector<unsigned char> sid(packet->data()+offset, packet->data()+offset+XBridgePacket::hashSize);
    uint256 id(sid);
    offset += XBridgePacket::hashSize;

    // pubkey from packet
    std::vector<unsigned char> spubkey(packet->pubkey(), packet->pubkey()+XBridgePacket::pubkeySize);

    // requested amounts (taker from and to amounts)
    uint64_t samount = *static_cast<uint64_t *>(static_cast<void *>(packet->data()+offset));
    offset += sizeof(uint64_t);
    uint64_t damount = *static_cast<uint64_t *>(static_cast<void *>(packet->data()+offset));
    offset += sizeof(uint64_t);

    TransactionDescrPtr xtx = xapp.transaction(id);
    if (!xtx)
    {
        xbridge::LogOrderMsg(id.GetHex(), "unknown order", __FUNCTION__);
        return true;
    }
    // Reject if snode key doesn't match original (prevent order manipulation)
    if (!packet->verify(xtx->sPubKey))
    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", id.GetHex());
        log_obj.pushKV("expected_snode", HexStr(xtx->sPubKey));
        log_obj.pushKV("received_snode", HexStr(spubkey));
        log_obj.pushKV("hubaddress", HexStr(hubAddress));
        xbridge::LogOrderMsg(log_obj, "wrong servicenode handling order", __FUNCTION__);
        return true;
    }

    // Make sure that servicenode is still valid and in the snode list
    CPubKey pksnode;
    pksnode.Set(packet->pubkey(), packet->pubkey() + CPubKey::COMPRESSED_PUBLIC_KEY_SIZE);
    if (!pksnode.IsFullyValid()) {
        xbridge::LogOrderMsg(id.GetHex(), "bad snode public key", __FUNCTION__);
        return false;
    }

    // check servicenode
    auto snode = sn::ServiceNodeMgr::instance().getSn(pksnode);
    if (snode.isNull()) {
        // try to uncompress pubkey and search
        if (pksnode.Decompress())
            snode = sn::ServiceNodeMgr::instance().getSn(pksnode);
        if (snode.isNull()) {
            // bad service node, no more
            UniValue log_obj(UniValue::VOBJ);
            log_obj.pushKV("orderid", id.GetHex());
            log_obj.pushKV("snode_pubkey", HexStr(pksnode));
            xbridge::LogOrderMsg(log_obj, "unknown service node", __FUNCTION__);
            return true;
        }
    }

    xbridge::LogOrderMsg(id.GetHex(), "using service node " + HexStr(pksnode) + " for order", __FUNCTION__);

    double makerPrice{0}, takerPrice{0};
    bool failPriceCheck{false};
    // Taker check that amounts match
    if (xtx->role == 'B') {
        makerPrice = xBridgeValueFromAmount(xtx->toAmount) / xBridgeValueFromAmount(xtx->fromAmount);
        takerPrice = xBridgeValueFromAmount(damount) / xBridgeValueFromAmount(samount);
        if (samount != xtx->fromAmount) {
            UniValue log_obj(UniValue::VOBJ);
            log_obj.pushKV("orderid", id.GetHex());
            log_obj.pushKV("received_amount", xbridge::xBridgeStringValueFromAmount(samount));
            log_obj.pushKV("expected_amount", xbridge::xBridgeStringValueFromAmount(xtx->fromAmount));
            xbridge::LogOrderMsg(log_obj, "taker from amount from snode should match expected amount", __FUNCTION__);
            return true;
        } else if (damount != xtx->toAmount) {
            UniValue log_obj(UniValue::VOBJ);
            log_obj.pushKV("orderid", id.GetHex());
            log_obj.pushKV("received_amount", xbridge::xBridgeStringValueFromAmount(damount));
            log_obj.pushKV("expected_amount", xbridge::xBridgeStringValueFromAmount(xtx->toAmount));
            xbridge::LogOrderMsg(log_obj, "taker to amount from snode should match expected amount", __FUNCTION__);
            return true;
        }
        // Taker's source currency is maker's dest currency. That is why
        // toAmount and fromAmount's are flipped below
        failPriceCheck = !xBridgePartialOrderDriftCheck(xtx->origFromAmount, xtx->origToAmount, xtx->fromAmount, xtx->toAmount);
    } else if (xtx->role == 'A') { // Handle taker checks
        makerPrice = xBridgeValueFromAmount(xtx->fromAmount) / xBridgeValueFromAmount(xtx->toAmount);
        takerPrice = xBridgeValueFromAmount(damount) / xBridgeValueFromAmount(samount);
        // Taker cannot take more than maker has available
        if (damount > xtx->fromAmount) {
            UniValue log_obj(UniValue::VOBJ);
            log_obj.pushKV("orderid", id.GetHex());
            log_obj.pushKV("received_amount", xbridge::xBridgeStringValueFromAmount(damount));
            log_obj.pushKV("expected_amount", xbridge::xBridgeStringValueFromAmount(xtx->fromAmount));
            xbridge::LogOrderMsg(log_obj, "taker requesting an amount that is too large", __FUNCTION__);
            return true;
        }

        // Taker cannot offer too much
        if (samount > xtx->toAmount) {
            UniValue log_obj(UniValue::VOBJ);
            log_obj.pushKV("orderid", id.GetHex());
            log_obj.pushKV("received_amount", xbridge::xBridgeStringValueFromAmount(samount));
            log_obj.pushKV("expected_amount", xbridge::xBridgeStringValueFromAmount(xtx->toAmount));
            xbridge::LogOrderMsg(log_obj, "taker sending an amount that is too large", __FUNCTION__);
            return true;
        }

        if (xtx->isPartialOrderAllowed()) {
            // Taker cannot take less than the maker's minimum
            if (damount < xtx->minFromAmount) {
                UniValue log_obj(UniValue::VOBJ);
                log_obj.pushKV("orderid", id.GetHex());
                log_obj.pushKV("received_amount", xbridge::xBridgeStringValueFromAmount(damount));
                log_obj.pushKV("expected_amount", xbridge::xBridgeStringValueFromAmount(xtx->minFromAmount));
                xbridge::LogOrderMsg(log_obj, "taker requesting an amount that is too small", __FUNCTION__);
                return true;
            }
        }
        // Maker's source currency is taker's dest currency.
        failPriceCheck = !xBridgePartialOrderDriftCheck(xtx->fromAmount, xtx->toAmount, samount, damount);
    }

    // Price match check
    if (failPriceCheck) {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", id.GetHex());
        log_obj.pushKV("received_price", xbridge::xBridgeStringValueFromPrice(takerPrice));
        log_obj.pushKV("expected_price", xbridge::xBridgeStringValueFromPrice(makerPrice));
        xbridge::LogOrderMsg(log_obj, "taker price doesn't match maker expected price", __FUNCTION__);
        return true;
    }

    {
        // for xchange node remove tx
        // TODO mark as finished for debug
        Exchange & e = Exchange::instance();
        if (e.isStarted())
        {
            TransactionPtr tr = e.transaction(id);
            if (!tr->matches(id)) // ignore no matching orders
                return true;

            xbridge::LogOrderMsg(tr, __FUNCTION__);

            if (tr->state() != xbridge::Transaction::trJoined)
            {
                e.deletePendingTransaction(id);
            }

            return true;
        }
    }

    if (xtx->state >= TransactionDescr::trHold)
    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", id.GetHex());
        log_obj.pushKV("state", xtx->state);
        xbridge::LogOrderMsg(log_obj, "wrong tx state, expecting hold state", __FUNCTION__);
        return true;
    }

    if (!xtx->isLocal())
    {
        xtx->state = TransactionDescr::trFinished;

        xbridge::LogOrderMsg(id.GetHex(), "tx moving to history", __FUNCTION__);

        xapp.moveTransactionToHistory(id);
        xuiConnector.NotifyXBridgeTransactionChanged(xtx->id);
        return true;
    }

    // processing

    WalletConnectorPtr conn = xapp.connectorByCurrency(xtx->toCurrency);
    if (!conn)
    {
        xbridge::LogOrderMsg(id.GetHex(), "no connector for <" + xtx->toCurrency + ">", __FUNCTION__);
        return true;
    }

    // At this point maker agrees with taker amounts, assign new amounts to maker's order
    if (xtx->role == 'A') {
        xtx->fromAmount = damount;
        xtx->toAmount = samount;
    }

    xtx->state = TransactionDescr::trHold;
    xbridge::LogOrderMsg(xtx, __FUNCTION__);
    xuiConnector.NotifyXBridgeTransactionChanged(id);

    // send hold apply
    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionHoldApply));
    reply->append(hubAddress);
    reply->append(xtx->from);
    reply->append(id.begin(), 32);
    reply->sign(xtx->mPubKey, xtx->mPrivKey);

    sendPacket(hubAddress, reply);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::processTransactionHoldApply(XBridgePacketPtr packet) const
{

    DEBUG_TRACE();

    // size must be eq 72
    if (packet->size() != 72)
    {
        ERR() << "invalid packet size for xbcTransactionHoldApply "
              << "need 72 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    // check is for me
    if (!checkPacketAddress(packet))
    {
        return true;
    }

    Exchange & e = Exchange::instance();
    if (!e.isStarted())
    {
        return true;
    }

    uint32_t offset = XBridgePacket::addressSize;

    std::vector<unsigned char> from(packet->data()+offset,
                                    packet->data()+offset+XBridgePacket::addressSize);
    offset += XBridgePacket::addressSize;

    // transaction id
    std::vector<unsigned char> sid(packet->data()+offset, packet->data()+offset+XBridgePacket::hashSize);
    uint256 id(sid);
    offset += XBridgePacket::hashSize;

    // packet pubkey
    std::vector<unsigned char> pubkey(packet->pubkey(), packet->pubkey()+XBridgePacket::pubkeySize);

    TransactionPtr tr = e.transaction(id);
    if (!tr->matches(id)) // ignore no matching orders
        return true;

    if (!packet->verify(tr->a_pk1()) && !packet->verify(tr->b_pk1()))
    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", id.GetHex());
        log_obj.pushKV("expecting_pubkey1", HexStr(tr->a_pk1()));
        log_obj.pushKV("expecting_pubkey2", HexStr(tr->b_pk1()));
        log_obj.pushKV("received_pubkey", HexStr(pubkey));
        xbridge::LogOrderMsg(log_obj, "bad counterparty packet signature", __FUNCTION__);
        return true;
    }

    if (tr->state() != xbridge::Transaction::trJoined)
    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", tr->id().GetHex());
        log_obj.pushKV("state", tr->state());
        xbridge::LogOrderMsg(log_obj, "wrong tx state, expecting joined state", __FUNCTION__);
        return true;
    }

    tr->updateTimestamp();

    if (!isAddressInTransaction(from, tr))
    {
        LogOrderMsg(id.GetHex(), "bad order address specified", __FUNCTION__);
        sendCancelTransaction(tr, crInvalidAddress);
        return true;
    }

    if (e.updateTransactionWhenHoldApplyReceived(tr, from))
    {
        if (tr->state() == xbridge::Transaction::trHold)
        {
            // send initialize transaction command to clients

            // field length must be 8 bytes
            std::vector<unsigned char> a_currency(8, 0);
            std::vector<unsigned char> b_currency(8, 0);
            {
                std::string tmp = tr->a_currency();
                std::copy(tmp.begin(), tmp.end(), a_currency.begin());
                tmp = tr->b_currency();
                std::copy(tmp.begin(), tmp.end(), b_currency.begin());
            }

            // Maker
            XBridgePacketPtr reply1(new XBridgePacket(xbcTransactionInit));
            reply1->append(tr->a_destination());
            reply1->append(m_myid);
            reply1->append(id.begin(), XBridgePacket::hashSize);
            reply1->append(tr->a_address());
            reply1->append(a_currency);
            reply1->append(tr->a_amount());
            reply1->append(tr->a_destination());
            reply1->append(b_currency);
            reply1->append(tr->b_amount());

            reply1->sign(e.pubKey(), e.privKey());

            sendPacket(tr->a_destination(), reply1);

            // Taker
            XBridgePacketPtr reply2(new XBridgePacket(xbcTransactionInit));
            reply2->append(tr->b_destination());
            reply2->append(m_myid);
            reply2->append(id.begin(), XBridgePacket::hashSize);
            reply2->append(tr->b_address());
            reply2->append(b_currency);
            reply2->append(tr->b_amount());
            reply2->append(tr->b_destination());
            reply2->append(a_currency);
            reply2->append(tr->a_amount());

            reply2->sign(e.pubKey(), e.privKey());

            sendPacket(tr->b_destination(), reply2);
        }
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool Session::Impl::processTransactionInit(XBridgePacketPtr packet) const
{
    DEBUG_TRACE();

    if (packet->size() != 144)
    {
        ERR() << "incorrect packet size for xbcTransactionInit "
              << "need 144 bytes, received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    xbridge::App & xapp = xbridge::App::instance();
    uint32_t offset = 0;

    std::vector<unsigned char> thisAddress(packet->data(),
                                           packet->data()+XBridgePacket::addressSize);
    offset += XBridgePacket::addressSize;

    std::vector<unsigned char> hubAddress(packet->data()+offset,
                                          packet->data()+offset+XBridgePacket::addressSize);
    offset += XBridgePacket::addressSize;

    std::vector<unsigned char> stxid(packet->data()+offset, packet->data()+offset+XBridgePacket::hashSize);
    uint256 txid(stxid);
    offset += XBridgePacket::hashSize;

    TransactionDescrPtr xtx = xapp.transaction(txid);
    if (!xtx)
    {
        LogOrderMsg(txid.GetHex(), "unknown order", __FUNCTION__);
        return true;
    }
    if (!xtx->isLocal())
    {
        LogOrderMsg(txid.GetHex(), "not a local order", __FUNCTION__);
        return true;
    }
    // Reject if snode key doesn't match original (prevent order manipulation)
    std::vector<unsigned char> spubkey(packet->pubkey(), packet->pubkey()+XBridgePacket::pubkeySize);
    if (!packet->verify(xtx->sPubKey))
    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", txid.GetHex());
        log_obj.pushKV("expected_snode", HexStr(xtx->sPubKey));
        log_obj.pushKV("received_snode", HexStr(spubkey));
        log_obj.pushKV("hubaddress", HexStr(hubAddress));
        LogOrderMsg(log_obj, "wrong servicenode handling order", __FUNCTION__);
        return true;
    }
    if (xtx->state >= TransactionDescr::trInitialized)
    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", txid.GetHex());
        log_obj.pushKV("state", xtx->state);
        LogOrderMsg(log_obj, "wrong tx state, expecting initialized state", __FUNCTION__);
        return true;
    }

    std::vector<unsigned char> from(packet->data()+offset,
                                    packet->data()+offset+XBridgePacket::addressSize);
    offset += XBridgePacket::addressSize;
    std::string   fromCurrency(reinterpret_cast<const char *>(packet->data()+offset));
    offset += 8;
    uint64_t      fromAmount(*reinterpret_cast<uint64_t *>(packet->data()+offset));
    offset += sizeof(uint64_t);

    std::vector<unsigned char> to(packet->data()+offset,
                                  packet->data()+offset+XBridgePacket::addressSize);
    offset += XBridgePacket::addressSize;
    std::string   toCurrency(reinterpret_cast<const char *>(packet->data()+offset));
    offset += 8;
    uint64_t      toAmount(*reinterpret_cast<uint64_t *>(packet->data()+offset));
    offset += sizeof(uint64_t);

    if(xtx->id           != txid &&
       xtx->from         != from &&
       xtx->fromCurrency != fromCurrency &&
       xtx->fromAmount   != fromAmount &&
       xtx->to           != to &&
       xtx->toCurrency   != toCurrency &&
       xtx->toAmount     != toAmount)
    {
        LogOrderMsg(txid.GetHex(), "order details do not match expected", __FUNCTION__);
        return true;
    }

    xtx->state = TransactionDescr::trInitialized;
    xuiConnector.NotifyXBridgeTransactionChanged(xtx->id);

    // send initialized
    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionInitialized));
    reply->append(hubAddress);
    reply->append(thisAddress);
    reply->append(txid.begin(), 32);

    reply->sign(xtx->mPubKey, xtx->mPrivKey);

    sendPacket(hubAddress, reply);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::processTransactionInitialized(XBridgePacketPtr packet) const
{
    DEBUG_TRACE();

    // size must be eq 72 bytes
    if (packet->size() != 72)
    {
        ERR() << "invalid packet size for xbcTransactionInitialized "
              << "need 104 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    // check is for me
    if (!checkPacketAddress(packet))
    {
        return true;
    }

    Exchange & e = Exchange::instance();
    if (!e.isStarted())
    {
        return true;
    }

    uint32_t offset{XBridgePacket::addressSize};

    std::vector<unsigned char> from(packet->data()+offset, packet->data()+offset+XBridgePacket::addressSize);
    offset += XBridgePacket::addressSize;

    // transaction id
    std::vector<unsigned char> sid(packet->data()+offset, packet->data()+offset+XBridgePacket::hashSize);
    uint256 id(sid);

    // opponent public key
    std::vector<unsigned char> pk1(packet->pubkey(), packet->pubkey()+XBridgePacket::pubkeySize);

    TransactionPtr tr = e.transaction(id);
    if (!tr->matches(id)) // ignore no matching orders
        return true;
    
    if (!packet->verify(tr->a_pk1()) && !packet->verify(tr->b_pk1()))
    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", tr->id().GetHex());
        log_obj.pushKV("expected_counterparty_a", HexStr(tr->a_pk1()));
        log_obj.pushKV("expected_counterparty_b", HexStr(tr->b_pk1()));
        log_obj.pushKV("received_counterparty", HexStr(pk1));
        LogOrderMsg(log_obj, "bad counterparty packet signature", __FUNCTION__);
        return true;
    }

    if (tr->state() != xbridge::Transaction::trHold)
    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", tr->id().GetHex());
        log_obj.pushKV("state", tr->state());
        LogOrderMsg(log_obj, "wrong tx state, expecting hold state", __FUNCTION__);
        return true;
    }

    tr->updateTimestamp();

    if (!isAddressInTransaction(from, tr))
    {
        LogOrderMsg(tr->id().GetHex(), "invalid transaction address", __FUNCTION__);
        sendCancelTransaction(tr, crInvalidAddress);
        return true;
    }

    if (e.updateTransactionWhenInitializedReceived(tr, from, pk1))
    {
        if (tr->state() == xbridge::Transaction::trInitialized)
        {
            // send create transaction command to clients

            // Send to Maker
            XBridgePacketPtr reply1(new XBridgePacket(xbcTransactionCreateA));
            reply1->append(m_myid);
            reply1->append(id.begin(), 32);
            reply1->append(tr->b_pk1());
            reply1->sign(e.pubKey(), e.privKey());

            sendPacket(tr->a_address(), reply1);

            // TODO Blocknet Unlock maker utxos in partial order
            if (tr->isPartialAllowed())
                e.unlockUtxos(tr->id()); // unlock to allow reposts
        }
    }

    LogOrderMsg(tr, __FUNCTION__);

    return true;
}

//******************************************************************************
//******************************************************************************
bool Session::Impl::isAddressInTransaction(const std::vector<unsigned char> & address,
                                           const TransactionPtr & tx) const
{
    if (tx->a_address() == address ||
        tx->b_address() == address ||
        tx->a_destination() == address ||
        tx->b_destination() == address)
    {
        return true;
    }
    return false;
}

//******************************************************************************
//******************************************************************************
bool Session::Impl::processTransactionCreateA(XBridgePacketPtr packet) const
{
    DEBUG_TRACE();

    if (packet->size() != 85)
    {
        ERR() << "incorrect packet size for xbcTransactionCreateA "
              << "need 85 bytes, received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    std::vector<unsigned char> hubAddress(packet->data(), packet->data()+XBridgePacket::addressSize);
    uint32_t offset = XBridgePacket::addressSize;

    // transaction id
    std::vector<unsigned char> stxid(packet->data()+offset, packet->data()+offset+XBridgePacket::hashSize);
    uint256 txid(stxid);
    offset += XBridgePacket::hashSize;

    // counterparty pubkey
    std::vector<unsigned char> mPubKey(packet->data()+offset, packet->data()+offset+XBridgePacket::pubkeySize);

    xbridge::App & xapp = xbridge::App::instance();

    TransactionDescrPtr xtx = xapp.transaction(txid);
    if (!xtx)
    {
        LogOrderMsg(txid.GetHex(), "unknown order", __FUNCTION__);
        return true;
    }
    if (!xtx->isLocal())
    {
        LogOrderMsg(txid.GetHex(), "not a local order", __FUNCTION__);
        return true;
    }
    // Reject if snode key doesn't match original (prevent order manipulation)
    std::vector<unsigned char> spubkey(packet->pubkey(), packet->pubkey()+XBridgePacket::pubkeySize);
    if (!packet->verify(xtx->sPubKey))
    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", txid.GetHex());
        log_obj.pushKV("expected_snode", HexStr(xtx->sPubKey));
        log_obj.pushKV("received_snode", HexStr(spubkey));
        log_obj.pushKV("hubaddress", HexStr(hubAddress));
        LogOrderMsg(log_obj, "wrong servicenode handling order", __FUNCTION__);
        return true;
    }
    if (xtx->role != 'A')
    {
        LogOrderMsg(txid.GetHex(), "received packet for wrong role, expected role A", __FUNCTION__);
        return true;
    }

    if (xtx->state >= TransactionDescr::trCreated)
    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", txid.GetHex());
        log_obj.pushKV("state", xtx->state);
        LogOrderMsg(log_obj, "wrong tx state, expecting created state", __FUNCTION__);
        return true;
    }

    // connectors
    WalletConnectorPtr connFrom = xapp.connectorByCurrency(xtx->fromCurrency);
    WalletConnectorPtr connTo   = xapp.connectorByCurrency(xtx->toCurrency);
    if (!connFrom || !connTo)
    {
        LogOrderMsg(txid.GetHex(), "no connector for <" + (!connFrom ? xtx->fromCurrency : xtx->toCurrency) + ">", __FUNCTION__);
        sendCancelTransaction(xtx, crRpcError);
        return true;
    }

    const double outAmount = xBridgeValueFromAmount(xtx->fromAmount);
    const CAmount coutAmount = xtx->fromAmount;

    double inAmount = 0;
    CAmount cfee1{0};
    CAmount cfee2 = xBridgeIntFromReal(connFrom->minTxFee2(1, 1));
    CAmount cinAmount = xBridgeIntFromReal(inAmount);
    CAmount coutAmountPlusFees{0};

    std::vector<wallet::UtxoEntry> usedInTx;
    for (auto it = xtx->usedCoins.begin(); it != xtx->usedCoins.end(); ) {
        // if we have enough utxos, skip
        if (inAmount >= xBridgeValueFromAmount(coutAmountPlusFees)) {
            if (!xtx->isPartialOrderAllowed())
                break; // if not partial order, done
            // If this is a partial order store unused utxos for eventual repost
            if (xtx->isPartialRepost()) {
                xtx->repostCoins.push_back(*it);
                it = xtx->usedCoins.erase(it);
            } else
                ++it;
            // next
            continue;
        }
        usedInTx.push_back(*it);
        inAmount += it->amount;
        cinAmount = xBridgeIntFromReal(inAmount);
        cfee1 = xBridgeIntFromReal(connFrom->minTxFee1(usedInTx.size(), 3));
        coutAmountPlusFees = coutAmount+cfee1+cfee2;
        ++it;
    }

    const double fee1 = xBridgeValueFromAmount(cfee1);
    const double fee2 = xBridgeValueFromAmount(cfee2);

    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", txid.GetHex());
        log_obj.pushKV("fee1", xBridgeValueFromAmount(cfee1));
        log_obj.pushKV("fee2", xBridgeValueFromAmount(cfee2));
        log_obj.pushKV("in_amount", inAmount);
        log_obj.pushKV("out_amount", xBridgeValueFromAmount(coutAmountPlusFees));
        UniValue log_utxos(UniValue::VARR);
        for (const auto & entry : usedInTx) {
            UniValue log_utxo(UniValue::VOBJ);
            log_utxo.pushKV("txid", entry.txId);
            log_utxo.pushKV("vout", static_cast<int>(entry.vout));
            log_utxo.pushKV("amount", xBridgeStringValueFromPrice(entry.amount, COIN));
            log_utxos.push_back(log_utxo);
        }
        log_obj.pushKV("utxos", log_utxos);
        LogOrderMsg(log_obj, "utxo and fees for order", __FUNCTION__);
    }

    // check amount
    if (inAmount < xBridgeValueFromAmount(coutAmountPlusFees))
    {
        // no money, cancel transaction
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", txid.GetHex());
        log_obj.pushKV("in_amount", inAmount);
        log_obj.pushKV("out_amount", xBridgeValueFromAmount(coutAmountPlusFees));
        LogOrderMsg(log_obj, "insufficient funds for order: expecting in amount to be >= out amount, canceling", __FUNCTION__);
        sendCancelTransaction(xtx, crNoMoney);
        return true;
    }

    // lock time
    xtx->lockTime         = connFrom->lockTime(xtx->role);
    xtx->opponentLockTime = connTo->lockTime('B');
    if (xtx->lockTime == 0 || xtx->opponentLockTime == 0)
    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", txid.GetHex());
        log_obj.pushKV("my_locktime", static_cast<int>(xtx->lockTime));
        log_obj.pushKV("counterparty_locktime", static_cast<int>(xtx->opponentLockTime));
        LogOrderMsg(log_obj, "order lockTime error: locktimes should be greater than 0, canceling", __FUNCTION__);
        sendCancelTransaction(xtx, crBadLockTime);
        return true;
    }

    // store opponent public key (packet verification)
    xtx->oPubKey = mPubKey;

    // create transactions

    // hash of secret
    std::vector<unsigned char> hx = connTo->getKeyId(xtx->xPubKey);

#ifdef LOG_KEYPAIR_VALUES
    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", txid.GetHex());
        log_obj.pushKV("my_pubkey", HexStr(xtx->mPubKey));
        log_obj.pushKV("my_id", HexStr(connFrom->getKeyId(xtx->mPubKey)));
        log_obj.pushKV("other_pubkey", HexStr(mPubKey));
        log_obj.pushKV("other_id", HexStr(connFrom->getKeyId(mPubKey)));
        log_obj.pushKV("x_secret", HexStr(hx));
        LogOrderMsg(log_obj, "order keypair values, DO NOT SHARE with anyone", __FUNCTION__);
    }
#endif

    // create address for deposit
    connFrom->createDepositUnlockScript(xtx->mPubKey, xtx->oPubKey, hx, xtx->lockTime, xtx->lockScript);
    xtx->lockP2SHAddress = connFrom->scriptIdToString(connFrom->getScriptId(xtx->lockScript));

    auto fromAddr = connFrom->fromXAddr(xtx->from);
    auto toAddr = connTo->fromXAddr(xtx->to);

    // depositTx
    {
        std::vector<xbridge::XTxIn>                  inputs;
        std::vector<std::pair<std::string, double> > outputs;

        // inputs
        wallet::UtxoEntry largestUtxo;
        for (const wallet::UtxoEntry & entry : usedInTx)
        {
            if (entry.amount > largestUtxo.amount)
                largestUtxo = entry;
            inputs.emplace_back(entry.txId, entry.vout, entry.amount);
        }

        // outputs

        // amount
        outputs.push_back(std::make_pair(xtx->lockP2SHAddress, outAmount+fee2));

        // rest
        if (inAmount > outAmount+fee1+fee2)
        {
            double rest = inAmount-outAmount-fee1-fee2;
            if (!connFrom->isDustAmount(rest))
                outputs.push_back(std::make_pair(largestUtxo.address, rest)); // change back to largest input used in order
        }

        if (!connFrom->createDepositTransaction(inputs, outputs, xtx->binTxId, xtx->binTxVout, xtx->binTx))
        {
            // cancel transaction
            LogOrderMsg(txid.GetHex(), "failed to create deposit transaction, canceling", __FUNCTION__);
            TXLOG() << "deposit transaction for order " << xtx->id.ToString() << " (submit manually using sendrawtransaction) "
                    << xtx->fromCurrency << "(" << xbridge::xBridgeStringValueFromAmount(xtx->fromAmount) << " - " << fromAddr << ") / "
                    << xtx->toCurrency   << "(" << xbridge::xBridgeStringValueFromAmount(xtx->toAmount)   << " - " << toAddr   << ") "
                    << "using locktime " << xtx->lockTime << std::endl
                    << xtx->binTx;
            sendCancelTransaction(xtx, crBadADepositTx);
            return true;
        }

        TXLOG() << "deposit transaction for order " << xtx->id.ToString() << " (submit manually using sendrawtransaction) "
                << xtx->fromCurrency << "(" << xbridge::xBridgeStringValueFromAmount(xtx->fromAmount) << " - " << fromAddr << ") / "
                << xtx->toCurrency   << "(" << xbridge::xBridgeStringValueFromAmount(xtx->toAmount)   << " - " << toAddr   << ") "
                << "using locktime " << xtx->lockTime << std::endl
                << xtx->binTx;

    } // depositTx

    // refundTx
    {
        std::vector<xbridge::XTxIn>                  inputs;
        std::vector<std::pair<std::string, double> > outputs;

        // inputs from binTx
        inputs.emplace_back(xtx->binTxId, xtx->binTxVout, outAmount+fee2);

        // outputs
        {
            std::string addr = xtx->refundAddress();
            if (addr.empty()) {
                if (!connFrom->getNewAddress(addr))
                {
                    // cancel order
                    LogOrderMsg(txid.GetHex(), "failed to getnewaddress for refund tx, canceling", __FUNCTION__);
                    sendCancelTransaction(xtx, crRpcError);
                    return true;
                }
            }

            outputs.push_back(std::make_pair(addr, outAmount));
        }

        if (!connFrom->createRefundTransaction(inputs, outputs,
                                               xtx->mPubKey, xtx->mPrivKey,
                                               xtx->lockScript, xtx->lockTime,
                                               xtx->refTxId, xtx->refTx))
        {
            // cancel order
            LogOrderMsg(txid.GetHex(), "failed to create refund transaction, canceling", __FUNCTION__);
            TXLOG() << "refund transaction for order " << xtx->id.ToString() << " (submit manually using sendrawtransaction) "
                    << xtx->fromCurrency << "(" << xbridge::xBridgeStringValueFromAmount(xtx->fromAmount) << " - " << fromAddr << ") / "
                    << xtx->toCurrency   << "(" << xbridge::xBridgeStringValueFromAmount(xtx->toAmount)   << " - " << toAddr   << ")" << std::endl
                    << xtx->refTx;
            sendCancelTransaction(xtx, crBadARefundTx);
            return true;
        }

        TXLOG() << "refund transaction for order " << xtx->id.ToString() << " (submit manually using sendrawtransaction) "
                << xtx->fromCurrency << "(" << xbridge::xBridgeStringValueFromAmount(xtx->fromAmount) << " - " << fromAddr << ") / "
                << xtx->toCurrency   << "(" << xbridge::xBridgeStringValueFromAmount(xtx->toAmount)   << " - " << toAddr   << ")" << std::endl
                << xtx->refTx;

    } // refundTx

    xtx->state = TransactionDescr::trCreated;
    xuiConnector.NotifyXBridgeTransactionChanged(txid);
    
    // Sending deposit
    xtx->sentDeposit();

    // send transactions
    {
        std::string sentid;
        int32_t errCode = 0;
        std::string errorMessage;
        if (connFrom->sendRawTransaction(xtx->binTx, sentid, errCode, errorMessage))
        {
            UniValue log_obj(UniValue::VOBJ);
            log_obj.pushKV("orderid", txid.GetHex());
            log_obj.pushKV("p2sh_txid", xtx->binTxId);
            log_obj.pushKV("sent_id", sentid);
            LogOrderMsg(log_obj,  "successfully submitted p2sh deposit", __FUNCTION__);
            // Save db state after updating watch state on this order
            xapp.watchForSpentDeposit(xtx);
            xapp.saveOrders(true);
        }
        else
        {
            LogOrderMsg(txid.GetHex(), "failed to send p2sh deposit, canceling", __FUNCTION__);
            xtx->failDeposit();
            sendCancelTransaction(xtx, crBadADepositTx);
            return true;
        }
    }

    // send reply
    XBridgePacketPtr reply;
    reply.reset(new XBridgePacket(xbcTransactionCreatedA));

    reply->append(hubAddress);
    reply->append(txid.begin(), 32);
    reply->append(xtx->binTxId);
    reply->append(hx);
    reply->append(xtx->lockTime);
    reply->append(xtx->refTxId);
    reply->append(xtx->refTx);

    reply->sign(xtx->mPubKey, xtx->mPrivKey);

    sendPacket(hubAddress, reply);

    // Repost order
    if (xtx->isPartialRepost()) {
        try {
            const auto status = xapp.repostXBridgeTransaction(xtx->fromAddr, xtx->fromCurrency, xtx->toAddr, xtx->toCurrency,
                    xtx->origFromAmount, xtx->origToAmount, xtx->minFromAmount, xtx->repostCoins, xtx->id);
            if (status == xbridge::INSIFFICIENT_FUNDS)
                LogOrderMsg(xtx->id.GetHex(), "not reposting the partial order because all available utxos have been used up", __FUNCTION__);
            else if (status != xbridge::SUCCESS)
                LogOrderMsg(xtx->id.GetHex(), "failed to repost the partial order", __FUNCTION__);
        } catch (...) {
            LogOrderMsg(xtx->id.GetHex(), "failed to repost the partial order", __FUNCTION__);
        }
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::processTransactionCreatedA(XBridgePacketPtr packet) const
{
    DEBUG_TRACE();

    // size must be > 76 bytes
    if (packet->size() <= 76)
    {
        ERR() << "invalid packet size for xbcTransactionCreatedA "
              << "need more than 76, received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    // check is for me
    if (!checkPacketAddress(packet))
    {
        return true;
    }

    Exchange & e = Exchange::instance();
    if (!e.isStarted())
    {
        return true;
    }

    size_t offset = XBridgePacket::addressSize; // hub address

    std::vector<unsigned char> stxid(packet->data()+offset, packet->data()+offset+XBridgePacket::hashSize);
    uint256 txid(stxid);
    offset += XBridgePacket::hashSize;

    std::string binTxId(reinterpret_cast<const char *>(packet->data()+offset));
    offset += binTxId.size()+1;

    std::vector<unsigned char> hx(packet->data()+offset, packet->data()+offset+XBridgePacket::addressSize);
    offset += XBridgePacket::addressSize;

    uint32_t lockTimeA = *reinterpret_cast<uint32_t *>(packet->data()+offset);
    offset += sizeof(uint32_t);

    std::string refTxId(reinterpret_cast<const char *>(packet->data()+offset));
    offset += refTxId.size()+1;

    std::string refTx(reinterpret_cast<const char *>(packet->data()+offset));

    TransactionPtr tr = e.transaction(txid);
    if (!tr->matches(txid)) // ignore no matching orders
        return true;

    std::vector<unsigned char> pk1(packet->pubkey(), packet->pubkey()+XBridgePacket::pubkeySize);
    if (!packet->verify(tr->a_pk1()))
    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", tr->id().GetHex());
        log_obj.pushKV("expected_counterparty_a", HexStr(tr->a_pk1()));
        log_obj.pushKV("received_counterparty_a", HexStr(pk1));
        LogOrderMsg(log_obj, "bad counterparty_a packet signature", __FUNCTION__);
        return true;
    }

    if (tr->state() != xbridge::Transaction::trInitialized)
    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", tr->id().GetHex());
        log_obj.pushKV("state", tr->state());
        LogOrderMsg(log_obj, "wrong tx state, expecting initialized state", __FUNCTION__);
        return true;
    }

    tr->a_setLockTime(lockTimeA);
    tr->a_setRefundTx(refTxId, refTx);
    tr->updateTimestamp();

    xbridge::App & xapp = xbridge::App::instance();
    xapp.watchTraderDeposit(tr);

    if (e.updateTransactionWhenCreatedReceived(tr, tr->a_address(), binTxId))
    {
        LogOrderMsg(tr->id().GetHex(), "bad state detected on order", __FUNCTION__);
        return true;
    }

    XBridgePacketPtr reply2(new XBridgePacket(xbcTransactionCreateB));
    reply2->append(m_myid);
    reply2->append(txid.begin(), 32);
    reply2->append(tr->a_pk1());
    reply2->append(binTxId);
    reply2->append(hx);
    reply2->append(lockTimeA);

    reply2->sign(e.pubKey(), e.privKey());

    sendPacket(tr->b_address(), reply2);

    LogOrderMsg(tr, __FUNCTION__);

    return true;
}

//******************************************************************************
//******************************************************************************
bool Session::Impl::processTransactionCreateB(XBridgePacketPtr packet) const
{
    DEBUG_TRACE();

    if (packet->size() <= 109)
    {
        ERR() << "incorrect packet size for xbcTransactionCreateB "
              << "need more than 109 bytes, received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    std::vector<unsigned char> hubAddress(packet->data(), packet->data()+XBridgePacket::addressSize);
    uint32_t offset = XBridgePacket::addressSize;

    // transaction id
    std::vector<unsigned char> stxid(packet->data()+offset, packet->data()+offset+XBridgePacket::hashSize);
    uint256 txid(stxid);
    offset += XBridgePacket::hashSize;

    std::vector<unsigned char> mPubKey(packet->data()+offset, packet->data()+offset+XBridgePacket::pubkeySize);
    offset += XBridgePacket::pubkeySize;

    std::string binATxId(reinterpret_cast<const char *>(packet->data()+offset));
    offset += binATxId.size()+1;

    std::vector<unsigned char> hx(packet->data()+offset, packet->data()+offset+XBridgePacket::addressSize);
    offset += XBridgePacket::addressSize;

    uint32_t lockTimeA = *reinterpret_cast<uint32_t *>(packet->data()+offset);

    xbridge::App & xapp = xbridge::App::instance();

    TransactionDescrPtr xtx = xapp.transaction(txid);
    if (!xtx)
    {
        LogOrderMsg(txid.GetHex(), "unknown order", __FUNCTION__);
        return true;
    }
    if (!xtx->isLocal())
    {
        LogOrderMsg(txid.GetHex(), "not a local order", __FUNCTION__);
        return true;
    }
    // Reject if snode key doesn't match original (prevent order manipulation)
    std::vector<unsigned char> spubkey(packet->pubkey(), packet->pubkey()+XBridgePacket::pubkeySize);
    if (!packet->verify(xtx->sPubKey))
    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", txid.GetHex());
        log_obj.pushKV("expected_snode", HexStr(xtx->sPubKey));
        log_obj.pushKV("received_snode", HexStr(spubkey));
        log_obj.pushKV("hubaddress", HexStr(hubAddress));
        LogOrderMsg(log_obj, "wrong servicenode handling order", __FUNCTION__);
        return true;
    }
    if (xtx->state >= TransactionDescr::trCreated)
    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", txid.GetHex());
        log_obj.pushKV("state", xtx->state);
        LogOrderMsg(log_obj, "wrong tx state, expecting created state", __FUNCTION__);
        return true;
    }
    if (binATxId.size() == 0)
    {
        LogOrderMsg(txid.GetHex(), "bad counterparty deposit tx id received for order", __FUNCTION__);
        sendCancelTransaction(xtx, crBadADepositTx);
        return true;
    }
    if (xtx->role != 'B')
    {
        LogOrderMsg(txid.GetHex(), "received packet for wrong role, expected role B", __FUNCTION__);
        return true;
    }
    if(xtx->xPubKey.size() != 0)
    {
        LogOrderMsg(txid.GetHex(), "bad role, xpubkey should be empty", __FUNCTION__);
        return true;
    }

    // connectors
    WalletConnectorPtr connFrom = xapp.connectorByCurrency(xtx->fromCurrency);
    WalletConnectorPtr connTo   = xapp.connectorByCurrency(xtx->toCurrency);
    if (!connFrom || !connTo)
    {
        LogOrderMsg(txid.GetHex(), "no connector for <" + (!connFrom ? xtx->fromCurrency : xtx->toCurrency) + ">", __FUNCTION__);
        sendCancelTransaction(xtx, crRpcError);
        return true;
    }

    double outAmount = static_cast<double>(xtx->fromAmount) / TransactionDescr::COIN;
    double checkAmount = static_cast<double>(xtx->toAmount) / TransactionDescr::COIN;

    // check preliminary lock times for counterparty
    {
        if (lockTimeA == 0 || !connTo->acceptableLockTimeDrift('A', lockTimeA))
        {
            UniValue log_obj(UniValue::VOBJ);
            log_obj.pushKV("orderid", txid.GetHex());
            log_obj.pushKV("expected_counterparty_locktime", static_cast<int>(connTo->lockTime('A')));
            log_obj.pushKV("counterparty_locktime", static_cast<int>(lockTimeA));
            LogOrderMsg(log_obj, "bad locktime from counterparty, canceling", __FUNCTION__);
            sendCancelTransaction(xtx, crBadALockTime);
            return true;
        }
    }

    // Counterparty hashed secret
    xtx->oHashedSecret    = hx;
    // Set lock times
    xtx->lockTime         = connFrom->lockTime('B'); // expected locktime for trader B (me)
    xtx->opponentLockTime = lockTimeA;

    // Generate counterparty script
    std::vector<unsigned char> counterPartyScript;
    connTo->createDepositUnlockScript(mPubKey, xtx->mPubKey, xtx->oHashedSecret, xtx->opponentLockTime, counterPartyScript);
    std::string counterPartyP2SH = connTo->scriptIdToString(connTo->getScriptId(counterPartyScript));
    auto counterPartyScriptHex = HexStr(CScript() << OP_HASH160 << connTo->getScriptId(counterPartyScript) << OP_EQUAL);

    // Counter party voutN
    uint32_t counterPartyVoutN = 0;

    // check A deposit tx and check that counterparty script is valid in counterparty deposit tx
    {
        uint64_t p2shAmount{0};
        bool isGood = false;
        if (!connTo->checkDepositTransaction(binATxId, std::string(), checkAmount, p2shAmount, counterPartyVoutN, counterPartyScriptHex, xtx->oOverpayment, isGood))
        {
            // move packet to pending
            xapp.processLater(txid, packet);
            return true;
        }
        else if (!isGood)
        {
            UniValue log_obj(UniValue::VOBJ);
            log_obj.pushKV("orderid", txid.GetHex());
            log_obj.pushKV("counterparty_p2shdeposit_txid", binATxId);
            LogOrderMsg(log_obj, "bad counterparty deposit for order, canceling", __FUNCTION__);
            sendCancelTransaction(xtx, crBadADepositTx);
            return true;
        }
        else {
            xtx->oBinTxP2SHAmount = p2shAmount;
        }

        LogOrderMsg(txid.GetHex(), "counterparty deposit confirmed for order", __FUNCTION__);
    }

    double fee1      = 0;
    double fee2      = connFrom->minTxFee2(1, 1);
    double inAmount  = 0;

    std::vector<wallet::UtxoEntry> usedInTx;
    for (const wallet::UtxoEntry & entry : xtx->usedCoins)
    {
        usedInTx.push_back(entry);
        inAmount += entry.amount;
        fee1 = connFrom->minTxFee1(usedInTx.size(), 3);

        // check amount
        if (inAmount >= outAmount+fee1+fee2)
        {
            break;
        }
    }

    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", txid.GetHex());
        log_obj.pushKV("fee1", fee1);
        log_obj.pushKV("fee2", fee2);
        log_obj.pushKV("in_amount", inAmount);
        log_obj.pushKV("out_amount", outAmount + fee1 + fee2);
        UniValue log_utxos(UniValue::VARR);
        for (const auto & entry : usedInTx) {
            UniValue log_utxo(UniValue::VOBJ);
            log_utxo.pushKV("txid", entry.txId);
            log_utxo.pushKV("vout", static_cast<int>(entry.vout));
            log_utxo.pushKV("amount", xBridgeStringValueFromPrice(entry.amount, COIN));
            log_utxos.push_back(log_utxo);
        }
        log_obj.pushKV("utxos", log_utxos);
        LogOrderMsg(log_obj, "utxo and fees for order", __FUNCTION__);
    }

    // check amount
    if (inAmount < outAmount+fee1+fee2)
    {
        // no money, cancel transaction
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", txid.GetHex());
        log_obj.pushKV("in_amount", inAmount);
        log_obj.pushKV("out_amount", outAmount+fee1+fee2);
        LogOrderMsg(log_obj, "insufficient funds for order: expecting in amount to be >= out amount, canceling", __FUNCTION__);
        sendCancelTransaction(xtx, crNoMoney);
        return true;
    }

    // store counterparty public key (packet verification)
    xtx->oPubKey = mPubKey;
    // store counterparty tx info
    xtx->oBinTxId = binATxId;
    xtx->oBinTxVout = counterPartyVoutN;
    // store counterparty script
    xtx->unlockScript = counterPartyScript;
    xtx->unlockP2SHAddress = counterPartyP2SH;

    // create transactions

#ifdef LOG_KEYPAIR_VALUES
    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", txid.GetHex());
        log_obj.pushKV("my_pubkey", HexStr(xtx->mPubKey));
        log_obj.pushKV("my_id", HexStr(connFrom->getKeyId(xtx->mPubKey)));
        log_obj.pushKV("other_pubkey", HexStr(xtx->oPubKey));
        log_obj.pushKV("other_id", HexStr(connFrom->getKeyId(xtx->oPubKey)));
        log_obj.pushKV("x_secret", HexStr(xtx->oHashedSecret));
        LogOrderMsg(log_obj, "unlock script pubkeys, DO NOT SHARE with anyone", __FUNCTION__);
    }
#endif

    // create address for deposit
    connFrom->createDepositUnlockScript(xtx->mPubKey, xtx->oPubKey, xtx->oHashedSecret, xtx->lockTime, xtx->lockScript);
    xtx->lockP2SHAddress = connFrom->scriptIdToString(connFrom->getScriptId(xtx->lockScript));

    auto fromAddr = connFrom->fromXAddr(xtx->from);
    auto toAddr = connTo->fromXAddr(xtx->to);

    // depositTx
    {
        std::vector<xbridge::XTxIn>                  inputs;
        std::vector<std::pair<std::string, double> > outputs;

        // inputs
        wallet::UtxoEntry largestUtxo;
        for (const wallet::UtxoEntry & entry : usedInTx)
        {
            if (entry.amount > largestUtxo.amount)
                largestUtxo = entry;
            inputs.emplace_back(entry.txId, entry.vout, entry.amount);
        }

        // outputs

        // amount
        outputs.push_back(std::make_pair(xtx->lockP2SHAddress, outAmount+fee2));

        // rest
        if (inAmount > outAmount+fee1+fee2)
        {
            double rest = inAmount-outAmount-fee1-fee2;
            if (!connFrom->isDustAmount(rest))
                outputs.push_back(std::make_pair(largestUtxo.address, rest)); // change back to largest input used in order
        }

        if (!connFrom->createDepositTransaction(inputs, outputs, xtx->binTxId, xtx->binTxVout, xtx->binTx))
        {
            // cancel transaction
            LogOrderMsg(txid.GetHex(), "failed to create deposit transaction, canceling", __FUNCTION__);
            TXLOG() << "deposit transaction for order " << xtx->id.ToString() << " (submit manually using sendrawtransaction) "
                    << xtx->fromCurrency << "(" << xbridge::xBridgeStringValueFromAmount(xtx->fromAmount) << " - " << fromAddr << ") / "
                    << xtx->toCurrency   << "(" << xbridge::xBridgeStringValueFromAmount(xtx->toAmount)   << " - " << toAddr   << ") "
                    << "using locktime " << xtx->lockTime << std::endl
                    << xtx->binTx;
            sendCancelTransaction(xtx, crBadBDepositTx);
            return true;
        }

        TXLOG() << "deposit transaction for order " << xtx->id.ToString() << " (submit manually using sendrawtransaction) "
                << xtx->fromCurrency << "(" << xbridge::xBridgeStringValueFromAmount(xtx->fromAmount) << " - " << fromAddr << ") / "
                << xtx->toCurrency   << "(" << xbridge::xBridgeStringValueFromAmount(xtx->toAmount)   << " - " << toAddr   << ") "
                << "using locktime " << xtx->lockTime << std::endl
                << xtx->binTx;

    } // depositTx

    // refundTx
    {
        std::vector<xbridge::XTxIn>                  inputs;
        std::vector<std::pair<std::string, double> > outputs;

        // inputs from binTx
        inputs.emplace_back(xtx->binTxId, xtx->binTxVout, outAmount+fee2);

        // outputs
        {
            std::string addr = xtx->refundAddress();
            if (addr.empty()) {
                if (!connFrom->getNewAddress(addr)) {
                    // cancel order
                    LogOrderMsg(txid.GetHex(), "failed to getnewaddress for refund tx, canceling", __FUNCTION__);
                    sendCancelTransaction(xtx, crRpcError);
                    return true;
                }
            }

            outputs.push_back(std::make_pair(addr, outAmount));
        }

        if (!connFrom->createRefundTransaction(inputs, outputs,
                                               xtx->mPubKey, xtx->mPrivKey,
                                               xtx->lockScript, xtx->lockTime,
                                               xtx->refTxId, xtx->refTx))
        {
            // cancel order
            LogOrderMsg(txid.GetHex(), "failed to create refund transaction, canceling", __FUNCTION__);
            TXLOG() << "refund transaction for order " << xtx->id.ToString() << " "
                    << xtx->fromCurrency << "(" << xbridge::xBridgeStringValueFromAmount(xtx->fromAmount) << " - " << fromAddr << ") / "
                    << xtx->toCurrency   << "(" << xbridge::xBridgeStringValueFromAmount(xtx->toAmount)   << " - " << toAddr   << ")" << std::endl
                    << xtx->refTx;
            sendCancelTransaction(xtx, crBadBRefundTx);
            return true;
        }

        TXLOG() << "refund transaction for order " << xtx->id.ToString() << " "
                << xtx->fromCurrency << "(" << xbridge::xBridgeStringValueFromAmount(xtx->fromAmount) << " - " << fromAddr << ") / "
                << xtx->toCurrency   << "(" << xbridge::xBridgeStringValueFromAmount(xtx->toAmount)   << " - " << toAddr   << ")" << std::endl
                << xtx->refTx;

    } // refundTx


    // send transactions
    {
        // Get the current block
        uint32_t blockCount{0};
        if (!connFrom->getBlockCount(blockCount)) {
            LogOrderMsg(txid.GetHex(), "failed to obtain block count from <" + xtx->fromCurrency + "> blockchain, canceling", __FUNCTION__);
            sendCancelTransaction(xtx, crRpcError);
            return true;
        }
        
        xtx->state = TransactionDescr::trCreated;
        xuiConnector.NotifyXBridgeTransactionChanged(txid);

        // Mark deposit as sent
        xtx->sentDeposit();
        
        std::string sentid;
        int32_t errCode = 0;
        std::string errorMessage;
        if (connFrom->sendRawTransaction(xtx->binTx, sentid, errCode, errorMessage))
        {
            UniValue log_obj(UniValue::VOBJ);
            log_obj.pushKV("orderid", txid.GetHex());
            log_obj.pushKV("p2sh_txid", xtx->binTxId);
            log_obj.pushKV("sent_id", sentid);
            LogOrderMsg(log_obj,  "successfully submitted p2sh deposit", __FUNCTION__);
            xtx->setWatchBlock(blockCount);
            xapp.watchForSpentDeposit(xtx);
            // Save db state after updating watch state on this order
            xapp.saveOrders(true);
        }
        else
        {
            LogOrderMsg(txid.GetHex(), "failed to send p2sh deposit, canceling", __FUNCTION__);
            xtx->failDeposit();
            sendCancelTransaction(xtx, crBadBDepositTx);
            return true;
        }
    }

    // send reply
    XBridgePacketPtr reply;
    reply.reset(new XBridgePacket(xbcTransactionCreatedB));

    reply->append(hubAddress);
    reply->append(txid.begin(), 32);
    reply->append(xtx->binTxId);
    reply->append(xtx->lockTime);
    reply->append(xtx->refTxId);
    reply->append(xtx->refTx);

    reply->sign(xtx->mPubKey, xtx->mPrivKey);
    sendPacket(hubAddress, reply);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::processTransactionCreatedB(XBridgePacketPtr packet) const
{
    DEBUG_TRACE();

    // size must be > 56 bytes
    if (packet->size() <= 56)
    {
        ERR() << "invalid packet size for xbcTransactionCreated "
              << "need more than 56 bytes, received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    // check is for me
    if (!checkPacketAddress(packet))
    {
        return true;
    }

    Exchange & e = Exchange::instance();
    if (!e.isStarted())
    {
        return true;
    }

    size_t offset = XBridgePacket::addressSize; // hub address

    std::vector<unsigned char> stxid(packet->data()+offset, packet->data()+offset+XBridgePacket::hashSize);
    uint256 txid(stxid);
    offset += XBridgePacket::hashSize;

    std::string binTxId(reinterpret_cast<const char *>(packet->data()+offset));
    offset += binTxId.size()+1;

    uint32_t lockTimeB = *reinterpret_cast<uint32_t *>(packet->data()+offset);
    offset += sizeof(uint32_t);

    std::string refTxId(reinterpret_cast<const char *>(packet->data()+offset));
    offset += refTxId.size()+1;

    std::string refTx(reinterpret_cast<const char *>(packet->data()+offset));

    TransactionPtr tr = e.transaction(txid);
    if (!tr->matches(txid)) // ignore no matching orders
        return true;

    std::vector<unsigned char> pk1(packet->pubkey(), packet->pubkey()+XBridgePacket::pubkeySize);
    if (!packet->verify(tr->b_pk1()))
    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", tr->id().GetHex());
        log_obj.pushKV("expected_counterparty_b", HexStr(tr->b_pk1()));
        log_obj.pushKV("received_counterparty_b", HexStr(pk1));
        LogOrderMsg(log_obj, "bad counterparty_b packet signature", __FUNCTION__);
        return true;
    }

    if (tr->state() != xbridge::Transaction::trInitialized)
    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", tr->id().GetHex());
        log_obj.pushKV("state", tr->state());
        LogOrderMsg(log_obj, "wrong tx state, expecting initialized state", __FUNCTION__);
        return true;
    }

    tr->b_setLockTime(lockTimeB);
    tr->b_setRefundTx(refTxId, refTx);
    tr->updateTimestamp();

    if (e.updateTransactionWhenCreatedReceived(tr, tr->b_address(), binTxId))
    {
        if (tr->state() == xbridge::Transaction::trCreated)
        {
            // send confirm packets with deposit tx id
            // for create payment tx

            XBridgePacketPtr reply(new XBridgePacket(xbcTransactionConfirmA));
            reply->append(m_myid);
            reply->append(txid.begin(), 32);
            reply->append(tr->b_bintxid());
            reply->append(lockTimeB);

            reply->sign(e.pubKey(), e.privKey());

            sendPacket(tr->a_destination(), reply);
        }
    }

    LogOrderMsg(tr, __FUNCTION__);

    return true;
}

//******************************************************************************
//******************************************************************************
bool Session::Impl::processTransactionConfirmA(XBridgePacketPtr packet) const
{
    DEBUG_TRACE();

    // size must be > 56 bytes
    if (packet->size() <= 56)
    {
        LOG() << "incorrect packet size for xbcTransactionConfirmA "
              << "need more than 56 bytes, received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    std::vector<unsigned char> hubAddress(packet->data(), packet->data()+XBridgePacket::addressSize);
    size_t offset = XBridgePacket::addressSize;

    std::vector<unsigned char> stxid(packet->data()+offset, packet->data()+offset+XBridgePacket::hashSize);
    uint256 txid(stxid);
    offset += XBridgePacket::hashSize;

    std::string binTxId(reinterpret_cast<const char *>(packet->data()+offset));
    offset += binTxId.size()+1;

    uint32_t lockTimeB = *reinterpret_cast<uint32_t *>(packet->data()+offset);

    xbridge::App & xapp = xbridge::App::instance();

    TransactionDescrPtr xtx = xapp.transaction(txid);
    if (!xtx)
    {
        LogOrderMsg(txid.GetHex(), "unknown order", __FUNCTION__);
        return true;
    }
    if (!xtx->isLocal())
    {
        LogOrderMsg(txid.GetHex(), "not a local order", __FUNCTION__);
        return true;
    }
    // Reject if servicenode key doesn't match original (prevent order manipulation)
    std::vector<unsigned char> spubkey(packet->pubkey(), packet->pubkey()+XBridgePacket::pubkeySize);
    if (!packet->verify(xtx->sPubKey))
    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", txid.GetHex());
        log_obj.pushKV("expected_snode", HexStr(xtx->sPubKey));
        log_obj.pushKV("received_snode", HexStr(spubkey));
        log_obj.pushKV("hubaddress", HexStr(hubAddress));
        LogOrderMsg(log_obj, "wrong servicenode handling order", __FUNCTION__);
        return true;
    }
    if (xtx->state >= TransactionDescr::trCommited)
    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", txid.GetHex());
        log_obj.pushKV("state", xtx->state);
        LogOrderMsg(log_obj, "wrong tx state, expecting committed state", __FUNCTION__);
        return true;
    }
    if (xtx->role != 'A')
    {
        LogOrderMsg(txid.GetHex(), "received packet for wrong role, expected role A", __FUNCTION__);
        return true;
    }

    // connectors
    WalletConnectorPtr connFrom = xapp.connectorByCurrency(xtx->fromCurrency);
    WalletConnectorPtr connTo   = xapp.connectorByCurrency(xtx->toCurrency);
    if (!connFrom || !connTo)
    {
        LogOrderMsg(txid.GetHex(), "no connector for <" + (!connFrom ? xtx->fromCurrency : xtx->toCurrency) + ">, canceling", __FUNCTION__);
        sendCancelTransaction(xtx, crRpcError);
        return true;
    }

    double outAmount   = static_cast<double>(xtx->toAmount)/TransactionDescr::COIN;
    double checkAmount = outAmount;

    // check preliminary lock times for counterparty
    {
        if (lockTimeB == 0 || !connTo->acceptableLockTimeDrift('B', lockTimeB))
        {
            UniValue log_obj(UniValue::VOBJ);
            log_obj.pushKV("orderid", txid.GetHex());
            log_obj.pushKV("expected_counterparty_locktime", static_cast<int>(connTo->lockTime('B')));
            log_obj.pushKV("counterparty_locktime", static_cast<int>(lockTimeB));
            LogOrderMsg(log_obj, "bad locktime from counterparty, canceling", __FUNCTION__);
            sendCancelTransaction(xtx, crBadBLockTime);
            return true;
        }
    }

    // Set counterparty lock time
    xtx->opponentLockTime = lockTimeB;

    // Hash of secret
    std::vector<unsigned char> hx = connTo->getKeyId(xtx->xPubKey);

    // Counterparty script
    std::vector<unsigned char> counterPartyScript;
    connTo->createDepositUnlockScript(xtx->oPubKey, xtx->mPubKey, hx, xtx->opponentLockTime, counterPartyScript);
    std::string counterPartyP2SH = connTo->scriptIdToString(connTo->getScriptId(counterPartyScript));
    auto counterPartyScriptHex = HexStr(CScript() << OP_HASH160 << connTo->getScriptId(counterPartyScript) << OP_EQUAL);

    // Counter party voutN
    uint32_t counterPartyVoutN = 0;

    // check B deposit tx and check that counterparty script is valid in counterparty deposit tx
    {
        uint64_t p2shAmount{0};
        bool isGood = false;
        if (!connTo->checkDepositTransaction(binTxId, std::string(), checkAmount, p2shAmount, counterPartyVoutN, counterPartyScriptHex, xtx->oOverpayment, isGood))
        {
            // move packet to pending
            xapp.processLater(txid, packet);
            return true;
        }
        else if (!isGood)
        {
            UniValue log_obj(UniValue::VOBJ);
            log_obj.pushKV("orderid", txid.GetHex());
            log_obj.pushKV("counterparty_p2shdeposit_txid", binTxId);
            LogOrderMsg(log_obj, "bad counterparty deposit for order, canceling", __FUNCTION__);
            sendCancelTransaction(xtx, crBadBDepositTx);
            return true;
        }
        else {
            xtx->oBinTxP2SHAmount = p2shAmount;
        }

        LogOrderMsg(txid.GetHex(), "counterparty deposit confirmed for order", __FUNCTION__);
    }

    // Set counterparty tx info
    xtx->oBinTxId = binTxId;
    xtx->oBinTxVout = counterPartyVoutN;
    // Set counterparty script
    xtx->unlockScript = counterPartyScript;
    xtx->unlockP2SHAddress = counterPartyP2SH;

    // payTx
    {
        int32_t errCode = 0;
        if (!redeemOrderCounterpartyDeposit(xtx, errCode)) {
            if (errCode == RPCErrorCode::RPC_VERIFY_ERROR) { // missing inputs, wait deposit tx
                LogOrderMsg(txid.GetHex(), "redeem counterparty failed, trying to redeem again", __FUNCTION__);
                xapp.processLater(txid, packet);
                return true;
            } else {
                LogOrderMsg(txid.GetHex(), "failed to redeem p2sh deposit from counterparty, canceling", __FUNCTION__);
                sendCancelTransaction(xtx, crBadBDepositTx);
                return true;
            }
        }
    } // payTx

    xtx->state = TransactionDescr::trFinished;
    xuiConnector.NotifyXBridgeTransactionChanged(txid);

    // send reply
    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionConfirmedA));
    reply->append(hubAddress);
    reply->append(txid.begin(), 32);
    reply->append(xtx->payTxId);

    reply->sign(xtx->mPubKey, xtx->mPrivKey);

    sendPacket(hubAddress, reply);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::processTransactionConfirmedA(XBridgePacketPtr packet) const
{
    DEBUG_TRACE();

    // size must be > 52 bytes
    if (packet->size() <= 52 || packet->size() > 1000)
    {
        ERR() << "invalid packet size for xbcTransactionConfirmedA "
              << "need more than 52 bytes and less than 1kb, received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    // check is for me
    if (!checkPacketAddress(packet))
    {
        return true;
    }

    Exchange & e = Exchange::instance();
    if (!e.isStarted())
    {
        return true;
    }

    size_t offset = XBridgePacket::addressSize; // hub address

    // order id
    std::vector<unsigned char> stxid(packet->data()+offset, packet->data()+offset+XBridgePacket::hashSize);
    uint256 txid(stxid);
    offset += XBridgePacket::hashSize;

    // A side paytx id
    std::string a_payTxId(reinterpret_cast<const char *>(packet->data()+offset));

    TransactionPtr tr = e.transaction(txid);
    if (!tr->matches(txid)) // ignore no matching orders
        return true;

    std::vector<unsigned char> pk1(packet->pubkey(), packet->pubkey()+XBridgePacket::pubkeySize);
    if (!packet->verify(tr->a_pk1()))
    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", tr->id().GetHex());
        log_obj.pushKV("expected_counterparty_a", HexStr(tr->a_pk1()));
        log_obj.pushKV("received_counterparty_a", HexStr(pk1));
        LogOrderMsg(log_obj, "bad counterparty_a packet signature", __FUNCTION__);
        return true;
    }

    if (tr->state() != xbridge::Transaction::trCreated)
    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", tr->id().GetHex());
        log_obj.pushKV("state", tr->state());
        LogOrderMsg(log_obj, "wrong tx state, expecting created state", __FUNCTION__);
        return true;
    }

    tr->updateTimestamp();
    tr->a_setPayTxId(a_payTxId);

    if (e.updateTransactionWhenConfirmedReceived(tr, tr->a_destination()))
    {
        LogOrderMsg(tr->id().GetHex(), "invalid confirmation from counterparty_a", __FUNCTION__);
        // Can't cancel here, Maker already spent Taker deposit
    }

    XBridgePacketPtr reply2(new XBridgePacket(xbcTransactionConfirmB));
    reply2->append(m_myid);
    reply2->append(txid.begin(), 32);
    reply2->append(tr->a_payTxId());

    reply2->sign(e.pubKey(), e.privKey());

    sendPacket(tr->b_destination(), reply2);

    LogOrderMsg(tr, __FUNCTION__);

    return true;
}

//******************************************************************************
//******************************************************************************
bool Session::Impl::processTransactionConfirmB(XBridgePacketPtr packet) const
{
    DEBUG_TRACE();

    // size must be > 52 bytes
    if (packet->size() <= 52 || packet->size() > 1000)
    {
        LOG() << "incorrect packet size for xbcTransactionConfirmB "
              << "need more than 52 bytes or less than 1kb, received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    std::vector<unsigned char> hubAddress(packet->data(), packet->data()+XBridgePacket::addressSize);
    size_t offset = XBridgePacket::addressSize;

    std::vector<unsigned char> stxid(packet->data()+offset, packet->data()+offset+XBridgePacket::hashSize);
    uint256 txid(stxid);
    offset += XBridgePacket::hashSize;

    std::string payTxId(reinterpret_cast<const char *>(packet->data()+offset));
    offset += payTxId.size()+1;

    xbridge::App & xapp = xbridge::App::instance();

    TransactionDescrPtr xtx = xapp.transaction(txid);
    if (!xtx)
    {
        LogOrderMsg(txid.GetHex(), "unknown order", __FUNCTION__);
        return true;
    }
    if (!xtx->isLocal())
    {
        LogOrderMsg(txid.GetHex(), "not a local order", __FUNCTION__);
        return true;
    }
    // Reject if servicenode key doesn't match original (prevent order manipulation)
    std::vector<unsigned char> spubkey(packet->pubkey(), packet->pubkey()+XBridgePacket::pubkeySize);
    if (!packet->verify(xtx->sPubKey))
    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", txid.GetHex());
        log_obj.pushKV("expected_snode", HexStr(xtx->sPubKey));
        log_obj.pushKV("received_snode", HexStr(spubkey));
        log_obj.pushKV("hubaddress", HexStr(hubAddress));
        LogOrderMsg(log_obj, "wrong servicenode handling order", __FUNCTION__);
        return true;
    }
    if (xtx->state >= TransactionDescr::trCommited)
    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", txid.GetHex());
        log_obj.pushKV("state", xtx->state);
        LogOrderMsg(log_obj, "wrong tx state, expecting committed state", __FUNCTION__);
        return true;
    }

    // Only use counterparties payTxId for a few iterations
    if (xtx->otherPayTxTries() < xtx->maxOtherPayTxTries() && !xtx->isDoneWatching()) {
        xtx->setOtherPayTxId(payTxId);
        xtx->tryOtherPayTx();
    }

    WalletConnectorPtr connFrom = xapp.connectorByCurrency(xtx->fromCurrency);
    WalletConnectorPtr connTo = xapp.connectorByCurrency(xtx->toCurrency);
    if (!connFrom || !connTo)
    {
        LogOrderMsg(txid.GetHex(), "no connector for <" + (!connTo ? xtx->toCurrency : xtx->fromCurrency) + ">, trying again", __FUNCTION__);
        xapp.processLater(txid, packet);
        return true;
    }

    // payTx
    {
        int32_t errCode = 0;
        if (!redeemOrderCounterpartyDeposit(xtx, errCode)) {
            xapp.processLater(txid, packet); // trying again on failure
            return true;
        }
    } // payTx

    xtx->state = TransactionDescr::trFinished;
    xuiConnector.NotifyXBridgeTransactionChanged(txid);

    // send reply
    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionConfirmedB));
    reply->append(hubAddress);
    reply->append(txid.begin(), 32);
    reply->append(xtx->payTxId);

    reply->sign(xtx->mPubKey, xtx->mPrivKey);

    sendPacket(hubAddress, reply);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::processTransactionConfirmedB(XBridgePacketPtr packet) const
{
    DEBUG_TRACE();

    // size must be > 52 bytes
    if (packet->size() <= 52 || packet->size() > 1000)
    {
        ERR() << "invalid packet size for xbcTransactionConfirmedB "
              << "need more than 52 bytes and less than 1kb, received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    // check is for me
    if (!checkPacketAddress(packet))
    {
        return true;
    }

    Exchange & e = Exchange::instance();
    if (!e.isStarted())
    {
        return true;
    }

    size_t offset = XBridgePacket::addressSize; // hub address

    std::vector<unsigned char> stxid(packet->data()+offset, packet->data()+offset+XBridgePacket::hashSize);
    uint256 txid(stxid);
    offset += XBridgePacket::hashSize;

    // Pay tx id from B
    std::string b_payTxId(reinterpret_cast<const char *>(packet->data()+offset));

    TransactionPtr tr = e.transaction(txid);
    if (!tr->matches(txid)) // ignore no matching orders
        return true;

    std::vector<unsigned char> pk1(packet->pubkey(), packet->pubkey()+XBridgePacket::pubkeySize);
    if (!packet->verify(tr->b_pk1()))
    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", tr->id().GetHex());
        log_obj.pushKV("expected_counterparty_b", HexStr(tr->b_pk1()));
        log_obj.pushKV("received_counterparty_b", HexStr(pk1));
        LogOrderMsg(log_obj, "bad counterparty packet signature", __FUNCTION__);
        return true;
    }

    if (tr->state() != xbridge::Transaction::trCreated)
    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", tr->id().GetHex());
        log_obj.pushKV("state", tr->state());
        LogOrderMsg(log_obj, "wrong tx state, expecting created state", __FUNCTION__);
        return true;
    }

    tr->updateTimestamp();
    tr->b_setPayTxId(b_payTxId);

    if (e.updateTransactionWhenConfirmedReceived(tr, tr->b_destination()))
    {
        if (tr->state() == xbridge::Transaction::trFinished)
        {
            // Trade completed, no longer need to watch
            xbridge::App & xapp = xbridge::App::instance();
            xapp.unwatchTraderDeposit(tr);

            XBridgePacketPtr reply(new XBridgePacket(xbcTransactionFinished));
            reply->append(txid.begin(), 32);

            reply->sign(e.pubKey(), e.privKey());

            sendPacketBroadcast(reply);
        }
    }

    LogOrderMsg(tr, __FUNCTION__);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::processTransactionCancel(XBridgePacketPtr packet) const
{
    DEBUG_TRACE();

    // size must be == 36 bytes
    if (packet->size() != 36)
    {
        ERR() << "invalid packet size for xbcTransactionCancel "
              << "need 36 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    std::vector<unsigned char> stxid(packet->data(), packet->data()+XBridgePacket::hashSize);
    uint256 txid(stxid);
    TxCancelReason reason = static_cast<TxCancelReason>(*reinterpret_cast<uint32_t*>(packet->data() + 32));

    auto write_log = [](UniValue o, const std::string & errMsg) {
        o.pushKV("err_msg", errMsg);
        LOG() << o.write();
    };
    std::string errMsg;
    UniValue log_obj(UniValue::VOBJ);
    log_obj.pushKV("orderid", txid.GetHex());
    log_obj.pushKV("cancel_reason", TxCancelReasonText(reason));
    log_obj.pushKV("function", std::string(__FUNCTION__));

    // check packet signature
    Exchange & e = Exchange::instance();
    if (e.isStarted())
    {
        TransactionPtr tr = e.pendingTransaction(txid);

        if(!tr->isValid())
        {
            tr = e.transaction(txid);
        }

        if(!tr->isValid())
        {
            write_log(log_obj, "order not valid");
            return true;
        }

        if (!packet->verify(tr->a_pk1()) && !packet->verify(tr->b_pk1()))
        {
            write_log(log_obj, "invalid packet signature");
            return true;
        }

        sendCancelTransaction(tr, reason);
        write_log(log_obj, "counterparty requested cancel");
        return true;
    }

    xbridge::App & xapp = xbridge::App::instance();
    TransactionDescrPtr xtx = xapp.transaction(txid);
    if (!xtx)
    {
        write_log(log_obj, "unknown order");
        return true;
    }

    // Only Maker, Taker, or Servicenode can cancel order
    const auto iCanceled = packet->verify(xtx->mPubKey);
    if (!packet->verify(xtx->sPubKey) && !packet->verify(xtx->oPubKey) && !iCanceled)
    {
        write_log(log_obj, "bad packet signature for cancelation request on order, not canceling");
        return true;
    }

    WalletConnectorPtr conn = xapp.connectorByCurrency(xtx->fromCurrency);
    if (!conn)
    {
        write_log(log_obj, "no connector for <" + xtx->fromCurrency + "> , not canceling");
        return false;
    }
    
    // Set order cancel state
    auto cancel = [&xapp, &conn, &xtx, &reason, &txid, &write_log](const UniValue & o, const std::string & errMsg) {
        xapp.removePackets(txid);
        xapp.unlockCoins(conn->currency, xtx->usedCoins);
        if (xtx->state < TransactionDescr::trInitialized)
            xapp.unlockFeeUtxos(xtx->feeUtxos);
        xtx->state  = TransactionDescr::trCancelled;
        xtx->reason = reason;
        write_log(o, errMsg);
    };

    // If order is new or pending (open), then rebroadcast on another snode
    // only if our client didn't initiate the cancel.
    if (xtx->isLocal() && xtx->state <= TransactionDescr::trPending && !iCanceled) {
        auto requireUpdateTime = boost::posix_time::second_clock::universal_time() - boost::posix_time::seconds(241);
        xtx->setUpdateTime(requireUpdateTime); // will trigger an update (240 seconds stale)
        write_log(log_obj, "cancel received, rebroadcasting order on another service node");
        return true;
    } else if (xtx->state < TransactionDescr::trCreated) { // if no deposits yet
        xapp.moveTransactionToHistory(txid);
        cancel(log_obj, "counterparty cancel request");
        xuiConnector.NotifyXBridgeTransactionChanged(txid);
        return true;
    } else if (xtx->state == TransactionDescr::trCancelled) { // already canceled
        write_log(log_obj, "already canceled");
        return true;
    } else if (!xtx->didSendDeposit()) { // cancel if deposit not sent
        cancel(log_obj, "counterparty cancel request");
        return true;
    } else if (xtx->hasRedeemedCounterpartyDeposit()) { // Ignore if counterparty deposit already redeemed
        write_log(log_obj, "counterparty already redeemed, ignore cancel");
        return true;
    }

    // If refund transaction id not defined, do not attempt to rollback
    if (xtx->refTx.empty()) {
        cancel(log_obj, "could not find a refund transaction for order");
        return true;
    }

    // remove from pending packets (if added)
    xapp.removePackets(txid);

    // Set rollback state
    xtx->state = TransactionDescr::trRollback;
    xtx->reason = reason;

    // Attempt to rollback transaction and redeem deposit (this can take time since locktime needs to expire)
    int32_t errCode = 0;
    if (!redeemOrderDeposit(xtx, errCode)) {
        xapp.processLater(txid, packet);
    } else {
        // unlock coins (not fees)
        xapp.unlockCoins(conn->currency, xtx->usedCoins);
    }

    LogOrderMsg(xtx, __FUNCTION__);

    // update transaction state for gui
    xuiConnector.NotifyXBridgeTransactionChanged(txid);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::processTransactionReject(XBridgePacketPtr packet) const
{
    DEBUG_TRACE();

    // size must be == 36 bytes
    if (packet->size() != 36)
    {
        ERR() << "invalid packet size for xbcTransactionReject "
              << "need 36 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    std::vector<unsigned char> stxid(packet->data(), packet->data()+XBridgePacket::hashSize);
    uint256 txid(stxid);
    TxCancelReason reason = static_cast<TxCancelReason>(*reinterpret_cast<uint32_t*>(packet->data() + XBridgePacket::hashSize));

    xbridge::App & xapp = xbridge::App::instance();
    TransactionDescrPtr xtx = xapp.transaction(txid);
    // Do not proceed if we're not the taker or the state is beyond the accepting phase
    if (!xtx || xtx->role != 'B' || xtx->state > TransactionDescr::trAccepting)
        return true;

    // Only snode can reject an order
    if (!packet->verify(xtx->sPubKey))
        return true;

    xtx->reason = reason;
    LogOrderMsg(xtx, __FUNCTION__);

    // restore state on rejection
    xtx->state = TransactionDescr::trPending;
    // unlock coins
    xapp.unlockCoins(xtx->fromCurrency, xtx->usedCoins);
    xapp.unlockFeeUtxos(xtx->feeUtxos);
    xtx->clearUsedCoins();
    xtx->fromAddr.clear();
    xtx->from.clear();
    xtx->toAddr.clear();
    xtx->to.clear();
    xtx->role = 0;
    xtx->mPubKey.clear();
    xtx->mPrivKey.clear();
    xtx->reason = crUnknown;
    // Restore states
    xtx->fromCurrency = xtx->origFromCurrency;
    xtx->toCurrency = xtx->origToCurrency;
    xtx->fromAmount = xtx->origFromAmount;
    xtx->toAmount = xtx->origToAmount;
    // remove from pending packets (if added)
    xapp.removePackets(txid);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::finishTransaction(TransactionPtr tr) const
{
    if (tr == nullptr)
    {
        return false;
    }
    LogOrderMsg(tr->id().GetHex(), "order finished", __FUNCTION__);

    Exchange & e = Exchange::instance();
    if (!e.isStarted())
    {
        return false;
    }

    {
        XBridgePacketPtr reply(new XBridgePacket(xbcTransactionFinished));
        reply->append(tr->id().begin(), 32);

        reply->sign(e.pubKey(), e.privKey());

        sendPacketBroadcast(reply);
    }

    tr->finish();
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::sendCancelTransaction(const TransactionPtr & tx,
                                    const TxCancelReason & reason) const {
    return m_p->sendCancelTransaction(tx, reason);
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::sendCancelTransaction(const TransactionPtr & tx,
                                          const TxCancelReason & reason) const
{
    Exchange & e = Exchange::instance();
    if (!e.isStarted())
    {
        return false;
    }

    UniValue log_obj(UniValue::VOBJ);
    log_obj.pushKV("orderid", tx->id().GetHex());
    log_obj.pushKV("cancel_reason", TxCancelReasonText(reason));
    LogOrderMsg(log_obj, "canceling order, initiated by me", __FUNCTION__);

    tx->cancel();
    e.deletePendingTransaction(tx->id());

    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionCancel));
    reply->append(tx->id().begin(), 32);
    reply->append(static_cast<uint32_t>(reason));

    reply->sign(e.pubKey(), e.privKey());

    sendPacketBroadcast(reply);
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::sendCancelTransaction(const TransactionDescrPtr & tx,
                                    const TxCancelReason & reason) const {
    return m_p->sendCancelTransaction(tx, reason);
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::sendCancelTransaction(const TransactionDescrPtr & tx,
                                          const TxCancelReason & reason) const
{
    UniValue log_obj(UniValue::VOBJ);
    log_obj.pushKV("orderid", tx->id.GetHex());
    log_obj.pushKV("cancel_reason", TxCancelReasonText(reason));
    LogOrderMsg(log_obj, "canceling order, initiated by me", __FUNCTION__);

    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionCancel));
    reply->append(tx->id.begin(), 32);
    reply->append(static_cast<uint32_t>(reason));

    reply->sign(tx->mPubKey, tx->mPrivKey);

    processTransactionCancel(reply); // process local cancel immediately
    sendPacketBroadcast(reply);

    // update transaction state for gui
    xuiConnector.NotifyXBridgeTransactionChanged(tx->id);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::sendRejectTransaction(const uint256 & id,
                                          const TxCancelReason & reason) const
{
    Exchange & e = Exchange::instance();
    if (!e.isStarted()) // snode only
        return false;

    UniValue log_obj(UniValue::VOBJ);
    log_obj.pushKV("orderid", id.GetHex());
    log_obj.pushKV("cancel_reason", TxCancelReasonText(reason));
    LogOrderMsg(log_obj, "rejecting order, initiated by me", __FUNCTION__);

    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionReject));
    reply->append(id.begin(), 32);
    reply->append(static_cast<uint32_t>(reason));
    reply->sign(e.pubKey(), e.privKey());

    sendPacketBroadcast(reply);
    return true;
}

//*****************************************************************************
//*****************************************************************************
void Session::sendListOfTransactions() const
{
    xbridge::App & xapp = xbridge::App::instance();

    // send exchange trx
    Exchange & e = Exchange::instance();
    if (!e.isStarted())
    {
        return;
    }

    std::list<TransactionPtr> list = e.pendingTransactions();
    std::list<TransactionPtr>::iterator i = list.begin();
    for (; i != list.end(); ++i)
    {
        TransactionPtr & ptr = *i;

        XBridgePacketPtr packet(new XBridgePacket(xbcPendingTransaction));

        // field length must be 8 bytes
        std::vector<unsigned char> fc(8, 0);
        std::string tmp = ptr->a_currency();
        std::copy(tmp.begin(), tmp.end(), fc.begin());

        // field length must be 8 bytes
        std::vector<unsigned char> tc(8, 0);
        tmp = ptr->b_currency();
        std::copy(tmp.begin(), tmp.end(), tc.begin());

        packet->append(ptr->id().begin(), 32);
        packet->append(fc);
        packet->append(ptr->a_amount());
        packet->append(tc);
        packet->append(ptr->b_amount());
        packet->append(m_p->m_myid);
        packet->append(xbridge::timeToInt(ptr->createdTime()));
        packet->append(ptr->blockHash().begin(), 32);
        packet->append(uint16_t(ptr->isPartialAllowed()));

        packet->sign(e.pubKey(), e.privKey());

        m_p->sendPacketBroadcast(packet);
    }
}

void Session::Impl::sendTransaction(uint256 & id) const
{
    Exchange & e = Exchange::instance();
    if (!e.isStarted())
    {
        return;
    }

    TransactionPtr tr = e.pendingTransaction(id);
    if (!tr->matches(id))
        return;

    XBridgePacketPtr packet(new XBridgePacket(xbcPendingTransaction));

    // field length must be 8 bytes
    std::vector<unsigned char> fc(8, 0);
    std::string tmp = tr->a_currency();
    std::copy(tmp.begin(), tmp.end(), fc.begin());

    // field length must be 8 bytes
    std::vector<unsigned char> tc(8, 0);
    tmp = tr->b_currency();
    std::copy(tmp.begin(), tmp.end(), tc.begin());

    packet->append(tr->id().begin(), 32);
    packet->append(fc);
    packet->append(tr->a_amount());
    packet->append(tc);
    packet->append(tr->b_amount());
    packet->append(m_myid);
    packet->append(xbridge::timeToInt(tr->createdTime()));
    packet->append(tr->blockHash().begin(), 32);
    packet->append(uint16_t(tr->isPartialAllowed()));
    packet->append(tr->min_partial_amount());

    packet->sign(e.pubKey(), e.privKey());

    sendPacketBroadcast(packet);
}

//*****************************************************************************
//*****************************************************************************
void Session::checkFinishedTransactions() const
{
    Exchange & e = Exchange::instance();
    if (!e.isStarted())
    {
        return;
    }

    std::list<TransactionPtr> list = e.finishedTransactions();
    std::list<TransactionPtr>::iterator i = list.begin();
    for (; i != list.end(); ++i)
    {
        TransactionPtr & ptr = *i;

        uint256 txid = ptr->id();

        if (ptr->state() == xbridge::Transaction::trCancelled)
        {
            LogOrderMsg(txid.GetHex(), "drop canceled order", __FUNCTION__);
            ptr->drop();
        }
        else if (ptr->state() == xbridge::Transaction::trFinished)
        {
            // delete finished tx
            LogOrderMsg(txid.GetHex(), "delete finished order", __FUNCTION__);
            e.deleteTransaction(txid);
        }
        else if (ptr->state() == xbridge::Transaction::trDropped)
        {
            // delete dropped tx
            LogOrderMsg(txid.GetHex(), "delete dropped order", __FUNCTION__);
            e.deleteTransaction(txid);
        }
        else if (!ptr->isValid())
        {
            // delete invalid tx
            LogOrderMsg(txid.GetHex(), "delete invalid order", __FUNCTION__);
            e.deleteTransaction(txid);
        }
        else
        {
            LogOrderMsg(txid.GetHex(), "order timeout", __FUNCTION__);
            // cancel timed out order
            m_p->sendCancelTransaction(ptr, TxCancelReason::crTimeout);
        }
    }
}

//*****************************************************************************
//*****************************************************************************
void Session::getAddressBook() const
{
    App & xapp = App::instance();
    Connectors conns = xapp.connectors();

    for (Connectors::iterator i = conns.begin(); i != conns.end(); ++i)
    {
        std::string currency = (*i)->currency;

        std::vector<wallet::AddressBookEntry> entries;
        (*i)->requestAddressBook(entries);

        for (const wallet::AddressBookEntry & e : entries)
        {
            for (const std::string & addr : e.second)
            {
                std::vector<unsigned char> vaddr = (*i)->toXAddr(addr);

                xapp.updateConnector(*i, vaddr, currency);

                xuiConnector.NotifyXBridgeAddressBookEntryReceived
                        ((*i)->currency, e.first, addr);
            }
        }
    }
}

//******************************************************************************
//******************************************************************************
bool Session::Impl::processTransactionFinished(XBridgePacketPtr packet) const
{
    if (packet->size() != 32) {
        ERR() << "invalid packet size for xbcTransactionFinished "
              << "need 32 bytes, received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    // transaction id
    std::vector<unsigned char> stxid(packet->data(), packet->data()+XBridgePacket::hashSize);
    uint256 txid(stxid);
    // snode key
    std::vector<unsigned char> spubkey(packet->pubkey(), packet->pubkey()+XBridgePacket::pubkeySize);

    xbridge::App & xapp = xbridge::App::instance();

    // check and process packet if bridge is exchange
    Exchange & e = Exchange::instance();
    if (e.isStarted()) {
        TransactionPtr tr = e.transaction(txid);
        if (!tr->matches(txid)) { // ignore no matching orders
            LogOrderMsg(txid.GetHex(), "unknown order", __FUNCTION__);
            return true;
        }
        if (!packet->verify(e.pubKey())) {
            LogOrderMsg(txid.GetHex(), "bad packet signature, not updating order state", __FUNCTION__);
            return true;
        }
        LogOrderMsg(txid.GetHex(), "order finished", __FUNCTION__);
    } else { // Non-exchange nodes
        TransactionDescrPtr xtx = xapp.transaction(txid);
        if (xtx == nullptr) {
            LogOrderMsg(txid.GetHex(), "unknown order", __FUNCTION__);
            return true;
        }
        if (!packet->verify(xtx->sPubKey)) { // ignore packet if not from snode
            UniValue log_obj(UniValue::VOBJ);
            log_obj.pushKV("orderid", txid.GetHex());
            log_obj.pushKV("expected_snode", HexStr(xtx->sPubKey));
            log_obj.pushKV("received_snode", HexStr(spubkey));
            LogOrderMsg(log_obj, "wrong servicenode handling order", __FUNCTION__);
            return true;
        }
        // update transaction state for gui
        xtx->state = TransactionDescr::trFinished;
        LogOrderMsg(xtx, __FUNCTION__);
    }

    xapp.moveTransactionToHistory(txid);
    xuiConnector.NotifyXBridgeTransactionChanged(txid);

    return true;
}

bool Session::redeemOrderDeposit(const TransactionDescrPtr & xtx, int32_t & errCode) const {
    return m_p->redeemOrderDeposit(xtx, errCode);
}

bool Session::redeemOrderCounterpartyDeposit(const TransactionDescrPtr & xtx, int32_t & errCode) const {
    return m_p->redeemOrderCounterpartyDeposit(xtx, errCode);
}

bool Session::refundTraderDeposit(const std::string & orderId, const std::string & currency, const uint32_t & lockTime,
                         const std::string & refTx, int32_t & errCode) const {
    return m_p->refundTraderDeposit(orderId, currency, lockTime, refTx, errCode);
}

bool Session::Impl::redeemOrderDeposit(const TransactionDescrPtr & xtx, int32_t & errCode) const {
    xbridge::App & xapp = xbridge::App::instance();
    WalletConnectorPtr connFrom = xapp.connectorByCurrency(xtx->fromCurrency);
    if (!connFrom) {
        LogOrderMsg(xtx->id.GetHex(), "rollback attempted failed, no connector for <" + xtx->fromCurrency + "> is the wallet running?", __FUNCTION__);
        return false;
    }

    auto & txid = xtx->id;
    if (xtx->state < TransactionDescr::trCreated) {
        return true; // done
    }

    // If refund transaction id not defined, do not attempt to rollback
    if (xtx->refTx.empty()) {
        WalletConnectorPtr connTo = xapp.connectorByCurrency(xtx->toCurrency);
        auto fromAddr = connFrom->fromXAddr(xtx->from);
        auto toAddr = connTo ? connTo->fromXAddr(xtx->to) : "";
        if (!xtx->binTx.empty()) { // if there's a bin tx but no rollback, could mean potential loss of funds
            UniValue log_obj(UniValue::VOBJ);
            log_obj.pushKV("orderid", txid.GetHex());
            log_obj.pushKV("from_addr", fromAddr);
            log_obj.pushKV("from_currency", xtx->fromCurrency);
            log_obj.pushKV("from_amount", xbridge::xBridgeStringValueFromAmount(xtx->fromAmount));
            log_obj.pushKV("to_addr", toAddr);
            log_obj.pushKV("to_currency", xtx->toCurrency);
            log_obj.pushKV("to_amount", xbridge::xBridgeStringValueFromAmount(xtx->toAmount));
            LogOrderMsg(log_obj, "Fatal error, unable to rollback. Could not find a refund transaction for order", __FUNCTION__);
        }
        return true; // done
    }

    uint32_t blockCount{0};
    bool infoReceived = connFrom->getBlockCount(blockCount);

    // Check if locktime for the deposit has expired (can't redeem until locktime expires)
    if (infoReceived && blockCount < xtx->lockTime)
    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", txid.GetHex());
        log_obj.pushKV("redeem_currency", xtx->fromCurrency);
        log_obj.pushKV("redeem_locktime", static_cast<int>(xtx->lockTime));
        log_obj.pushKV("redeem_p2sh_txid", xtx->binTxId);
        LogOrderMsg(log_obj, "will be able to redeem canceled order at block " + std::to_string(xtx->lockTime), __FUNCTION__);
        return false;
    }
    else // if locktime has expired, attempt to send refund tx
    {
        std::string sid;
        int32_t errCode = 0;
        std::string errorMessage;
        if (!connFrom->sendRawTransaction(xtx->refTx, sid, errCode, errorMessage))
        {
            WalletConnectorPtr connTo = xapp.connectorByCurrency(xtx->toCurrency);
            auto fromAddr = connFrom->fromXAddr(xtx->from);
            auto toAddr = connTo ? connTo->fromXAddr(xtx->to) : "";
            UniValue log_obj(UniValue::VOBJ);
            log_obj.pushKV("orderid", txid.GetHex());
            log_obj.pushKV("from_addr", fromAddr);
            log_obj.pushKV("from_currency", xtx->fromCurrency);
            log_obj.pushKV("from_amount", xbridge::xBridgeStringValueFromAmount(xtx->fromAmount));
            log_obj.pushKV("to_addr", toAddr);
            log_obj.pushKV("to_currency", xtx->toCurrency);
            log_obj.pushKV("to_amount", xbridge::xBridgeStringValueFromAmount(xtx->toAmount));
            LogOrderMsg(log_obj, "failed to rollback locked deposit for order, trying again later", __FUNCTION__);
            xtx->state = TransactionDescr::trRollbackFailed;
            return false;
        } else {
            xtx->state = TransactionDescr::trRollback;
        }
    }

    xuiConnector.NotifyXBridgeTransactionChanged(txid);

    return true; // success
}

bool Session::Impl::redeemOrderCounterpartyDeposit(const TransactionDescrPtr & xtx, int32_t & errCode) const {
    xbridge::App & xapp = xbridge::App::instance();
    WalletConnectorPtr connFrom = xapp.connectorByCurrency(xtx->fromCurrency);
    WalletConnectorPtr connTo = xapp.connectorByCurrency(xtx->toCurrency);
    if (!connFrom || !connTo) {
        LogOrderMsg(xtx->id.GetHex(), "failed to redeem order due to bad wallet connection, is " +
                                      (!connFrom ? xtx->fromCurrency : xtx->toCurrency) + " running?", __FUNCTION__);
        return false;
    }

    // Try and find the secret
    if (!xtx->hasSecret()) {
        std::vector<unsigned char> x;
        bool isGood = false;
        if (!connFrom->getSecretFromPaymentTransaction(xtx->otherPayTxId(), xtx->binTxId, xtx->binTxVout, xtx->oHashedSecret, x, isGood))
        {
            // Keep looking for the maker pay tx, move packet to pending
            return false;
        }
        else if (!isGood)
        {
            UniValue log_obj(UniValue::VOBJ);
            log_obj.pushKV("orderid", xtx->id.GetHex());
            log_obj.pushKV("currency", xtx->fromCurrency);
            log_obj.pushKV("counterparty_p2sh_txid", xtx->otherPayTxId());
            log_obj.pushKV("my_spent_p2sh_deposit_txid", xtx->binTxId);
            log_obj.pushKV("my_spent_p2sh_deposit_vout", static_cast<int>(xtx->binTxVout));
            LogOrderMsg(log_obj, "secret not found in counterparty's pay tx on <" + xtx->fromCurrency + "> , counterparty could be misbehaving", __FUNCTION__);
            return false;
        }

        // assign the secret
        xtx->setSecret(x);
        // done watching for spent pay tx
        xtx->doneWatching();
        xapp.unwatchSpentDeposit(xtx);
        xapp.saveOrders(true);
    }

    auto fromAddr = connFrom->fromXAddr(xtx->from);
    auto toAddr = connTo->fromXAddr(xtx->to);

    double outAmount   = static_cast<double>(xtx->toAmount)/TransactionDescr::COIN;
    std::vector<xbridge::XTxIn>                  inputs;
    std::vector<std::pair<std::string, double> > outputs;

    // inputs from binTx
    inputs.emplace_back(xtx->oBinTxId, xtx->oBinTxVout, static_cast<double>(xtx->oBinTxP2SHAmount)/static_cast<double>(connTo->COIN));

    // outputs
    {
        outputs.emplace_back(toAddr, outAmount + xtx->oOverpayment);
    }

    if (!connTo->createPaymentTransaction(inputs, outputs,
                                          xtx->mPubKey, xtx->mPrivKey,
                                          xtx->secret(), xtx->unlockScript,
                                          xtx->payTxId, xtx->payTx))
    {
        LogOrderMsg(xtx->id.GetHex(), "failed to create payment redeem transaction, retrying", __FUNCTION__);
        if (!xtx->didLogPayTx2()) {
            TXLOG() << "redeem counterparty deposit for order " << xtx->id.ToString() << " (submit manually using sendrawtransaction) "
                    << xtx->fromCurrency << "(" << xbridge::xBridgeStringValueFromAmount(xtx->fromAmount) << " - " << fromAddr << ") / "
                    << xtx->toCurrency   << "(" << xbridge::xBridgeStringValueFromAmount(xtx->toAmount)   << " - " << toAddr   << ")" << std::endl
                    << xtx->payTx;
            xtx->setLogPayTx2();
        }
        return false;
    }

    if (!xtx->didLogPayTx1()) {
        TXLOG() << "redeem counterparty deposit for order " << xtx->id.ToString() << " (submit manually using sendrawtransaction) "
                << xtx->fromCurrency << "(" << xbridge::xBridgeStringValueFromAmount(xtx->fromAmount) << " - " << fromAddr << ") / "
                << xtx->toCurrency   << "(" << xbridge::xBridgeStringValueFromAmount(xtx->toAmount)   << " - " << toAddr   << ")" << std::endl
                << xtx->payTx;
        xtx->setLogPayTx1();
    }

    // send pay tx
    std::string sentid;
    std::string errorMessage;
    if (connTo->sendRawTransaction(xtx->payTx, sentid, errCode, errorMessage))
    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", xtx->id.GetHex());
        log_obj.pushKV("currency", xtx->toCurrency);
        log_obj.pushKV("redeem_pay_txid", xtx->payTxId);
        LogOrderMsg(log_obj, "redeeming order from counterparty on <" + xtx->toCurrency + "> chain with pay txid " + xtx->payTxId, __FUNCTION__);
    }
    else
    {
        if (errCode == RPCErrorCode::RPC_VERIFY_ALREADY_IN_CHAIN) {
            LogOrderMsg(xtx->id.GetHex(), "redeem tx already found in chain, proceeding", __FUNCTION__);
        }
        else
        {
            if (errCode == RPCErrorCode::RPC_VERIFY_ERROR) { // missing inputs, wait deposit tx
                LogOrderMsg(xtx->id.GetHex(), "failed to redeem tx from counterparty: bad inputs, trying again", __FUNCTION__);
            } else {
                LogOrderMsg(xtx->id.GetHex(), "failed to redeem tx from counterparty, trying again", __FUNCTION__);
            }
            return false; // can't cancel, maker already redeemed taker funds
        }
    }

    // Note that we've been paid
    xtx->counterpartyDepositRedeemed();

    return true;
}

bool Session::Impl::refundTraderDeposit(const std::string & orderId, const std::string & currency, const uint32_t & lockTime,
                                        const std::string & refTx, int32_t & errCode) const
{
    xbridge::App & xapp = xbridge::App::instance();
    WalletConnectorPtr conn = xapp.connectorByCurrency(currency);
    if (!conn) {
        LogOrderMsg(orderId, "refund attempt failed, no connector for <" + currency + "> on order, is the wallet running?", __FUNCTION__);
        return false;
    }

    // If refund transaction id not defined, do not attempt to rollback
    if (refTx.empty()) {
        LogOrderMsg(orderId, "Fatal error, unable to submit refund for <" + currency + "> on order due to an unknown refund tx", __FUNCTION__);
        errCode = RPCErrorCode::RPC_MISC_ERROR;
        return true; // done
    }

    std::string sid;
    std::string errorMessage;
    if (!conn->sendRawTransaction(refTx, sid, errCode, errorMessage))
        return false;

    return true; // success
}

} // namespace xbridge
