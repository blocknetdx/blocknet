//*****************************************************************************
//*****************************************************************************

// #include <boost/asio.hpp>
// #include <boost/asio/buffer.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time/posix_time/conversion.hpp>

#include "xbridgesession.h"
#include "xbridgeapp.h"
#include "xbridgeexchange.h"
#include "xbridgepacket.h"
#include "xuiconnector.h"
#include "util/xutil.h"
#include "util/logger.h"
#include "util/txlog.h"
#include "util/xassert.h"
#include "bitcoinrpcconnector.h"
#include "xbitcointransaction.h"
#include "xbitcoinaddress.h"
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

#include "posixtimeconversion.h"

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

    bool processTransactionCancel(XBridgePacketPtr packet) const;

    bool processTransactionFinished(XBridgePacketPtr packet) const;

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

    if (Exchange::instance().isEnabled())
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
        m_handlers[xbcTransactionFinished]   .bind(this, &Impl::processTransactionFinished);
    }

    // xchat ()
    m_handlers[xbcXChatMessage].bind(this, &Impl::processXChatMessage);

    // Services ping (xwallets)
    m_handlers[xbcServicesPing].bind(this, &Impl::processServicesPing);
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
/**
 * @brief Process and verify the received services ping. This will enforce a maximum size for the packet. Packets
 *        in excess of the max size will be rejected. The pubkey of the packet will be used to associate the services.
 *        The sending node is required to sign the packet with its private key, this will mitigate MITM.
 * @param packet
 * @return
 */
