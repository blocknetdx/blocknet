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

#include "json/json_spirit.h"
#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

using namespace json_spirit;

#define LOG_KEYPAIR_VALUES

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
XBridgeSession::~XBridgeSession()
{
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::init()
{ 
    if(m_handlers.size())
    {
        LOG() << "packet handlers map must be empty" << __FUNCTION__;
        return;
    }

    m_myid.resize(20);
    GetStrongRandBytes(&m_myid[0], 20);
//    if (!rpc::getNewAddress(m_myid))
//    {
//        m_myid = std::vector<unsigned char>(20, 0);
//        LOG() << "fail generate address for <" << m_wallet.currency << "> session " << __FUNCTION__;
//        return;
//    }

    // process invalid
    m_handlers[xbcInvalid]               .bind(this, &XBridgeSession::processInvalid);

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

        m_handlers[xbcTransactionConfirmedA] .bind(this, &XBridgeSession::processTransactionConfirmedA);
        m_handlers[xbcTransactionConfirmedB] .bind(this, &XBridgeSession::processTransactionConfirmedB);
    }

    // retranslate messages to xbridge network
    m_handlers[xbcXChatMessage].bind(this, &XBridgeSession::processXChatMessage);
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
// retranslate packets from wallet to xbridge network
//*****************************************************************************
bool XBridgeSession::processXChatMessage(XBridgePacketPtr /*packet*/)
{
    LOG() << "method BridgeSession::processXChatMessage not implemented";
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

    DEBUG_TRACE();

    // size must be > 146 bytes
    if (packet->size() <= 146)
    {
        ERR() << "invalid packet size for xbcTransaction "
              << "need min 146 bytes, received " << packet->size() << " "
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

            utxoItems.push_back(entry);
        }
    }

    LOG() << "received transaction " << HexStr(id) << std::endl
          << "    from " << HexStr(saddr) << std::endl
          << "             " << scurrency << " : " << samount << std::endl
          << "    to   " << HexStr(daddr) << std::endl
          << "             " << dcurrency << " : " << damount << std::endl;

    // check utxo items
    XBridgeApp & xapp = XBridgeApp::instance();
    if (false && !xapp.checkUtxoItems(utxoItems))
    {
        sendCancelTransaction(id, crBadUtxo);
        LOG() << "error check utxo items, transaction request rejected "
              << __FUNCTION__;
        return true;
    }

    {
        bool isCreated = false;
        uint256 pendingId;
        if (!e.createTransaction(id,
                                 saddr, scurrency, samount,
                                 daddr, dcurrency, damount,
                                 pendingId, isCreated))
        {
            // not created
            return true;
        }

        // tx created, lock utxo items
        if (!xapp.lockUtxoItems(utxoItems))
        {
            e.deletePendingTransactions(id);

            LOG() << "error lock utxo items, transaction request rejected "
                  << __FUNCTION__;
            return true;
        }

        // TODO send signal to gui for debug
        {
            XBridgeTransactionDescrPtr d(new XBridgeTransactionDescr);
            d->id           = id;
            d->fromCurrency = scurrency;
            d->fromAmount   = samount;
            d->toCurrency   = dcurrency;
            d->toAmount     = damount;
            d->state        = XBridgeTransactionDescr::trPending;

            xuiConnector.NotifyXBridgePendingTransactionReceived(d);
        }

        XBridgeTransactionPtr tr = e.pendingTransaction(pendingId);
        if (tr->id() == uint256())
        {
            LOG() << "transaction not found after create. " << id.GetHex();
            return false;
        }

        LOG() << "transaction created, id " << id.GetHex();

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

    DEBUG_TRACE();

    if (packet->size() != 84)
    {
        ERR() << "incorrect packet size for xbcPendingTransaction "
              << "need 84 received " << packet->size() << " "
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

    LOG() << "received tx <" << HexStr(ptr->id) << "> " << __FUNCTION__;

    xuiConnector.NotifyXBridgePendingTransactionReceived(ptr);

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

    DEBUG_TRACE();

    // size must be >= 164 bytes
    if (packet->size() < 164)
    {
        ERR() << "invalid packet size for xbcTransactionAccepting "
              << "need min 164 bytes, received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    // read packet data
    uint256 id(packet->data());

    // source
    uint32_t offset = 52;
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

            utxoItems.push_back(entry);
        }
    }

    LOG() << "received accepting transaction " << HexStr(id) << std::endl
          << "    from " << HexStr(saddr) << std::endl
          << "             " << scurrency << " : " << samount << std::endl
          << "    to   " << HexStr(daddr) << std::endl
          << "             " << dcurrency << " : " << damount << std::endl;

    // TODO check utxo items

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

                std::set<std::vector<unsigned char> > hosts;
                hosts.insert(tr->a_address());
                hosts.insert(tr->b_address());

                assert(hosts.size() == 2 && "bad addresses");

                for (const std::vector<unsigned char> & host : hosts)
                {
                    XBridgePacketPtr reply1(new XBridgePacket(xbcTransactionHold));
                    reply1->append(host);
                    reply1->append(sessionAddr());
                    reply1->append(tr->id().begin(), 32);
                    reply1->append(activeServicenode.pubKeyServicenode.begin(),
                                   activeServicenode.pubKeyServicenode.size());

                    sendPacket(host, reply1);
                }
            }
        }
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processTransactionHold(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    if (packet->size() != 105 && packet->size() != 137)
    {
        ERR() << "incorrect packet size for xbcTransactionHold "
              << "need 105 or 137 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    uint32_t offset = 20;

    // servicenode addr
    std::vector<unsigned char> hubAddress(packet->data()+offset, packet->data()+offset+20);
    offset += 20;

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
            LOG() << "unknown transaction " << HexStr(id) << " " << __FUNCTION__;
            return true;
        }

        if (XBridgeApp::m_transactions.count(id))
        {
            // wtf?
            LOG() << "duplicate transaction " << HexStr(id) << " " << __FUNCTION__;
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

    XBridgeApp & xapp = XBridgeApp::instance();
    XBridgeWalletConnectorPtr conn = xapp.connectorByCurrency(xtx->toCurrency);
    if (!conn)
    {
        WARN() << "no connector for <" << xtx->toCurrency << "> " << __FUNCTION__;
        return true;
    }

    xuiConnector.NotifyXBridgeTransactionStateChanged(id, xtx->state);

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
bool XBridgeSession::processTransactionHoldApply(XBridgePacketPtr packet)
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

    if (!isAddressInTransaction(from, tr))
    {
        ERR() << "invalid transaction address " << __FUNCTION__;
        sendCancelTransaction(id, crInvalidAddress);
        return true;
    }

    if (e.updateTransactionWhenHoldApplyReceived(tr, from))
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
                  << HexStr(tr->a_destination());

            XBridgePacketPtr reply1(new XBridgePacket(xbcTransactionInit));
            reply1->append(tr->a_destination());
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

            sendPacket(tr->a_destination(), reply1);

            // second
            // TODO remove this log
            LOG() << "send xbcTransactionInit to "
                  << HexStr(tr->b_destination());

            XBridgePacketPtr reply2(new XBridgePacket(xbcTransactionInit));
            reply2->append(tr->b_destination());
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

            sendPacket(tr->b_destination(), reply2);
        }
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processTransactionInit(XBridgePacketPtr packet)
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

    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_transactions.count(txid))
        {
            // wtf? unknown transaction
            LOG() << "unknown transaction " << HexStr(txid) << " " << __FUNCTION__;
            return true;
        }

        xtx = XBridgeApp::m_transactions[txid];
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

    XBridgeApp & xapp = XBridgeApp::instance();
    XBridgeWalletConnectorPtr conn = xapp.connectorByCurrency(xtx->toCurrency);
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
bool XBridgeSession::processTransactionInitialized(XBridgePacketPtr packet)
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
    offset += 32;

    // opponent publick key
    std::vector<unsigned char> pk1(packet->data()+offset, packet->data()+offset+33);
    // offset += 33;

    XBridgeTransactionPtr tr = e.transaction(id);
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
        if (tr->state() == XBridgeTransaction::trInitialized)
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
            reply1->append(sessionAddr());
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
bool XBridgeSession::isAddressInTransaction(const std::vector<unsigned char> & address,
                                            const XBridgeTransactionPtr & tx)
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
bool XBridgeSession::processTransactionCreate(XBridgePacketPtr packet)
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

    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_transactions.count(txid))
        {
            // wtf? unknown transaction
            LOG() << "unknown transaction " << HexStr(txid) << " " << __FUNCTION__;
            return true;
        }

        xtx = XBridgeApp::m_transactions[txid];
    }

    // connectors
    XBridgeApp & xapp = XBridgeApp::instance();
    XBridgeWalletConnectorPtr connFrom = xapp.connectorByCurrency(xtx->fromCurrency);
    XBridgeWalletConnectorPtr connTo   = xapp.connectorByCurrency(xtx->toCurrency);
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
        boost::mutex::scoped_lock l(XBridgeApp::m_ppLocker);
        XBridgeApp::m_pendingPackets[txid] = packet;
        return true;
    }
    else
    {
        // remove from pending packets (if added)
        boost::mutex::scoped_lock l(XBridgeApp::m_ppLocker);
        XBridgeApp::m_pendingPackets.erase(txid);
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
            boost::mutex::scoped_lock l(XBridgeApp::m_ppLocker);
            XBridgeApp::m_pendingPackets[txid] = packet;
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

    std::vector<wallet::UtxoEntry> entries;
    if (!connFrom->getUnspent(entries))
    {
        LOG() << "conector::listUnspent failed" << __FUNCTION__;
        sendCancelTransaction(xtx, crRpcError);
        return true;
    }

    double outAmount = static_cast<double>(xtx->fromAmount) / XBridgeTransactionDescr::COIN;

    double fee1      = 0;
    double fee2      = connTo->minTxFee2(1, 1);
    double inAmount  = 0;

    std::vector<wallet::UtxoEntry> usedInTx;
    for (const wallet::UtxoEntry & entry : entries)
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

    xtx->state = XBridgeTransactionDescr::trCreated;

    xuiConnector.NotifyXBridgeTransactionStateChanged(txid, xtx->state);

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
bool XBridgeSession::processTransactionCreatedA(XBridgePacketPtr packet)
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

    uint32_t innerSize = *reinterpret_cast<uint32_t *>(packet->data()+offset);
    offset += sizeof(uint32_t);

    std::vector<unsigned char> innerScript(packet->data()+offset, packet->data()+offset+innerSize);
    // offset += innerScript.size();

    XBridgeTransactionPtr tr = e.transaction(txid);
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

    uint32_t innerSize = *reinterpret_cast<uint32_t *>(packet->data()+offset);
    offset += sizeof(uint32_t);

    std::vector<unsigned char> innerScript(packet->data()+offset, packet->data()+offset+innerSize);
    // offset += innerScript.size();

    XBridgeTransactionPtr tr = e.transaction(txid);
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
        if (tr->state() == XBridgeTransaction::trCreated)
        {
            // send confirm packets with deposit tx id
            // for create payment tx

            // TODO remove this log
            LOG() << "send xbcTransactionConfirmA to "
                  << HexStr(tr->a_destination());

            XBridgePacketPtr reply(new XBridgePacket(xbcTransactionConfirmA));
            reply->append(tr->a_destination());
            reply->append(sessionAddr());
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
bool XBridgeSession::processTransactionConfirmA(XBridgePacketPtr packet)
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

    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_transactions.count(txid))
        {
            // wtf? unknown transaction
            LOG() << "unknown transaction " << HexStr(txid) << " " << __FUNCTION__;
            return true;
        }

        xtx = XBridgeApp::m_transactions[txid];
    }

    XBridgeApp & xapp = XBridgeApp::instance();
    XBridgeWalletConnectorPtr conn = xapp.connectorByCurrency(xtx->toCurrency);
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
            // move packet to pending
            boost::mutex::scoped_lock l(XBridgeApp::m_ppLocker);
            XBridgeApp::m_pendingPackets[txid] = packet;
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
            double outAmount = static_cast<double>(xtx->toAmount)/XBridgeTransactionDescr::COIN;
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
            // move packet to pending
            LOG() << "payment A not send, no deposit tx, move to pending";

            boost::mutex::scoped_lock l(XBridgeApp::m_ppLocker);
            XBridgeApp::m_pendingPackets[txid] = packet;
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
    reply->append(xtx->xPubKey);

    sendPacket(hubAddress, reply);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processTransactionConfirmedA(XBridgePacketPtr packet)
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

    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isStarted())
    {
        return true;
    }

    std::vector<unsigned char> from(packet->data()+20, packet->data()+40);

    uint256 txid(packet->data()+40);

    std::vector<unsigned char> xPubkey(packet->data()+72, packet->data()+72+33);

    XBridgeTransactionPtr tr = e.transaction(txid);
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
    reply2->append(sessionAddr());
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
bool XBridgeSession::processTransactionConfirmB(XBridgePacketPtr packet)
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

    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_transactions.count(txid))
        {
            // wtf? unknown transaction
            LOG() << "unknown transaction " << HexStr(txid) << " " << __FUNCTION__;
            return true;
        }

        xtx = XBridgeApp::m_transactions[txid];
    }

    XBridgeApp & xapp = XBridgeApp::instance();
    XBridgeWalletConnectorPtr conn = xapp.connectorByCurrency(xtx->toCurrency);
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
            double outAmount = static_cast<double>(xtx->toAmount)/XBridgeTransactionDescr::COIN;
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

            boost::mutex::scoped_lock l(XBridgeApp::m_ppLocker);
            XBridgeApp::m_pendingPackets[txid] = packet;
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

    if (!isAddressInTransaction(from, tr))
    {
        ERR() << "invalid transaction address " << __FUNCTION__;
        sendCancelTransaction(txid, crInvalidAddress);
        return true;
    }

    if (e.updateTransactionWhenConfirmedReceived(tr, from))
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
bool XBridgeSession::cancelOrRollbackTransaction(const uint256 & txid, const TxCancelReason & reason)
{
    DEBUG_TRACE();

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
            LOG() << "unknown transaction " << HexStr(txid) << " " << __FUNCTION__;
            return true;
        }

        xtx = XBridgeApp::m_transactions[txid];
    }

    {
        // remove from pending packets (if added)
        boost::mutex::scoped_lock l(XBridgeApp::m_ppLocker);
        XBridgeApp::m_pendingPackets.erase(txid);
    }

    if (xtx->state < XBridgeTransactionDescr::trCreated)
    {
        // unlock coins
//        for (const wallet::UtxoEntry & entry : xtx->usedCoins)
//        {
//            pwalletMain->UnlockCoin(COutPoint(entry.txId, entry.vout));
//        }

        xtx->state = XBridgeTransactionDescr::trCancelled;
        xuiConnector.NotifyXBridgeTransactionCancelled(txid, XBridgeTransactionDescr::trCancelled, reason);
    }
    else
    {
        // rollback, commit revert transaction
        XBridgeApp & xapp = XBridgeApp::instance();
        XBridgeWalletConnectorPtr conn = xapp.connectorByCurrency(xtx->fromCurrency);
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
                LOG() << "send rollback error, tx " << HexStr(txid) << " " << __FUNCTION__;
                xtx->state = XBridgeTransactionDescr::trRollbackFailed;
            }
            else
            {
                xtx->state = XBridgeTransactionDescr::trRollback;
            }
        }

        // update transaction state for gui
        xuiConnector.NotifyXBridgeTransactionStateChanged(txid, (XBridgeTransactionDescr::State)xtx->state);
    }

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
bool XBridgeSession::sendCancelTransaction(const uint256 & txid,
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

    tr->finish();

    return true;
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
void XBridgeSession::getAddressBook()
{
    XBridgeApp::instance().getAddressBook();
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processTransactionFinished(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

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
bool XBridgeSession::rollbacktXBridgeTransaction(const uint256 & id)
{
    DEBUG_TRACE();

    return cancelOrRollbackTransaction(id, crRollback);
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processTransactionRollback(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    if (packet->size() != 32)
    {
        ERR() << "incorrect packet size for xbcTransactionRollback" << __FUNCTION__;
        return false;
    }

    // transaction id
    uint256 txid(packet->data());

    // for rollback need local transaction id
    // TODO maybe hub id?
    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_transactions.count(txid))
        {
            // wtf? unknown tx
            LOG() << "unknown transaction " << HexStr(txid) << " " << __FUNCTION__;
            return true;
        }

        xtx = XBridgeApp::m_transactions[txid];
    }

    rollbacktXBridgeTransaction(xtx->id);
    return true;
}
