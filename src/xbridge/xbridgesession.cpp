//*****************************************************************************
//*****************************************************************************

// #include <boost/asio.hpp>
// #include <boost/asio/buffer.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include "xbridgesession.h"
#include "xbridgeapp.h"
#include "xbridgeexchange.h"
#include "xuiconnector.h"
#include "util/xutil.h"
#include "util/logger.h"
#include "util/txlog.h"
#include "bitcoinrpcconnector.h"
#include "xbitcointransaction.h"
#include "xbitcoinaddress.h"
#include "xbitcoinsecret.h"
#include "script/script.h"
#include "base58.h"
#include "activeservicenode.h"
#include "servicenode.h"
#include "servicenodeman.h"
#include "random.h"
#include "FastDelegate.h"

#include "json/json_spirit.h"
#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

using namespace json_spirit;

#define LOG_KEYPAIR_VALUES

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
    void sendPacket(const std::vector<unsigned char> & to, const XBridgePacketPtr & packet);
    void sendPacketBroadcast(XBridgePacketPtr packet);

    // return true if packet not for me, relayed
    bool checkPacketAddress(XBridgePacketPtr packet);

    // fn search xaddress in transaction and restore full 'coin' address as string
    bool isAddressInTransaction(const std::vector<unsigned char> & address,
                                const TransactionPtr & tx);

protected:
    bool encryptPacket(XBridgePacketPtr packet);
    bool decryptPacket(XBridgePacketPtr packet);