//*****************************************************************************
bool Session::Impl::processServicesPing(XBridgePacketPtr packet) const
{
    if (packet->size() > 10000)
    {
        // enforce a limit of max bytes
        ERR() << "Services packet too large, a max of 10000 bytes is supported "
              << __FUNCTION__;
        return false;
    }
    else if (packet->size() < (sizeof(uint32_t) + 1))
    {
        // service count (4 bytes) + service name (at least 1 byte)
        ERR() << "Rejecting Services packet, it's too small "
              << __FUNCTION__;
        return false;
    }

    ::CPubKey nodePubKey;
    std::vector<std::string> services;

    uint32_t offset = 0;

    // Get pubkey from the packet so that we can check if it's in the snode list
    {
        uint32_t len = ::CPubKey::GetLen(*(char *)(packet->pubkey()));
        if (len != 33)
        {
            LOG() << "Bad Servicenode public key, length: " << len << " " << __FUNCTION__;
            return false;
        }
        nodePubKey.Set(packet->pubkey(), packet->pubkey() + len);
    }

    // Find Servicenode in list
    CServicenode *pmn = mnodeman.Find(nodePubKey);
    if (pmn == nullptr)
    {
        // try to uncompress pubkey and search
        if (nodePubKey.Decompress())
        {
            pmn = mnodeman.Find(nodePubKey);
        }
        if (pmn == nullptr)
        {
            ERR() << "Bad Services packet, Servicenode not found with vin "
                  << nodePubKey.GetHex() << " "
                  << __FUNCTION__;
            return false;
        }
    }

    // Services
    uint32_t servicesCount = *static_cast<uint32_t *>(static_cast<void *>(packet->data() + offset));
    offset += sizeof(uint32_t);
    std::string rawServices(reinterpret_cast<const char *>(packet->data() + offset));
    if (rawServices.length() > 0)
    {
        boost::split(services, rawServices, boost::is_any_of(","));
    }

    if (services.size() != servicesCount)
    {
        ERR() << "Rejecting Services packet, the reported services count doesn't match the actual count "
              << __FUNCTION__;
        return false;
    }

    // Store updated services list for this node
    App::instance().addNodeServices(nodePubKey, services);

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
    uint256 id(packet->data());
    uint32_t offset = XBridgePacket::hashSize;

    // Check if order already exists, if it does ignore processing
    TransactionPtr t = e.pendingTransaction(id);
    if (t->id() == id) {
        // Update the transaction timestamp
        if (e.updateTimestampOrRemoveExpired(t)) {
            LOG() << "order already received " << id.ToString()
                  << " " << __FUNCTION__;
            return true;
        }
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

    uint256 blockHash(packet->data()+offset);
    offset += XBridgePacket::hashSize;

    std::vector<unsigned char> mpubkey(packet->pubkey(), packet->pubkey()+XBridgePacket::pubkeySize);

    if (!packet->verify(mpubkey))
    {
        WARN() << "invalid packet signature " << __FUNCTION__;
        return true;
    }

    xbridge::App & xapp = xbridge::App::instance();
    WalletConnectorPtr sconn = xapp.connectorByCurrency(scurrency);
    WalletConnectorPtr dconn = xapp.connectorByCurrency(dcurrency);
    if (!sconn || !dconn)
    {
        WARN() << "no connector for <" << (!sconn ? scurrency : dcurrency) << "> " << __FUNCTION__;
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
            const static uint32_t utxoItemSize = XBridgePacket::hashSize + sizeof(uint32_t) +
                                                 XBridgePacket::addressSize + XBridgePacket::signatureSize;
            if (packet->size() < offset+utxoItemSize)
            {
                WARN() << "bad packet size while reading utxo items, packet dropped in " << __FUNCTION__;
                return true;
            }

            wallet::UtxoEntry entry;

            uint256 txid(packet->data()+offset);
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
                LOG() << "not found utxo entry <" << entry.txId
                      << "> no " << entry.vout << " " << __FUNCTION__;
                continue;
            }

            // check signature
            std::string signature = EncodeBase64(&entry.signature[0], entry.signature.size());
            if (!sconn->verifyMessage(entry.address, entry.toString(), signature))
            {
                LOG() << "not valid signature, bad utxo entry" << entry.txId
                      << "> no " << entry.vout << " " << __FUNCTION__;
                continue;
            }

            commonAmount += entry.amount;

            utxoItems.push_back(entry);
        }
    }

    if (utxoItems.empty())
    {
        LOG() << "transaction rejected, utxo items are empty <" << __FUNCTION__;
        return true;
    }

    if (commonAmount * TransactionDescr::COIN < samount)
    {
        LOG() << "transaction rejected, amount from utxo items <" << commonAmount
              << "> less than required <" << samount << "> " << __FUNCTION__;
        return true;
    }

    // check dust amount
    if (sconn->isDustAmount(static_cast<double>(samount) / TransactionDescr::COIN) ||
        sconn->isDustAmount(commonAmount - (static_cast<double>(samount) / TransactionDescr::COIN)) ||
        dconn->isDustAmount(static_cast<double>(damount) / TransactionDescr::COIN))
    {
        LOG() << "reject dust amount transaction " << id.ToString() << " " << __FUNCTION__;
        return true;
    }

    LOG() << "received transaction " << id.GetHex() << std::endl
          << "    from " << HexStr(saddr) << std::endl
          << "             " << scurrency << " : " << samount << std::endl
          << "    to   " << HexStr(daddr) << std::endl
          << "             " << dcurrency << " : " << damount << std::endl;

    std::string saddrStr = sconn->fromXAddr(saddr);
    std::string daddrStr = dconn->fromXAddr(daddr);

    std::vector<unsigned char> firstUtxoSig = utxoItems.at(0).signature;

    uint256 checkId = Hash(saddrStr.begin(), saddrStr.end(),
                           scurrency.begin(), scurrency.end(),
                           BEGIN(samount), END(samount),
                           daddrStr.begin(), daddrStr.end(),
                           dcurrency.begin(), dcurrency.end(),
                           BEGIN(damount), END(damount),
                           BEGIN(timestamp), END(timestamp),
                           blockHash.begin(), blockHash.end(),
                           firstUtxoSig.begin(), firstUtxoSig.end());
    if(checkId != id)
    {
        WARN() << "id from packet is differs from body hash:" << std::endl
               << "packet id: " << id.GetHex() << std::endl
               << "body hash:" << checkId.GetHex() << std::endl
               << __FUNCTION__;

        return true;
    }

    // check utxo items
    if (!e.checkUtxoItems(id, utxoItems))
    {
        LOG() << "transaction rejected, error check utxo items "  << id.ToString()
              << " " << __FUNCTION__;
        return true;
    }

    {
        bool isCreated = false;
        if (!e.createTransaction(id,
                                 saddr, scurrency, samount,
                                 daddr, dcurrency, damount,
                                 timestamp,
                                 mpubkey, utxoItems,
                                 blockHash, isCreated))
        {
            // not created
            LOG() << "transaction create error "  << id.ToString() << " " << __FUNCTION__;
            return true;
        }

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

                LOG() << __FUNCTION__ << d;

                xuiConnector.NotifyXBridgeTransactionReceived(d);
            }

            TransactionPtr tr = e.pendingTransaction(id);
            if (tr->id() == uint256())
            {
                LOG() << "transaction not found after create. " << id.ToString()
                      << " " << __FUNCTION__;
                return false;
            }

            boost::mutex::scoped_lock l(tr->m_lock);

            std::string firstCurrency = tr->a_currency();
            std::vector<unsigned char> fc(8, 0);
            std::copy(firstCurrency.begin(), firstCurrency.end(), fc.begin());
            std::string secondCurrency = tr->b_currency();
            std::vector<unsigned char> sc(8, 0);
            std::copy(secondCurrency.begin(), secondCurrency.end(), sc.begin());

            // broadcast send pending transaction packet
            XBridgePacketPtr reply(new XBridgePacket(xbcPendingTransaction));
            reply->append(tr->id().begin(), XBridgePacket::hashSize);
            reply->append(fc);
            reply->append(tr->a_amount());
            reply->append(sc);
            reply->append(tr->b_amount());
            reply->append(m_myid);
            reply->append(util::timeToInt(tr->createdTime()));
            reply->append(tr->blockHash().begin(), 32);

            reply->sign(e.pubKey(), e.privKey());

            sendPacketBroadcast(reply);

            LOG() << __FUNCTION__ << tr;
        }
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

    if (packet->size() != 124)
    {
        ERR() << "incorrect packet size for xbcPendingTransaction "
              << "need 124 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    uint256 txid = uint256(packet->data());
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
        WARN() << "wrong servicenode handling order, expected " << HexStr(ptr->sPubKey)
               << " but received pubkey " << HexStr(spubkey)
               << " and hub address " << HexStr(hubAddress) << " " << __FUNCTION__;
        return true;
    }

    // All traders verify sig, important when tx ptr is not known yet
    if (!packet->verify(spubkey))
    {
        WARN() << "invalid packet signature " << __FUNCTION__;
        return true;
    }

    WalletConnectorPtr sconn = xapp.connectorByCurrency(scurrency);
    WalletConnectorPtr dconn = xapp.connectorByCurrency(dcurrency);
    if (!sconn || !dconn)
    {
        WARN() << "no connector for <" << (!sconn ? scurrency : dcurrency) << "> " << __FUNCTION__;
        return true;
    }

    if (ptr)
    {
        if (ptr->state > TransactionDescr::trPending)
        {
            LOG() << "received pending for hold transaction " << __FUNCTION__;

            LOG() << __FUNCTION__ << ptr;

            return true;
        }

        if (ptr->state == TransactionDescr::trNew)
        {
            LOG() << "received confirmed order from snode, setting status to pending " << __FUNCTION__;
            ptr->state = TransactionDescr::trPending;
        }

        // update timestamp
        ptr->updateTimestamp();

        LOG() << __FUNCTION__ << ptr;

        xuiConnector.NotifyXBridgeTransactionChanged(ptr->id);

        return true;
    }

    // create tx item
    ptr.reset(new TransactionDescr);
    ptr->id           = txid;

    ptr->fromCurrency = scurrency;
    ptr->fromAmount   = samount;

    ptr->toCurrency   = dcurrency;
    ptr->toAmount     = damount;

    ptr->hubAddress   = hubAddress;
    offset += XBridgePacket::addressSize;

    ptr->created      = util::intToTime(*reinterpret_cast<boost::uint64_t *>(packet->data()+offset));
    offset += sizeof(uint64_t);

    ptr->state        = TransactionDescr::trPending;
    ptr->sPubKey      = spubkey;

    ptr->blockHash    = uint256(packet->data()+offset);
    offset += XBridgePacket::hashSize;

    xapp.appendTransaction(ptr);

    LOG() << "received order <" << ptr->id.GetHex() << "> " << __FUNCTION__;

    LOG() << __FUNCTION__ << ptr;

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

    // size must be >= 164 bytes
    if (packet->size() < 164)
    {
        ERR() << "invalid packet size for xbcTransactionAccepting "
              << "need min 164 bytes, received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    uint32_t offset = XBridgePacket::addressSize;

    // read packet data
    uint256 id(packet->data()+offset);
    offset += XBridgePacket::hashSize;

    // source
    std::vector<unsigned char> saddr(packet->data()+offset, packet->data()+offset+XBridgePacket::addressSize);
    offset += XBridgePacket::addressSize;
    std::string scurrency((const char *)packet->data()+offset);
    offset += 8;
    uint64_t samount = *static_cast<uint64_t *>(static_cast<void *>(packet->data()+offset));
    offset += sizeof(uint64_t);

    // destination
    std::vector<unsigned char> daddr(packet->data()+offset, packet->data()+offset+XBridgePacket::addressSize);
    offset += XBridgePacket::addressSize;
    std::string dcurrency((const char *)packet->data()+offset);
    offset += 8;
    uint64_t damount = *static_cast<uint64_t *>(static_cast<void *>(packet->data()+offset));
    offset += sizeof(uint64_t);

    std::vector<unsigned char> mpubkey(packet->pubkey(), packet->pubkey()+XBridgePacket::pubkeySize);

    // If order already accepted, ignore further attempts
    TransactionPtr trExists = e.transaction(id);
    if (trExists && trExists->id() == id) {
        WARN() << "order already accepted " << id.GetHex() << __FUNCTION__;
        return true;
    }

    if (!packet->verify(mpubkey))
    {
        WARN() << "invalid packet signature " << __FUNCTION__;
        return true;
    }

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
            const static uint32_t utxoItemSize = XBridgePacket::hashSize + sizeof(uint32_t) +
                                                 XBridgePacket::addressSize + XBridgePacket::signatureSize;
            if (packet->size() < offset+utxoItemSize)
            {
                WARN() << "bad packet size while reading utxo items, packet dropped in "
                       << __FUNCTION__;
                return true;
            }

            wallet::UtxoEntry entry;

            uint256 txid(packet->data()+offset);
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

    // check dust amount
    if (conn->isDustAmount(static_cast<double>(samount) / TransactionDescr::COIN) ||
        conn->isDustAmount(commonAmount - (static_cast<double>(samount) / TransactionDescr::COIN)))
    {
        LOG() << "reject dust amount transaction " << id.ToString() << " " << __FUNCTION__;
        return true;
    }

    LOG() << "received accepting transaction " << id.ToString() << std::endl
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
        if (e.acceptTransaction(id, saddr, scurrency, samount, daddr, dcurrency, damount, mpubkey, utxoItems))
        {
            // check transaction state, if trNew - do nothing,
            // if trJoined = send hold to client
            TransactionPtr tr = e.transaction(id);

            boost::mutex::scoped_lock l(tr->m_lock);

            if (tr->state() != xbridge::Transaction::trJoined)
            {
                xassert(!"wrong state");
                WARN() << "wrong tx state " << tr->id().ToString()
                       << " state " << tr->state()
                       << " in " << __FUNCTION__;
                return true;
            }

            LOG() << __FUNCTION__ << tr;

            // send hold
            // TODO remove this log
            LOG() << "send xbcTransactionHold ";

            XBridgePacketPtr reply1(new XBridgePacket(xbcTransactionHold));
            reply1->append(m_myid);
            reply1->append(tr->id().begin(), XBridgePacket::hashSize);

            reply1->sign(e.pubKey(), e.privKey());

            sendPacketBroadcast(reply1);
        }
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool Session::Impl::processTransactionHold(XBridgePacketPtr packet) const
{

    DEBUG_TRACE();

    if (packet->size() != 52)
    {
        ERR() << "incorrect packet size for xbcTransactionHold "
              << "need 52 received " << packet->size() << " "
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
    uint256 id(packet->data()+offset);
    offset += XBridgePacket::hashSize;

    // pubkey from packet
    std::vector<unsigned char> spubkey(packet->pubkey(), packet->pubkey()+XBridgePacket::pubkeySize);

    TransactionDescrPtr xtx = xapp.transaction(id);
    if (!xtx)
    {
        LOG() << "unknown order " << id.GetHex() << " " << __FUNCTION__;
        return true;
    }
    // Reject if snode key doesn't match original (prevent order manipulation)
    if (!packet->verify(xtx->sPubKey))
    {
        WARN() << "wrong servicenode handling order, expected " << HexStr(xtx->sPubKey)
               << " but received pubkey " << HexStr(spubkey)
               << " and hub address " << HexStr(hubAddress) << " " << __FUNCTION__;
        return true;
    }

    // Make sure that servicenode is still valid and in the snode list
    CPubKey pksnode;

    {
        uint32_t len = ::CPubKey::GetLen(*(char *)(packet->pubkey()));
        if (len != 33)
        {
            LOG() << "bad public key, len " << len
                  << " startsWith " << *(char *)(packet->data()+offset) << " " << __FUNCTION__;
            return false;
        }

        pksnode.Set(packet->pubkey(), packet->pubkey()+len);

        // check servicenode
        CServicenode * snode = mnodeman.Find(pksnode);
        if (!snode)
        {
            // try to uncompress pubkey and search
            if (pksnode.Decompress())
            {
                snode = mnodeman.Find(pksnode);
            }
            if (!snode)
            {
                // bad service node, no more
                LOG() << "unknown service node " << pksnode.GetID().ToString() << " " << __FUNCTION__;
                return true;
            }
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

            LOG() << __FUNCTION__ << tr;

            if (tr->state() != xbridge::Transaction::trJoined)
            {
                e.deletePendingTransaction(id);
            }

            return true;
        }
    }

    if (xtx->state >= TransactionDescr::trHold)
    {
        xassert(!"wrong state");
        WARN() << "wrong tx state " << xtx->id.ToString()
               << " state " << xtx->state
               << " in " << __FUNCTION__;
        return true;
    }

    if (!xtx->isLocal())
    {
        xtx->state = TransactionDescr::trFinished;

        LOG() << "tx moving to history " << xtx->id.ToString() << " " << __FUNCTION__;

        xapp.moveTransactionToHistory(id);
        xuiConnector.NotifyXBridgeTransactionChanged(xtx->id);
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

    LOG() << __FUNCTION__ << std::endl << "order holded" << xtx;

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

    // size must be eq 72 bytes
    if (packet->size() != 72 )
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
    uint256 id(packet->data()+offset);
    // packet pubkey
    std::vector<unsigned char> pubkey(packet->pubkey(), packet->pubkey()+XBridgePacket::pubkeySize);

    TransactionPtr tr = e.transaction(id);

    boost::mutex::scoped_lock l(tr->m_lock);

    if (!packet->verify(tr->a_pk1()) && !packet->verify(tr->b_pk1()))
    {
        WARN() << "bad trader packet signature, received " << HexStr(pubkey)
               << " expected " << HexStr(tr->a_pk1()) << " or " << HexStr(tr->b_pk1()) << " "
               << __FUNCTION__;
        return true;
    }

    if (tr->state() != xbridge::Transaction::trJoined)
    {
        xassert(!"wrong state");
        WARN() << "wrong tx state " << tr->id().ToString()
               << " state " << tr->state()
               << " in " << __FUNCTION__;
        return true;
    }

    tr->updateTimestamp();

    if (!isAddressInTransaction(from, tr))
    {
        ERR() << "invalid transaction address " << __FUNCTION__;
        sendCancelTransaction(tr, crInvalidAddress);
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
            reply1->append(id.begin(), XBridgePacket::hashSize);
            reply1->append(tr->a_address());
            reply1->append(fc);
            reply1->append(tr->a_amount());
            reply1->append(tr->a_destination());
            reply1->append(sc);
            reply1->append(tr->b_amount());

            reply1->sign(e.pubKey(), e.privKey());

            sendPacket(tr->a_destination(), reply1);

            // second
            // TODO remove this log
            LOG() << "send xbcTransactionInit to "
                  << HexStr(tr->b_destination());

            XBridgePacketPtr reply2(new XBridgePacket(xbcTransactionInit));
            reply2->append(tr->b_destination());
            reply2->append(m_myid);
            reply2->append(id.begin(), XBridgePacket::hashSize);
            reply2->append(tr->b_address());
            reply2->append(sc);
            reply2->append(tr->b_amount());
            reply2->append(tr->b_destination());
            reply2->append(fc);
            reply2->append(tr->a_amount());

            reply2->sign(e.pubKey(), e.privKey());

            sendPacket(tr->b_destination(), reply2);
        }
    }

    // LOG() << __FUNCTION__ << tr;

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


    uint256 txid(packet->data()+offset);
    offset += XBridgePacket::hashSize;

    TransactionDescrPtr xtx = xapp.transaction(txid);
    if (!xtx)
    {
        LOG() << "unknown transaction " << txid.ToString() << " " << __FUNCTION__;
        return true;
    }
    if (!xtx->isLocal())
    {
        ERR() << "not local transaction " << txid.ToString() << " " << __FUNCTION__;
        return true;
    }
    // Reject if snode key doesn't match original (prevent order manipulation)
    std::vector<unsigned char> spubkey(packet->pubkey(), packet->pubkey()+XBridgePacket::pubkeySize);
    if (!packet->verify(xtx->sPubKey))
    {
        WARN() << "wrong servicenode handling order, expected " << HexStr(xtx->sPubKey)
               << " but received pubkey " << HexStr(spubkey)
               << " and hub address " << HexStr(hubAddress) << " " << __FUNCTION__;
        return true;
    }
    if (xtx->state >= TransactionDescr::trInitialized)
    {
        xassert(!"wrong state");
        WARN() << "wrong tx state " << xtx->id.ToString()
               << " state " << xtx->state
               << " in " << __FUNCTION__;
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

    // service node pub key
    ::CPubKey pksnode;
    {
        uint32_t len = ::CPubKey::GetLen(*(char *)(packet->pubkey()));
        if (len != 33)
        {
            LOG() << "bad public key, len " << len
                  << " startsWith " << *(char *)(packet->data()+offset) << " " << __FUNCTION__;
            return false;
        }

        pksnode.Set(packet->pubkey(), packet->pubkey()+len);
    }
    // Get servicenode collateral address
    std::vector<unsigned char> snodePubKey;
    {
        CServicenode * snode = mnodeman.Find(pksnode);
        if (!snode)
        {
            // try to uncompress pubkey and search
            if (pksnode.Decompress())
            {
                snode = mnodeman.Find(pksnode);
            }
            if (!snode)
            {
                // bad service node, no more
                LOG() << "unknown service node " << pksnode.GetID().ToString() << " " << __FUNCTION__;
                return true;
            }
        }

        snodePubKey = snode->pubKeyCollateralAddress.Raw();

        LOG() << "use service node " << HexStr(snodePubKey) << " " << __FUNCTION__;
    }

    // acceptor fee
    uint256 feetxtd;
    if (xtx->role == 'B')
    {
        WalletConnectorPtr conn = xapp.connectorByCurrency(xtx->toCurrency);
        if (!conn)
        {
            WARN() << "no connector for <" << xtx->toCurrency << "> " << __FUNCTION__;
            return true;
        }

        // transaction info
        CScript destScript;
        destScript << CScript::EncodeOP_N(1);
        destScript << snodePubKey;
        {
            Array info;
            info.push_back(txid.GetHex());
            info.push_back(xtx->fromCurrency);
            info.push_back(xtx->fromAmount);
            info.push_back(xtx->toCurrency);
            info.push_back(xtx->toAmount);
            std::string strInfo = write_string(Value(info));

            uint32_t keyCounter = 0;
            for (auto si = strInfo.begin(); si < strInfo.end(); si += XBridgePacket::uncompressedPubkeySizeRaw)
            {
                std::vector<unsigned char> pk(XBridgePacket::uncompressedPubkeySize, ' ');
                pk[0] = 0x04;
                std::copy(si, si + std::min(strInfo.size() - XBridgePacket::uncompressedPubkeySizeRaw * keyCounter,
                                            static_cast<size_t>(XBridgePacket::uncompressedPubkeySizeRaw)), pk.begin()+1);

                destScript << pk;
                ++keyCounter;
            }

            destScript << CScript::EncodeOP_N(keyCounter+1) << OP_CHECKMULTISIG;
        }

        std::string strtxid;
        if (!rpc::storeDataIntoBlockchain(destScript, conn->serviceNodeFee,
                                          std::vector<unsigned char>(), strtxid))
        {
            ERR() << "storeDataIntoBlockchain failed, error send blocknet tx " << __FUNCTION__;
            sendCancelTransaction(xtx, crBlocknetError);
            return true;
        }

        feetxtd = uint256(strtxid);

        if(feetxtd.IsNull())
        {
            LOG() << "storeDataIntoBlockchain failed with zero tx id, process packet later " << __FUNCTION__;
            xapp.processLater(txid, packet);
            return true;
        }
    }

    xtx->state = TransactionDescr::trInitialized;
    xuiConnector.NotifyXBridgeTransactionChanged(xtx->id);

    // LOG() << __FUNCTION__ << xtx;

    // send initialized
    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionInitialized));
    reply->append(hubAddress);
    reply->append(thisAddress);
    reply->append(txid.begin(), 32);
    reply->append(feetxtd.begin(), 32);

    reply->sign(xtx->mPubKey, xtx->mPrivKey);

    sendPacket(hubAddress, reply);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::processTransactionInitialized(XBridgePacketPtr packet) const
{
    DEBUG_TRACE();

    // size must be eq 104 bytes
    if (packet->size() != 104)
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

    std::vector<unsigned char> from(packet->data()+20, packet->data()+40);

    // transaction id
    uint256 id(packet->data()+40);

    // opponent public key
    std::vector<unsigned char> pk1(packet->pubkey(), packet->pubkey()+XBridgePacket::pubkeySize);

    // TODO check fee transaction

    TransactionPtr tr = e.transaction(id);
    if (!packet->verify(tr->a_pk1()) && !packet->verify(tr->b_pk1()))
    {
        WARN() << "bad trader packet signature, received " << HexStr(pk1)
               << " expected " << HexStr(tr->a_pk1()) << " or " << HexStr(tr->b_pk1()) << " "
               << __FUNCTION__;
        return true;
    }

    boost::mutex::scoped_lock l(tr->m_lock);

    tr->updateTimestamp();

    if (!isAddressInTransaction(from, tr))
    {
        ERR() << "invalid transaction address " << __FUNCTION__;
        sendCancelTransaction(tr, crInvalidAddress);
        return true;
    }

    if (e.updateTransactionWhenInitializedReceived(tr, from, pk1))
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
            reply1->append(tr->b_pk1());

            reply1->sign(e.pubKey(), e.privKey());

            sendPacket(tr->a_address(), reply1);
        }
    }

    LOG() << __FUNCTION__ << tr;

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

    if (packet->size() != 105)
    {
        ERR() << "incorrect packet size for xbcTransactionCreateA "
              << "need 105 bytes, received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    std::vector<unsigned char> thisAddress(packet->data(), packet->data()+20);
    std::vector<unsigned char> hubAddress(packet->data()+20, packet->data()+40);

    // transaction id
    uint256 txid(packet->data()+40);

    // destination address
    uint32_t offset = 72;

    std::vector<unsigned char> mPubKey(packet->data()+offset, packet->data()+offset+33);
    // offset += 33;

    xbridge::App & xapp = xbridge::App::instance();

    TransactionDescrPtr xtx = xapp.transaction(txid);
    if (!xtx)
    {
        LOG() << "unknown transaction " << txid.GetHex() << " " << __FUNCTION__;
        return true;
    }
    if (!xtx->isLocal())
    {
        ERR() << "not local transaction " << txid.GetHex() << " " << __FUNCTION__;
        return true;
    }
    // Reject if snode key doesn't match original (prevent order manipulation)
    std::vector<unsigned char> spubkey(packet->pubkey(), packet->pubkey()+XBridgePacket::pubkeySize);
    if (!packet->verify(xtx->sPubKey))
    {
        WARN() << "wrong servicenode handling order, expected " << HexStr(xtx->sPubKey)
               << " but received pubkey " << HexStr(spubkey)
               << " and hub address " << HexStr(hubAddress) << " " << __FUNCTION__;
        return true;
    }
    if (xtx->role != 'A')
    {
        ERR() << "received packet for wrong role, expected role A " << __FUNCTION__;
        return true;
    }

    if (xtx->state >= TransactionDescr::trCreated)
    {
        xassert(!"wrong state");
        WARN() << "wrong tx state " << xtx->id.ToString()
               << " state " << xtx->state
               << " in " << __FUNCTION__;
        return true;
    }

    // LOG() << __FUNCTION__ << xtx;

    // connectors
    WalletConnectorPtr connFrom = xapp.connectorByCurrency(xtx->fromCurrency);
    WalletConnectorPtr connTo   = xapp.connectorByCurrency(xtx->toCurrency);
    if (!connFrom || !connTo)
    {
        WARN() << "no connector for <" << (!connFrom ? xtx->fromCurrency : xtx->toCurrency) << "> " << __FUNCTION__;
        sendCancelTransaction(xtx, crBadADepositTx);
        return true;
    }

    double outAmount = static_cast<double>(xtx->fromAmount) / TransactionDescr::COIN;

    double fee1      = 0;
    double fee2      = connFrom->minTxFee2(1, 1);
    double inAmount  = 0;

    std::vector<wallet::UtxoEntry> usedInTx;
    for (const wallet::UtxoEntry & entry : xtx->usedCoins)
    {
        usedInTx.push_back(entry);
        inAmount += entry.amount;
        fee1 = connFrom->minTxFee1(usedInTx.size(), 3);

        LOG() << "using utxo item, id: <" << entry.txId << "> amount: " << entry.amount << " vout: " << entry.vout;

        // check amount
        if (inAmount >= outAmount+fee1+fee2)
        {
            break;
        }
    }

    LOG() << "fee1: " << fee1;
    LOG() << "fee2: " << fee2;
    LOG() << "amount of used utxo items: " << inAmount << " required amount + fees: " << outAmount + fee1 + fee2;

    // check amount
    if (inAmount < outAmount+fee1+fee2)
    {
        // no money, cancel transaction
        LOG() << "no money, transaction canceled " << __FUNCTION__;
        sendCancelTransaction(xtx, crNoMoney);
        return true;
    }

    // lock time
    xtx->lockTimeTx1 = connFrom->lockTime(xtx->role);
    if (xtx->lockTimeTx1 == 0)
    {
        LOG() << "lockTime error, transaction canceled " << __FUNCTION__;
        sendCancelTransaction(xtx, crRpcError);
        return true;
    }

    // store opponent public key (packet verification)
    xtx->oPubKey = mPubKey;

    // create transactions

    // hash of secret
    std::vector<unsigned char> hx = connFrom->getKeyId(xtx->xPubKey);

#ifdef LOG_KEYPAIR_VALUES
    LOG() << "unlock script pub keys" << std::endl <<
             "    my       " << HexStr(xtx->mPubKey) << std::endl <<
             "    my id    " << HexStr(connFrom->getKeyId(xtx->mPubKey)) << std::endl <<
             "    other    " << HexStr(mPubKey) << std::endl <<
             "    other id " << HexStr(connFrom->getKeyId(mPubKey)) << std::endl <<
             "    x id     " << HexStr(hx);
#endif

    // create address for first tx
    connFrom->createDepositUnlockScript(xtx->mPubKey, mPubKey, hx, xtx->lockTimeTx1, xtx->innerScript);
    xtx->depositP2SH = connFrom->scriptIdToString(connFrom->getScriptId(xtx->innerScript));

    // depositTx
    {
        std::vector<xbridge::XTxIn>                  inputs;
        std::vector<std::pair<std::string, double> > outputs;

        // inputs
        for (const wallet::UtxoEntry & entry : usedInTx)
        {
            inputs.emplace_back(entry.txId, entry.vout, entry.amount);
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
            ERR() << "deposit not created, transaction canceled " << __FUNCTION__;
            TXERR() << "deposit sendrawtransaction " << xtx->binTx;
            sendCancelTransaction(xtx, crRpcError);
            return true;
        }

        TXLOG() << "deposit sendrawtransaction " << xtx->binTx;

    } // depositTx

    // refundTx
    {
        std::vector<xbridge::XTxIn>                  inputs;
        std::vector<std::pair<std::string, double> > outputs;

        // inputs from binTx
        inputs.emplace_back(xtx->binTxId, 0, outAmount+fee2);

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
                                               xtx->innerScript, xtx->lockTimeTx1,
                                               xtx->refTxId, xtx->refTx))
        {
            // cancel transaction
            ERR() << "refund transaction not created, transaction canceled " << __FUNCTION__;
            TXERR() << "refund sendrawtransaction " << xtx->refTx;
            sendCancelTransaction(xtx, crRpcError);
            return true;
        }

        TXLOG() << "refund sendrawtransaction " << xtx->refTx;

    } // refundTx

    xtx->state = TransactionDescr::trCreated;

    xuiConnector.NotifyXBridgeTransactionChanged(txid);

    // send transactions
    {
        std::string sentid;
        int32_t errCode = 0;
        std::string errorMessage;
        if (connFrom->sendRawTransaction(xtx->binTx, sentid, errCode, errorMessage))
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
    reply.reset(new XBridgePacket(xbcTransactionCreatedA));

    reply->append(hubAddress);
    reply->append(thisAddress);
    reply->append(txid.begin(), 32);
    reply->append(xtx->binTxId);
    reply->append(hx);
    reply->append(static_cast<uint32_t>(xtx->innerScript.size()));
    reply->append(xtx->innerScript);

    reply->sign(xtx->mPubKey, xtx->mPrivKey);

    sendPacket(hubAddress, reply);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::processTransactionCreatedA(XBridgePacketPtr packet) const
{
    DEBUG_TRACE();

    // size must be > 92 bytes
    if (packet->size() < 92)
    {
        ERR() << "invalid packet size for xbcTransactionCreatedA "
              << "need more than 92 received " << packet->size() << " "
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

    std::vector<unsigned char> hx(packet->data()+offset, packet->data()+offset+20);
    offset += 20;

    uint32_t innerSize = *reinterpret_cast<uint32_t *>(packet->data()+offset);
    offset += sizeof(uint32_t);

    std::vector<unsigned char> innerScript(packet->data()+offset, packet->data()+offset+innerSize);
    // offset += innerScript.size();

    TransactionPtr tr = e.transaction(txid);

    std::vector<unsigned char> pk1(packet->pubkey(), packet->pubkey()+XBridgePacket::pubkeySize);
    if (!packet->verify(tr->a_pk1()))
    {
        WARN() << "bad traderA packet signature, received " << HexStr(pk1)
               << " expected " << HexStr(tr->a_pk1()) << " " << __FUNCTION__;
        return true;
    }

    boost::mutex::scoped_lock l(tr->m_lock);

    tr->updateTimestamp();

    if (!isAddressInTransaction(from, tr))
    {
        ERR() << "invalid transaction address " << __FUNCTION__;
        sendCancelTransaction(tr, crInvalidAddress);
        return true;
    }

    if (e.updateTransactionWhenCreatedReceived(tr, from, binTxId, innerScript))
    {
        // wtf ?
        ERR() << "invalid createdA " << __FUNCTION__;
        sendCancelTransaction(tr, crInvalidAddress);
        return true;
    }

    // TODO remove this log
    LOG() << "send xbcTransactionCreate to "
          << HexStr(tr->b_address());

    XBridgePacketPtr reply2(new XBridgePacket(xbcTransactionCreateB));
    reply2->append(tr->b_address());
    reply2->append(m_myid);
    reply2->append(txid.begin(), 32);
    reply2->append(tr->a_pk1());
    reply2->append(binTxId);
    reply2->append(hx);

    reply2->sign(e.pubKey(), e.privKey());

    sendPacket(tr->b_address(), reply2);

    LOG() << __FUNCTION__ << tr;

    return true;
}

//******************************************************************************
//******************************************************************************
bool Session::Impl::processTransactionCreateB(XBridgePacketPtr packet) const
{
    DEBUG_TRACE();

    if (packet->size() < 125)
    {
        ERR() << "incorrect packet size for xbcTransactionCreateB "
              << "need min 125 bytes, received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    std::vector<unsigned char> thisAddress(packet->data(), packet->data()+20);
    std::vector<unsigned char> hubAddress(packet->data()+20, packet->data()+40);

    // transaction id
    uint256 txid(packet->data()+40);

    // destination address
    uint32_t offset = 72;

    std::vector<unsigned char> mPubKey(packet->data()+offset, packet->data()+offset+33);
    offset += 33;

    std::string binATxId(reinterpret_cast<const char *>(packet->data()+offset));
    offset += binATxId.size()+1;

    std::vector<unsigned char> hx(packet->data()+offset, packet->data()+offset+20);
    offset += 20;

    xbridge::App & xapp = xbridge::App::instance();

    TransactionDescrPtr xtx = xapp.transaction(txid);
    if (!xtx)
    {
        LOG() << "unknown transaction " << txid.GetHex() << " " << __FUNCTION__;
        return true;
    }
    if (!xtx->isLocal())
    {
        ERR() << "not local transaction " << txid.GetHex() << " " << __FUNCTION__;
        return true;
    }
    // Reject if snode key doesn't match original (prevent order manipulation)
    std::vector<unsigned char> spubkey(packet->pubkey(), packet->pubkey()+XBridgePacket::pubkeySize);
    if (!packet->verify(xtx->sPubKey))
    {
        WARN() << "wrong servicenode handling order, expected " << HexStr(xtx->sPubKey)
               << " but received pubkey " << HexStr(spubkey)
               << " and hub address " << HexStr(hubAddress) << " " << __FUNCTION__;
        return true;
    }
    if (binATxId.size() == 0)
    {
        LOG() << "bad A deposit tx id received for " << txid.GetHex() << " " << __FUNCTION__;
        sendCancelTransaction(xtx, crBadADepositTx);
        return true;
    }
    if (xtx->role != 'B')
    {
        ERR() << "received packet for wrong role, expected role B " << __FUNCTION__;
        return true;
    }
    if(xtx->xPubKey.size() != 0)
    {
        ERR() << "bad role" << __FUNCTION__;
        return true;
    }

    if (xtx->state >= TransactionDescr::trCreated)
    {
        xassert(!"wrong state");
        WARN() << "wrong tx state " << xtx->id.ToString()
               << " state " << xtx->state
               << " in " << __FUNCTION__;
        return true;
    }

    // LOG() << __FUNCTION__ << xtx;

    // connectors
    WalletConnectorPtr connFrom = xapp.connectorByCurrency(xtx->fromCurrency);
    WalletConnectorPtr connTo   = xapp.connectorByCurrency(xtx->toCurrency);
    if (!connFrom || !connTo)
    {
        WARN() << "no connector for <" << (!connFrom ? xtx->fromCurrency : xtx->toCurrency) << "> " << __FUNCTION__;
        sendCancelTransaction(xtx, crBadADepositTx);
        return true;
    }

    double outAmount = static_cast<double>(xtx->fromAmount) / TransactionDescr::COIN;
    double checkAmount = static_cast<double>(xtx->toAmount) / TransactionDescr::COIN;

    // TODO check A iner script


    // check A deposit tx
    {
        bool isGood = false;
        if (!connTo->checkDepositTransaction(binATxId, std::string(), checkAmount, isGood))
        {
            // move packet to pending
            xapp.processLater(txid, packet);
            return true;
        }
        else if (!isGood)
        {
            LOG() << "check A deposit tx error for " << txid.GetHex() << " " << __FUNCTION__;
            sendCancelTransaction(xtx, crBadADepositTx);
            return true;
        }

        LOG() << "deposit A tx confirmed " << txid.GetHex();
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

        LOG() << "using utxo item, id: <" << entry.txId << "> amount: " << entry.amount << " vout: " << entry.vout;

        // check amount
        if (inAmount >= outAmount+fee1+fee2)
        {
            break;
        }
    }

    LOG() << "fee1: " << fee1;
    LOG() << "fee2: " << fee2;
    LOG() << "amount of used utxo items: " << inAmount << " required amount + fees: " << outAmount + fee1 + fee2;

    // check amount
    if (inAmount < outAmount+fee1+fee2)
    {
        // no money, cancel transaction
        LOG() << "no money, transaction canceled " << __FUNCTION__;
        sendCancelTransaction(xtx, crNoMoney);
        return true;
    }

    // lock time
    xtx->lockTimeTx1 = connFrom->lockTime(xtx->role);
    if (xtx->lockTimeTx1 == 0)
    {
        LOG() << "lockTime error, transaction canceled " << __FUNCTION__;
        sendCancelTransaction(xtx, crRpcError);
        return true;
    }

    // store opponent public key (packet verification)
    xtx->oPubKey = mPubKey;

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
    connFrom->createDepositUnlockScript(xtx->mPubKey, mPubKey, hx, xtx->lockTimeTx1, xtx->innerScript);
    xtx->depositP2SH = connFrom->scriptIdToString(connFrom->getScriptId(xtx->innerScript));

    // depositTx
    {
        std::vector<xbridge::XTxIn>                  inputs;
        std::vector<std::pair<std::string, double> > outputs;

        // inputs
        for (const wallet::UtxoEntry & entry : usedInTx)
        {
            inputs.emplace_back(entry.txId, entry.vout, entry.amount);
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
            ERR() << "deposit not created, transaction canceled " << __FUNCTION__;
            TXERR() << "deposit sendrawtransaction " << xtx->binTx;
            sendCancelTransaction(xtx, crRpcError);
            return true;
        }

        TXLOG() << "deposit sendrawtransaction " << xtx->binTx;

    } // depositTx

    // refundTx
    {
        std::vector<xbridge::XTxIn>                  inputs;
        std::vector<std::pair<std::string, double> > outputs;

        // inputs from binTx
        inputs.emplace_back(xtx->binTxId, 0, outAmount+fee2);

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
                                               xtx->innerScript, xtx->lockTimeTx1,
                                               xtx->refTxId, xtx->refTx))
        {
            // cancel transaction
            ERR() << "refund transaction not created, transaction canceled " << __FUNCTION__;
            TXERR() << "refund sendrawtransaction " << xtx->refTx;
            sendCancelTransaction(xtx, crRpcError);
            return true;
        }

        TXLOG() << "refund sendrawtransaction " << xtx->refTx;

    } // refundTx

    xtx->state = TransactionDescr::trCreated;

    xuiConnector.NotifyXBridgeTransactionChanged(txid);

    // send transactions
    {
        std::string sentid;
        int32_t errCode = 0;
        std::string errorMessage;
        if (connFrom->sendRawTransaction(xtx->binTx, sentid, errCode, errorMessage))
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
    reply.reset(new XBridgePacket(xbcTransactionCreatedB));

    reply->append(hubAddress);
    reply->append(thisAddress);
    reply->append(txid.begin(), 32);
    reply->append(xtx->binTxId);
    reply->append(static_cast<uint32_t>(xtx->innerScript.size()));
    reply->append(xtx->innerScript);

    reply->sign(xtx->mPubKey, xtx->mPrivKey);

    sendPacket(hubAddress, reply);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::processTransactionCreatedB(XBridgePacketPtr packet) const
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

    std::vector<unsigned char> pk1(packet->pubkey(), packet->pubkey()+XBridgePacket::pubkeySize);
    if (!packet->verify(tr->b_pk1()))
    {
        WARN() << "bad traderB packet signature, received " << HexStr(pk1)
               << " expected " << HexStr(tr->b_pk1()) << " " << __FUNCTION__;
        return true;
    }

    boost::mutex::scoped_lock l(tr->m_lock);

    tr->updateTimestamp();

    if (!isAddressInTransaction(from, tr))
    {
        ERR() << "invalid transaction address " << __FUNCTION__;
        sendCancelTransaction(tr, crInvalidAddress);
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

            reply->sign(e.pubKey(), e.privKey());

            sendPacket(tr->a_destination(), reply);
        }
    }

    LOG() << __FUNCTION__ << tr;

    return true;
}

//******************************************************************************
//******************************************************************************
bool Session::Impl::processTransactionConfirmA(XBridgePacketPtr packet) const
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
    if (!xtx->isLocal())
    {
        ERR() << "not local transaction " << HexStr(txid) << " " << __FUNCTION__;
        return true;
    }
    // Reject if servicenode key doesn't match original (prevent order manipulation)
    std::vector<unsigned char> spubkey(packet->pubkey(), packet->pubkey()+XBridgePacket::pubkeySize);
    if (!packet->verify(xtx->sPubKey))
    {
        WARN() << "wrong servicenode handling order, expected " << HexStr(xtx->sPubKey)
               << " but received pubkey " << HexStr(spubkey)
               << " and hub address " << HexStr(hubAddress) << " " << __FUNCTION__;
        return true;
    }
    if (xtx->state >= TransactionDescr::trCommited)
    {
        xassert(!"wrong state");
        WARN() << "wrong tx state " << xtx->id.ToString()
               << " state " << xtx->state
               << " in " << __FUNCTION__;
        return true;
    }

    WalletConnectorPtr conn = xapp.connectorByCurrency(xtx->toCurrency);
    if (!conn)
    {
        WARN() << "no connector for <" << xtx->toCurrency << "> " << __FUNCTION__;
        sendCancelTransaction(xtx, crBadBDepositTx);
        return true;
    }

    double outAmount   = static_cast<double>(xtx->toAmount)/TransactionDescr::COIN;
    double checkAmount = outAmount;

    // check B deposit tx
    {
        // TODO check tx in blockchain and move packet to pending if not

        bool   isGood      = false;
        if (!conn->checkDepositTransaction(binTxId, std::string(), checkAmount, isGood))
        {
            xapp.processLater(txid, packet);
            return true;
        }
        else if (!isGood)
        {
            LOG() << "check B deposit tx error for " << txid.GetHex() << " " << __FUNCTION__;
            sendCancelTransaction(xtx, crBadBDepositTx);
            return true;
        }

        LOG() << "deposit B tx confirmed " << txid.GetHex();
    }

    // payTx
    {
        std::vector<xbridge::XTxIn>                  inputs;
        std::vector<std::pair<std::string, double> > outputs;

        // inputs from binTx
        inputs.emplace_back(binTxId, 0, checkAmount);

        // outputs
        {
            outputs.push_back(std::make_pair(conn->fromXAddr(xtx->to), outAmount));
        }

        if (!conn->createPaymentTransaction(inputs, outputs,
                                            xtx->mPubKey, xtx->mPrivKey,
                                            xtx->xPubKey, innerScript,
                                            xtx->payTxId, xtx->payTx))
        {
            // cancel transaction
            ERR() << "payment transaction create error, transaction canceled " << __FUNCTION__;
            TXERR() << "payment A sendrawtransaction " << xtx->payTx;
            sendCancelTransaction(xtx, crRpcError);
            return true;
        }

        TXLOG() << "payment A sendrawtransaction " << xtx->payTx;

    } // payTx

    // send pay tx
    std::string sentid;
    int32_t errCode = 0;
    std::string errorMessage;
    if (conn->sendRawTransaction(xtx->payTx, sentid, errCode, errorMessage))
    {
        LOG() << "payment A " << sentid;
    }
    else
    {
        if (errCode == -25)
        {
            // missing inputs, wait deposit tx
            LOG() << "payment A not sent, no deposit tx, move to pending";

            xapp.processLater(txid, packet);
            return true;
        }

        LOG() << "payment A tx not sent, transaction canceled " << __FUNCTION__;
        sendCancelTransaction(xtx, crRpcError);
        return true;
    }

    xtx->state = TransactionDescr::trCommited;
    xuiConnector.NotifyXBridgeTransactionChanged(txid);

    // LOG() << __FUNCTION__ << xtx;

    // send reply
    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionConfirmedA));
    reply->append(hubAddress);
    reply->append(thisAddress);
    reply->append(txid.begin(), 32);
    reply->append(xtx->xPubKey);

    reply->sign(xtx->mPubKey, xtx->mPrivKey);

    sendPacket(hubAddress, reply);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::processTransactionConfirmedA(XBridgePacketPtr packet) const
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

    std::vector<unsigned char> pk1(packet->pubkey(), packet->pubkey()+XBridgePacket::pubkeySize);
    if (!packet->verify(tr->a_pk1()))
    {
        WARN() << "bad traderA packet signature, received " << HexStr(pk1)
               << " expected " << HexStr(tr->a_pk1()) << " " << __FUNCTION__;
        return true;
    }

    boost::mutex::scoped_lock l(tr->m_lock);

    tr->updateTimestamp();

    if (!isAddressInTransaction(from, tr))
    {
        ERR() << "invalid transaction address " << __FUNCTION__;
        sendCancelTransaction(tr, crInvalidAddress);
        return true;
    }

    if (e.updateTransactionWhenConfirmedReceived(tr, from))
    {
        // wtf ?
        ERR() << "invalid confirmation " << __FUNCTION__;
        sendCancelTransaction(tr, crInvalidAddress);
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

    reply2->sign(e.pubKey(), e.privKey());

    sendPacket(tr->b_destination(), reply2);

    LOG() << __FUNCTION__ << tr;

    return true;
}

