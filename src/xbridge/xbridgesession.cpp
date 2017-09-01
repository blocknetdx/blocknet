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

#include "json/json_spirit.h"
#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

using namespace json_spirit;

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
XBridgeSession::XBridgeSession()
{
    init();
}

//*****************************************************************************
//*****************************************************************************
XBridgeSession::XBridgeSession(const WalletParam & wallet)
    : m_wallet(wallet)
{
    init();
}

//*****************************************************************************
//*****************************************************************************
XBridgeSession::~XBridgeSession()
{
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::init()
{
    assert(!m_handlers.size());

    if (!rpc::getNewAddress(m_myid))
    {
        m_myid = std::vector<unsigned char>(20, 0);
        LOG() << "fail generate address for <" << m_wallet.currency << "> session " << __FUNCTION__;
        return;
    }

    // RAND_bytes(m_myid, sizeof(m_myid));
    LOG() << "session <" << m_wallet.currency << "> generated id <"
          << util::base64_encode(m_myid).c_str()
          << ">";

    // process invalid
    m_handlers[xbcInvalid]               .bind(this, &XBridgeSession::processInvalid);

    // m_handlers[xbcAnnounceAddresses]     .bind(this, &XBridgeSession::processAnnounceAddresses);

    // process transaction from client wallet
    // if (XBridgeExchange::instance().isEnabled())
    {
        m_handlers[xbcTransaction]           .bind(this, &XBridgeSession::processTransaction);
        m_handlers[xbcTransactionAccepting]  .bind(this, &XBridgeSession::processTransactionAccepting);
    }
    // else
    {
        m_handlers[xbcPendingTransaction]    .bind(this, &XBridgeSession::processPendingTransaction);
    }

    // transaction processing
    {
        m_handlers[xbcTransactionHold]       .bind(this, &XBridgeSession::processTransactionHold);
        m_handlers[xbcTransactionHoldApply]  .bind(this, &XBridgeSession::processTransactionHoldApply);

        m_handlers[xbcTransactionInit]       .bind(this, &XBridgeSession::processTransactionInit);
        m_handlers[xbcTransactionInitialized].bind(this, &XBridgeSession::processTransactionInitialized);

        m_handlers[xbcTransactionCreateA]    .bind(this, &XBridgeSession::processTransactionCreate);
        m_handlers[xbcTransactionCreateB]    .bind(this, &XBridgeSession::processTransactionCreate);
        m_handlers[xbcTransactionCreatedA]   .bind(this, &XBridgeSession::processTransactionCreatedA);
        m_handlers[xbcTransactionCreatedB]   .bind(this, &XBridgeSession::processTransactionCreatedB);

        m_handlers[xbcTransactionConfirmA]   .bind(this, &XBridgeSession::processTransactionConfirmA);
        m_handlers[xbcTransactionConfirmB]   .bind(this, &XBridgeSession::processTransactionConfirmB);

        m_handlers[xbcTransactionCancel]     .bind(this, &XBridgeSession::processTransactionCancel);
        m_handlers[xbcTransactionRollback]   .bind(this, &XBridgeSession::processTransactionRollback);
        m_handlers[xbcTransactionFinished]   .bind(this, &XBridgeSession::processTransactionFinished);
        m_handlers[xbcTransactionDropped]    .bind(this, &XBridgeSession::processTransactionDropped);

        m_handlers[xbcTransactionConfirmedA] .bind(this, &XBridgeSession::processTransactionConfirmedA);
        m_handlers[xbcTransactionConfirmedB] .bind(this, &XBridgeSession::processTransactionConfirmedB);

        // wallet received transaction
        m_handlers[xbcReceivedTransaction]   .bind(this, &XBridgeSession::processBitcoinTransactionHash);
    }

    m_handlers[xbcAddressBookEntry].bind(this, &XBridgeSession::processAddressBookEntry);

    // retranslate messages to xbridge network
    m_handlers[xbcXChatMessage].bind(this, &XBridgeSession::processXChatMessage);
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::start(XBridge::SocketPtr socket)
{
    // DEBUG_TRACE();

    LOG() << "client connected " << socket.get();

    m_socket = socket;

    doReadHeader(XBridgePacketPtr(new XBridgePacket));
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::disconnect()
{
    // DEBUG_TRACE();

    m_socket->close();

    LOG() << "client disconnected " << m_socket.get();

    XBridgeApp & app = XBridgeApp::instance();
    app.storageClean(shared_from_this());
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::doReadHeader(XBridgePacketPtr packet,
                                  const std::size_t offset)
{
    // DEBUG_TRACE();

    m_socket->async_read_some(
                boost::asio::buffer(packet->header()+offset,
                                    packet->headerSize-offset),
                boost::bind(&XBridgeSession::onReadHeader,
                            shared_from_this(),
                            packet, offset,
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred));
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::onReadHeader(XBridgePacketPtr packet,
                                  const std::size_t offset,
                                  const boost::system::error_code & error,
                                  std::size_t transferred)
{
    // DEBUG_TRACE();

    if (error)
    {
        ERR() << PrintErrorCode(error);
        disconnect();
        return;
    }

    if (offset + transferred != static_cast<size_t>(packet->headerSize))
    {
        LOG() << "partially read header, read " << transferred
              << " of " << packet->headerSize << " bytes";

        doReadHeader(packet, offset + transferred);
        return;
    }

    if (!checkXBridgePacketVersion(packet))
    {
        ERR() << "incorrect protocol version <" << packet->version() << "> " << __FUNCTION__;
        disconnect();
        return;
    }

    packet->alloc();
    doReadBody(packet);
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::doReadBody(XBridgePacketPtr packet,
                const std::size_t offset)
{
    // DEBUG_TRACE();

    m_socket->async_read_some(
                boost::asio::buffer(packet->data()+offset,
                                    packet->size()-offset),
                boost::bind(&XBridgeSession::onReadBody,
                            shared_from_this(),
                            packet, offset,
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred));
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::onReadBody(XBridgePacketPtr packet,
                                const std::size_t offset,
                                const boost::system::error_code & error,
                                std::size_t transferred = 0)
{
    // DEBUG_TRACE();

    if (error)
    {
        ERR() << PrintErrorCode(error);
        disconnect();
        return;
    }

    if (offset + transferred != packet->size())
    {
        LOG() << "partially read packet, read " << transferred
              << " of " << packet->size() << " bytes";

        doReadBody(packet, offset + transferred);
        return;
    }

    if (!processPacket(packet))
    {
        ERR() << "packet processing error " << __FUNCTION__;
    }

    doReadHeader(XBridgePacketPtr(new XBridgePacket));
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::encryptPacket(XBridgePacketPtr /*packet*/)
{
    // DEBUG_TRACE();
    // TODO implement this
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::decryptPacket(XBridgePacketPtr /*packet*/)
{
    // DEBUG_TRACE();
    // TODO implement this
    return true;
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::sendPacket(const std::vector<unsigned char> & to,
                                const XBridgePacketPtr & packet)
{
    XBridgeApp & app = XBridgeApp::instance();
    app.onSend(to, packet);
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::sendPacket(const std::string & to, const XBridgePacketPtr & packet)
{
    sendPacket(rpc::toXAddr(to), packet);
}

//*****************************************************************************
// return true if packet for me and need to process
//*****************************************************************************
bool XBridgeSession::checkPacketAddress(XBridgePacketPtr packet)
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

    for (const std::vector<unsigned char> & addr : m_addressBook)
    {
        if (addr.size() != 20)
        {
            assert(false && "incorrect address length");
            continue;
        }

        if (memcmp(packet->data(), &addr[0], 20) == 0)
        {
            // client address, need to process
            return true;
        }
    }

    // not for me
    return false;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processPacket(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();

    if (!decryptPacket(packet))
    {
        ERR() << "packet decoding error " << __FUNCTION__;
        return false;
    }

    XBridgeCommand c = packet->command();

    if (m_handlers.count(c) == 0)
    {
        m_handlers[xbcInvalid](packet);
        // ERR() << "incorrect command code <" << c << "> " << __FUNCTION__;
        return false;
    }

    TRACE() << "received packet, command code <" << c << ">";

    if (!m_handlers[c](packet))
    {
        ERR() << "packet processing error <" << c << "> " << __FUNCTION__;
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processInvalid(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();
    LOG() << "xbcInvalid instead of " << packet->command();
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processZero(XBridgePacketPtr /*packet*/)
{
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processAnnounceAddresses(XBridgePacketPtr /*packet*/)
{
//    // DEBUG_TRACE();

//    // size must be 20 bytes (160bit)
//    if (packet->size() != 20)
//    {
//        ERR() << "invalid packet size for xbcAnnounceAddresses "
//              << "need 20 received " << packet->size() << " "
//              << __FUNCTION__;
//        return false;
//    }

//    XBridgeApp & app = XBridgeApp::instance();
//    app.storageStore(shared_from_this(), packet->data());
    return true;
}

//*****************************************************************************
//*****************************************************************************
// static
bool XBridgeSession::checkXBridgePacketVersion(XBridgePacketPtr packet)
{
    if (packet->version() != static_cast<boost::uint32_t>(XBRIDGE_PROTOCOL_VERSION))
    {
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::sendXBridgeMessage(XBridgePacketPtr packet)
{
    boost::system::error_code error;
    m_socket->send(boost::asio::buffer(packet->header(), packet->allSize()), 0, error);
    if (error)
    {
        ERR() << "packet send error " << PrintErrorCode(error) << __FUNCTION__;
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::takeXBridgeMessage(const std::vector<unsigned char> & message)
{
    // DEBUG_TRACE();

    XBridgePacketPtr packet(new XBridgePacket());
    // packet->setData(message);
    if (!packet->copyFrom(message))
    {
        ERR() << "incorrect packet " << __FUNCTION__;
        return false;
    }

    // return sendXBridgeMessage(packet);
    return processPacket(packet);
}

//*****************************************************************************
// retranslate packets from wallet to xbridge network
//*****************************************************************************
bool XBridgeSession::processXChatMessage(XBridgePacketPtr /*packet*/)
{
    assert(!"check rhis fn");
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
void XBridgeSession::sendPacketBroadcast(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();

    XBridgeApp & app = XBridgeApp::instance();
    app.onSend(packet);
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processTransaction(XBridgePacketPtr packet)
{
    // check and process packet if bridge is exchange
    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isStarted())
    {
        return true;
    }

    // DEBUG_TRACE();
    DEBUG_TRACE_LOG(currencyToLog());

    // size must be > 132 bytes
    if (packet->size() <= 132)
    {
        ERR() << "invalid packet size for xbcTransaction "
              << "need min 132 bytes, received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    // read packet data
    uint256 id(packet->data());

    // source
    uint32_t offset = 32;
    std::string saddr(reinterpret_cast<const char *>(packet->data())+offset);
    offset += saddr.size() + 1;
    std::string scurrency((const char *)packet->data()+offset);
    offset += 8;
    uint64_t samount = *static_cast<boost::uint64_t *>(static_cast<void *>(packet->data()+offset));
    offset += sizeof(uint64_t);

    // destination
    std::string daddr(reinterpret_cast<const char *>(packet->data())+offset);
    offset += daddr.size() + 1;
    std::string dcurrency((const char *)packet->data()+offset);
    offset += 8;
    boost::uint64_t damount = *static_cast<boost::uint64_t *>(static_cast<void *>(packet->data()+offset));

    LOG() << "received transaction " << util::to_str(id) << std::endl
          << "    from " << saddr << std::endl
          << "             " << scurrency << " : " << samount << std::endl
          << "    to   " << daddr << std::endl
          << "             " << dcurrency << " : " << damount << std::endl;

    {
        bool isCreated = false;
        uint256 pendingId;
        if (!e.createTransaction(id,
                                 saddr, scurrency, samount,
                                 daddr, dcurrency, damount,
                                 pendingId, isCreated))
        {
            // not created, send cancel
            sendCancelTransaction(id, crXbridgeRejected);
            return true;
        }

        // tx created

        // TODO send signal to gui for debug
        {
            XBridgeTransactionDescr d;
            d.id           = id;
            d.fromCurrency = scurrency;
            d.fromAmount   = samount;
            d.toCurrency   = dcurrency;
            d.toAmount     = damount;
            d.state        = XBridgeTransactionDescr::trPending;

            xuiConnector.NotifyXBridgePendingTransactionReceived(d);
        }

        XBridgeTransactionPtr tr = e.pendingTransaction(pendingId);
        if (tr->id() == uint256())
        {
            LOG() << "transaction not found after create. "
                  << util::to_str(id);
            return false;
        }

        LOG() << "transaction created, id "
              << util::to_str(id);

        if (isCreated)
        {
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
            reply->append(sessionAddr());

            sendPacketBroadcast(reply);
        }
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processPendingTransaction(XBridgePacketPtr packet)
{
    XBridgeExchange & e = XBridgeExchange::instance();
    if (e.isEnabled())
    {
        return true;
    }

    DEBUG_TRACE_LOG(currencyToLog());

    if (packet->size() != 84)
    {
        ERR() << "incorrect packet size for xbcPendingTransaction "
              << "need 88 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    XBridgeTransactionDescrPtr ptr(new XBridgeTransactionDescr);
    ptr->id           = uint256(packet->data());
    ptr->fromCurrency = std::string(reinterpret_cast<const char *>(packet->data()+32));
    ptr->fromAmount   = *reinterpret_cast<boost::uint64_t *>(packet->data()+40);
    ptr->toCurrency   = std::string(reinterpret_cast<const char *>(packet->data()+48));
    ptr->toAmount     = *reinterpret_cast<boost::uint64_t *>(packet->data()+56);
    ptr->hubAddress   = std::vector<unsigned char>(packet->data()+64, packet->data()+84);
    ptr->tax          = *reinterpret_cast<boost::uint32_t *>(packet->data()+84);
    ptr->state        = XBridgeTransactionDescr::trPending;

    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);
        if (!XBridgeApp::m_pendingTransactions.count(ptr->id))
        {
            // new transaction, copy data
            XBridgeApp::m_pendingTransactions[ptr->id] = ptr;
        }
        else
        {
            // existing, update timestamp
            XBridgeApp::m_pendingTransactions[ptr->id]->updateTimestamp(*ptr);
        }
    }

    LOG() << "received tx <" << util::to_str(ptr->id) << "> " << __FUNCTION__;

    xuiConnector.NotifyXBridgePendingTransactionReceived(*ptr);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processTransactionAccepting(XBridgePacketPtr packet)
{
    // check and process packet if bridge is exchange
    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isStarted())
    {
        return true;
    }

    // DEBUG_TRACE();
    DEBUG_TRACE_LOG(currencyToLog());

    if (packet->size() <= 152)
    {
        ERR() << "invalid packet size for xbcTransactionAccepting "
              << "need min 152 bytes, received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    // source
    uint32_t offset = 52;
    std:: string saddr(reinterpret_cast<const char *>(packet->data()+offset));
    offset += saddr.size()+1;
    std::string scurrency((const char *)packet->data()+offset);
    offset += 8;
    uint64_t samount = *static_cast<uint64_t *>(static_cast<void *>(packet->data()+offset));
    offset += sizeof(uint64_t);

    // destination
    std::string daddr(reinterpret_cast<const char *>(packet->data()+offset));
    offset += daddr.size()+1;
    std::string dcurrency((const char *)packet->data()+offset);
    offset += 8;
    uint64_t damount = *static_cast<uint64_t *>(static_cast<void *>(packet->data()+offset));
    // offset += sizeof(uint64_t);

    // read packet data
    uint256 id(packet->data());

    // TODO for debug
//    {
//        XBridgeTransactionDescr d;
//        d.id           = id;
//        d.fromCurrency = scurrency;
//        d.fromAmount   = samount;
//        d.toCurrency   = dcurrency;
//        d.toAmount     = damount;
//        d.state        = XBridgeTransactionDescr::trPending;
//        xuiConnector.NotifyXBridgePendingTransactionReceived(d);
//    }

    LOG() << "received accepting transaction " << util::to_str(id) << std::endl
          << "    from " << saddr << std::endl
          << "             " << scurrency << " : " << samount << std::endl
          << "    to   " << daddr << std::endl
          << "             " << dcurrency << " : " << damount << std::endl;

    {
        uint256 transactionId;
        if (e.acceptTransaction(id, saddr, scurrency, samount, daddr, dcurrency, damount, transactionId))
        {
            // check transaction state, if trNew - do nothing,
            // if trJoined = send hold to client
            XBridgeTransactionPtr tr = e.transaction(transactionId);

            boost::mutex::scoped_lock l(tr->m_lock);

            if (tr && tr->state() == XBridgeTransaction::trJoined)
            {
                // send hold to clients

                // first
                // TODO remove this log
                LOG() << "send xbcTransactionHold ";

                XBridgePacketPtr reply1(new XBridgePacket(xbcTransactionHold));
                reply1->append(sessionAddr());
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
bool XBridgeSession::processTransactionHold(XBridgePacketPtr packet)
{
    DEBUG_TRACE_LOG(currencyToLog());

    if (packet->size() != 85 && packet->size() != 117)
    {
        ERR() << "incorrect packet size for xbcTransactionHold "
              << "need 85 or 117 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    // servicenode addr
    std::vector<unsigned char> hubAddress(packet->data(), packet->data()+20);
    uint32_t offset = 20;

    // read packet data
    uint256 id(packet->data()+offset);
    offset += 32;

    // service node pub key
    CPubKey pksnode;
    {
        uint32_t len = CPubKey::GetLen(*(char *)(packet->data()+offset));
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
        XBridgeExchange & e = XBridgeExchange::instance();
        if (e.isStarted())
        {
            XBridgeTransactionPtr tr = e.transaction(id);

            boost::mutex::scoped_lock l(tr->m_lock);

            if (!tr || tr->state() != XBridgeTransaction::trJoined)
            {
                e.deletePendingTransactions(id);

                xuiConnector.NotifyXBridgeTransactionStateChanged(id, XBridgeTransactionDescr::trFinished);
            }

            return true;
        }
    }

    XBridgeTransactionDescrPtr xtx;

    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_pendingTransactions.count(id))
        {
            // wtf? unknown transaction
            assert(!"unknown transaction");
            LOG() << "unknown transaction " << util::to_str(id) << " " << __FUNCTION__;
            return true;
        }

        if (XBridgeApp::m_transactions.count(id))
        {
            // wtf?
            assert(!"duplicate transaction");
            LOG() << "duplicate transaction " << util::to_str(id) << " " << __FUNCTION__;
            return true;
        }

        xtx = XBridgeApp::m_pendingTransactions[id];

        // remove from pending
        XBridgeApp::m_pendingTransactions.erase(id);

        if (!xtx->isLocal())
        {
            xtx->state = XBridgeTransactionDescr::trFinished;
            XBridgeApp::m_historicTransactions[id] = xtx;
        }
        else
        {
            // move to processing
            XBridgeApp::m_transactions[id] = xtx;

            xtx->state = XBridgeTransactionDescr::trHold;
        }
    }

    xuiConnector.NotifyXBridgeTransactionStateChanged(id, xtx->state);

    if (xtx->isLocal())
    {
        // send hold apply
        XBridgePacketPtr reply(new XBridgePacket(xbcTransactionHoldApply));
        reply->append(hubAddress);
        reply->append(rpc::toXAddr(xtx->from));
        reply->append(id.begin(), 32);

        sendPacket(hubAddress, reply);
    }
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processTransactionHoldApply(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();
    DEBUG_TRACE_LOG(currencyToLog());

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

    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isStarted())
    {
        return true;
    }

    std::vector<unsigned char> from(packet->data()+20, packet->data()+40);

    // transaction id
    uint256 id(packet->data()+40);

    XBridgeTransactionPtr tr = e.transaction(id);
    boost::mutex::scoped_lock l(tr->m_lock);

    tr->updateTimestamp();

    std::string sfrom = tr->fromXAddr(from);
    if (!sfrom.size())
    {
        ERR() << "invalid transaction address " << __FUNCTION__;
        sendCancelTransaction(id, crInvalidAddress);
        return true;
    }

    if (e.updateTransactionWhenHoldApplyReceived(tr, sfrom))
    {
        if (tr->state() == XBridgeTransaction::trHold)
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
                  << tr->a_destination();

            XBridgePacketPtr reply1(new XBridgePacket(xbcTransactionInit));
            reply1->append(rpc::toXAddr(tr->a_destination()));
            reply1->append(sessionAddr());
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

            sendPacket(rpc::toXAddr(tr->a_destination()), reply1);

            // second
            // TODO remove this log
            LOG() << "send xbcTransactionInit to "
                  << tr->b_destination();

            XBridgePacketPtr reply2(new XBridgePacket(xbcTransactionInit));
            reply2->append(rpc::toXAddr(tr->b_destination()));
            reply2->append(sessionAddr());
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

            sendPacket(rpc::toXAddr(tr->b_destination()), reply2);
        }
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processTransactionInit(XBridgePacketPtr packet)
{
    DEBUG_TRACE_LOG(currencyToLog());

    if (packet->size() <= 205)
    {
        ERR() << "incorrect packet size for xbcTransactionInit "
              << "need 205 or 237 bytes min, received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    std::vector<unsigned char> thisAddress(packet->data(), packet->data()+20);
    std::vector<unsigned char> hubAddress(packet->data()+20, packet->data()+40);

    uint256 txid(packet->data()+40);

    uint32_t offset = 72;

    // service node pub key
    CPubKey pksnode;
    {
        uint32_t len = CPubKey::GetLen(*(char *)(packet->data()+offset));
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

    std::string   from(reinterpret_cast<const char *>(packet->data()+offset));
    offset += from.size()+1;
    std::string   fromCurrency(reinterpret_cast<const char *>(packet->data()+offset));
    offset += 8;
    uint64_t      fromAmount(*reinterpret_cast<uint64_t *>(packet->data()+offset));
    offset += sizeof(uint64_t);

    std::string   to(reinterpret_cast<const char *>(packet->data()+offset));
    offset += to.size()+1;
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

    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_transactions.count(txid))
        {
            // wtf? unknown transaction
            LOG() << "unknown transaction " << util::to_str(txid) << " " << __FUNCTION__;
            return true;
        }

        xtx = XBridgeApp::m_transactions[txid];
    }

    assert(xtx->id           == txid);
    assert(xtx->from         == from);
    assert(xtx->fromCurrency == fromCurrency);
    assert(xtx->fromAmount   == fromAmount);
    assert(xtx->to           == to);
    assert(xtx->toCurrency   == toCurrency);
    assert(xtx->toAmount     == toAmount);

    xtx->role = role;

    // m key
    {
        xbridge::CKey km;
        km.MakeNewKey(true);

        xtx->mPubKey = km.GetPubKey();
        xtx->mSecret = xbridge::CBitcoinSecret(km);
    }

    // x key
    uint256 datatxtd;
    if (role == 'A')
    {
        xbridge::CKey kx;
        kx.MakeNewKey(true);

        xtx->xPubKey = kx.GetPubKey();
        xtx->xSecret = xbridge::CBitcoinSecret(kx);

        // send blocknet tx with hash of X
        CKeyID xid = xtx->xPubKey.GetID();
        std::string strtxid;
        if (!rpc::storeDataIntoBlockchain(snodeAddress, m_wallet.serviceNodeFee,
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
    reply->append(xtx->mPubKey.begin(), xtx->mPubKey.size());

    sendPacket(hubAddress, reply);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processTransactionInitialized(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();
    DEBUG_TRACE_LOG(currencyToLog());

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

    XBridgeExchange & e = XBridgeExchange::instance();
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
    // std::vector<unsigned char> hx(packet->data()+offset, packet->data()+offset+20);
    offset += 32;

    // opponent publick key
    xbridge::CPubKey pk1(packet->data()+offset, packet->data()+offset+33);
    // offset += 33;

    XBridgeTransactionPtr tr = e.transaction(id);
    boost::mutex::scoped_lock l(tr->m_lock);

    tr->updateTimestamp();

    std::string sfrom = tr->fromXAddr(from);
    if (!sfrom.size())
    {
        ERR() << "invalid transaction address " << __FUNCTION__;
        sendCancelTransaction(id, crInvalidAddress);
        return true;
    }

    if (e.updateTransactionWhenInitializedReceived(tr, sfrom, datatxid, pk1))
    {
        if (tr->state() == XBridgeTransaction::trInitialized)
        {
            // send create transaction command to clients

            // first
            // TODO remove this log
            LOG() << "send xbcTransactionCreate to "
                  << tr->a_address();

            // send xbcTransactionCreate
            // with nLockTime == lockTime*2 for first client,
            // with nLockTime == lockTime*4 for second
            XBridgePacketPtr reply1(new XBridgePacket(xbcTransactionCreateA));
            reply1->append(rpc::toXAddr(tr->a_address()));
            reply1->append(sessionAddr());
            reply1->append(id.begin(), 32);
            reply1->append(tr->b_destination());
            reply1->append(tr->a_datatxid().begin(), 32);
            reply1->append(tr->b_pk1().begin(), tr->b_pk1().size());

            sendPacket(tr->a_address(), reply1);
        }
    }

    return true;
}

//******************************************************************************
// calculate tx fee for deposit tx
// output count always 1
//******************************************************************************
double XBridgeSession::minTxFee1(const uint32_t inputCount, const uint32_t outputCount)
{
    uint64_t fee = (148*inputCount + 34*outputCount + 10) * m_wallet.feePerByte;
    if (fee < m_wallet.minTxFee)
    {
        fee = m_wallet.minTxFee;
    }
    return (double)fee / m_wallet.COIN;
}

//******************************************************************************
// calculate tx fee for payment/refund tx
// input count always 1
//******************************************************************************
double XBridgeSession::minTxFee2(const uint32_t inputCount, const uint32_t outputCount)
{
    uint64_t fee = (180*inputCount + 34*outputCount + 10) * m_wallet.feePerByte;
    if (fee < m_wallet.minTxFee)
    {
        fee = m_wallet.minTxFee;
    }
    return (double)fee / m_wallet.COIN;
}

//******************************************************************************
//******************************************************************************
std::string XBridgeSession::round_x(const long double val, uint32_t prec)
{
    long double value = val;
    value *= pow(10, prec);

    value += .5;
    value = std::floor(value);

    std::string svalue = boost::lexical_cast<std::string>(value); // std::to_string(value);

    if (prec >= svalue.length())
    {
        svalue.insert(0, prec-svalue.length()+1, '0');
    }
    svalue.insert(svalue.length()-prec, 1, '.');

    value = std::stold(svalue);
//    return value;
    return svalue;
}

//******************************************************************************
//******************************************************************************
uint32_t XBridgeSession::lockTime(const char /*role*/) const
{
    assert(!"not implemented");
    return 0;

    // lock time
//    uint32_t lt = 0;
//    if (role == 'A')
//    {
//        lt = 259200; // 72h in seconds
//    }
//    else if (role == 'B')
//    {
//        lt = 259200/2; // 36h in seconds
//    }

//    time_t local = time(0); // GetAdjustedTime();
//    return lt += local;
}

//******************************************************************************
//******************************************************************************
xbridge::CTransactionPtr XBridgeSession::createTransaction()
{
    return xbridge::CTransactionPtr(new xbridge::CXCTransaction);
}

//******************************************************************************
//******************************************************************************
xbridge::CTransactionPtr XBridgeSession::createTransaction(const std::vector<std::pair<std::string, int> > & inputs,
                                                           const std::vector<std::pair<CScript, double> > &outputs,
                                                           const uint32_t lockTime)
{
    xbridge::CTransactionPtr tx(new xbridge::CXCTransaction);
    tx->nVersion  = m_wallet.txVersion;
    tx->nLockTime = lockTime;

    for (const std::pair<std::string, int> & in : inputs)
    {
        tx->vin.push_back(CTxIn(COutPoint(uint256(in.first), in.second)));
    }

    for (const std::pair<CScript, double> & out : outputs)
    {
        tx->vout.push_back(CTxOut(out.second*m_wallet.COIN, out.first));
    }

    return tx;
}

//******************************************************************************
//******************************************************************************
std::string XBridgeSession::createRawTransaction(const std::vector<std::pair<std::string, int> > & inputs,
                                                 const std::vector<std::pair<CScript, double> > &outputs,
                                                 const uint32_t lockTime)
{
    xbridge::CTransactionPtr tx(createTransaction(inputs, outputs, lockTime));
    return tx->toString();
    return std::string();
}

//******************************************************************************
// return false if deposit tx not found (need wait tx)
// true if tx found and checked
// isGood == true id depost tx is OK
//******************************************************************************
bool XBridgeSession::checkDepositTx(const XBridgeTransactionDescrPtr & /*xtx*/,
                                    const std::string & depositTxId,
                                    const uint32_t & confirmations,
                                    const uint64_t & /*neededAmount*/,
                                    bool & isGood)
{
    isGood  = false;

    std::string rawtx;
    if (!rpc::getRawTransaction(m_wallet.user, m_wallet.passwd,
                                m_wallet.ip, m_wallet.port,
                                depositTxId, true, rawtx))
    {
        LOG() << "no tx found " << depositTxId << " " << __FUNCTION__;
        return false;
    }

    // check confirmations
    json_spirit::Value txv;
    if (!json_spirit::read_string(rawtx, txv))
    {
        LOG() << "json read error for " << depositTxId << " " << rawtx << " " << __FUNCTION__;
        return false;
    }

    json_spirit::Object txo = txv.get_obj();

    if (confirmations > 0)
    {
        json_spirit::Value txvConfCount = json_spirit::find_value(txo, "confirmations");
        if (txvConfCount.type() != json_spirit::int_type)
        {
            // not found confirmations field, wait
            LOG() << "confirmations not found in " << rawtx << " " << __FUNCTION__;
            return false;
        }

        if (confirmations > static_cast<uint32_t>(txvConfCount.get_int()))
        {
            // wait more
            LOG() << "tx " << depositTxId << " unconfirmed, need " << confirmations << " " << __FUNCTION__;
            return false;
        }
    }

    // TODO check amount in tx

    isGood = true;

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processTransactionCreate(XBridgePacketPtr packet)
{
    DEBUG_TRACE_LOG(currencyToLog());

    if (packet->size() < 172)
    {
        ERR() << "incorrect packet size for xbcTransactionCreate "
              << "need min 205 bytes, received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    std::vector<unsigned char> thisAddress(packet->data(), packet->data()+20);
    std::vector<unsigned char> hubAddress(packet->data()+20, packet->data()+40);

    // transaction id
    uint256 txid(packet->data()+40);

    // destination address
    uint32_t offset = 72;
    std::string destAddress(reinterpret_cast<const char *>(packet->data()+offset));
    offset += destAddress.size()+1;

    uint256 datatxid(packet->data()+offset);
    offset += 32;

    xbridge::CPubKey mPubkey(packet->data()+offset, packet->data()+offset+33);
    offset += 33;

    std::vector<unsigned char> hx;
    if (!rpc::getDataFromTx(datatxid.GetHex(), hx))
    {
        // no data, move to pending
        boost::mutex::scoped_lock l(XBridgeApp::m_ppLocker);
        XBridgeApp::m_pendingPackets[txid] = std::make_pair(m_wallet.currency, packet);
        return true;
    }
    else
    {
        // remove from pending packets (if added)
        boost::mutex::scoped_lock l(XBridgeApp::m_ppLocker);
        XBridgeApp::m_pendingPackets.erase(txid);
    }

    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_transactions.count(txid))
        {
            // wtf? unknown transaction
            LOG() << "unknown transaction " << util::to_str(txid) << " " << __FUNCTION__;
            return true;
        }

        xtx = XBridgeApp::m_transactions[txid];
    }

    if (xtx->role == 'B')
    {
        // for B need to check A deposit tx
        // check packet length

        std::string binATxId(reinterpret_cast<const char *>(packet->data()+offset));
        offset += binATxId.size()+1;

        if (binATxId.size() == 0)
        {
            LOG() << "bad A deposit tx id received for " << util::to_str(txid) << " " << __FUNCTION__;
            sendCancelTransaction(xtx, crBadADepositTx);
            return true;
        }

        XBridgeSessionPtr receiver = XBridgeApp::instance().sessionByCurrency(xtx->toCurrency);
        if (!receiver)
        {
            LOG() << "no active session for " << xtx->toCurrency << " " << __FUNCTION__;
            sendCancelTransaction(xtx, crBadADepositTx);
            return true;
        }

        bool isGood = false;
        if (!receiver->checkDepositTx(xtx, binATxId, m_wallet.requiredConfirmations, 0, isGood))
        {
            // move packet to pending
            boost::mutex::scoped_lock l(XBridgeApp::m_ppLocker);
            XBridgeApp::m_pendingPackets[txid] = std::make_pair(m_wallet.currency, packet);
            return true;
        }
        else if (!isGood)
        {
            LOG() << "check A deposit tx error for " << util::to_str(txid) << " " << __FUNCTION__;
            sendCancelTransaction(xtx, crBadADepositTx);
            return true;
        }

        LOG() << "deposit A tx confirmed " << util::to_str(txid);
    }

    std::vector<rpc::Unspent> entries;
    if (!rpc::listUnspent(m_wallet.user, m_wallet.passwd,
                          m_wallet.ip, m_wallet.port, entries))
    {
        LOG() << "rpc::listUnspent failed" << __FUNCTION__;
        sendCancelTransaction(xtx, crRpcError);
        return true;
    }

    double outAmount = static_cast<double>(xtx->fromAmount) / XBridgeTransactionDescr::COIN;

    double fee1      = 0;
    double fee2      = minTxFee2(1, 1);
    double inAmount  = 0;

    std::vector<rpc::Unspent> usedInTx;
    for (const rpc::Unspent & entry : entries)
    {
        usedInTx.push_back(entry);
        inAmount += entry.amount;
        fee1 = minTxFee1(usedInTx.size(), 3);

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
    uint32_t lTime = lockTime(xtx->role);
    if (lTime == 0)
    {
        LOG() << "lockTime error, transaction canceled " << __FUNCTION__;
        sendCancelTransaction(xtx, crRpcError);
        return true;
    }

    // create transactions

    // create address for first tx
    {
        CScript inner;
        inner << OP_IF
                    << lTime << OP_CHECKLOCKTIMEVERIFY << OP_DROP
                    << OP_DUP << OP_HASH160 << xtx->mPubKey.GetID() << OP_EQUALVERIFY << OP_CHECKSIG
              << OP_ELSE
                    << OP_DUP << OP_HASH160 << mPubkey.GetID() << OP_EQUALVERIFY << OP_CHECKSIGVERIFY
                    << OP_HASH160 << hx << OP_EQUAL
              << OP_ENDIF;

        xbridge::XBitcoinAddress baddr;
        baddr.Set(CScriptID(inner), m_wallet.scriptPrefix[0]);

        xtx->multisig    = baddr.ToString();
        xtx->innerScript = HexStr(inner.begin(), inner.end());

    } // address for first tx

    // binTx
    std::string binjson;
    {
        std::vector<std::pair<std::string, int> >    inputs;
        std::vector<std::pair<std::string, double> > outputs;

        // inputs
        for (const rpc::Unspent & entry : usedInTx)
        {
            inputs.push_back(std::make_pair(entry.txId, entry.vout));
        }

        // outputs

        // amount
        outputs.push_back(std::make_pair(xtx->multisig, outAmount+fee2));

        // rest
        if (inAmount > outAmount+fee1+fee2)
        {
            std::string addr;
            if (!rpc::getNewAddress(m_wallet.user, m_wallet.passwd,
                                    m_wallet.ip, m_wallet.port,
                                    addr))
            {
                // cancel transaction
                LOG() << "rpc error, transaction canceled " << __FUNCTION__;
                sendCancelTransaction(xtx, crRpcError);
                return true;
            }

            double rest = inAmount-outAmount-fee1-fee2;
            outputs.push_back(std::make_pair(addr, rest));
        }

        std::string bintx;
        if (!rpc::createRawTransaction(m_wallet.user, m_wallet.passwd,
                                       m_wallet.ip, m_wallet.port,
                                       inputs, outputs, 0, bintx))
        {
            // cancel transaction
            LOG() << "create transaction error, transaction canceled " << __FUNCTION__;
            sendCancelTransaction(xtx, crRpcError);
            return true;
        }

        // sign
        bool complete = false;
        if (!rpc::signRawTransaction(m_wallet.user, m_wallet.passwd,
                                     m_wallet.ip, m_wallet.port,
                                     bintx, complete))
        {
            // do not sign, cancel
            LOG() << "sign transaction error, transaction canceled " << __FUNCTION__;
            sendCancelTransaction(xtx, crNotSigned);
            return true;
        }

        assert(complete && "not fully signed");

        std::string bintxid;
        if (!rpc::decodeRawTransaction(m_wallet.user, m_wallet.passwd,
                                       m_wallet.ip, m_wallet.port,
                                       bintx, bintxid, binjson))
        {
            LOG() << "decode signed transaction error, transaction canceled " << __FUNCTION__;
            sendCancelTransaction(xtx, crRpcError);
            return true;
        }

        TXLOG() << "deposit sendrawtransaction " << bintx;
        // TXLOG() << binjson;

        xtx->binTx   = bintx;
        xtx->binTxId = bintxid;

    } // binTx

    // refTx
    {
        std::vector<std::pair<std::string, int> > inputs;
        std::vector<std::pair<CScript, double> >  outputs;

        // inputs from binTx
        inputs.push_back(std::make_pair(xtx->binTxId, 0));

        // outputs
        {
            std::string addr;
            if (!rpc::getNewAddress(m_wallet.user, m_wallet.passwd, m_wallet.ip, m_wallet.port, addr))
            {
                // cancel transaction
                LOG() << "rpc error, transaction canceled " << __FUNCTION__;
                sendCancelTransaction(xtx, crRpcError);
                return true;
            }

            CScript scr = GetScriptForDestination(xbridge::XBitcoinAddress(addr).Get());

            outputs.push_back(std::make_pair(scr, outAmount));
        }

        xbridge::CTransactionPtr txUnsigned(createTransaction(inputs, outputs, lTime));
        txUnsigned->vin[0].nSequence = std::numeric_limits<uint32_t>::max()-1;

        std::vector<unsigned char> vchinner = ParseHex(xtx->innerScript.c_str());
        CScript inner(vchinner.begin(), vchinner.end());

        xbridge::CKey m = xtx->mSecret.GetKey();
        if (!m.IsValid())
        {
            // cancel transaction
            LOG() << "sign transaction error (SetSecret failed), transaction canceled " << __FUNCTION__;
            sendCancelTransaction(xtx, crNotSigned);
            return true;
        }

        CScript redeem;
        {
            CScript tmp;
            std::vector<unsigned char> raw(xtx->mPubKey.begin(), xtx->mPubKey.end());
            tmp << raw << OP_TRUE << inner;

            std::vector<unsigned char> signature2;
            {
                uint256 hash = xbridge::SignatureHash2(inner, txUnsigned, 0, SIGHASH_ALL);
                if (!m.Sign(hash, signature2))
                {
                    // cancel transaction
                    LOG() << "sign transaction error, transaction canceled " << __FUNCTION__;
                    sendCancelTransaction(xtx, crNotSigned);
                    return true;
                }

                signature2.push_back((unsigned char)SIGHASH_ALL);

                // TXLOG() << "signature2 " << HexStr(signature2.begin(), signature2.end());
            }

            redeem << signature2;
            redeem += tmp;
        }

        std::vector<unsigned char> raw(xtx->mPubKey.begin(), xtx->mPubKey.end());
        // TXLOG() << "xtx->mPubKey " << HexStr(raw.begin(), raw.end());
        // TXLOG() << "xtx->mPubKey->GetID " << xtx->mPubKey.GetID().GetHex();
        // TXLOG() << "inner " << HexStr(inner.begin(), inner.end());

        xbridge::CTransactionPtr tx(createTransaction());
        tx->nVersion  = txUnsigned->nVersion;
        tx->vin.push_back(CTxIn(txUnsigned->vin[0].prevout, redeem, std::numeric_limits<uint32_t>::max()-1));
        tx->vout      = txUnsigned->vout;
        tx->nLockTime = txUnsigned->nLockTime;

        std::string reftx = tx->toString();
        std::string json;
        std::string reftxid;

        if (!rpc::decodeRawTransaction(m_wallet.user, m_wallet.passwd,
                                       m_wallet.ip, m_wallet.port,
                                       reftx, reftxid, json))
        {
            LOG() << "decode signed transaction error, transaction canceled " << __FUNCTION__;
            sendCancelTransaction(xtx, crRpcError);
            return true;
        }

        TXLOG() << "refund sendrawtransaction " << reftx;
        // TXLOG() << json;

        xtx->refTx   = reftx;
        xtx->refTxId = reftxid;

    } // refTx

    xtx->state = XBridgeTransactionDescr::trCreated;

    xuiConnector.NotifyXBridgeTransactionStateChanged(txid, xtx->state);

    // send transactions
    {
        std::string sentid;
        int32_t errCode = 0;
        if (rpc::sendRawTransaction(m_wallet.user, m_wallet.passwd,
                                    m_wallet.ip, m_wallet.port, xtx->binTx, sentid, errCode))
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
        assert(!"unknown role");
        ERR() << "unknown role " << __FUNCTION__;
        return false;
    }

    reply->append(hubAddress);
    reply->append(thisAddress);
    reply->append(txid.begin(), 32);
    reply->append(xtx->binTxId);
    reply->append(xtx->innerScript);

    sendPacket(hubAddress, reply);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processTransactionCreatedA(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();
    DEBUG_TRACE_LOG(currencyToLog());

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

    XBridgeExchange & e = XBridgeExchange::instance();
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

    std::string innerScript(reinterpret_cast<const char *>(packet->data()+offset));
    // offset += innerScript.size()+1;

    XBridgeTransactionPtr tr = e.transaction(txid);
    boost::mutex::scoped_lock l(tr->m_lock);

    tr->updateTimestamp();

    std::string sfrom = tr->fromXAddr(from);
    if (!sfrom.size())
    {
        ERR() << "invalid transaction address " << __FUNCTION__;
        sendCancelTransaction(txid, crInvalidAddress);
        return true;
    }

    if (e.updateTransactionWhenCreatedReceived(tr, sfrom, binTxId, innerScript))
    {
        // wtf ?
        assert(!"invalid createdA");

        ERR() << "invalid createdA " << __FUNCTION__;
        sendCancelTransaction(txid, crInvalidAddress);
        return true;
    }

    // TODO remove this log
    LOG() << "send xbcTransactionCreate to "
          << tr->b_address();

    XBridgePacketPtr reply2(new XBridgePacket(xbcTransactionCreateB));
    reply2->append(rpc::toXAddr(tr->b_address()));
    reply2->append(sessionAddr());
    reply2->append(txid.begin(), 32);
    reply2->append(tr->a_destination());
    reply2->append(tr->a_datatxid().begin(), 32);
    reply2->append(tr->a_pk1().begin(), tr->a_pk1().size());
    reply2->append(binTxId);

    sendPacket(tr->b_address(), reply2);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processTransactionCreatedB(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();
    DEBUG_TRACE_LOG(currencyToLog());

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

    XBridgeExchange & e = XBridgeExchange::instance();
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

    std::string innerScript(reinterpret_cast<const char *>(packet->data()+offset));
    // offset += innerScript.size()+1;

    XBridgeTransactionPtr tr = e.transaction(txid);
    boost::mutex::scoped_lock l(tr->m_lock);

    tr->updateTimestamp();

    std::string sfrom = tr->fromXAddr(from);
    if (!sfrom.size())
    {
        ERR() << "invalid transaction address " << __FUNCTION__;
        sendCancelTransaction(txid, crInvalidAddress);
        return true;
    }

    if (e.updateTransactionWhenCreatedReceived(tr, sfrom, binTxId, innerScript))
    {
        if (tr->state() == XBridgeTransaction::trCreated)
        {
            // send confirm packets with deposit tx id
            // for create payment tx

            // TODO remove this log
            LOG() << "send xbcTransactionConfirmA to "
                  << tr->a_destination();

            XBridgePacketPtr reply(new XBridgePacket(xbcTransactionConfirmA));
            reply->append(rpc::toXAddr(tr->a_destination()));
            reply->append(sessionAddr());
            reply->append(txid.begin(), 32);
            reply->append(tr->b_bintxid());
            reply->append(tr->b_innerScript());

            sendPacket(tr->a_destination(), reply);
        }
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processTransactionConfirmA(XBridgePacketPtr packet)
{
    DEBUG_TRACE_LOG(currencyToLog());

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

    std::string innerScript(reinterpret_cast<const char *>(packet->data()+offset));
    offset += innerScript.size()+1;

    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_transactions.count(txid))
        {
            // wtf? unknown transaction
            LOG() << "unknown transaction " << util::to_str(txid) << " " << __FUNCTION__;
            return true;
        }

        xtx = XBridgeApp::m_transactions[txid];
    }

    // check B deposit tx
    {
        // TODO check tx in blockchain and move packet to pending if not

        bool isGood = false;
        if (!checkDepositTx(xtx, binTxId, m_wallet.requiredConfirmations, 0, isGood))
        {
            // move packet to pending
            boost::mutex::scoped_lock l(XBridgeApp::m_ppLocker);
            XBridgeApp::m_pendingPackets[txid] = std::make_pair(m_wallet.currency, packet);
            return true;
        }
        else if (!isGood)
        {
            LOG() << "check B deposit tx error for " << util::to_str(txid) << " " << __FUNCTION__;
            sendCancelTransaction(xtx, crBadBDepositTx);
            return true;
        }

        LOG() << "deposit B tx confirmed " << util::to_str(txid);
    }

    // payTx
    {
        std::vector<std::pair<std::string, int> > inputs;
        std::vector<std::pair<CScript, double> >  outputs;

        // inputs from binTx
        inputs.push_back(std::make_pair(binTxId, 0));

        // outputs
        {
            CScript scr = GetScriptForDestination(xbridge::XBitcoinAddress(xtx->to).Get());

            double outAmount = static_cast<double>(xtx->toAmount)/XBridgeTransactionDescr::COIN;
            outputs.push_back(std::make_pair(scr, outAmount));
        }

        xbridge::CTransactionPtr txUnsigned(createTransaction(inputs, outputs));

        std::vector<unsigned char> vchinner = ParseHex(innerScript.c_str());
        CScript inner(vchinner.begin(), vchinner.end());

        xbridge::CKey m = xtx->mSecret.GetKey();
        if (!m.IsValid())
        {
            // cancel transaction
            LOG() << "sign transaction error (SetSecret failed), transaction canceled " << __FUNCTION__;
            sendCancelTransaction(xtx, crNotSigned);
            return true;
        }

        std::vector<unsigned char> signature2;
        {
            uint256 hash = xbridge::SignatureHash2(inner, txUnsigned, 0, SIGHASH_ALL);
            if (!m.Sign(hash, signature2))
            {
                // cancel transaction
                LOG() << "sign transaction error, transaction canceled " << __FUNCTION__;
                sendCancelTransaction(xtx, crNotSigned);
                return true;
            }

            signature2.push_back((unsigned char)SIGHASH_ALL);

            // TXLOG() << "signature2 " << HexStr(signature2.begin(), signature2.end());
        }

        // sign2
        {
            CScript redeem;
            std::vector<unsigned char> rawx(xtx->xPubKey.begin(), xtx->xPubKey.end());
            std::vector<unsigned char> rawm(xtx->mPubKey.begin(), xtx->mPubKey.end());
            redeem << rawx
                   << signature2 << rawm
                   << OP_FALSE << inner;

            // TXLOG() << "xtx->mPubKey " << HexStr(rawm.begin(), rawm.end());
            // TXLOG() << "inner " << HexStr(inner.begin(), inner.end());

            xbridge::CTransactionPtr tx(createTransaction());
            tx->nVersion  = txUnsigned->nVersion;
            tx->vin.push_back(CTxIn(txUnsigned->vin[0].prevout, redeem));
            tx->vout      = txUnsigned->vout;

            std::string paytx = tx->toString();
            std::string json;
            std::string paytxid;
            if (!rpc::decodeRawTransaction(m_wallet.user, m_wallet.passwd,
                                           m_wallet.ip, m_wallet.port,
                                           paytx, paytxid, json))
            {
                LOG() << "decode signed transaction error, transaction canceled " << __FUNCTION__;
                sendCancelTransaction(xtx, crRpcError);
                return true;
            }

            TXLOG() << "payment A sendrawtransaction " << paytx;
            // TXLOG() << json;

            xtx->payTx   = paytx;
            xtx->payTxId = paytxid;

        } // sign2

    } // payTx

    // send pay tx
    std::string sentid;
    int32_t errCode = 0;
    if (rpc::sendRawTransaction(m_wallet.user, m_wallet.passwd,
                                m_wallet.ip, m_wallet.port, xtx->payTx, sentid, errCode))
    {
        LOG() << "payment A " << sentid;
    }
    else
    {
        if (errCode == -25)
        {
            // missing inputs, wait deposit tx
            // move packet to pending
            LOG() << "payment A not send, no deposit tx, move to pending";

            boost::mutex::scoped_lock l(XBridgeApp::m_ppLocker);
            XBridgeApp::m_pendingPackets[txid] = std::make_pair(m_wallet.currency, packet);
            return true;
        }

        LOG() << "payment A tx not send, transaction canceled " << __FUNCTION__;
        sendCancelTransaction(xtx, crRpcError);
        return true;
    }

    xtx->state = XBridgeTransactionDescr::trCommited;

    xuiConnector.NotifyXBridgeTransactionStateChanged(txid, xtx->state);

    // send reply
    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionConfirmedA));
    reply->append(hubAddress);
    reply->append(thisAddress);
    reply->append(txid.begin(), 32);
    reply->append(xtx->xPubKey.begin(), xtx->xPubKey.size());

    sendPacket(hubAddress, reply);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processTransactionConfirmedA(XBridgePacketPtr packet)
{
    DEBUG_TRACE_LOG(currencyToLog());

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

    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isStarted())
    {
        return true;
    }

    std::vector<unsigned char> from(packet->data()+20, packet->data()+40);

    uint256 txid(packet->data()+40);

    xbridge::CPubKey xPubkey(packet->data()+72, packet->data()+72+33);

    XBridgeTransactionPtr tr = e.transaction(txid);
    boost::mutex::scoped_lock l(tr->m_lock);

    tr->updateTimestamp();

    std::string sfrom = tr->fromXAddr(from);
    if (!sfrom.size())
    {
        ERR() << "invalid transaction address " << __FUNCTION__;
        sendCancelTransaction(txid, crInvalidAddress);
        return true;
    }

    if (e.updateTransactionWhenConfirmedReceived(tr, sfrom))
    {
        // wtf ?
        assert(!"invalid confirmation");

        ERR() << "invalid confirmation " << __FUNCTION__;
        sendCancelTransaction(txid, crInvalidAddress);
        return true;
    }

    // TODO remove this log
    LOG() << "send xbcTransactionConfirmB to "
          << tr->b_destination();

    XBridgePacketPtr reply2(new XBridgePacket(xbcTransactionConfirmB));
    reply2->append(rpc::toXAddr(tr->b_destination()));
    reply2->append(sessionAddr());
    reply2->append(txid.begin(), 32);
    reply2->append(xPubkey.begin(), xPubkey.size());
    reply2->append(tr->a_bintxid());
    reply2->append(tr->a_innerScript());

    sendPacket(tr->b_destination(), reply2);

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processTransactionConfirmB(XBridgePacketPtr packet)
{
    DEBUG_TRACE_LOG(currencyToLog());

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

    xbridge::CPubKey x(packet->data()+offset, packet->data()+offset+33);
    offset += 33;

    std::string binTxId(reinterpret_cast<const char *>(packet->data()+offset));
    offset += binTxId.size()+1;

    std::string innerScript(reinterpret_cast<const char *>(packet->data()+offset));
    offset += innerScript.size()+1;

    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_transactions.count(txid))
        {
            // wtf? unknown transaction
            LOG() << "unknown transaction " << util::to_str(txid) << " " << __FUNCTION__;
            return true;
        }

        xtx = XBridgeApp::m_transactions[txid];
    }

    // payTx
    {
        std::vector<std::pair<std::string, int> > inputs;
        std::vector<std::pair<CScript, double> >  outputs;

        // inputs from binTx
        inputs.push_back(std::make_pair(binTxId, 0));

        // outputs
        {
            CScript scr = GetScriptForDestination(xbridge::XBitcoinAddress(xtx->to).Get());

            double outAmount = static_cast<double>(xtx->toAmount)/XBridgeTransactionDescr::COIN;
            outputs.push_back(std::make_pair(scr, outAmount));
        }

        xbridge::CTransactionPtr txUnsigned(createTransaction(inputs, outputs));

        std::vector<unsigned char> vchredeem = ParseHex(innerScript.c_str());
        CScript inner(vchredeem.begin(), vchredeem.end());

        xbridge::CKey m = xtx->mSecret.GetKey();
        if (!m.IsValid())
        {
            // cancel transaction
            LOG() << "sign transaction error (SetSecret failed), transaction canceled " << __FUNCTION__;
            sendCancelTransaction(xtx, crNotSigned);
            return true;
        }

        std::vector<unsigned char> signature2;
        {
            uint256 hash = SignatureHash2(inner, txUnsigned, 0, SIGHASH_ALL);
            if (!m.Sign(hash, signature2))
            {
                // cancel transaction
                LOG() << "sign transaction error, transaction canceled " << __FUNCTION__;
                sendCancelTransaction(xtx, crNotSigned);
                return true;
            }

            signature2.push_back((unsigned char)SIGHASH_ALL);

            // TXLOG() << "signature2 " << HexStr(signature2.begin(), signature2.end());
        }

        // sign2
        {
            std::vector<unsigned char> rawm(xtx->mPubKey.begin(), xtx->mPubKey.end());
            std::vector<unsigned char> rawx(x.begin(), x.end());

            CScript redeem;
            redeem << rawx
                   << signature2 << rawm
                   << OP_FALSE << inner;

            // TXLOG() << "xtx->mPubKey " << HexStr(rawm.begin(), rawm.end());
            // TXLOG() << "inner " << HexStr(inner.begin(), inner.end());

            xbridge::CTransactionPtr tx(createTransaction());
            tx->nVersion  = txUnsigned->nVersion;
            tx->vin.push_back(CTxIn(txUnsigned->vin[0].prevout, redeem));
            tx->vout      = txUnsigned->vout;

            std::string paytx = tx->toString();
            std::string json;
            std::string paytxid;
            if (!rpc::decodeRawTransaction(m_wallet.user, m_wallet.passwd,
                                           m_wallet.ip, m_wallet.port,
                                           paytx, paytxid, json))
            {
                LOG() << "decode signed transaction error, transaction canceled " << __FUNCTION__;
                sendCancelTransaction(xtx, crRpcError);
                return true;
            }

            TXLOG() << "payment B sendrawtransaction " << paytx;
            // TXLOG() << json;

            xtx->payTx   = paytx;
            xtx->payTxId = paytxid;

        } // sign2

    } // payTx

    // send pay tx
    std::string sentid;
    int32_t errCode = 0;
    if (rpc::sendRawTransaction(m_wallet.user, m_wallet.passwd,
                                m_wallet.ip, m_wallet.port, xtx->payTx, sentid, errCode))
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

            boost::mutex::scoped_lock l(XBridgeApp::m_ppLocker);
            XBridgeApp::m_pendingPackets[txid] = std::make_pair(m_wallet.currency, packet);
            return true;
        }

        LOG() << "payment B tx not send, transaction canceled " << __FUNCTION__;
        sendCancelTransaction(xtx, crRpcError);
        return true;
    }

    xtx->state = XBridgeTransactionDescr::trCommited;

    xuiConnector.NotifyXBridgeTransactionStateChanged(txid, xtx->state);

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
bool XBridgeSession::processTransactionConfirmedB(XBridgePacketPtr packet)
{
    DEBUG_TRACE_LOG(currencyToLog());

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

    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isStarted())
    {
        return true;
    }

    std::vector<unsigned char> from(packet->data()+20, packet->data()+40);
    uint256 txid(packet->data()+40);

    XBridgeTransactionPtr tr = e.transaction(txid);
    boost::mutex::scoped_lock l(tr->m_lock);

    tr->updateTimestamp();

    std::string sfrom = tr->fromXAddr(from);
    if (!sfrom.size())
    {
        ERR() << "invalid transaction address " << __FUNCTION__;
        sendCancelTransaction(txid, crInvalidAddress);
        return true;
    }

    if (e.updateTransactionWhenConfirmedReceived(tr, sfrom))
    {
        if (tr->state() == XBridgeTransaction::trFinished)
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
bool XBridgeSession::processTransactionCancel(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();

    // size must be == 36 bytes
    if (packet->size() != 36)
    {
        ERR() << "invalid packet size for xbcReceivedTransaction "
              << "need 36 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    uint256 txid(packet->data());
    TxCancelReason reason = static_cast<TxCancelReason>(*reinterpret_cast<uint32_t*>(packet->data() + 32));

    // check and process packet if bridge is exchange
    XBridgeExchange & e = XBridgeExchange::instance();
    if (e.isStarted())
    {
        e.deletePendingTransactions(txid);
    }

    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_transactions.count(txid))
        {
            LOG() << "unknown transaction " << util::to_str(txid) << " " << __FUNCTION__;
            return true;
        }

        xtx = XBridgeApp::m_transactions[txid];
    }

    {
        // remove from pending packets (if added)
        boost::mutex::scoped_lock l(XBridgeApp::m_ppLocker);
        XBridgeApp::m_pendingPackets.erase(txid);
    }

    // update transaction state for gui
    xtx->state = XBridgeTransactionDescr::trCancelled;
    xuiConnector.NotifyXBridgeTransactionCancelled(txid, XBridgeTransactionDescr::trCancelled, reason);

    XBridgeApp::m_historicTransactions[txid] = xtx;

    // ..and retranslate
    // sendPacketBroadcast(packet);
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::finishTransaction(XBridgeTransactionPtr tr)
{
    LOG() << "finish transaction <" << tr->id().GetHex() << ">";

    if (tr->state() != XBridgeTransaction::trConfirmed)
    {
        ERR() << "finished unconfirmed transaction <" << tr->id().GetHex() << ">";
        return false;
    }

//    std::vector<std::vector<unsigned char> > rcpts;
//    rcpts.push_back(tr->firstAddress());
//    rcpts.push_back(tr->firstDestination());
//    rcpts.push_back(tr->secondAddress());
//    rcpts.push_back(tr->secondDestination());

//    for (const std::vector<unsigned char> & to : rcpts)
    {
        XBridgePacketPtr reply(new XBridgePacket(xbcTransactionFinished));
        // reply->append(to);
        reply->append(tr->id().begin(), 32);

        // sendPacket(to, reply);
        sendPacketBroadcast(reply);
    }

    tr->finish();

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::sendCancelTransaction(const uint256 & txid,
                                           const TxCancelReason & reason)
{
    LOG() << "cancel transaction <" << txid.GetHex() << ">";

    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionCancel));
    reply->append(txid.begin(), 32);
    reply->append(static_cast<uint32_t>(reason));

    // sendPacket(to, reply);
    sendPacketBroadcast(reply);
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::sendCancelTransaction(const XBridgeTransactionDescrPtr & tx,
                                           const TxCancelReason & reason)
{
    sendCancelTransaction(tx->id, reason);

    // update transaction state for gui
    tx->state = XBridgeTransactionDescr::trCancelled;
    xuiConnector.NotifyXBridgeTransactionCancelled(tx->id, XBridgeTransactionDescr::trCancelled, reason);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::rollbackTransaction(XBridgeTransactionPtr tr)
{
    LOG() << "rollback transaction <" << tr->id().GetHex() << ">";

    if (tr->state() >= XBridgeTransaction::trCreated)
    {
        XBridgeApp & app = XBridgeApp::instance();
        app.sendRollbackTransaction(tr->id());
    }

    // sendCancelTransaction(tr->id(), crRollback);
    tr->finish();

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processBitcoinTransactionHash(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();

    // size must be == 32 bytes (256bit)
    if (packet->size() != 32)
    {
        ERR() << "invalid packet size for xbcReceivedTransaction "
              << "need 32 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    static XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isStarted())
    {
        return true;
    }

    uint256 id(packet->data());
//    // LOG() << "received transaction <" << id.GetHex() << ">";

    e.updateTransaction(id);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processAddressBookEntry(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();

    std::string currency(reinterpret_cast<const char *>(packet->data()));
    std::string name(reinterpret_cast<const char *>(packet->data()+currency.length()+1));
    std::string address(reinterpret_cast<const char *>(packet->data()+currency.length()+name.length()+2));

    XBridgeApp::instance().storeAddressBookEntry(currency, name, address);

    return true;
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::sendListOfWallets()
{
    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isStarted())
    {
        return;
    }

    std::vector<StringPair> wallets = e.listOfWallets();
    std::vector<std::string> list;
    for (std::vector<StringPair>::iterator i = wallets.begin(); i != wallets.end(); ++i)
    {
        list.push_back(i->first + '|' + i->second);
    }

    XBridgePacketPtr packet(new XBridgePacket(xbcExchangeWallets));
    packet->setData(boost::algorithm::join(list, "|"));

    sendPacketBroadcast(packet);
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::sendListOfTransactions()
{
    XBridgeApp & app = XBridgeApp::instance();

    // send my trx
    if (XBridgeApp::m_pendingTransactions.size())
    {
        if (XBridgeApp::m_txLocker.try_lock())
        {
            // send pending transactions
            for (std::map<uint256, XBridgeTransactionDescrPtr>::iterator i = XBridgeApp::m_pendingTransactions.begin();
                 i != XBridgeApp::m_pendingTransactions.end(); ++i)
            {
                app.sendPendingTransaction(i->second);
            }

            XBridgeApp::m_txLocker.unlock();
        }
    }

    // send exchange trx
    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isStarted())
    {
        return;
    }

    std::list<XBridgeTransactionPtr> list = e.pendingTransactions();
    std::list<XBridgeTransactionPtr>::iterator i = list.begin();
    for (; i != list.end(); ++i)
    {
        XBridgeTransactionPtr & ptr = *i;

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
        packet->append(sessionAddr());
        sendPacketBroadcast(packet);
    }
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::eraseExpiredPendingTransactions()
{
    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isStarted())
    {
        return;
    }

    std::list<XBridgeTransactionPtr> list = e.pendingTransactions();
    std::list<XBridgeTransactionPtr>::iterator i = list.begin();
    for (; i != list.end(); ++i)
    {
        XBridgeTransactionPtr & ptr = *i;

        boost::mutex::scoped_lock l(ptr->m_lock);

        if (ptr->isExpired())
        {
            LOG() << "transaction expired <" << ptr->id().GetHex() << ">";
            e.deletePendingTransactions(ptr->hash1());
        }
    }
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::checkUnconfirmedTx()
{
    XBridgeApp::instance().checkUnconfirmedTx();
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::requestUnconfirmedTx()
{
//    DEBUG_TRACE_LOG(currencyToLog());

//    std::map<uint256, XBridgeTransactionDescrPtr> utx;
//    {
//        boost::mutex::scoped_lock l(XBridgeApp::m_txUnconfirmedLocker);
//        utx = XBridgeApp::m_unconfirmed;
//    }

//    for (std::map<uint256, XBridgeTransactionDescrPtr>::iterator i = utx.begin(); i != utx.end(); ++i)
//    {
//        // TODO debug fn rpc::getTransaction, payTxId is string instead uint256

//        LOG() << "check transaction " << i->second->payTxId;
//        if (rpc::getTransaction(m_wallet.user, m_wallet.passwd, m_wallet.ip, m_wallet.port,
//                                i->second->payTxId))
//        {
//            {
//                boost::mutex::scoped_lock l(XBridgeApp::m_txUnconfirmedLocker);
//                XBridgeApp::m_unconfirmed.erase(i->first);
//            }

//            XBridgeTransactionDescrPtr & tx = i->second;

//            XBridgePacketPtr ptr(new XBridgePacket(xbcTransactionConfirmed));
//            ptr->append(tx->hubAddress);
//            ptr->append(tx->confirmAddress);
//            ptr->append(i->first.begin(), 32);

//            sendPacket(tx->hubAddress, ptr);
//        }
//    }
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::checkFinishedTransactions()
{
    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isStarted())
    {
        return;
    }

    std::list<XBridgeTransactionPtr> list = e.finishedTransactions();
    std::list<XBridgeTransactionPtr>::iterator i = list.begin();
    for (; i != list.end(); ++i)
    {
        XBridgeTransactionPtr & ptr = *i;

        boost::mutex::scoped_lock l(ptr->m_lock);

        uint256 txid = ptr->id();

        if (ptr->state() == XBridgeTransaction::trConfirmed)
        {
            // send finished
            LOG() << "confirmed transaction <" << txid.GetHex() << ">";
            finishTransaction(ptr);
        }
        else if (ptr->state() == XBridgeTransaction::trCancelled)
        {
            // drop cancelled tx
            LOG() << "drop cancelled transaction <" << txid.GetHex() << ">";
            ptr->drop();
        }
        else if (ptr->state() == XBridgeTransaction::trFinished)
        {
            // delete finished tx
            LOG() << "delete finished transaction <" << txid.GetHex() << ">";
            e.deleteTransaction(txid);
        }
        else if (ptr->state() == XBridgeTransaction::trDropped)
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
            rollbackTransaction(ptr);
        }
    }
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::resendAddressBook()
{
    XBridgeApp::instance().resendAddressBook();
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::getAddressBook()
{
    XBridgeApp::instance().getAddressBook();
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::requestAddressBook()
{
    // no address book for exchange node
    XBridgeExchange & e = XBridgeExchange::instance();
    if (e.isStarted())
    {
        return;
    }

    std::vector<rpc::AddressBookEntry> entries;
    if (!rpc::requestAddressBook(m_wallet.user, m_wallet.passwd, m_wallet.ip, m_wallet.port, entries))
    {
        return;
    }

    XBridgeApp & app = XBridgeApp::instance();

    for (const rpc::AddressBookEntry & e : entries)
    {
        for (const std::string & addr : e.second)
        {
            std::vector<unsigned char> vaddr = rpc::toXAddr(addr);
            m_addressBook.insert(vaddr);
            app.storageStore(shared_from_this(), vaddr);

            xuiConnector.NotifyXBridgeAddressBookEntryReceived
                    (m_wallet.currency, e.first, addr);
        }
    }
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::sendAddressbookEntry(const std::string & currency,
                                          const std::string & name,
                                          const std::string & address)
{
    if (m_socket->is_open())
    {
        XBridgePacketPtr p(new XBridgePacket(xbcAddressBookEntry));
        p->append(currency);
        p->append(name);
        p->append(address);

        sendXBridgeMessage(p);
    }
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processTransactionFinished(XBridgePacketPtr packet)
{
    DEBUG_TRACE_LOG(currencyToLog());

    if (packet->size() != 32)
    {
        ERR() << "incorrect packet size for xbcTransactionFinished" << __FUNCTION__;
        return false;
    }

    // transaction id
    uint256 txid(packet->data());

    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_transactions.count(txid))
        {
            // signal for gui
            xuiConnector.NotifyXBridgeTransactionStateChanged(txid, XBridgeTransactionDescr::trFinished);
            return true;
        }

        xtx = XBridgeApp::m_transactions[txid];
    }

    // update transaction state for gui
    xtx->state = XBridgeTransactionDescr::trFinished;

    xuiConnector.NotifyXBridgeTransactionStateChanged(txid, xtx->state);

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::revertXBridgeTransaction(const uint256 & id)
{
    DEBUG_TRACE_LOG(currencyToLog());

    // TODO temporary implementation
    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        // search tx
        for (std::map<uint256, XBridgeTransactionDescrPtr>::iterator i = XBridgeApp::m_transactions.begin();
             i != XBridgeApp::m_transactions.end(); ++i)
        {
            if (i->second->id == id)
            {
                xtx = i->second;
                break;
            }
        }
    }

    if (!xtx)
    {
        LOG() << "unknown transaction " << util::to_str(id) << " " << __FUNCTION__;
        return true;
    }

    // rollback, commit revert transaction
    std::string txid;
    int32_t errCode = 0;
    if (!rpc::sendRawTransaction(m_wallet.user, m_wallet.passwd, m_wallet.ip, m_wallet.port, xtx->refTx, txid, errCode))
    {
        // not commited....send cancel???
        // sendCancelTransaction(id);
        return false;
    }

    // update transaction state for gui
    xtx->state = XBridgeTransactionDescr::trRollback;
    xuiConnector.NotifyXBridgeTransactionStateChanged(id, xtx->state);

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processTransactionRollback(XBridgePacketPtr packet)
{
    DEBUG_TRACE_LOG(currencyToLog());

    if (packet->size() != 52)
    {
        ERR() << "incorrect packet size for xbcTransactionRollback" << __FUNCTION__;
        return false;
    }

    // transaction id
    uint256 txid(packet->data()+20);

    // for rollback need local transaction id
    // TODO maybe hub id?
    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_transactions.count(txid))
        {
            // wtf? unknown tx
            LOG() << "unknown transaction " << util::to_str(txid) << " " << __FUNCTION__;
            return true;
        }

        xtx = XBridgeApp::m_transactions[txid];
    }

    revertXBridgeTransaction(xtx->id);
    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processTransactionDropped(XBridgePacketPtr packet)
{
    if (packet->size() != 32)
    {
        ERR() << "incorrect packet size for xbcTransactionDropped" << __FUNCTION__;
        return false;
    }

    // transaction id
    uint256 id(packet->data());

    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_transactions.count(id))
        {
            // signal for gui
            xuiConnector.NotifyXBridgeTransactionStateChanged(id, XBridgeTransactionDescr::trDropped);
            return false;
        }

        xtx = XBridgeApp::m_transactions[id];
    }

    // update transaction state for gui
    xtx->state = XBridgeTransactionDescr::trDropped;
    xuiConnector.NotifyXBridgeTransactionStateChanged(id, xtx->state);

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::makeNewPubKey(xbridge::CPubKey & newPKey) const
{
    if (m_wallet.isGetNewPubKeySupported)
    {
        // use getnewpubkey
        std::string key;
        if (!rpc::getNewPubKey(m_wallet.user, m_wallet.passwd,
                              m_wallet.ip, m_wallet.port, key))
        {
            return false;
        }

        newPKey = xbridge::CPubKey(ParseHex(key));
    }
    else
    {
        // use importprivkey
        xbridge::CKey newKey;
        newKey.MakeNewKey(true);

        if (!rpc::importPrivKey(m_wallet.user, m_wallet.passwd,
                                m_wallet.ip, m_wallet.port,
                                xbridge::CBitcoinSecret(newKey).ToString(), "",
                                m_wallet.isImportWithNoScanSupported))
        {
            return false;
        }

        newPKey = newKey.GetPubKey();
    }
    return newPKey.IsValid();
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::checkAmount(const uint64_t _amount) const
{
    double amount = _amount / XBridgeTransactionDescr::COIN;

    std::vector<rpc::Unspent> entries;
    if (!rpc::listUnspent(m_wallet.user, m_wallet.passwd,
                          m_wallet.ip, m_wallet.port, entries))
    {
        LOG() << "rpc::listUnspent failed" << __FUNCTION__;
        return false;
    }

    double funds = 0;
    for (const rpc::Unspent & entry : entries)
    {
        funds += entry.amount;
        if (amount < funds)
        {
            return true;
        }
    }

    return false;
}

double XBridgeSession::getAccountBalance() const
{
    std::vector<rpc::Unspent> entries;
    if (!rpc::listUnspent(m_wallet.user, m_wallet.passwd,
                          m_wallet.ip, m_wallet.port, entries))
    {
        LOG() << "rpc::listUnspent failed" << __FUNCTION__;
        return 0;
    }

    double amount = 0;
    for (const rpc::Unspent & entry : entries)
        amount += entry.amount;

    return amount;
}