protected:
    bool processInvalid(XBridgePacketPtr packet);
    bool processZero(XBridgePacketPtr packet);
    bool processXChatMessage(XBridgePacketPtr packet);

    bool processTransaction(XBridgePacketPtr packet);
    bool processPendingTransaction(XBridgePacketPtr packet);
    bool processTransactionAccepting(XBridgePacketPtr packet);

    bool processTransactionHold(XBridgePacketPtr packet);
    bool processTransactionHoldApply(XBridgePacketPtr packet);

    bool processTransactionInit(XBridgePacketPtr packet);
    bool processTransactionInitialized(XBridgePacketPtr packet);

    bool processTransactionCreate(XBridgePacketPtr packet);
    bool processTransactionCreatedA(XBridgePacketPtr packet);
    bool processTransactionCreatedB(XBridgePacketPtr packet);

    bool processTransactionConfirmA(XBridgePacketPtr packet);
    bool processTransactionConfirmedA(XBridgePacketPtr packet);

    bool processTransactionConfirmB(XBridgePacketPtr packet);
    bool processTransactionConfirmedB(XBridgePacketPtr packet);

    bool finishTransaction(TransactionPtr tr);
    bool sendCancelTransaction(const uint256 & txid,
                                       const TxCancelReason & reason);
    bool sendCancelTransaction(const TransactionDescrPtr & tx,
                                       const TxCancelReason & reason);
    bool rollbackTransaction(TransactionPtr tr);

    bool processTransactionCancel(XBridgePacketPtr packet);
    bool cancelOrRollbackTransaction(const uint256 & txid, const TxCancelReason & reason);

    bool processTransactionFinished(XBridgePacketPtr packet);
    bool processTransactionRollback(XBridgePacketPtr packet);

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
    m_handlers[xbcInvalid]               .bind(this, &Impl::processInvalid);

    // process transaction from client wallet
    // if (XBridgeExchange::instance().isEnabled())
    {
        m_handlers[xbcTransaction]           .bind(this, &Impl::processTransaction);
        m_handlers[xbcTransactionAccepting]  .bind(this, &Impl::processTransactionAccepting);
    }
    // else
    {
        m_handlers[xbcPendingTransaction]    .bind(this, &Impl::processPendingTransaction);
    }

    // transaction processing
    {
        m_handlers[xbcTransactionHold]       .bind(this, &Impl::processTransactionHold);
        m_handlers[xbcTransactionHoldApply]  .bind(this, &Impl::processTransactionHoldApply);

        m_handlers[xbcTransactionInit]       .bind(this, &Impl::processTransactionInit);
        m_handlers[xbcTransactionInitialized].bind(this, &Impl::processTransactionInitialized);

        m_handlers[xbcTransactionCreateA]    .bind(this, &Impl::processTransactionCreate);
        m_handlers[xbcTransactionCreateB]    .bind(this, &Impl::processTransactionCreate);
        m_handlers[xbcTransactionCreatedA]   .bind(this, &Impl::processTransactionCreatedA);
        m_handlers[xbcTransactionCreatedB]   .bind(this, &Impl::processTransactionCreatedB);

        m_handlers[xbcTransactionConfirmA]   .bind(this, &Impl::processTransactionConfirmA);
        m_handlers[xbcTransactionConfirmB]   .bind(this, &Impl::processTransactionConfirmB);

        m_handlers[xbcTransactionCancel]     .bind(this, &Impl::processTransactionCancel);
        m_handlers[xbcTransactionRollback]   .bind(this, &Impl::processTransactionRollback);
        m_handlers[xbcTransactionFinished]   .bind(this, &Impl::processTransactionFinished);

        m_handlers[xbcTransactionConfirmedA] .bind(this, &Impl::processTransactionConfirmedA);
        m_handlers[xbcTransactionConfirmedB] .bind(this, &Impl::processTransactionConfirmedB);
    }

    // retranslate messages to xbridge network
    m_handlers[xbcXChatMessage].bind(this, &Impl::processXChatMessage);
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::encryptPacket(XBridgePacketPtr /*packet*/)
{
    // DEBUG_TRACE();
    // TODO implement this
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::decryptPacket(XBridgePacketPtr /*packet*/)
{
    // DEBUG_TRACE();
    // TODO implement this
    return true;
}

//*****************************************************************************
//*****************************************************************************
void Session::Impl::sendPacket(const std::vector<unsigned char> & to,
                                const XBridgePacketPtr & packet)
{
    xbridge::App & app = xbridge::App::instance();
    app.sendPacket(to, packet);
}

//*****************************************************************************
// return true if packet for me and need to process
//*****************************************************************************
bool Session::Impl::checkPacketAddress(XBridgePacketPtr packet)
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
bool Session::processPacket(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();

    if (!m_p->decryptPacket(packet))
    {
        ERR() << "packet decoding error " << __FUNCTION__;
        return false;
    }

    XBridgeCommand c = packet->command();

    if (m_p->m_handlers.count(c) == 0)
    {
        m_p->m_handlers[xbcInvalid](packet);
        // ERR() << "incorrect command code <" << c << "> " << __FUNCTION__;
        return false;
    }

    TRACE() << "received packet, command code <" << c << ">";

    if (!m_p->m_handlers[c](packet))
    {
        ERR() << "packet processing error <" << c << "> " << __FUNCTION__;
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::processInvalid(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();
    LOG() << "xbcInvalid instead of " << packet->command();
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::processZero(XBridgePacketPtr /*packet*/)
{
    return true;
}

//*****************************************************************************
//*****************************************************************************
// static
bool Session::checkXBridgePacketVersion(XBridgePacketPtr packet)
{
    if (packet->version() != static_cast<boost::uint32_t>(XBRIDGE_PROTOCOL_VERSION))
    {
        return false;
    }

    return true;
}

//*****************************************************************************
// retranslate packets from wallet to xbridge network
//*****************************************************************************
bool Session::Impl::processXChatMessage(XBridgePacketPtr /*packet*/)
{
    LOG() << "method BridgeSession::Impl::processXChatMessage not implemented";
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
// retranslate packets from wallet to xbridge network
//*****************************************************************************
void Session::Impl::sendPacketBroadcast(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();

    xbridge::App & app = xbridge::App::instance();
    app.sendPacket(packet);
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::processTransaction(XBridgePacketPtr packet)
{
    // check and process packet if bridge is exchange
    Exchange & e = Exchange::instance();
    if (!e.isStarted())
    {
        return true;
    }

    DEBUG_TRACE();

    // size must be > 148 bytes
    if (packet->size() < 148)
    {
        ERR() << "invalid packet size for xbcTransaction "
              << "need min 148 bytes, received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    // read packet data
    uint256 id(packet->data());

    // source
    uint32_t offset = 32;
    std::vector<unsigned char> saddr(packet->data()+offset, packet->data()+offset+20);
    offset += 20;
    std::string scurrency((const char *)packet->data()+offset);
    offset += 8;
    uint64_t samount = *static_cast<boost::uint64_t *>(static_cast<void *>(packet->data()+offset));
    offset += sizeof(uint64_t);

    // destination
    std::vector<unsigned char> daddr(packet->data()+offset, packet->data()+offset+20);
    offset += 20;
    std::string dcurrency((const char *)packet->data()+offset);
    offset += 8;
    uint64_t damount = *static_cast<uint64_t *>(static_cast<void *>(packet->data()+offset));
    offset += sizeof(uint64_t);

    uint32_t timestamp = *static_cast<uint32_t *>(static_cast<void *>(packet->data()+offset));
    offset += sizeof(uint32_t);

    xbridge::App & xapp = xbridge::App::instance();
    WalletConnectorPtr conn = xapp.connectorByCurrency(scurrency);
    if (!conn)
    {
        WARN() << "no connector for <" << scurrency << "> " << __FUNCTION__;
        return true;
    }

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
            wallet::UtxoEntry entry;

            uint256 txid(packet->data()+offset);
            offset += 32;

            entry.txId = txid.ToString();

            entry.vout = *static_cast<uint32_t *>(static_cast<void *>(packet->data()+offset));
            offset += sizeof(uint32_t);

            entry.rawAddress = std::vector<unsigned char>(packet->data()+offset, packet->data()+offset+20);
            offset += 20;

            entry.address = conn->fromXAddr(entry.rawAddress);

            entry.signature = std::vector<unsigned char>(packet->data()+offset, packet->data()+offset+65);
            offset += 65;

            if (!conn->getTxOut(entry))
            {
                LOG() << "not found utxo entry <" << entry.txId
                      << "> no " << entry.vout << " " << __FUNCTION__;
                continue;
            }

            // check signature
            std::string signature = EncodeBase64(&entry.signature[0], entry.signature.size());
            if (!conn->verifyMessage(entry.address, entry.toString(), signature))
            {
                LOG() << "not valid signature, bad utxo entry <" << entry.txId
                      << "> no " << entry.vout << " " << __FUNCTION__;
                continue;
            }

            commonAmount += entry.amount;

            utxoItems.push_back(entry);
        }
    }

    if (commonAmount * TransactionDescr::COIN < samount)
    {
        LOG() << "transaction rejected, amount from utxo items <" << commonAmount
              << "> less than required <" << samount << "> " << __FUNCTION__;
        return true;
    }

    LOG() << "received transaction " << HexStr(id) << std::endl
          << "    from " << HexStr(saddr) << std::endl
          << "             " << scurrency << " : " << samount << std::endl
          << "    to   " << HexStr(daddr) << std::endl
          << "             " << dcurrency << " : " << damount << std::endl;

    // check utxo items
    if (!e.checkUtxoItems(id, utxoItems))
    {
        LOG() << "transaction rejected, error check utxo items "
              << __FUNCTION__;
        return true;
    }

    {
        bool isCreated = false;
        if (!e.createTransaction(id,
                                 saddr, scurrency, samount,
                                 daddr, dcurrency, damount,
                                 utxoItems, timestamp, isCreated))
        {
            // not created
            LOG() << "transaction create error " << __FUNCTION__;
            return true;
        }

        if (isCreated)
        {
            // TODO send signal to gui for debug
            {
                TransactionDescrPtr d(new TransactionDescr);
                d->id           = id;
                d->fromCurrency = scurrency;
                d->fromAmount   = samount;
                d->toCurrency   = dcurrency;
                d->toAmount     = damount;
                d->state        = TransactionDescr::trPending;

                xuiConnector.NotifyXBridgeTransactionReceived(d);
            }

            TransactionPtr tr = e.pendingTransaction(id);
            if (tr->id() == uint256())
            {
                LOG() << "transaction not found after create. " << id.GetHex() << " " << __FUNCTION__;
                return false;
            }

            LOG() << "transaction created, id " << id.GetHex();

            boost::mutex::scoped_lock l(tr->m_lock);

            std::string firstCurrency = tr->a_currency();
            std::vector<unsigned char> fc(8, 0);
            std::copy(firstCurrency.begin(), firstCurrency.end(), fc.begin());
            std::string secondCurrency = tr->b_currency();
            std::vector<unsigned char> sc(8, 0);
            std::copy(secondCurrency.begin(), secondCurrency.end(), sc.begin());

            // broadcast send pending transaction packet
            XBridgePacketPtr reply(new XBridgePacket(xbcPendingTransaction));
            reply->append(tr->id().begin(), 32);
            reply->append(fc);
            reply->append(tr->a_amount());
            reply->append(sc);
            reply->append(tr->b_amount());
            reply->append(m_myid);
            reply->append(static_cast<uint32_t>(boost::posix_time::to_time_t(tr->createdTime())));

            sendPacketBroadcast(reply);
        }
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool Session::Impl::processPendingTransaction(XBridgePacketPtr packet)
{
    Exchange & e = Exchange::instance();
    if (e.isEnabled())
    {
        return true;
    }

    DEBUG_TRACE();

    if (packet->size() != 88)
    {
        ERR() << "incorrect packet size for xbcPendingTransaction "
              << "need 88 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    uint256 txid = uint256(packet->data());
    TransactionDescrPtr ptr = App::instance().transaction(txid);
    if (ptr)
    {
        ptr->updateTimestamp();
        xuiConnector.NotifyXBridgeTransactionReceived(ptr);
        return true;
    }

    ptr.reset(new TransactionDescr);
    ptr->id           = txid;
    ptr->fromCurrency = std::string(reinterpret_cast<const char *>(packet->data()+32));
    ptr->fromAmount   = *reinterpret_cast<boost::uint64_t *>(packet->data()+40);
    ptr->toCurrency   = std::string(reinterpret_cast<const char *>(packet->data()+48));
    ptr->toAmount     = *reinterpret_cast<boost::uint64_t *>(packet->data()+56);
    ptr->hubAddress   = std::vector<unsigned char>(packet->data()+64, packet->data()+84);
    ptr->created      = boost::posix_time::from_time_t(*reinterpret_cast<boost::uint32_t *>(packet->data()+84));
    ptr->state        = TransactionDescr::trPending;

    App::instance().appendTransaction(ptr);

    LOG() << "received tx <" << HexStr(ptr->id) << "> " << __FUNCTION__;

    xuiConnector.NotifyXBridgeTransactionReceived(ptr);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::processTransactionAccepting(XBridgePacketPtr packet)
{
    // check and process packet if bridge is exchange
    Exchange & e = Exchange::instance();
    if (!e.isStarted())
    {
        return true;
    }

    DEBUG_TRACE();

    // size must be >= 164 bytes
    if (packet->size() < 164)
    {
        ERR() << "invalid packet size for xbcTransactionAccepting "
              << "need min 164 bytes, received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    uint32_t offset = 20;

    // read packet data
    uint256 id(packet->data()+offset);
    offset += 32;

    // source
    std::vector<unsigned char> saddr(packet->data()+offset, packet->data()+offset+20);
    offset += 20;
    std::string scurrency((const char *)packet->data()+offset);
    offset += 8;
    uint64_t samount = *static_cast<uint64_t *>(static_cast<void *>(packet->data()+offset));
    offset += sizeof(uint64_t);

    // destination
    std::vector<unsigned char> daddr(packet->data()+offset, packet->data()+offset+20);
    offset += 20;
    std::string dcurrency((const char *)packet->data()+offset);
    offset += 8;
    uint64_t damount = *static_cast<uint64_t *>(static_cast<void *>(packet->data()+offset));
    offset += sizeof(uint64_t);

    xbridge::App & xapp = xbridge::App::instance();
    WalletConnectorPtr conn = xapp.connectorByCurrency(scurrency);
    if (!conn)
    {
        WARN() << "no connector for <" << scurrency << "> " << __FUNCTION__;
        return true;
    }

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
            wallet::UtxoEntry entry;

            uint256 txid(packet->data()+offset);
            offset += 32;

            entry.txId = txid.ToString();

            entry.vout = *static_cast<uint32_t *>(static_cast<void *>(packet->data()+offset));
            offset += sizeof(uint32_t);

            entry.rawAddress = std::vector<unsigned char>(packet->data()+offset, packet->data()+offset+20);
            offset += 20;

            entry.address = conn->fromXAddr(entry.rawAddress);

            entry.signature = std::vector<unsigned char>(packet->data()+offset, packet->data()+offset+65);
            offset += 65;

            if (!conn->getTxOut(entry))
            {
                LOG() << "not found utxo entry <" << entry.txId
                      << "> no " << entry.vout << " " << __FUNCTION__;
                continue;
            }

            // check signature
            std::string signature = EncodeBase64(&entry.signature[0], entry.signature.size());
            if (!conn->verifyMessage(entry.address, entry.toString(), signature))
            {
                LOG() << "not valid signature, bad utxo entry <" << entry.txId
                      << "> no " << entry.vout << " " << __FUNCTION__;
                continue;
            }

            commonAmount += entry.amount;

            utxoItems.push_back(entry);
        }
    }

    if (commonAmount * TransactionDescr::COIN < samount)
    {
        LOG() << "transaction rejected, amount from utxo items <" << commonAmount
              << "> less than required <" << samount << "> " << __FUNCTION__;
        return true;
    }

    LOG() << "received accepting transaction " << HexStr(id) << std::endl
          << "    from " << HexStr(saddr) << std::endl
          << "             " << scurrency << " : " << samount << std::endl
          << "    to   " << HexStr(daddr) << std::endl
          << "             " << dcurrency << " : " << damount << std::endl;


    if (!e.checkUtxoItems(id, utxoItems))
    {
        LOG() << "error check utxo items, transaction accept request rejected "
              << __FUNCTION__;
        return true;
    }

    {
        if (e.acceptTransaction(id, saddr, scurrency, samount, daddr, dcurrency, damount, utxoItems))
        {
            // check transaction state, if trNew - do nothing,
            // if trJoined = send hold to client
            TransactionPtr tr = e.transaction(id);

            boost::mutex::scoped_lock l(tr->m_lock);

            if (tr && tr->state() == xbridge::Transaction::trJoined)
            {
                // send hold

                // first
                // TODO remove this log
                LOG() << "send xbcTransactionHold ";

                XBridgePacketPtr reply1(new XBridgePacket(xbcTransactionHold));
                reply1->append(m_myid);
                reply1->append(tr->id().begin(), 32);
                reply1->append(activeServicenode.pubKeyServicenode.begin(),
                               activeServicenode.pubKeyServicenode.size());

                sendPacketBroadcast(reply1);
            }
        }
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool Session::Impl::processTransactionHold(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    if (packet->size() != 85 && packet->size() != 117)
    {
        ERR() << "incorrect packet size for xbcTransactionHold "
              << "need 105 or 137 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    uint32_t offset = 0;

    // servicenode addr
    std::vector<unsigned char> hubAddress(packet->data()+offset, packet->data()+offset+20);
    offset += 20;

    // read packet data
    uint256 id(packet->data()+offset);
    offset += 32;

    // service node pub key
    ::CPubKey pksnode;
    {
        uint32_t len = ::CPubKey::GetLen(*(char *)(packet->data()+offset));
        if (len != 33 && len != 65)
        {
            LOG() << "bad public key, startsWith " << *(char *)(packet->data()+offset) << " " << __FUNCTION__;
            return false;
        }

        pksnode.Set(packet->data()+offset, packet->data()+offset+len);
        // offset += len;
    }

    {
        // check servicenode
        CServicenode * snode = mnodeman.Find(pksnode);
        if (!snode)
        {
            // bad service node, no more
            LOG() << "unknown service node " << pksnode.GetID().ToString() << " " << __FUNCTION__;
            return true;
        }
    }

    LOG() << "use service node " << pksnode.GetID().ToString() << " " << __FUNCTION__;

    {
        // for xchange node remove tx
        // TODO mark as finished for debug
        Exchange & e = Exchange::instance();
        if (e.isStarted())
        {
            TransactionPtr tr = e.transaction(id);

            boost::mutex::scoped_lock l(tr->m_lock);

            if (!tr || tr->state() != xbridge::Transaction::trJoined)
            {
                e.deletePendingTransactions(id);
            }

            return true;
        }
    }

    xbridge::App & xapp = xbridge::App::instance();

    TransactionDescrPtr xtx = xapp.transaction(id);
    if (!xtx)
    {
        LOG() << "unknown transaction " << HexStr(id) << " " << __FUNCTION__;
        return true;
    }

    if (!xtx->isLocal())
    {
        xtx->state = TransactionDescr::trFinished;
        xapp.moveTransactionToHistory(id);
        return true;
    }

    // processing

    WalletConnectorPtr conn = xapp.connectorByCurrency(xtx->toCurrency);
    if (!conn)
    {
        WARN() << "no connector for <" << xtx->toCurrency << "> " << __FUNCTION__;
        return true;
    }

    xtx->state = TransactionDescr::trHold;
    xuiConnector.NotifyXBridgeTransactionStateChanged(id);

    if (xtx->isLocal())
    {
        // send hold apply
        XBridgePacketPtr reply(new XBridgePacket(xbcTransactionHoldApply));
        reply->append(hubAddress);
        reply->append(xtx->from);
        reply->append(id.begin(), 32);

        sendPacket(hubAddress, reply);
    }
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::processTransactionHoldApply(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    // size must be eq 72 bytes
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

    std::vector<unsigned char> from(packet->data()+20, packet->data()+40);

    // transaction id
    uint256 id(packet->data()+40);

    TransactionPtr tr = e.transaction(id);
    boost::mutex::scoped_lock l(tr->m_lock);

    tr->updateTimestamp();

    if (!isAddressInTransaction(from, tr))
    {
        ERR() << "invalid transaction address " << __FUNCTION__;
        sendCancelTransaction(id, crInvalidAddress);
        return true;
    }

    if (e.updateTransactionWhenHoldApplyReceived(tr, from))
    {
        if (tr->state() == xbridge::Transaction::trHold)
        {
            // send initialize transaction command to clients

            // field length must be 8 bytes
            std::string firstCurrency = tr->a_currency();
            std::vector<unsigned char> fc(8, 0);
            std::copy(firstCurrency.begin(), firstCurrency.end(), fc.begin());
            std::string secondCurrency = tr->b_currency();
            std::vector<unsigned char> sc(8, 0);
            std::copy(secondCurrency.begin(), secondCurrency.end(), sc.begin());

            // first
            // TODO remove this log
            LOG() << "send xbcTransactionInit to "
                  << HexStr(tr->a_destination());

            XBridgePacketPtr reply1(new XBridgePacket(xbcTransactionInit));
            reply1->append(tr->a_destination());
            reply1->append(m_myid);
            reply1->append(id.begin(), 32);
            reply1->append(activeServicenode.pubKeyServicenode.begin(),
                           activeServicenode.pubKeyServicenode.size());
            reply1->append(static_cast<uint16_t>('A'));
            reply1->append(tr->a_address());
            reply1->append(fc);
            reply1->append(tr->a_amount());
            reply1->append(tr->a_destination());
            reply1->append(sc);
            reply1->append(tr->b_amount());

            sendPacket(tr->a_destination(), reply1);

            // second
            // TODO remove this log
            LOG() << "send xbcTransactionInit to "
                  << HexStr(tr->b_destination());

            XBridgePacketPtr reply2(new XBridgePacket(xbcTransactionInit));
            reply2->append(tr->b_destination());
            reply2->append(m_myid);
            reply2->append(id.begin(), 32);
            reply2->append(activeServicenode.pubKeyServicenode.begin(),
                           activeServicenode.pubKeyServicenode.size());
            reply2->append(static_cast<uint16_t>('B'));
            reply2->append(tr->b_address());
            reply2->append(sc);
            reply2->append(tr->b_amount());
            reply2->append(tr->b_destination());
            reply2->append(fc);
            reply2->append(tr->a_amount());

            sendPacket(tr->b_destination(), reply2);
        }
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool Session::Impl::processTransactionInit(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    if (packet->size() <= 188)
    {
        ERR() << "incorrect packet size for xbcTransactionInit "
              << "need 188 or 221 bytes min, received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    std::vector<unsigned char> thisAddress(packet->data(), packet->data()+20);
    std::vector<unsigned char> hubAddress(packet->data()+20, packet->data()+40);

    uint256 txid(packet->data()+40);

    uint32_t offset = 72;

    // service node pub key
    ::CPubKey pksnode;
    {
        uint32_t len = ::CPubKey::GetLen(*(char *)(packet->data()+offset));
        if (len != 33 && len != 65)
        {
            LOG() << "bad public key, startsWith " << *(char *)(packet->data()+offset) << " " << __FUNCTION__;
            return false;
        }

        pksnode.Set(packet->data()+offset, packet->data()+offset+len);
        offset += len;
    }

    const char role = static_cast<char>((*reinterpret_cast<uint16_t *>(packet->data()+offset)));
    offset += sizeof(uint16_t);

    std::vector<unsigned char> from(packet->data()+offset, packet->data()+offset+20);
    offset += 20;
    std::string   fromCurrency(reinterpret_cast<const char *>(packet->data()+offset));
    offset += 8;
    uint64_t      fromAmount(*reinterpret_cast<uint64_t *>(packet->data()+offset));
    offset += sizeof(uint64_t);

    std::vector<unsigned char> to(packet->data()+offset, packet->data()+offset+20);
    offset += 20;
    std::string   toCurrency(reinterpret_cast<const char *>(packet->data()+offset));
    offset += 8;
    uint64_t      toAmount(*reinterpret_cast<uint64_t *>(packet->data()+offset));
    // offset += sizeof(uint64_t);

    // check servicenode
    std::vector<unsigned char> snodeAddress;
    {
        CServicenode * snode = mnodeman.Find(pksnode);
        if (!snode)
        {
            // bad service node, no more
            LOG() << "unknown service node " << pksnode.GetID().ToString() << " " << __FUNCTION__;
            return true;
        }

        CKeyID id = snode->pubKeyCollateralAddress.GetID();
        std::copy(id.begin(), id.end(), std::back_inserter(snodeAddress));

        LOG() << "use service node " << id.ToString() << " " << __FUNCTION__;
    }

    xbridge::App & xapp = xbridge::App::instance();

    TransactionDescrPtr xtx = xapp.transaction(txid);
    if (!xtx)
    {
        LOG() << "unknown transaction " << HexStr(txid) << " " << __FUNCTION__;
        return true;
    }

    if(xtx->id           != txid &&
       xtx->from         != from &&
       xtx->fromCurrency != fromCurrency &&
       xtx->fromAmount   != fromAmount &&
       xtx->to           != to &&
       xtx->toCurrency   != toCurrency &&
       xtx->toAmount     != toAmount)
    {
        LOG() << "not equal transaction body" << __FUNCTION__;
        return true;
    }

    xtx->role = role;

    WalletConnectorPtr conn = xapp.connectorByCurrency(xtx->toCurrency);
    if (!conn)
    {
        WARN() << "no connector for <" << xtx->toCurrency << "> " << __FUNCTION__;
        return true;
    }

    // m key
    conn->newKeyPair(xtx->mPubKey, xtx->mPrivKey);
    assert(xtx->mPubKey.size() == 33 && "bad pubkey size");

#ifdef LOG_KEYPAIR_VALUES
    LOG() << "generated M keypair " << std::endl <<
             "    pub    " << HexStr(xtx->mPubKey) << std::endl <<
             "    pub id " << HexStr(conn->getKeyId(xtx->mPubKey)) << std::endl <<
             "    priv   " << HexStr(xtx->mPrivKey);
#endif

//    // x key
    uint256 datatxtd;
    if (role == 'A')
    {
        conn->newKeyPair(xtx->xPubKey, xtx->xPrivKey);
        assert(xtx->xPubKey.size() == 33 && "bad pubkey size");

#ifdef LOG_KEYPAIR_VALUES
        LOG() << "generated X keypair " << std::endl <<
                 "    pub    " << HexStr(xtx->xPubKey) << std::endl <<
                 "    pub id " << HexStr(conn->getKeyId(xtx->xPubKey)) << std::endl <<
                 "    priv   " << HexStr(xtx->xPrivKey);
#endif

        // send blocknet tx with hash of X
        std::vector<unsigned char> xid = conn->getKeyId(xtx->xPubKey);
        assert(xid.size() == 20 && "bad pubkey id size");

        std::string strtxid;
        if (!rpc::storeDataIntoBlockchain(snodeAddress, conn->serviceNodeFee,
                                          std::vector<unsigned char>(xid.begin(), xid.end()), strtxid))
        {
            ERR() << "storeDataIntoBlockchain failed, error send blocknet tx " << __FUNCTION__;
            sendCancelTransaction(xtx, crBlocknetError);
            return true;
        }

        datatxtd = uint256(strtxid);
    }

    // send initialized
    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionInitialized));
    reply->append(hubAddress);
    reply->append(thisAddress);
    reply->append(txid.begin(), 32);
    reply->append(datatxtd.begin(), 32);
    reply->append(xtx->mPubKey);

    sendPacket(hubAddress, reply);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::processTransactionInitialized(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    // size must be eq 137 bytes
    if (packet->size() != 137)
    {
        ERR() << "invalid packet size for xbcTransactionHoldApply "
              << "need 137 received " << packet->size() << " "
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

    std::vector<unsigned char> from(packet->data()+20, packet->data()+40);

    // transaction id
    uint256 id(packet->data()+40);

    uint32_t offset = 72;

    // data tx id
    uint256 datatxid(packet->data() + offset);
    offset += 32;

    // opponent publick key
    std::vector<unsigned char> pk1(packet->data()+offset, packet->data()+offset+33);
    // offset += 33;

    TransactionPtr tr = e.transaction(id);
    boost::mutex::scoped_lock l(tr->m_lock);

    tr->updateTimestamp();

    if (!isAddressInTransaction(from, tr))
    {
        ERR() << "invalid transaction address " << __FUNCTION__;
        sendCancelTransaction(id, crInvalidAddress);
        return true;
    }

    if (e.updateTransactionWhenInitializedReceived(tr, from, datatxid, pk1))
    {
        if (tr->state() == xbridge::Transaction::trInitialized)
        {
            // send create transaction command to clients

            // first
            // TODO remove this log
            LOG() << "send xbcTransactionCreate to "
                  << HexStr(tr->a_address());

            // send xbcTransactionCreate
            // with nLockTime == lockTime*2 for first client,
            // with nLockTime == lockTime*4 for second
            XBridgePacketPtr reply1(new XBridgePacket(xbcTransactionCreateA));
            reply1->append(tr->a_address());
            reply1->append(m_myid);
            reply1->append(id.begin(), 32);
            reply1->append(tr->b_destination());
            reply1->append(tr->a_datatxid().begin(), 32);
            reply1->append(tr->b_pk1());

            sendPacket(tr->a_address(), reply1);
        }
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool Session::Impl::isAddressInTransaction(const std::vector<unsigned char> & address,
                                           const TransactionPtr & tx)
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
bool Session::Impl::processTransactionCreate(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    if (packet->size() < 157)
    {
        ERR() << "incorrect packet size for xbcTransactionCreate "
              << "need min 157 bytes, received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    std::vector<unsigned char> thisAddress(packet->data(), packet->data()+20);
    std::vector<unsigned char> hubAddress(packet->data()+20, packet->data()+40);

    // transaction id
    uint256 txid(packet->data()+40);

    // destination address
    uint32_t offset = 72;
    std::vector<unsigned char> destAddress(packet->data()+offset, packet->data()+offset+20);
    offset += 20;

    uint256 datatxid(packet->data()+offset);
    offset += 32;

    std::vector<unsigned char> mPubKey(packet->data()+offset, packet->data()+offset+33);
    offset += 33;

    xbridge::App & xapp = xbridge::App::instance();

    TransactionDescrPtr xtx = xapp.transaction(txid);
    if (!xtx)
    {
        LOG() << "unknown transaction " << HexStr(txid) << " " << __FUNCTION__;
        return true;
    }

    // connectors
    WalletConnectorPtr connFrom = xapp.connectorByCurrency(xtx->fromCurrency);
    WalletConnectorPtr connTo   = xapp.connectorByCurrency(xtx->toCurrency);
    if (!connFrom || !connTo)
    {
        WARN() << "no connector for <" << (!connFrom ? xtx->fromCurrency : xtx->toCurrency) << "> " << __FUNCTION__;
        sendCancelTransaction(xtx, crBadADepositTx);
        return true;
    }

    std::vector<unsigned char> hx;
    if (!rpc::getDataFromTx(datatxid.GetHex(), hx))
    {
        // no data, move to pending
        xapp.processLater(txid, packet);
        return true;
    }

    if (xtx->role == 'B')
    {
        // for B need to check A deposit tx
        // check packet length

        std::string binATxId(reinterpret_cast<const char *>(packet->data()+offset));
        offset += binATxId.size()+1;

        if (binATxId.size() == 0)
        {
            LOG() << "bad A deposit tx id received for " << HexStr(txid) << " " << __FUNCTION__;
            sendCancelTransaction(xtx, crBadADepositTx);
            return true;
        }

        bool isGood = false;
        if (!connTo->checkTransaction(binATxId, std::string(), 0, isGood))
        {
            // move packet to pending
            xapp.processLater(txid, packet);
            return true;
        }
        else if (!isGood)
        {
            LOG() << "check A deposit tx error for " << HexStr(txid) << " " << __FUNCTION__;
            sendCancelTransaction(xtx, crBadADepositTx);
            return true;
        }

        LOG() << "deposit A tx confirmed " << HexStr(txid);
    }

    double outAmount = static_cast<double>(xtx->fromAmount) / TransactionDescr::COIN;

    double fee1      = 0;
    double fee2      = connTo->minTxFee2(1, 1);
    double inAmount  = 0;

    std::vector<wallet::UtxoEntry> usedInTx;
    for (const wallet::UtxoEntry & entry : xtx->usedCoins)
    {
        usedInTx.push_back(entry);
        inAmount += entry.amount;
        fee1 = connFrom->minTxFee1(usedInTx.size(), 3);

        LOG() << "USED FOR TX <" << entry.txId << "> amount " << entry.amount << " " << entry.vout << " fee " << fee1;

        // check amount
        if (inAmount >= outAmount+fee1+fee2)
        {
            break;
        }
    }

    // check amount
    if (inAmount < outAmount+fee1+fee2)
    {
        // no money, cancel transaction
        LOG() << "no money, transaction canceled " << __FUNCTION__;
        sendCancelTransaction(xtx, crNoMoney);
        return true;
    }

    // lock time
    uint32_t lTime = connFrom->lockTime(xtx->role);
    if (lTime == 0)
    {
        LOG() << "lockTime error, transaction canceled " << __FUNCTION__;
        sendCancelTransaction(xtx, crRpcError);
        return true;
    }

    // create transactions

#ifdef LOG_KEYPAIR_VALUES
    LOG() << "unlock script pub keys" << std::endl <<
             "    my       " << HexStr(xtx->mPubKey) << std::endl <<
             "    my id    " << HexStr(connFrom->getKeyId(xtx->mPubKey)) << std::endl <<
             "    other    " << HexStr(mPubKey) << std::endl <<
             "    other id " << HexStr(connFrom->getKeyId(mPubKey)) << std::endl <<
             "    x id     " << HexStr(hx);
#endif

    // create address for first tx
    connFrom->createDepositUnlockScript(xtx->mPubKey, mPubKey, hx, lTime, xtx->innerScript);
    xtx->depositP2SH = connFrom->scriptIdToString(connFrom->getScriptId(xtx->innerScript));

    // depositTx
    {
        std::vector<std::pair<std::string, int> >    inputs;
        std::vector<std::pair<std::string, double> > outputs;

        // inputs
        for (const wallet::UtxoEntry & entry : usedInTx)
        {
            inputs.push_back(std::make_pair(entry.txId, entry.vout));
        }

        // outputs

        // amount
        outputs.push_back(std::make_pair(xtx->depositP2SH, outAmount+fee2));

        // rest
        if (inAmount > outAmount+fee1+fee2)
        {
            std::string addr;
            if (!connFrom->getNewAddress(addr))
            {
                // cancel transaction
                LOG() << "rpc error, transaction canceled " << __FUNCTION__;
                sendCancelTransaction(xtx, crRpcError);
                return true;
            }

            double rest = inAmount-outAmount-fee1-fee2;
            outputs.push_back(std::make_pair(addr, rest));
        }

        if (!connFrom->createDepositTransaction(inputs, outputs, xtx->binTxId, xtx->binTx))
        {
            // cancel transaction
            LOG() << "deposit not created, transaction canceled " << __FUNCTION__;
            sendCancelTransaction(xtx, crRpcError);
            return true;
        }

        TXLOG() << "deposit sendrawtransaction " << xtx->binTx;

    } // depositTx

    // refundTx
    {
        std::vector<std::pair<std::string, int> >    inputs;
        std::vector<std::pair<std::string, double> > outputs;
        // std::vector<std::pair<CScript, double> >  outputs;

        // inputs from binTx
        inputs.push_back(std::make_pair(xtx->binTxId, 0));

        // outputs
        {
            std::string addr;
            if (!connFrom->getNewAddress(addr))
            {
                // cancel transaction
                LOG() << "rpc error, transaction canceled " << __FUNCTION__;
                sendCancelTransaction(xtx, crRpcError);
                return true;
            }

            outputs.push_back(std::make_pair(addr, outAmount));
        }

        if (!connFrom->createRefundTransaction(inputs, outputs,
                                               xtx->mPubKey, xtx->mPrivKey,
                                               xtx->innerScript, lTime,
                                               xtx->refTxId, xtx->refTx))
        {
            // cancel transaction
            LOG() << "refund transaction not created, transaction canceled " << __FUNCTION__;
            sendCancelTransaction(xtx, crRpcError);
            return true;
        }

        TXLOG() << "refund sendrawtransaction " << xtx->refTx;

    } // refundTx

    xtx->state = TransactionDescr::trCreated;

    xuiConnector.NotifyXBridgeTransactionStateChanged(txid);

    // send transactions
    {
        std::string sentid;
        int32_t errCode = 0;
        if (connFrom->sendRawTransaction(xtx->binTx, sentid, errCode))
        {
            LOG() << "deposit " << xtx->role << " " << sentid;
        }
        else
        {
            LOG() << "deposit tx not send, transaction canceled " << __FUNCTION__;
            sendCancelTransaction(xtx, crRpcError);
            return true;
        }
    }

    // send reply
    XBridgePacketPtr reply;
    if (xtx->role == 'A')
    {
        reply.reset(new XBridgePacket(xbcTransactionCreatedA));
    }
    else if (xtx->role == 'B')
    {
        reply.reset(new XBridgePacket(xbcTransactionCreatedB));
    }
    else
    {
        ERR() << "unknown role " << __FUNCTION__;
        return false;
    }

    reply->append(hubAddress);
    reply->append(thisAddress);
    reply->append(txid.begin(), 32);
    reply->append(xtx->binTxId);
    reply->append(static_cast<uint32_t>(xtx->innerScript.size()));
    reply->append(xtx->innerScript);

    sendPacket(hubAddress, reply);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::processTransactionCreatedA(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    // size must be > 72 bytes
    if (packet->size() < 72)
    {
        ERR() << "invalid packet size for xbcTransactionCreatedA "
              << "need more than 74 received " << packet->size() << " "
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

    size_t offset = 20;

    std::vector<unsigned char> from(packet->data()+offset, packet->data()+offset+20);
    offset += 20;

    uint256 txid(packet->data()+offset);
    offset += 32;

    std::string binTxId(reinterpret_cast<const char *>(packet->data()+offset));
    offset += binTxId.size()+1;

    uint32_t innerSize = *reinterpret_cast<uint32_t *>(packet->data()+offset);
    offset += sizeof(uint32_t);

    std::vector<unsigned char> innerScript(packet->data()+offset, packet->data()+offset+innerSize);
    // offset += innerScript.size();

    TransactionPtr tr = e.transaction(txid);
    boost::mutex::scoped_lock l(tr->m_lock);

    tr->updateTimestamp();

    if (!isAddressInTransaction(from, tr))
    {
        ERR() << "invalid transaction address " << __FUNCTION__;
        sendCancelTransaction(txid, crInvalidAddress);
        return true;
    }

    if (e.updateTransactionWhenCreatedReceived(tr, from, binTxId, innerScript))
    {
        // wtf ?
        ERR() << "invalid createdA " << __FUNCTION__;
        sendCancelTransaction(txid, crInvalidAddress);
        return true;
    }

    // TODO remove this log
    LOG() << "send xbcTransactionCreate to "
          << HexStr(tr->b_address());

    XBridgePacketPtr reply2(new XBridgePacket(xbcTransactionCreateB));
    reply2->append(tr->b_address());
    reply2->append(m_myid);
    reply2->append(txid.begin(), 32);
    reply2->append(tr->a_destination());
    reply2->append(tr->a_datatxid().begin(), 32);
    reply2->append(tr->a_pk1());
    reply2->append(binTxId);

    sendPacket(tr->b_address(), reply2);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::processTransactionCreatedB(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    // size must be > 72 bytes
    if (packet->size() < 72)
    {
        ERR() << "invalid packet size for xbcTransactionCreated "
              << "need more than 74 received " << packet->size() << " "
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

    size_t offset = 20;

    std::vector<unsigned char> from(packet->data()+offset, packet->data()+offset+20);
    offset += 20;

    uint256 txid(packet->data()+offset);
    offset += 32;

    std::string binTxId(reinterpret_cast<const char *>(packet->data()+offset));
    offset += binTxId.size()+1;

    uint32_t innerSize = *reinterpret_cast<uint32_t *>(packet->data()+offset);
    offset += sizeof(uint32_t);

    std::vector<unsigned char> innerScript(packet->data()+offset, packet->data()+offset+innerSize);
    // offset += innerScript.size();

    TransactionPtr tr = e.transaction(txid);
    boost::mutex::scoped_lock l(tr->m_lock);

    tr->updateTimestamp();

    if (!isAddressInTransaction(from, tr))
    {
        ERR() << "invalid transaction address " << __FUNCTION__;
        sendCancelTransaction(txid, crInvalidAddress);
        return true;
    }

    if (e.updateTransactionWhenCreatedReceived(tr, from, binTxId, innerScript))
    {
        if (tr->state() == xbridge::Transaction::trCreated)
        {
            // send confirm packets with deposit tx id
            // for create payment tx

            // TODO remove this log
            LOG() << "send xbcTransactionConfirmA to "
                  << HexStr(tr->a_destination());

            XBridgePacketPtr reply(new XBridgePacket(xbcTransactionConfirmA));
            reply->append(tr->a_destination());
            reply->append(m_myid);
            reply->append(txid.begin(), 32);
            reply->append(tr->b_bintxid());
            reply->append(static_cast<uint32_t>(tr->b_innerScript().size()));
            reply->append(tr->b_innerScript());

            sendPacket(tr->a_destination(), reply);
        }
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool Session::Impl::processTransactionConfirmA(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    // size must be > 72 bytes
    if (packet->size() < 72)
    {
        LOG() << "incorrect packet size for xbcTransactionConfirmA "
              << "need more than 72 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    std::vector<unsigned char> thisAddress(packet->data(), packet->data()+20);
    std::vector<unsigned char> hubAddress(packet->data()+20, packet->data()+40);

    uint256 txid(packet->data()+40);

    uint32_t offset = 72;

    std::string binTxId(reinterpret_cast<const char *>(packet->data()+offset));
    offset += binTxId.size()+1;

    uint32_t innerSize = *reinterpret_cast<uint32_t *>(packet->data()+offset);
    offset += sizeof(uint32_t);

    std::vector<unsigned char> innerScript(packet->data()+offset, packet->data()+offset+innerSize);
    offset += innerScript.size();

    xbridge::App & xapp = xbridge::App::instance();

    TransactionDescrPtr xtx = xapp.transaction(txid);
    if (!xtx)
    {
        LOG() << "unknown transaction " << HexStr(txid) << " " << __FUNCTION__;
        return true;
    }

    WalletConnectorPtr conn = xapp.connectorByCurrency(xtx->toCurrency);
    if (!conn)
    {
        WARN() << "no connector for <" << xtx->toCurrency << "> " << __FUNCTION__;
        sendCancelTransaction(xtx, crBadBDepositTx);
        return true;
    }

    // check B deposit tx
    {
        // TODO check tx in blockchain and move packet to pending if not

        bool isGood = false;
        if (!conn->checkTransaction(binTxId, std::string(), 0, isGood))
        {
            xapp.processLater(txid, packet);
            return true;
        }
        else if (!isGood)
        {
            LOG() << "check B deposit tx error for " << HexStr(txid) << " " << __FUNCTION__;
            sendCancelTransaction(xtx, crBadBDepositTx);
            return true;
        }

        LOG() << "deposit B tx confirmed " << HexStr(txid);
    }

    // payTx
    {
        std::vector<std::pair<std::string, int> >    inputs;
        std::vector<std::pair<std::string, double> > outputs;

        // inputs from binTx
        inputs.push_back(std::make_pair(binTxId, 0));

        // outputs
        {
            double outAmount = static_cast<double>(xtx->toAmount)/TransactionDescr::COIN;
            outputs.push_back(std::make_pair(conn->fromXAddr(xtx->to), outAmount));
        }

        if (!conn->createPaymentTransaction(inputs, outputs,
                                            xtx->mPubKey, xtx->mPrivKey,
                                            xtx->xPubKey, innerScript,
                                            xtx->payTxId, xtx->payTx))
        {
            // cancel transaction
            LOG() << "payment transaction create error, transaction canceled " << __FUNCTION__;
            sendCancelTransaction(xtx, crRpcError);
            return true;
        }

        TXLOG() << "payment A sendrawtransaction " << xtx->payTx;

    } // payTx

    // send pay tx
    std::string sentid;
    int32_t errCode = 0;
    if (conn->sendRawTransaction(xtx->payTx, sentid, errCode))
    {
        LOG() << "payment A " << sentid;
    }
    else
    {
        if (errCode == -25)
        {
            // missing inputs, wait deposit tx
            LOG() << "payment A not send, no deposit tx, move to pending";

            xapp.processLater(txid, packet);
            return true;
        }

        LOG() << "payment A tx not send, transaction canceled " << __FUNCTION__;
        sendCancelTransaction(xtx, crRpcError);
        return true;
    }

    xtx->state = TransactionDescr::trCommited;

    xuiConnector.NotifyXBridgeTransactionStateChanged(txid);

    // send reply
    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionConfirmedA));
    reply->append(hubAddress);
    reply->append(thisAddress);
    reply->append(txid.begin(), 32);
    reply->append(xtx->xPubKey);

    sendPacket(hubAddress, reply);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::processTransactionConfirmedA(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    // size must be > 72 bytes
    if (packet->size() <= 72)
    {
        ERR() << "invalid packet size for xbcTransactionConfirmedA "
              << "need 72 bytes min " << packet->size() << " "
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

    std::vector<unsigned char> from(packet->data()+20, packet->data()+40);

    uint256 txid(packet->data()+40);

    std::vector<unsigned char> xPubkey(packet->data()+72, packet->data()+72+33);

    TransactionPtr tr = e.transaction(txid);
    boost::mutex::scoped_lock l(tr->m_lock);

    tr->updateTimestamp();

    if (!isAddressInTransaction(from, tr))
    {
        ERR() << "invalid transaction address " << __FUNCTION__;
        sendCancelTransaction(txid, crInvalidAddress);
        return true;
    }

    if (e.updateTransactionWhenConfirmedReceived(tr, from))
    {
        // wtf ?
        ERR() << "invalid confirmation " << __FUNCTION__;
        sendCancelTransaction(txid, crInvalidAddress);
        return true;
    }

    // TODO remove this log
    LOG() << "send xbcTransactionConfirmB to "
          << HexStr(tr->b_destination());

    XBridgePacketPtr reply2(new XBridgePacket(xbcTransactionConfirmB));
    reply2->append(tr->b_destination());
    reply2->append(m_myid);
    reply2->append(txid.begin(), 32);
    reply2->append(xPubkey);
    reply2->append(tr->a_bintxid());
    reply2->append(static_cast<uint32_t>(tr->a_innerScript().size()));
    reply2->append(tr->a_innerScript());

    sendPacket(tr->b_destination(), reply2);

    return true;
}

//******************************************************************************
//******************************************************************************
bool Session::Impl::processTransactionConfirmB(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    // size must be > 105 bytes
    if (packet->size() <= 105)
    {
        LOG() << "incorrect packet size for xbcTransactionConfirmB "
              << "need more than 105 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    std::vector<unsigned char> thisAddress(packet->data(), packet->data()+20);
    std::vector<unsigned char> hubAddress(packet->data()+20, packet->data()+40);

    uint256 txid(packet->data()+40);

    uint32_t offset = 72;

    std::vector<unsigned char> x(packet->data()+offset, packet->data()+offset+33);
    offset += 33;

    std::string binTxId(reinterpret_cast<const char *>(packet->data()+offset));
    offset += binTxId.size()+1;

    uint32_t innerSize = *reinterpret_cast<uint32_t *>(packet->data()+offset);
    offset += sizeof(uint32_t);

    std::vector<unsigned char> innerScript(packet->data()+offset, packet->data()+offset+innerSize);
    offset += innerScript.size();

    xbridge::App & xapp = xbridge::App::instance();

    TransactionDescrPtr xtx = xapp.transaction(txid);
    if (!xtx)
    {
        LOG() << "unknown transaction " << HexStr(txid) << " " << __FUNCTION__;
        return true;
    }

    WalletConnectorPtr conn = xapp.connectorByCurrency(xtx->toCurrency);
    if (!conn)
    {
        WARN() << "no connector for <" << xtx->toCurrency << "> " << __FUNCTION__;
        sendCancelTransaction(xtx, crBadADepositTx);
        return true;
    }

    // payTx
    {
        std::vector<std::pair<std::string, int> >    inputs;
        std::vector<std::pair<std::string, double> > outputs;

        // inputs from binTx
        inputs.push_back(std::make_pair(binTxId, 0));

        // outputs
        {
            double outAmount = static_cast<double>(xtx->toAmount)/TransactionDescr::COIN;
            outputs.push_back(std::make_pair(conn->fromXAddr(xtx->to), outAmount));
        }

        if (!conn->createPaymentTransaction(inputs, outputs,
                                            xtx->mPubKey, xtx->mPrivKey,
                                            x, innerScript,
                                            xtx->payTxId, xtx->payTx))
        {
            // cancel transaction
            LOG() << "payment transaction create error, transaction canceled " << __FUNCTION__;
            sendCancelTransaction(xtx, crRpcError);
            return true;
        }

        TXLOG() << "payment B sendrawtransaction " << xtx->payTx;

    } // payTx

    // send pay tx
    std::string sentid;
    int32_t errCode = 0;
    if (conn->sendRawTransaction(xtx->payTx, sentid, errCode))
    {
        LOG() << "payment B " << sentid;
    }
    else
    {
        if (errCode == -25)
        {
            // missing inputs, wait deposit tx
            // move packet to pending
            LOG() << "payment B not send, no deposit tx, move to pending";

            xapp.processLater(txid, packet);
            return true;
        }

        LOG() << "payment B tx not send, transaction canceled " << __FUNCTION__;
        sendCancelTransaction(xtx, crRpcError);
        return true;
    }

    xtx->state = TransactionDescr::trCommited;

    xuiConnector.NotifyXBridgeTransactionStateChanged(txid);

    // send reply
    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionConfirmedB));
    reply->append(hubAddress);
    reply->append(thisAddress);
    reply->append(txid.begin(), 32);

    sendPacket(hubAddress, reply);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::processTransactionConfirmedB(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    // size must be == 72 bytes
    if (packet->size() != 72)
    {
        ERR() << "invalid packet size for xbcTransactionConfirmedB "
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

    std::vector<unsigned char> from(packet->data()+20, packet->data()+40);
    uint256 txid(packet->data()+40);

    TransactionPtr tr = e.transaction(txid);
    boost::mutex::scoped_lock l(tr->m_lock);

    tr->updateTimestamp();

    if (!isAddressInTransaction(from, tr))
    {
        ERR() << "invalid transaction address " << __FUNCTION__;
        sendCancelTransaction(txid, crInvalidAddress);
        return true;
    }

    if (e.updateTransactionWhenConfirmedReceived(tr, from))
    {
        if (tr->state() == xbridge::Transaction::trFinished)
        {
            LOG() << "broadcast send xbcTransactionFinished";

            XBridgePacketPtr reply(new XBridgePacket(xbcTransactionFinished));
            reply->append(txid.begin(), 32);
            sendPacketBroadcast(reply);
        }
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::processTransactionCancel(XBridgePacketPtr packet)
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

    uint256 txid(packet->data());
    TxCancelReason reason = static_cast<TxCancelReason>(*reinterpret_cast<uint32_t*>(packet->data() + 32));

    return cancelOrRollbackTransaction(txid, reason);
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::cancelOrRollbackTransaction(const uint256 & txid, const TxCancelReason & reason)
{
    DEBUG_TRACE();

    // check and process packet if bridge is exchange
    Exchange & e = Exchange::instance();
    if (e.isStarted())
    {
        e.deletePendingTransactions(txid);
    }

    App & app = App::instance();

    TransactionDescrPtr xtx = app.transaction(txid);
    if (!xtx)
    {
        return true;
    }

    if (xtx->state < TransactionDescr::trCreated)
    {
        app.moveTransactionToHistory(txid);

        xtx->state  = TransactionDescr::trCancelled;
        xtx->reason = reason;
        xuiConnector.NotifyXBridgeTransactionStateChanged(txid);
    }
    else
    {
        // remove from pending packets (if added)
        app.removePackets(txid);

        // rollback, commit revert transaction
        WalletConnectorPtr conn = app.connectorByCurrency(xtx->fromCurrency);
        if (!conn)
        {
            WARN() << "no connector for <" << xtx->toCurrency << "> " << __FUNCTION__;
        }
        else
        {
            std::string sid;
            int32_t errCode = 0;
            if (!conn->sendRawTransaction(xtx->refTx, sid, errCode))
            {
                // TODO move packet to pending if error
                LOG() << "send rollback error, tx " << HexStr(txid) << " " << __FUNCTION__;
                xtx->state = TransactionDescr::trRollbackFailed;
            }
            else
            {
                xtx->state = TransactionDescr::trRollback;
            }
        }

        // update transaction state for gui
        xuiConnector.NotifyXBridgeTransactionStateChanged(txid);
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::finishTransaction(TransactionPtr tr)
{
    LOG() << "finish transaction <" << tr->id().GetHex() << ">";

    if (tr->state() != xbridge::Transaction::trConfirmed)
    {
        ERR() << "finished unconfirmed transaction <" << tr->id().GetHex() << ">";
        return false;
    }

    {
        XBridgePacketPtr reply(new XBridgePacket(xbcTransactionFinished));
        reply->append(tr->id().begin(), 32);
        sendPacketBroadcast(reply);
    }

    tr->finish();

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::sendCancelTransaction(const uint256 & txid,
                                          const TxCancelReason & reason)
{
    LOG() << "cancel transaction <" << txid.GetHex() << ">";

    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionCancel));
    reply->append(txid.begin(), 32);
    reply->append(static_cast<uint32_t>(reason));
    sendPacketBroadcast(reply);
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::sendCancelTransaction(const TransactionDescrPtr & tx,
                                           const TxCancelReason & reason)
{
    sendCancelTransaction(tx->id, reason);

    // update transaction state for gui
    tx->state  = TransactionDescr::trCancelled;
    tx->reason = reason;
    xuiConnector.NotifyXBridgeTransactionStateChanged(tx->id);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::rollbackTransaction(TransactionPtr tr)
{
    LOG() << "rollback transaction <" << tr->id().GetHex() << ">";

    if (tr->state() >= xbridge::Transaction::trCreated)
    {
        xbridge::App & app = xbridge::App::instance();
        app.sendRollbackTransaction(tr->id());
    }

    tr->finish();

    return true;
}

//*****************************************************************************
//*****************************************************************************
void Session::sendListOfTransactions()
{
    xbridge::App & xapp = xbridge::App::instance();

    // send my trx
    // TODO maybe move this to app?
    std::map<uint256, xbridge::TransactionDescrPtr> transactions = xapp.transactions();
    if (transactions.size())
    {
        // send pending transactions
        for (const auto & i : transactions)
        {
            if (i.second->state == xbridge::TransactionDescr::trPending)
            {
                xapp.sendPendingTransaction(i.second);
            }
        }
    }

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

        boost::mutex::scoped_lock l(ptr->m_lock);

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
        packet->append(static_cast<uint32_t>(boost::posix_time::to_time_t(ptr->createdTime())));
        m_p->sendPacketBroadcast(packet);
    }
}

//*****************************************************************************
//*****************************************************************************
void Session::eraseExpiredPendingTransactions()
{
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

        boost::mutex::scoped_lock l(ptr->m_lock);

        if (ptr->isExpired())
        {
            LOG() << "transaction expired <" << ptr->id().GetHex() << ">";
            e.deletePendingTransactions(ptr->id());
        }
    }
}

//*****************************************************************************
//*****************************************************************************
void Session::checkFinishedTransactions()
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

        boost::mutex::scoped_lock l(ptr->m_lock);

        uint256 txid = ptr->id();

        if (ptr->state() == xbridge::Transaction::trConfirmed)
        {
            // send finished
            LOG() << "confirmed transaction <" << txid.GetHex() << ">";
            m_p->finishTransaction(ptr);
        }
        else if (ptr->state() == xbridge::Transaction::trCancelled)
        {
            // drop cancelled tx
            LOG() << "drop cancelled transaction <" << txid.GetHex() << ">";
            ptr->drop();
        }
        else if (ptr->state() == xbridge::Transaction::trFinished)
        {
            // delete finished tx
            LOG() << "delete finished transaction <" << txid.GetHex() << ">";
            e.deleteTransaction(txid);
        }
        else if (ptr->state() == xbridge::Transaction::trDropped)
        {
            // delete dropped tx
            LOG() << "delete dropped transaction <" << txid.GetHex() << ">";
            e.deleteTransaction(txid);
        }
        else if (!ptr->isValid())
        {
            // delete invalid tx
            LOG() << "delete invalid transaction <" << txid.GetHex() << ">";
            e.deleteTransaction(txid);
        }
        else
        {
            LOG() << "timeout transaction <" << txid.GetHex() << ">"
                  << " state " << ptr->strState();

            // send rollback
            m_p->rollbackTransaction(ptr);
        }
    }
}

//*****************************************************************************
//*****************************************************************************
void Session::getAddressBook()
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
bool Session::Impl::processTransactionFinished(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    if (packet->size() != 32)
    {
        ERR() << "incorrect packet size for xbcTransactionFinished" << __FUNCTION__;
        return false;
    }

    // transaction id
    uint256 txid(packet->data());

    xbridge::App & xapp = xbridge::App::instance();

    TransactionDescrPtr xtx = xapp.transaction(txid);
    if (!xtx)
    {
        // LOG() << "unknown transaction " << HexStr(txid) << " " << __FUNCTION__;
        return true;
    }

    // update transaction state for gui
    xtx->state = TransactionDescr::trFinished;

    xuiConnector.NotifyXBridgeTransactionStateChanged(txid);

    return true;
}

//******************************************************************************
//******************************************************************************
bool Session::Impl::processTransactionRollback(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    if (packet->size() != 32)
    {
        ERR() << "incorrect packet size for xbcTransactionRollback" << __FUNCTION__;
        return false;
    }

    // transaction id
    uint256 txid(packet->data());

    xbridge::App & xapp = xbridge::App::instance();

    TransactionDescrPtr xtx = xapp.transaction(txid);
    if (!xtx)
    {
        LOG() << "unknown transaction " << HexStr(txid) << " " << __FUNCTION__;
        return true;
    }

    return cancelOrRollbackTransaction(xtx->id, crRollback);
}

} // namespace xbridge