//******************************************************************************
//******************************************************************************
bool Session::Impl::processTransactionConfirmB(XBridgePacketPtr packet) const
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
        LOG() << "unknown transaction " << txid.GetHex() << " " << __FUNCTION__;
        return true;
    }
    if (!xtx->isLocal())
    {
        ERR() << "not local transaction " << txid.GetHex() << " " << __FUNCTION__;
        return true;
    }
    // Reject if servicenode key doesn't match original (prevent order manipulation)
    std::vector<unsigned char> spubkey(packet->pubkey(), packet->pubkey()+XBridgePacket::pubkeySize);
    if (!packet->verify(xtx->sPubKey))
    {
        WARN() << "wrong servicenode handling order, expected " << HexStr(xtx->sPubKey)
               << " but received pubkey " << HexStr(spubkey)
               << " and hub address " << HexStr(hubAddress) << " " << __FUNCTION__;
        return true;
    }
    if (xtx->state >= TransactionDescr::trCommited)
    {
        xassert(!"wrong state");
        WARN() << "wrong tx state " << xtx->id.ToString()
               << " state " << xtx->state
               << " in " << __FUNCTION__;
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
        double outAmount   = static_cast<double>(xtx->toAmount)/TransactionDescr::COIN;
        double checkAmount = outAmount;

        bool isGood = false;
        if (!conn->checkDepositTransaction(binTxId, std::string(), checkAmount, isGood) || !isGood)
        {
            // oops....shit happens, alert needed
            // this tx already checked before deposit created
            WARN() << "deposit not found " << binTxId << " " << __FUNCTION__;
            sendCancelTransaction(xtx, crBadADepositTx);
            return true;
        }

        std::vector<xbridge::XTxIn>                  inputs;
        std::vector<std::pair<std::string, double> > outputs;

        // inputs from binTx
        inputs.emplace_back(binTxId, 0, checkAmount);

        // outputs
        {
            outputs.push_back(std::make_pair(conn->fromXAddr(xtx->to), outAmount));
        }

        if (!conn->createPaymentTransaction(inputs, outputs,
                                            xtx->mPubKey, xtx->mPrivKey,
                                            x, innerScript,
                                            xtx->payTxId, xtx->payTx))
        {
            // cancel transaction
            ERR() << "payment transaction create error, transaction canceled " << __FUNCTION__;
            TXERR() << "payment B sendrawtransaction " << xtx->payTx;
            sendCancelTransaction(xtx, crRpcError);
            return true;
        }

        TXLOG() << "payment B sendrawtransaction " << xtx->payTx;

    } // payTx

    // send pay tx
    std::string sentid;
    int32_t errCode = 0;
    std::string errorMessage;
    if (conn->sendRawTransaction(xtx->payTx, sentid, errCode, errorMessage))
    {
        LOG() << "payment B " << sentid;
    }
    else
    {
        if (errCode == -25)
        {
            // missing inputs, wait deposit tx
            // move packet to pending
            LOG() << "payment B not sent, no deposit tx, move to pending";

            xapp.processLater(txid, packet);
            return true;
        }

        LOG() << "payment B tx not sent, transaction canceled " << __FUNCTION__;
        sendCancelTransaction(xtx, crRpcError);
        return true;
    }

    xtx->state = TransactionDescr::trCommited;
    xuiConnector.NotifyXBridgeTransactionChanged(txid);

    // LOG() << __FUNCTION__ << xtx;

    // send reply
    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionConfirmedB));
    reply->append(hubAddress);
    reply->append(thisAddress);
    reply->append(txid.begin(), 32);

    reply->sign(xtx->mPubKey, xtx->mPrivKey);

    sendPacket(hubAddress, reply);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::processTransactionConfirmedB(XBridgePacketPtr packet) const
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

    std::vector<unsigned char> pk1(packet->pubkey(), packet->pubkey()+XBridgePacket::pubkeySize);
    if (!packet->verify(tr->b_pk1()))
    {
        WARN() << "bad traderB packet signature, received " << HexStr(pk1)
               << " expected " << HexStr(tr->b_pk1()) << " " << __FUNCTION__;
        return true;
    }

    boost::mutex::scoped_lock l(tr->m_lock);

    tr->updateTimestamp();

    if (!isAddressInTransaction(from, tr))
    {
        ERR() << "invalid transaction address " << __FUNCTION__;
        sendCancelTransaction(tr, crInvalidAddress);
        return true;
    }

    if (e.updateTransactionWhenConfirmedReceived(tr, from))
    {
        if (tr->state() == xbridge::Transaction::trFinished)
        {
            LOG() << "broadcast send xbcTransactionFinished";

            XBridgePacketPtr reply(new XBridgePacket(xbcTransactionFinished));
            reply->append(txid.begin(), 32);

            reply->sign(e.pubKey(), e.privKey());

            sendPacketBroadcast(reply);
        }
    }

    LOG() << __FUNCTION__ << tr;

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

    uint256 txid(packet->data());
    TxCancelReason reason = static_cast<TxCancelReason>(*reinterpret_cast<uint32_t*>(packet->data() + 32));

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
            LOG() << "can't find transaction " << __FUNCTION__;
            return true;
        }

        boost::mutex::scoped_lock l(tr->m_lock);

        LOG() << __FUNCTION__ << tr;

        if (!packet->verify(tr->a_pk1()) && !packet->verify(tr->b_pk1()))
        {
            WARN() << "invalid packet signature " << __FUNCTION__;
            return true;
        }

        sendCancelTransaction(tr, reason);
        e.deletePendingTransaction(txid);
        return true;
    }

    xbridge::App & xapp = xbridge::App::instance();
    TransactionDescrPtr xtx = xapp.transaction(txid);
    if (!xtx)
    {
        LOG() << "unknown transaction " << HexStr(txid) << " " << __FUNCTION__;
        return true;
    }

    if (!packet->verify(xtx->sPubKey) && !packet->verify(xtx->oPubKey))
    {
        LOG() << "no packet signature for cancel on order " << xtx->id.GetHex() << " " << __FUNCTION__;
        return true;
    }

    // rollback, commit revert transaction
    WalletConnectorPtr conn = xapp.connectorByCurrency(xtx->fromCurrency);
    if (!conn)
    {
        WARN() << "no connector for <" << xtx->toCurrency << "> " << __FUNCTION__;
        return false;
    }

    // unlock coins
    conn->lockCoins(xtx->usedCoins, false);

    if (xtx->state < TransactionDescr::trCreated)
    {
        xapp.moveTransactionToHistory(txid);
        xtx->state  = TransactionDescr::trCancelled;
        xtx->reason = reason;

        LOG() << __FUNCTION__ << xtx;

        xuiConnector.NotifyXBridgeTransactionChanged(txid);
        return true;
    }

    // remove from pending packets (if added)
    xapp.removePackets(txid);

    // If refund transaction id not defined, do not attempt to rollback
    if (xtx->refTx.empty()) {
        LOG() << "no refund tx id " << xtx->id.GetHex() << " " << __FUNCTION__;
        return true;
    }

    // Process rollback

    std::string sid;
    int32_t errCode = 0;
    std::string errorMessage;
    xbridge::rpc::WalletInfo info;

    bool infoRecieved = conn->getInfo(info);

    if(infoRecieved && info.blocks < xtx->lockTimeTx1)
    {
        LOG() << "waiting for loctime expiration before refund tx " << txid.GetHex() << " " << __FUNCTION__;
        xapp.processLater(txid, packet);
    }
    else
    {
        if(!infoRecieved)
            LOG() << "can't get wallet info to calc locktime, let's try to send without it " << txid.GetHex() << " " << __FUNCTION__;

        if (!conn->sendRawTransaction(xtx->refTx, sid, errCode, errorMessage))
        {
            // TODO move packet to pending if error
            LOG() << "send rollback error, tx " << txid.GetHex() << " " << __FUNCTION__;
            xtx->state = TransactionDescr::trRollbackFailed;
            xapp.processLater(txid, packet);
        }
        else
        {
            xtx->state = TransactionDescr::trRollback;
        }
    }

    LOG() << __FUNCTION__ << xtx;

    // update transaction state for gui
    xuiConnector.NotifyXBridgeTransactionChanged(txid);

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
    LOG() << "finish transaction <" << tr->id().GetHex() << ">";

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
bool Session::Impl::sendCancelTransaction(const TransactionPtr & tx,
                                          const TxCancelReason & reason) const
{
    Exchange & e = Exchange::instance();
    if (!e.isStarted())
    {
        return false;
    }

    LOG() << "cancel transaction <" << tx->id().GetHex() << ">";

    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionCancel));
    reply->append(tx->id().begin(), 32);
    reply->append(static_cast<uint32_t>(reason));

    reply->sign(e.pubKey(), e.privKey());

    sendPacketBroadcast(reply);
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool Session::Impl::sendCancelTransaction(const TransactionDescrPtr & tx,
                                          const TxCancelReason & reason) const
{
    LOG() << "cancel transaction <" << tx->id.GetHex() << ">";

    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionCancel));
    reply->append(tx->id.begin(), 32);
    reply->append(static_cast<uint32_t>(reason));

    reply->sign(tx->mPubKey, tx->mPrivKey);

    sendPacketBroadcast(reply);

    // update transaction state for gui
    tx->state  = TransactionDescr::trCancelled;
    tx->reason = reason;
    xuiConnector.NotifyXBridgeTransactionChanged(tx->id);

    return true;
}

