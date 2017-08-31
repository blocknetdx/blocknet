//*****************************************************************************
//*****************************************************************************

#include "xbridgeapp.h"
#include "xbridgeexchange.h"
#include "util/xutil.h"
#include "util/logger.h"
#include "util/settings.h"
#include "version.h"
#include "config.h"
#include "xuiconnector.h"
#include "rpcserver.h"
#include "net.h"
#include "util.h"
#include "xkey.h"
#include "ui_interface.h"

#include <boost/chrono/chrono.hpp>
#include <boost/thread/thread.hpp>
#include <assert.h>

#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/lexical_cast.hpp>

#include <openssl/rand.h>
#include <openssl/md5.h>

#define dht_fromAddress 0
#define dht_toAddress 1
#define dht_packet 2
#define dht_resendFlag 3

//*****************************************************************************
//*****************************************************************************
XUIConnector xuiConnector;

//*****************************************************************************
//*****************************************************************************
boost::mutex                                  XBridgeApp::m_txLocker;
std::map<uint256, XBridgeTransactionDescrPtr> XBridgeApp::m_pendingTransactions;
std::map<uint256, XBridgeTransactionDescrPtr> XBridgeApp::m_transactions;
std::map<uint256, XBridgeTransactionDescrPtr> XBridgeApp::m_historicTransactions;
boost::mutex                                  XBridgeApp::m_txUnconfirmedLocker;
std::map<uint256, XBridgeTransactionDescrPtr> XBridgeApp::m_unconfirmed;
boost::mutex                                  XBridgeApp::m_ppLocker;
std::map<uint256, std::pair<std::string, XBridgePacketPtr> >  XBridgeApp::m_pendingPackets;

//*****************************************************************************
//*****************************************************************************
void badaboom()
{
    int * a = 0;
    *a = 0;
}

//*****************************************************************************
//*****************************************************************************
XBridgeApp::XBridgeApp()
    : m_signalGenerate(false)
    , m_signalDump(false)
    , m_signalSearch(false)
    , m_signalSend(false)
    , m_ipv4(true)
    , m_ipv6(true)
    , m_dhtPort(Config::DHT_PORT)
{
}

//*****************************************************************************
//*****************************************************************************
XBridgeApp::~XBridgeApp()
{
    stop();

#ifdef WIN32
    WSACleanup();
#endif
}

//*****************************************************************************
//*****************************************************************************
// static
XBridgeApp & XBridgeApp::instance()
{
    static XBridgeApp app;
    return app;
}

//*****************************************************************************
//*****************************************************************************
// static
std::string XBridgeApp::version()
{
    std::ostringstream o;
    o << XBRIDGE_VERSION_MAJOR
      << "." << XBRIDGE_VERSION_MINOR
      << "." << XBRIDGE_VERSION_DESCR
      << " [" << XBRIDGE_VERSION << "]";
    return o.str();
}