//*****************************************************************************
//*****************************************************************************
void Session::sendListOfTransactions() const
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
            if (i.second->state == xbridge::TransactionDescr::trNew ||
                i.second->state == xbridge::TransactionDescr::trPending)
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
        packet->append(util::timeToInt(ptr->createdTime()));
        packet->append(ptr->blockHash().begin(), 32);

        packet->sign(e.pubKey(), e.privKey());

        m_p->sendPacketBroadcast(packet);
    }
}

//*****************************************************************************
//*****************************************************************************
void Session::eraseExpiredPendingTransactions() const
{
    // check xbridge transactions
    Exchange & e = Exchange::instance();
    e.eraseExpiredTransactions();

    // check client transactions
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

        boost::mutex::scoped_lock l(ptr->m_lock);

        uint256 txid = ptr->id();

        if (ptr->state() == xbridge::Transaction::trCancelled)
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
            m_p->sendCancelTransaction(ptr, TxCancelReason::crTimeout);
            ptr->finish();
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
    if (xtx == nullptr)
    {
        LOG() << "unknown transaction " << HexStr(txid) << " " << __FUNCTION__;
        return true;
    }
    std::vector<unsigned char> spubkey(packet->pubkey(), packet->pubkey()+XBridgePacket::pubkeySize);
    if (!packet->verify(xtx->sPubKey))
    {
        WARN() << "wrong servicenode handling order, expected " << HexStr(xtx->sPubKey)
               << " but received pubkey " << HexStr(spubkey) << " " << __FUNCTION__;
        return true;
    }

    // update transaction state for gui
    xtx->state = TransactionDescr::trFinished;

    LOG() << __FUNCTION__ << xtx;

    xapp.moveTransactionToHistory(txid);
    xuiConnector.NotifyXBridgeTransactionChanged(txid);

    return true;
}

} // namespace xbridge