//*****************************************************************************
//*****************************************************************************
// static
bool XBridgeApp::isEnabled()
{
    // enabled by default
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeApp::start()
{
    m_serviceSession.reset(new XBridgeSession);

    // start xbrige
    m_bridge = XBridgePtr(new XBridge());

    return true;
}

//*****************************************************************************
//*****************************************************************************
const unsigned char hash[20] =
{
    0x54, 0x57, 0x87, 0x89, 0xdf, 0xc4, 0x23, 0xee, 0xf6, 0x03,
    0x1f, 0x81, 0x94, 0xa9, 0x3a, 0x16, 0x98, 0x8b, 0x72, 0x7b
};

//*****************************************************************************
//*****************************************************************************
bool XBridgeApp::init(int argc, char *argv[])
{
    // init xbridge settings
    Settings & s = settings();
    {
        std::string path(GetDataDir(false).string());
        path += "/xbridge.conf";
        s.read(path.c_str());
        s.parseCmdLine(argc, argv);
        LOG() << "Finished loading config" << path;
    }

    // init secp256
    xbridge::ECC_Start();

    // init exchange
    XBridgeExchange & e = XBridgeExchange::instance();
    e.init();

    return true;
}

//*****************************************************************************
//*****************************************************************************
//bool XBridgeApp::initDht()
//{
//    LOG() << "initialize v." << version();

//#ifdef WIN32
//    WSADATA wsa = {0};
//    int rc = WSAStartup(MAKEWORD(2, 2), &wsa);
//    if (rc != 0)
//    {
//        LOG() << "startup error";
//        return false;
//    }
//#endif

//    Settings & s = settings();
//    m_dhtPort    = s.dhtPort();

//    std::vector<std::string> peers = s.peers();
//    for (std::vector<std::string>::iterator i = peers.begin(); i != peers.end(); ++i)
//    {
//        std::string peer = *i;
//        std::string port = boost::lexical_cast<std::string>(Config::DHT_PORT);

//        size_t idx = peer.find(':');
//        if (idx != std::string::npos)
//        {
//            port = peer.substr(idx+1);
//            peer = peer.substr(0, idx);
//        }

//        LOG() << "peer -> " << peer << ":" << port;

//        addrinfo   hints;
//        memset(&hints, 0, sizeof(hints));
//        hints.ai_socktype = SOCK_DGRAM;
//        hints.ai_family   = !m_ipv6 ? AF_INET :
//                            !m_ipv4 ? AF_INET6 : 0;

//        addrinfo * info = 0;
//        int rc = getaddrinfo(peer.c_str(), port.c_str(), &hints, &info);
//        if (rc != 0)
//        {
//            LOG() << "getaddrinfo failed " << rc << gai_strerror(rc);
//            continue;
//        }

//        addrinfo * infop = info;
//        while(infop)
//        {
//            sockaddr_storage tmp;
//            memcpy(&tmp, infop->ai_addr, infop->ai_addrlen);
//            m_nodes.push_back(tmp);
//            infop = infop->ai_next;
//        }
//        freeaddrinfo(info);
//    }

//    // start xbrige
//    m_bridge = XBridgePtr(new XBridge());

//    // start dht
//    memset(&m_sin, 0, sizeof(m_sin));
//    m_sin.sin_family = AF_INET;
//    m_sin.sin_port = htons(static_cast<unsigned short>(m_dhtPort));

//    memset(&m_sin6, 0, sizeof(m_sin6));
//    m_sin6.sin6_family = AF_INET6;
//    m_sin6.sin6_port = htons(static_cast<unsigned short>(m_dhtPort));

////    dht_debug = true;

//    // start dht thread
//    m_dhtStarted = false;
//    m_dhtStop    = false;

//    // m_threads.create_thread(boost::bind(&XBridgeApp::dhtThreadProc, this));
//    // m_threads.create_thread(boost::bind(&XBridgeApp::bridgeThreadProc, this));

//    return true;
//}

//*****************************************************************************
//*****************************************************************************
bool XBridgeApp::stop()
{
    LOG() << "stopping threads...";
    m_dhtStop = true;
    m_rpcStop = true;

    m_bridge->stop();

    m_threads.join_all();

    // secp stop
    xbridge::ECC_Stop();

    return true;
}

//*****************************************************************************
//*****************************************************************************
//bool XBridgeApp::initRpc()
//{
//    Settings & s = settings();
//    if (!s.rpcEnabled())
//    {
//        return true;
//    }

//    assert(!"rpc not tested");
//    return true;

//    m_rpcStop = false;

//    m_threads.create_thread(boost::bind(&XBridgeApp::rpcThreadProc, this));
//    return true;
//}

//*****************************************************************************
//*****************************************************************************
bool XBridgeApp::signalRpcStopActive() const
{
    return m_rpcStop;
}

//*****************************************************************************
//*****************************************************************************
//void XBridgeApp::logMessage(const QString & msg)
//{
//    emit showLogMessage(msg);
//}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::onSend(const XBridgePacketPtr & packet)
{
    static UcharVector addr(20, 0);
    UcharVector v(packet->header(), packet->header()+packet->allSize());
    onSend(addr, v);
}

//*****************************************************************************
// send packet to xbridge network to specified id,
// or broadcast, when id is empty
//*****************************************************************************
void XBridgeApp::onSend(const UcharVector & id, const UcharVector & message)
{
    UcharVector msg(id);
    if (msg.size() != 20)
    {
        assert(!"bad address");
        ERR() << "bad send address " << __FUNCTION__;
        return;
    }

    // timestamp
    uint64_t timestamp = std::time(0);
    unsigned char * ptr = reinterpret_cast<unsigned char *>(&timestamp);
    msg.insert(msg.end(), ptr, ptr + sizeof(uint64_t));

    // body
    msg.insert(msg.end(), message.begin(), message.end());

    uint256 hash = Hash(msg.begin(), msg.end());

    LOCK(cs_vNodes);
    for  (CNode * pnode : vNodes)
    {
        if (pnode->setKnown.insert(hash).second)
        {
            pnode->PushMessage("xbridge", msg);
        }
    }
}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::onSend(const UcharVector & id, const XBridgePacketPtr & packet)
{
    UcharVector v;
    std::copy(packet->header(), packet->header()+packet->allSize(), std::back_inserter(v));
    onSend(id, v);
}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::onMessageReceived(const UcharVector & id, const UcharVector & message)
{
    if (isKnownMessage(message))
    {
        return;
    }

    addToKnown(message);

    static UcharVector localid(m_myid, m_myid+20);

    XBridgePacketPtr packet(new XBridgePacket);
    if (!packet->copyFrom(message))
    {
        LOG() << "incorrect packet received";
        return;
    }

    LOG() << "received message to " << util::base64_encode(std::string((char *)&id[0], 20)).c_str()
             << " command " << packet->command();

    if (!XBridgeSession::checkXBridgePacketVersion(packet))
    {
        ERR() << "incorrect protocol version <" << packet->version() << "> " << __FUNCTION__;
        return;
    }

    XBridgeSessionPtr ptr;

    {
        boost::mutex::scoped_lock l(m_sessionsLock);
        if (m_sessionAddrs.count(id))
        {
            // found local client
            ptr = m_sessionAddrs[id];
        }

        // check service session
        else if (m_serviceSession->sessionAddr() == id)
        {
            ptr = serviceSession();
        }

        // check local address
        else if (id == localid)
        {
            // process packet
            // XBridgeSessionPtr ptr(new XBridgeSession);
            ptr = serviceSession();
        }

        else
        {
            // LOG() << "process message for unknown address";
        }
    }

    if (ptr)
    {
        ptr->processPacket(packet);
    }
}

//*****************************************************************************
//*****************************************************************************
XBridgeSessionPtr XBridgeApp::serviceSession()
{
    return m_serviceSession;
}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::onBroadcastReceived(const std::vector<unsigned char> & message)
{
    if (isKnownMessage(message))
    {
        return;
    }

    addToKnown(message);

    // process message
    XBridgePacketPtr packet(new XBridgePacket);
    if (!packet->copyFrom(message))
    {
        LOG() << "incorrect broadcast packet received";
        return;
    }

    LOG() << "broadcast message, command " << packet->command();

    if (!XBridgeSession::checkXBridgePacketVersion(packet))
    {
        ERR() << "incorrect protocol version <" << packet->version() << "> " << __FUNCTION__;
        return;
    }

    // XBridgeSessionPtr ptr(new XBridgeSession);
    serviceSession()->processPacket(packet);
}

//*****************************************************************************
//*****************************************************************************
// static
void XBridgeApp::sleep(const unsigned int umilliseconds)
{
    boost::this_thread::sleep_for(boost::chrono::milliseconds(umilliseconds));
}

#include <stdio.h>

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::bridgeThreadProc()
{
    m_bridge->run();
}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::rpcThreadProc()
{
    assert(!"rpc server");
    // rpc::threadRPCServer();
}

//*****************************************************************************
//*****************************************************************************
XBridgeSessionPtr XBridgeApp::sessionByCurrency(const std::string & currency) const
{
    boost::mutex::scoped_lock l(m_sessionsLock);
    if (m_sessionIds.count(currency))
    {
        return m_sessionIds.at(currency);
    }

    return XBridgeSessionPtr();
}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::addSession(XBridgeSessionPtr session)
{
    storageStore(session, session->sessionAddr());

    boost::mutex::scoped_lock l(m_sessionsLock);
    m_sessionQueue.push(session);
}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::storageStore(XBridgeSessionPtr session, const std::vector<unsigned char> & id)
{
    // TODO :)
    // if (m_sessionAddrs.contains(id))

    boost::mutex::scoped_lock l(m_sessionsLock);
    m_sessionAddrs[id] = session;
    m_sessionIds[session->currency()] = session;
}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::storageClean(XBridgeSessionPtr session)
{
    boost::mutex::scoped_lock l(m_sessionsLock);
    for (auto i = m_sessionAddrs.begin(); i != m_sessionAddrs.end();)
    {
        if (i->second == session)
        {
            m_sessionAddrs.erase(i++);
        }
        else
        {
            ++i;
        }
    }
    for (auto i = m_sessionIds.begin(); i != m_sessionIds.end();)
    {
        if (i->second == session)
        {
            m_sessionIds.erase(i++);
        }
        else
        {
            ++i;
        }
    }
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeApp::isLocalAddress(const std::vector<unsigned char> & id)
{
    static UcharVector localid(m_myid, m_myid+20);

    boost::mutex::scoped_lock l(m_sessionsLock);
    if (m_sessionAddrs.count(id))
    {
        return true;
    }

    // check service session address
    else if (m_serviceSession->sessionAddr() == id)
    {
        return true;
    }

    // check local address
    else if (id == localid)
    {
        // process packet
        return true;
    }

    return false;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeApp::isKnownMessage(const std::vector<unsigned char> & message)
{
    boost::mutex::scoped_lock l(m_messagesLock);
    return m_processedMessages.count(Hash(message.begin(), message.end())) > 0;
}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::addToKnown(const std::vector<unsigned char> & message)
{
    // add to known
    boost::mutex::scoped_lock l(m_messagesLock);
    m_processedMessages.insert(Hash(message.begin(), message.end()));
}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::storeAddressBookEntry(const std::string & currency,
                                       const std::string & name,
                                       const std::string & address)
{
    // TODO fix this, potentially deadlock
    // boost::mutex::try_lock l(m_addressBookLock);
    // boost::mutex::scoped_lock l(m_addressBookLock);
    // if (l.lock())
    {
        if (!m_addresses.count(address))
        {
            m_addresses.insert(address);
            m_addressBook.push_back(std::make_tuple(currency, name, address));
        }
    }
}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::resendAddressBook()
{
    boost::mutex::scoped_lock l(m_addressBookLock);

    for (SessionIdMap::iterator i = m_sessionIds.begin(); i != m_sessionIds.end(); ++i)
    {
        for (AddressBook::iterator ii = m_addressBook.begin(); ii != m_addressBook.end(); ++ii)
        {
            i->second->sendAddressbookEntry(std::get<0>(*ii), std::get<1>(*ii), std::get<2>(*ii));
        }
    }
}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::getAddressBook()
{
    boost::mutex::scoped_lock l(m_addressBookLock);

    for (SessionIdMap::iterator i = m_sessionIds.begin(); i != m_sessionIds.end(); ++i)
    {
        i->second->requestAddressBook();
    }
}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::checkUnconfirmedTx()
{
    boost::mutex::scoped_lock l(m_addressBookLock);

    for (SessionIdMap::iterator i = m_sessionIds.begin(); i != m_sessionIds.end(); ++i)
    {
        i->second->requestUnconfirmedTx();
    }
}

//******************************************************************************
//******************************************************************************
uint256 XBridgeApp::sendXBridgeTransaction(const std::string & from,
                                           const std::string & fromCurrency,
                                           const uint64_t & fromAmount,
                                           const std::string & to,
                                           const std::string & toCurrency,
                                           const uint64_t & toAmount)
{
    if (fromCurrency.size() > 8 || toCurrency.size() > 8)
    {
        assert(false && "invalid currency");
        return uint256();
    }

    // check amount
    XBridgeSessionPtr s = sessionByCurrency(fromCurrency);
    if (!s)
    {
        // no session
        uiInterface.ThreadSafeMessageBox(_("No session for ") + fromCurrency,
                                         "blocknet",
                                         CClientUIInterface::BTN_OK | CClientUIInterface::ICON_INFORMATION | CClientUIInterface::MODAL);

        WARN() << "no session for <" << fromCurrency << "> " << __FUNCTION__;
        return uint256();
    }

    if (!s->checkAmount(fromAmount))
    {
        uiInterface.ThreadSafeMessageBox(_("Insufficient funds for ") + fromCurrency,
                                         "blocknet",
                                         CClientUIInterface::BTN_OK | CClientUIInterface::ICON_INFORMATION | CClientUIInterface::MODAL);

        LOG() << "insufficient funds for <" << fromCurrency << "> " << __FUNCTION__;
        return uint256();
    }

    boost::uint32_t timestamp = time(0);
    uint256 id = Hash(from.begin(), from.end(),
                      fromCurrency.begin(), fromCurrency.end(),
                      BEGIN(fromAmount), END(fromAmount),
                      to.begin(), to.end(),
                      toCurrency.begin(), toCurrency.end(),
                      BEGIN(toAmount), END(toAmount),
                      BEGIN(timestamp), END(timestamp));

    XBridgeTransactionDescrPtr ptr(new XBridgeTransactionDescr);
    ptr->id           = id;
    ptr->from         = from;
    ptr->fromCurrency = fromCurrency;
    ptr->fromAmount   = fromAmount;
    ptr->to           = to;
    ptr->toCurrency   = toCurrency;
    ptr->toAmount     = toAmount;

    {
        boost::mutex::scoped_lock l(m_txLocker);
        m_pendingTransactions[id] = ptr;
    }

    // try send immediatelly
    sendPendingTransaction(ptr);

    return id;
}

//******************************************************************************
//******************************************************************************
bool XBridgeApp::sendPendingTransaction(XBridgeTransactionDescrPtr & ptr)
{
    // if (!ptr->packet)
    {
        if (ptr->from.size() == 0 || ptr->to.size() == 0)
        {
            // TODO temporary
            return false;
        }

        if (ptr->packet && ptr->packet->command() != xbcTransaction)
        {
            // not send pending packets if not an xbcTransaction
            return true;
        }

        ptr->packet.reset(new XBridgePacket(xbcTransaction));

        // field length must be 8 bytes
        std::vector<unsigned char> fc(8, 0);
        std::copy(ptr->fromCurrency.begin(), ptr->fromCurrency.end(), fc.begin());

        // field length must be 8 bytes
        std::vector<unsigned char> tc(8, 0);
        std::copy(ptr->toCurrency.begin(), ptr->toCurrency.end(), tc.begin());

        // 32 bytes - id of transaction
        // 2x
        // 34 bytes - address
        //  8 bytes - currency
        //  8 bytes - amount
        ptr->packet->append(ptr->id.begin(), 32);
        ptr->packet->append(ptr->from);
        ptr->packet->append(fc);
        ptr->packet->append(ptr->fromAmount);
        ptr->packet->append(ptr->to);
        ptr->packet->append(tc);
        ptr->packet->append(ptr->toAmount);
    }

    // onSend(std::vector<unsigned char>(m_myid, m_myid+20), ptr->packet);
    onSend(ptr->packet);

    ptr->state = XBridgeTransactionDescr::trPending;

    xuiConnector.NotifyXBridgeTransactionStateChanged(ptr->id, XBridgeTransactionDescr::trPending);

    return true;
}

//******************************************************************************
//******************************************************************************
uint256 XBridgeApp::acceptXBridgeTransaction(const uint256 & id,
                                             const std::string & from,
                                             const std::string & to)
{
    XBridgeTransactionDescrPtr ptr;

    {
        boost::mutex::scoped_lock l(m_txLocker);
        if (!m_pendingTransactions.count(id))
        {
            uiInterface.ThreadSafeMessageBox(_("Transaction not foud"),
                                             "blocknet",
                                             CClientUIInterface::BTN_OK | CClientUIInterface::ICON_INFORMATION | CClientUIInterface::MODAL);
            return uint256();
        }
        ptr = m_pendingTransactions[id];
    }

    // check amount
    XBridgeSessionPtr s = sessionByCurrency(ptr->toCurrency);
    if (!s)
    {
        // no session
        uiInterface.ThreadSafeMessageBox(_("No session for ") + ptr->toCurrency,
                                         "blocknet",
                                         CClientUIInterface::BTN_OK | CClientUIInterface::ICON_INFORMATION | CClientUIInterface::MODAL);

        WARN() << "no session for <" << ptr->toCurrency << "> " << __FUNCTION__;
        return uint256();
    }

    if (!s->checkAmount(ptr->toAmount))
    {
        uiInterface.ThreadSafeMessageBox(_("Insufficient funds for ") + ptr->toCurrency,
                                         "blocknet",
                                         CClientUIInterface::BTN_OK | CClientUIInterface::ICON_INFORMATION | CClientUIInterface::MODAL);

        LOG() << "insufficient funds for <" << ptr->toCurrency << "> " << __FUNCTION__;
        return uint256();
    }

    ptr->from = from;
    ptr->to   = to;
    std::swap(ptr->fromCurrency, ptr->toCurrency);
    std::swap(ptr->fromAmount,   ptr->toAmount);

    // try send immediatelly
    sendAcceptingTransaction(ptr);

    return id;
}

//******************************************************************************
//******************************************************************************
bool XBridgeApp::sendAcceptingTransaction(XBridgeTransactionDescrPtr & ptr)
{
    ptr->packet.reset(new XBridgePacket(xbcTransactionAccepting));

    // field length must be 8 bytes
    std::vector<unsigned char> fc(8, 0);
    std::copy(ptr->fromCurrency.begin(), ptr->fromCurrency.end(), fc.begin());

    // field length must be 8 bytes
    std::vector<unsigned char> tc(8, 0);
    std::copy(ptr->toCurrency.begin(), ptr->toCurrency.end(), tc.begin());

    // std::vector<unsigned char> thisAddress(m_myid, m_myid+20);

    // 20 bytes - id of transaction
    // 2x
    // 34 bytes - address
    //  8 bytes - currency
    //  4 bytes - amount
    ptr->packet->append(ptr->hubAddress);
    ptr->packet->append(ptr->id.begin(), 32);
    ptr->packet->append(ptr->from);
    ptr->packet->append(fc);
    ptr->packet->append(ptr->fromAmount);
    ptr->packet->append(ptr->to);
    ptr->packet->append(tc);
    ptr->packet->append(ptr->toAmount);

    onSend(ptr->hubAddress, ptr->packet);

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeApp::cancelXBridgeTransaction(const uint256 & id,
                                          const TxCancelReason & reason)
{
    {
        boost::mutex::scoped_lock l(m_txLocker);

        m_pendingTransactions.erase(id);
        if (m_transactions.count(id))
        {
            m_transactions[id]->state = XBridgeTransactionDescr::trCancelled;
        }
    }

    return sendCancelTransaction(id, reason);
}

//******************************************************************************
//******************************************************************************
bool XBridgeApp::rollbackXBridgeTransaction(const uint256 & id)
{
    XBridgeSessionPtr session;
    {
        boost::mutex::scoped_lock l(m_txLocker);

        if (m_transactions.count(id))
        {
            XBridgeTransactionDescrPtr ptr = m_transactions[id];
            if (!ptr->refTx.empty())
            {
                session = sessionByCurrency(ptr->fromCurrency);
                if (!session)
                {
                    ERR() << "unknown session for currency " << ptr->fromCurrency;
                }
            }
        }
    }

    if (session)
    {
        // session use m_txLocker, must be unlocked because not recursive
        if (!session->revertXBridgeTransaction(id))
        {
            LOG() << "revert tx failed for " << id.ToString();
            return false;
        }
    }

    return sendRollbackTransaction(id);
}

//******************************************************************************
//******************************************************************************
bool XBridgeApp::sendCancelTransaction(const uint256 & txid,
                                       const TxCancelReason & reason)
{
    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionCancel));
    reply->append(txid.begin(), 32);
    reply->append(static_cast<uint32_t>(reason));

    static UcharVector addr(20, 0);
    onSend(addr, reply);

    // cancelled
    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeApp::sendRollbackTransaction(const uint256 & txid)
{
    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionRollback));
    reply->append(txid.begin(), 32);

    static UcharVector addr(20, 0);
    onSend(addr, reply);

    // rolled back
    return true;
}

//******************************************************************************
//******************************************************************************
void XBridgeApp::handleRpcRequest(rpc::AcceptedConnection * /*conn*/)
{
    // m_threads.create_thread(boost::bind(&XBridgeApp::rpcHandlerProc, this, conn));
}

