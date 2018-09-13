//*****************************************************************************
//*****************************************************************************

#include "xbridgeapp.h"
#include "xbridgeexchange.h"
#include "util/xutil.h"
#include "util/logger.h"
#include "util/settings.h"
#include "util/xbridgeerror.h"
#include "util/xassert.h"
#include "version.h"
#include "config.h"
#include "xuiconnector.h"
#include "rpcserver.h"
#include "net.h"
#include "util.h"
#include "ui_interface.h"
#include "init.h"
#include "wallet.h"
#include "servicenodeman.h"
#include "activeservicenode.h"
#include "xbridgewalletconnector.h"
#include "xbridgewalletconnectorbtc.h"
#include "xbridgecryptoproviderbtc.h"
#include "xbridgewalletconnectorbch.h"
#include "xbridgewalletconnectordgb.h"

#include <assert.h>
#include <numeric>

#include <boost/chrono/chrono.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/algorithm/string/join.hpp>

#include <openssl/rand.h>
#include <openssl/md5.h>

#include "posixtimeconversion.h"

using TransactionMap    = std::map<uint256, xbridge::TransactionDescrPtr>;
using TransactionPair   = std::pair<uint256, xbridge::TransactionDescrPtr>;
namespace bpt = boost::posix_time;

//*****************************************************************************
//*****************************************************************************
XUIConnector xuiConnector;

//*****************************************************************************
//*****************************************************************************
namespace xbridge
{

//*****************************************************************************
//*****************************************************************************
void badaboom()
{
    int * a = 0;
    *a = 0;
}

//*****************************************************************************
//*****************************************************************************
class App::Impl
{
    friend class App;

    enum
    {
        TIMER_INTERVAL = 15
    };

protected:
    /**
     * @brief Impl - default constructor, init
     * services and timer
     */
    Impl();

    /**
     * @brief start - run sessions, threads and services
     * @return true, if run succesfull
     */
    bool start();
    /**
     * @brief stop stopped service, timer, secp stop
     * @return true
     */
    bool stop();

protected:
    /**
     * @brief onSend  send packet to xbridge network to specified id,
     *  or broadcast, when id is empty
     * @param id
     * @param message
     */
    void onSend(const std::vector<unsigned char> & id, const std::vector<unsigned char> & message);

    /**
     * @brief onTimer call check expired transactions,
     * send transactions list, erase expired transactions,
     * get addressbook,
     */
    void onTimer();

    /**
     * @brief getSession - move session to head of queue
     * @return pointer to head of sessions queue
     */
    SessionPtr getSession();
    /**
     * @brief getSession
     * @param address - session address
     * @return pointer to exists session if found, else new instance
     */
    SessionPtr getSession(const std::vector<unsigned char> & address);

protected:
    /**
     * @brief sendPendingTransaction - check transaction data,
     * make packet with data and send to network
     * @param ptr - pointer to transaction
     * @return  true, if all date  correctly and packet has send to network
     */
    bool sendPendingTransaction(const TransactionDescrPtr & ptr);
    /**
     * @brief sendAcceptingTransaction - check transaction date,
     * make new packet and - sent packet with cancelled command
     * to network, update transaction state, notify ui about trabsaction state changed
     * @param ptr - pointer to transaction
     * @return
     */
    bool sendAcceptingTransaction(const TransactionDescrPtr & ptr);
    /**
     * @brief sendCancelTransaction  - sent packet with cancelled command
     * to network, update transaction state, notify ui about trabsaction state changed
     * @param txid - id of transaction
     * @param reason - cancel reason
     * @return
     */
    bool sendCancelTransaction(const uint256 & txid, const TxCancelReason & reason);

    bool addNodeServices(const ::CPubKey & node, const std::vector<std::string> & services);
    bool hasNodeService(const ::CPubKey & node, const std::string & service);

    bool findNodeWithService(const std::set<string> & services, CPubKey & node) const;

protected:
    // workers
    std::deque<IoServicePtr>                           m_services;
    std::deque<WorkPtr>                                m_works;
    boost::thread_group                                m_threads;

    // timer
    boost::asio::io_service                            m_timerIo;
    std::shared_ptr<boost::asio::io_service::work>     m_timerIoWork;
    boost::thread                                      m_timerThread;
    boost::asio::deadline_timer                        m_timer;

    // sessions
    mutable boost::mutex                               m_sessionsLock;
    SessionQueue                                       m_sessions;
    SessionsAddrMap                                    m_sessionAddressMap;

    // connectors
    mutable boost::mutex                               m_connectorsLock;
    Connectors                                         m_connectors;
    ConnectorsAddrMap                                  m_connectorAddressMap;
    ConnectorsCurrencyMap                              m_connectorCurrencyMap;

    // pending messages (packet processing loop)
    boost::mutex                                       m_messagesLock;
    typedef std::set<uint256> ProcessedMessages;
    ProcessedMessages                                  m_processedMessages;

    // address book
    boost::mutex                                       m_addressBookLock;
    AddressBook                                        m_addressBook;
    std::set<std::string>                              m_addresses;

    // transactions
    boost::mutex                                       m_txLocker;
    std::map<uint256, TransactionDescrPtr>             m_transactions;
    std::map<uint256, TransactionDescrPtr>             m_historicTransactions;

    // network packets queue
    boost::mutex                                       m_ppLocker;
    std::map<uint256, XBridgePacketPtr>                m_pendingPackets;

    // services and xwallets
    mutable boost::mutex                               m_xwalletsLocker;
    std::map<::CPubKey, std::set<std::string> >        m_xwallets;
};

//*****************************************************************************
//*****************************************************************************
App::Impl::Impl()
    : m_timerIoWork(new boost::asio::io_service::work(m_timerIo))
    , m_timerThread(boost::bind(&boost::asio::io_service::run, &m_timerIo))
    , m_timer(m_timerIo, boost::posix_time::seconds(TIMER_INTERVAL))
{

}

//*****************************************************************************
//*****************************************************************************
App::App()
    : m_p(new Impl)
{
}

//*****************************************************************************
//*****************************************************************************
App::~App()
{
    stop();

#ifdef WIN32
    WSACleanup();
#endif
}

//*****************************************************************************
//*****************************************************************************
// static
App & App::instance()
{
    static App app;
    return app;
}

//*****************************************************************************
//*****************************************************************************
// static
std::string App::version()
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
bool App::isEnabled()
{
    // enabled by default
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool App::start()
{
    return m_p->start();
}

//*****************************************************************************
//*****************************************************************************
bool App::Impl::start()
{
    // start xbrige
    try
    {
        // services and thredas
        for (size_t i = 0; i < boost::thread::hardware_concurrency(); ++i)
        {
            IoServicePtr ios(new boost::asio::io_service);

            m_services.push_back(ios);
            m_works.push_back(WorkPtr(new boost::asio::io_service::work(*ios)));

            m_threads.create_thread(boost::bind(&boost::asio::io_service::run, ios));
        }

        m_timer.async_wait(boost::bind(&Impl::onTimer, this));

        // sessions
        xbridge::App & app = xbridge::App::instance();
        {
            Settings & s = settings();
            std::vector<std::string> wallets = s.exchangeWallets();
            for (std::vector<std::string>::iterator i = wallets.begin(); i != wallets.end(); ++i)
            {
                WalletParam wp;
                wp.currency                    = *i;
                wp.title                       = s.get<std::string>(*i + ".Title");
                wp.address                     = s.get<std::string>(*i + ".Address");
                wp.m_ip                        = s.get<std::string>(*i + ".Ip");
                wp.m_port                      = s.get<std::string>(*i + ".Port");
                wp.m_user                      = s.get<std::string>(*i + ".Username");
                wp.m_passwd                    = s.get<std::string>(*i + ".Password");
                wp.addrPrefix                  = s.get<std::string>(*i + ".AddressPrefix");
                wp.scriptPrefix                = s.get<std::string>(*i + ".ScriptPrefix");
                wp.secretPrefix                = s.get<std::string>(*i + ".SecretPrefix");
                wp.COIN                        = s.get<uint64_t>   (*i + ".COIN", 0);
                wp.txVersion                   = s.get<uint32_t>   (*i + ".TxVersion", 1);
                wp.minTxFee                    = s.get<uint64_t>   (*i + ".MinTxFee", 0);
                wp.method                      = s.get<std::string>(*i + ".CreateTxMethod");
                wp.blockTime                   = s.get<int>        (*i + ".BlockTime", 0);
                wp.requiredConfirmations       = s.get<int>        (*i + ".Confirmations", 0);
                wp.txWithTimeField             = s.get<bool>       (*i + ".TxWithTimeField", false);
                wp.isLockCoinsSupported        = s.get<bool>       (*i + ".LockCoinsSupported", false);

                if (wp.m_ip.empty() || wp.m_port.empty() ||
                    wp.m_user.empty() || wp.m_passwd.empty() ||
                    wp.COIN == 0 || wp.blockTime == 0)
                {
                    LOG() << "read wallet " << *i << " with empty parameters>";
                    continue;
                }

                LOG() << "read wallet " << *i << " [" << wp.title << "] " << wp.m_ip
                      << ":" << wp.m_port; // << " COIN=" << wp.COIN;

                xbridge::WalletConnectorPtr conn;
                if (wp.method == "ETHER")
                {
                    LOG() << "wp.method ETHER not implemented" << __FUNCTION__;
                    // session.reset(new XBridgeSessionEthereum(wp));
                }
                else if (wp.method == "BTC" || wp.method == "SYS")
                {
                    conn.reset(new BtcWalletConnector<BtcCryptoProvider>);
                    *conn = wp;
                }
                else if (wp.method == "BCH")
                {
                    conn.reset(new BchWalletConnector);
                    *conn = wp;
                }
                else if (wp.method == "DGB")
                {
                    conn.reset(new DgbWalletConnector);
                    *conn = wp;
                }
                else
                {
                    ERR() << "unknown session type " << __FUNCTION__;
                }
                if (!conn)
                {
                    continue;
                }

                if (!conn->init())
                {
                    ERR() << "connection not initialized " << *i << " " << __FUNCTION__;
                    continue;
                }

                app.addConnector(conn);
            }
        }
    }
    catch (std::exception & e)
    {
        ERR() << e.what();
        ERR() << __FUNCTION__;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool App::init(int argc, char *argv[])
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

    // init exchange
    Exchange & e = Exchange::instance();
    e.init();

    // sessions
    {
        boost::mutex::scoped_lock l(m_p->m_sessionsLock);

        for (uint32_t i = 0; i < boost::thread::hardware_concurrency(); ++i)
        {
            SessionPtr ptr(new Session());
            m_p->m_sessions.push(ptr);
            m_p->m_sessionAddressMap[ptr->sessionAddr()] = ptr;
        }
    }

    return true;

}

//*****************************************************************************
//*****************************************************************************
bool App::stop()
{
    return m_p->stop();
}

//*****************************************************************************
//*****************************************************************************
bool App::Impl::stop()
{
    LOG() << "stopping threads...";

    m_timer.cancel();
    m_timerIo.stop();
    m_timerIoWork.reset();
    m_timerThread.join();

//    for (IoServicePtr & i : m_services)
//    {
//        i->stop();
//    }
    for (WorkPtr & i : m_works)
    {
        i.reset();
    }

    m_threads.join_all();

    return true;
}

//*****************************************************************************
//*****************************************************************************
void App::sendPacket(const XBridgePacketPtr & packet)
{
    static std::vector<unsigned char> addr(20, 0);
    m_p->onSend(addr, packet->body());
}

//*****************************************************************************
// send packet to xbridge network to specified id,
// or broadcast, when id is empty
//*****************************************************************************
void App::Impl::onSend(const std::vector<unsigned char> & id, const std::vector<unsigned char> & message)
{
    std::vector<unsigned char> msg(id);
    if (msg.size() != 20)
    {
        ERR() << "bad send address " << __FUNCTION__;
        return;
    }

    // timestamp
    boost::posix_time::ptime timestamp = boost::posix_time::microsec_clock::universal_time();
    uint64_t timestampValue = util::timeToInt(timestamp);
    unsigned char * ptr = reinterpret_cast<unsigned char *>(&timestampValue);
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
void App::sendPacket(const std::vector<unsigned char> & id, const XBridgePacketPtr & packet)
{
    m_p->onSend(id, packet->body());
}

//*****************************************************************************
//*****************************************************************************
SessionPtr App::Impl::getSession()
{
    SessionPtr ptr;

    boost::mutex::scoped_lock l(m_sessionsLock);

    ptr = m_sessions.front();
    m_sessions.pop();
    m_sessions.push(ptr);

    if(ptr->isWorking())
    {
        ptr = SessionPtr(new Session());
        m_sessions.push(ptr);
        m_sessionAddressMap[ptr->sessionAddr()] = ptr;
    }

    return ptr;
}

//*****************************************************************************
//*****************************************************************************
SessionPtr App::Impl::getSession(const std::vector<unsigned char> & address)
{
    boost::mutex::scoped_lock l(m_sessionsLock);
    if (m_sessionAddressMap.count(address))
    {
        return m_sessionAddressMap[address];
    }

    return SessionPtr();
}

//*****************************************************************************
//*****************************************************************************
void App::onMessageReceived(const std::vector<unsigned char> & id,
                            const std::vector<unsigned char> & message,
                            CValidationState & /*state*/)
{
    if (isKnownMessage(message))
    {
        return;
    }

    addToKnown(message);

    if (!Session::checkXBridgePacketVersion(message))
    {
        // TODO state.DoS()
        return;
    }

    XBridgePacketPtr packet(new XBridgePacket);
    if (!packet->copyFrom(message))
    {
        LOG() << "incorrect packet received " << __FUNCTION__;
        return;
    }

    if (!packet->verify())
    {
        LOG() << "unsigned packet or signature error " << __FUNCTION__;
        return;
    }

    LOG() << "received message to " << HexStr(id)
          << " command " << packet->command();

    // check direct session address
    SessionPtr ptr = m_p->getSession(id);
    if (ptr)
    {
        // TODO use post or future
        ptr->processPacket(packet);
        return;
    }
    else
    {
        {
            // if no session address - find connector address
            boost::mutex::scoped_lock l(m_p->m_connectorsLock);
            if (m_p->m_connectorAddressMap.count(id))
            {
                WalletConnectorPtr conn = m_p->m_connectorAddressMap.at(id);

                LOG() << "handling message with connector currency: "
                      << conn->currency
                      << " and address: "
                      << conn->fromXAddr(id);

                ptr = m_p->getSession();
            }
        }

        if (ptr)
        {
            // TODO use post or future
            ptr->processPacket(packet);
            return;
        }

    }

    // If Servicenode w/ exchange, process packet
    Exchange & e = Exchange::instance();
    if (e.isStarted())
    {
        SessionPtr ptr = m_p->getSession();
        if (ptr)
        {
            ptr->processPacket(packet);
        }
    }
}

//*****************************************************************************
//*****************************************************************************
void App::onBroadcastReceived(const std::vector<unsigned char> & message,
                              CValidationState & state)
{
    if (isKnownMessage(message))
    {
        return;
    }

    addToKnown(message);

    if (!Session::checkXBridgePacketVersion(message))
    {
        // TODO state.DoS()
        return;
    }

    // process message
    XBridgePacketPtr packet(new XBridgePacket);
    if (!packet->copyFrom(message))
    {
        LOG() << "incorrect packet received " << __FUNCTION__;
        return;
    }

    if (!packet->verify())
    {
        LOG() << "unsigned packet or signature error " << __FUNCTION__;
        return;
    }

    LOG() << "broadcast message, command " << packet->command();

    SessionPtr ptr = m_p->getSession();
    if (ptr)
    {
        // TODO use post or future
        ptr->processPacket(packet);
    }
}

//*****************************************************************************
//*****************************************************************************
bool App::processLater(const uint256 & txid, const XBridgePacketPtr & packet)
{
    boost::mutex::scoped_lock l(m_p->m_ppLocker);
    m_p->m_pendingPackets[txid] = packet;
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool App::removePackets(const uint256 & txid)
{
    // remove from pending packets (if added)

    boost::mutex::scoped_lock l(m_p->m_ppLocker);
    size_t removed = m_p->m_pendingPackets.erase(txid);
    if(removed > 1) {
        ERR() << "duplicate packets in packets queue" << __FUNCTION__;
        return false;
    }
//    assert(removed < 2 && "duplicate packets in packets queue");

    return true;
}

//*****************************************************************************
//*****************************************************************************
WalletConnectorPtr App::connectorByCurrency(const std::string & currency) const
{
    boost::mutex::scoped_lock l(m_p->m_connectorsLock);
    if (m_p->m_connectorCurrencyMap.count(currency))
    {
        return m_p->m_connectorCurrencyMap.at(currency);
    }

    return WalletConnectorPtr();
}

//*****************************************************************************
//*****************************************************************************
std::vector<std::string> App::availableCurrencies() const
{
    boost::mutex::scoped_lock l(m_p->m_connectorsLock);

    std::vector<std::string> currencies;

    for(auto i = m_p->m_connectorCurrencyMap.begin(); i != m_p->m_connectorCurrencyMap.end();)
    {
        currencies.push_back(i->first);
        ++i;
    }

    return currencies;
}

//*****************************************************************************
//*****************************************************************************
std::vector<std::string> App::networkCurrencies() const
{
    std::set<string> coins;
    std::vector<CServicenode> snodes = mnodeman.GetFullServicenodeVector();
    // Obtain unique xwallets supported across network
    for (CServicenode &sn : snodes) {
        for (auto &w : sn.connectedWallets) {
            if (!coins.count(w.strWalletName))
                coins.insert(w.strWalletName);
        }
    }
    if (!coins.empty()) {
        std::vector<std::string> result(coins.size());
        std::copy(coins.begin(), coins.end(), std::back_inserter(result));
        return result;
    }
    return std::vector<string>();
}

//*****************************************************************************
//*****************************************************************************
bool App::hasCurrency(const std::string & currency) const
{
    boost::mutex::scoped_lock l(m_p->m_connectorsLock);
    return m_p->m_connectorCurrencyMap.count(currency);
}

//*****************************************************************************
//*****************************************************************************
void App::addConnector(const WalletConnectorPtr & conn)
{
    boost::mutex::scoped_lock l(m_p->m_connectorsLock);
    m_p->m_connectors.push_back(conn);
    m_p->m_connectorCurrencyMap[conn->currency] = conn;
}

//*****************************************************************************
//*****************************************************************************
void App::updateConnector(const WalletConnectorPtr & conn,
                          const std::vector<unsigned char> addr,
                          const std::string & currency)
{
    boost::mutex::scoped_lock l(m_p->m_connectorsLock);

    m_p->m_connectorAddressMap[addr]      = conn;
    m_p->m_connectorCurrencyMap[currency] = conn;
}

//*****************************************************************************
//*****************************************************************************
std::vector<WalletConnectorPtr> App::connectors() const
{
    boost::mutex::scoped_lock l(m_p->m_connectorsLock);
    return m_p->m_connectors;
}

//*****************************************************************************
//*****************************************************************************
bool App::isKnownMessage(const std::vector<unsigned char> & message)
{
    boost::mutex::scoped_lock l(m_p->m_messagesLock);
    return m_p->m_processedMessages.count(Hash(message.begin(), message.end())) > 0;
}

//*****************************************************************************
//*****************************************************************************
void App::addToKnown(const std::vector<unsigned char> & message)
{
    // add to known
    boost::mutex::scoped_lock l(m_p->m_messagesLock);
    m_p->m_processedMessages.insert(Hash(message.begin(), message.end()));
}

//******************************************************************************
//******************************************************************************
TransactionDescrPtr App::transaction(const uint256 & id) const
{
    TransactionDescrPtr result;

    boost::mutex::scoped_lock l(m_p->m_txLocker);

    if (m_p->m_transactions.count(id))
    {
        result = m_p->m_transactions[id];
    }

    if (m_p->m_historicTransactions.count(id))
    {
        if(result != nullptr) {
            ERR() << "duplicate transaction " << __FUNCTION__;
            return result;
        }
//        assert(!result && "duplicate objects");
        result = m_p->m_historicTransactions[id];
    }
    return result;
}

//******************************************************************************
//******************************************************************************
std::map<uint256, xbridge::TransactionDescrPtr> App::transactions() const
{
    boost::mutex::scoped_lock l(m_p->m_txLocker);
    return m_p->m_transactions;
}

//******************************************************************************
//******************************************************************************
std::map<uint256, xbridge::TransactionDescrPtr> App::history() const
{
    boost::mutex::scoped_lock l(m_p->m_txLocker);
    return m_p->m_historicTransactions;
}

//******************************************************************************
//******************************************************************************
std::vector<App::FlushedOrder>
App::flushCancelledOrders(bpt::time_duration minAge) const
{
    std::vector<App::FlushedOrder> list{};
    const bpt::ptime keepTime{bpt::microsec_clock::universal_time() - minAge};
    const std::vector<TransactionMap*> maps{&m_p->m_transactions, &m_p->m_historicTransactions};

    boost::mutex::scoped_lock l{m_p->m_txLocker};

    for(auto mp : maps) {
        for(auto it = mp->begin(); it != mp->end(); ) {
            const TransactionDescrPtr & ptr = it->second;
            if (ptr->state == xbridge::TransactionDescr::trCancelled
                && ptr->txtime < keepTime) {
                list.emplace_back(ptr->id,ptr->txtime,ptr.use_count());
                mp->erase(it++);
            } else {
                ++it;
            }
        }
    }

    return list;
}

//******************************************************************************
//******************************************************************************
void App::appendTransaction(const TransactionDescrPtr & ptr)
{
    boost::mutex::scoped_lock l(m_p->m_txLocker);

    if (m_p->m_historicTransactions.count(ptr->id))
    {
        return;
    }

    if (!m_p->m_transactions.count(ptr->id))
    {
        // new transaction, copy data
        m_p->m_transactions[ptr->id] = ptr;
    }
    else
    {
        // existing, update timestamp
        m_p->m_transactions[ptr->id]->updateTimestamp(*ptr);
    }
}

//******************************************************************************
//******************************************************************************
void App::moveTransactionToHistory(const uint256 & id)
{
    TransactionDescrPtr xtx;

    {
        boost::mutex::scoped_lock l(m_p->m_txLocker);

        size_t counter = 0;

        if (m_p->m_transactions.count(id))
        {
            xtx = m_p->m_transactions[id];

            counter = m_p->m_transactions.erase(id);
            if(counter > 1) {
                ERR() << "duplicate transaction id = " << id.GetHex() << " " << __FUNCTION__;
            }
        }

        if (xtx)
        {
            if(m_p->m_historicTransactions.count(id) != 0) {
                ERR() << "duplicate tx " << id.GetHex() << " in tx list and history " << __FUNCTION__;
                return;
            }
            m_p->m_historicTransactions[id] = xtx;
        }
    }

    if (xtx)
    {
        // unlock tx coins
        WalletConnectorPtr conn = connectorByCurrency(xtx->fromCurrency);
        if (conn)
        {
            conn->lockCoins(xtx->usedCoins, false);
        }
    }

    // remove pending packets for this tx
    removePackets(id);
}

//******************************************************************************
//******************************************************************************
xbridge::Error App::sendXBridgeTransaction(const std::string & from,
                                           const std::string & fromCurrency,
                                           const uint64_t & fromAmount,
                                           const std::string & to,
                                           const std::string & toCurrency,
                                           const uint64_t & toAmount,
                                           uint256 & id,
                                           uint256 & blockHash)
{
    // search for service node
    std::vector<unsigned char> snodeAddress(20);
    std::vector<unsigned char> sPubKey(33);
    {
        std::set<std::string> currencies;
        currencies.insert(fromCurrency);
        currencies.insert(toCurrency);
        CPubKey snode;
        if (!findNodeWithService(currencies, snode))
        {
            return xbridge::Error::NO_SERVICE_NODE;
        }

        CKeyID id = snode.GetID();
        std::copy(id.begin(), id.end(), snodeAddress.begin());

        if (!snode.IsCompressed()) {
            snode.Compress();
        }
        sPubKey = std::vector<unsigned char>(snode.begin(), snode.end());
    }

    const auto statusCode = checkCreateParams(fromCurrency, toCurrency, fromAmount, from);
    if(statusCode != xbridge::SUCCESS)
    {
        return statusCode;
    }

    if (fromCurrency.size() > 8 || toCurrency.size() > 8)
    {
        WARN() << "invalid currency " << __FUNCTION__;
        return xbridge::Error::INVALID_CURRENCY;
    }

    WalletConnectorPtr connFrom = connectorByCurrency(fromCurrency);
    WalletConnectorPtr connTo   = connectorByCurrency(toCurrency);
    if (!connFrom || !connTo)
    {
        // no session
        WARN() << "no session for <" << (connFrom ? toCurrency : fromCurrency) << "> " << __FUNCTION__;
        return xbridge::Error::NO_SESSION;
    }

    if (connFrom->isDustAmount(static_cast<double>(fromAmount) / TransactionDescr::COIN))
    {
        return xbridge::Error::DUST;
    }

    if (connTo->isDustAmount(static_cast<double>(toAmount) / TransactionDescr::COIN))
    {
        return xbridge::Error::DUST;
    }

    uint64_t utxoAmount = 0;
    uint64_t fee1       = 0;
    uint64_t fee2       = 0;

    std::vector<wallet::UtxoEntry> outputs;
    connFrom->getUnspent(outputs);

    // Select utxos
    std::vector<wallet::UtxoEntry> outputsForUse;
    if (!selectUtxos(from, outputs, connFrom, fromAmount, outputsForUse, utxoAmount, fee1, fee2))
    {
        WARN() << "insufficient funds for <" << fromCurrency << "> " << __FUNCTION__;
        return xbridge::Error::INSIFFICIENT_FUNDS;
    }

    LOG() << "fee1: " << (static_cast<double>(fee1) / TransactionDescr::COIN);
    LOG() << "fee2: " << (static_cast<double>(fee2) / TransactionDescr::COIN);
    LOG() << "amount of used utxo items: " << (static_cast<double>(utxoAmount) / TransactionDescr::COIN)
          << " required amount + fees: " << (static_cast<double>(fromAmount + fee1 + fee2) / TransactionDescr::COIN);

    // sign used coins
    for (wallet::UtxoEntry & entry : outputsForUse)
    {
        std::string signature;
        if (!connFrom->signMessage(entry.address, entry.toString(), signature))
        {
            WARN() << "funds not signed <" << fromCurrency << "> " << __FUNCTION__;
            return xbridge::Error::FUNDS_NOT_SIGNED;
        }

        bool isInvalid = false;
        entry.signature = DecodeBase64(signature.c_str(), &isInvalid);
        if (isInvalid)
        {
            WARN() << "invalid signature <" << fromCurrency << "> " << __FUNCTION__;
            return xbridge::Error::FUNDS_NOT_SIGNED;
        }

        entry.rawAddress = connFrom->toXAddr(entry.address);

        if (entry.signature.size() != 65)
        {
            ERR() << "incorrect signature length, need 65 bytes " << __FUNCTION__;
            return xbridge::Error::INVALID_SIGNATURE;
        }
        xassert(entry.signature.size() == 65 && "incorrect signature length, need 20 bytes");
        if (entry.rawAddress.size() != 20)
        {
            ERR() << "incorrect raw address length, need 20 bytes " << __FUNCTION__;
            return  xbridge::Error::INVALID_ADDRESS;
        }
        xassert(entry.rawAddress.size() == 20 && "incorrect raw address length, need 20 bytes");
    }

    boost::posix_time::ptime timestamp = boost::posix_time::microsec_clock::universal_time();
    uint64_t timestampValue = util::timeToInt(timestamp);

    blockHash = chainActive.Tip()->pprev->GetBlockHash();

    std::vector<unsigned char> firstUtxoSig = outputsForUse.at(0).signature;

    id = Hash(from.begin(), from.end(),
              fromCurrency.begin(), fromCurrency.end(),
              BEGIN(fromAmount), END(fromAmount),
              to.begin(), to.end(),
              toCurrency.begin(), toCurrency.end(),
              BEGIN(toAmount), END(toAmount),
              BEGIN(timestampValue), END(timestampValue),
              blockHash.begin(), blockHash.end(),
              firstUtxoSig.begin(), firstUtxoSig.end());

    TransactionDescrPtr ptr(new TransactionDescr);
    ptr->hubAddress   = snodeAddress;
    ptr->sPubKey      = sPubKey;
    ptr->created      = timestamp;
    ptr->txtime       = timestamp;
    ptr->id           = id;
    ptr->from         = connFrom->toXAddr(from);
    ptr->fromCurrency = fromCurrency;
    ptr->fromAmount   = fromAmount;
    ptr->to           = connTo->toXAddr(to);
    ptr->toCurrency   = toCurrency;
    ptr->toAmount     = toAmount;
    ptr->usedCoins    = outputsForUse;
    ptr->blockHash    = blockHash;
    ptr->role         = 'A';

    // m key
    connTo->newKeyPair(ptr->mPubKey, ptr->mPrivKey);
    assert(ptr->mPubKey.size() == 33 && "bad pubkey size");

    // x key
    connTo->newKeyPair(ptr->xPubKey, ptr->xPrivKey);
    assert(ptr->xPubKey.size() == 33 && "bad pubkey size");

#ifdef LOG_KEYPAIR_VALUES
    LOG() << "generated M keypair " << std::endl <<
             "    pub    " << HexStr(ptr->mPubKey) << std::endl <<
             "    pub id " << HexStr(connTo->getKeyId(ptr->mPubKey)) << std::endl <<
             "    priv   " << HexStr(ptr->mPrivKey);
    LOG() << "generated X keypair " << std::endl <<
             "    pub    " << HexStr(ptr->xPubKey) << std::endl <<
             "    pub id " << HexStr(connTo->getKeyId(ptr->xPubKey)) << std::endl <<
             "    priv   " << HexStr(ptr->xPrivKey);
#endif

    // notify ui about new order
    xuiConnector.NotifyXBridgeTransactionReceived(ptr);

    // try send immediatelly
    m_p->sendPendingTransaction(ptr);

    // lock used coins
    connFrom->lockCoins(ptr->usedCoins, true);

    {
        boost::mutex::scoped_lock l(m_p->m_txLocker);
        m_p->m_transactions[id] = ptr;
    }

    LOG() << "order created" << ptr << __FUNCTION__;

    return xbridge::Error::SUCCESS;
}

//******************************************************************************
//******************************************************************************
bool App::sendPendingTransaction(const TransactionDescrPtr & ptr)
{
    return m_p->sendPendingTransaction(ptr);
}

//******************************************************************************
//******************************************************************************
bool App::Impl::sendPendingTransaction(const TransactionDescrPtr & ptr)
{
    if (ptr->hubAddress.size() == 0)
    {
        xassert(!"not defined service node for transaction");
        return false;
    }

    if (ptr->from.size() == 0 || ptr->to.size() == 0)
    {
        // TODO temporary
        return false;
    }

    XBridgePacketPtr packet(new XBridgePacket(xbcTransaction));

    // field length must be 8 bytes
    std::vector<unsigned char> fc(8, 0);
    std::copy(ptr->fromCurrency.begin(), ptr->fromCurrency.end(), fc.begin());

    // field length must be 8 bytes
    std::vector<unsigned char> tc(8, 0);
    std::copy(ptr->toCurrency.begin(), ptr->toCurrency.end(), tc.begin());

    // 32 bytes - id of transaction
    // 2x
    // 20 bytes - address
    //  8 bytes - currency
    //  8 bytes - amount
    // 32 bytes - hash of block when tr created
    packet->append(ptr->id.begin(), 32);
    packet->append(ptr->from);
    packet->append(fc);
    packet->append(ptr->fromAmount);
    packet->append(ptr->to);
    packet->append(tc);
    packet->append(ptr->toAmount);
    packet->append(util::timeToInt(ptr->created));
    packet->append(ptr->blockHash.begin(), 32);

    // utxo items
    packet->append(static_cast<uint32_t>(ptr->usedCoins.size()));
    for (const wallet::UtxoEntry & entry : ptr->usedCoins)
    {
        uint256 txid(entry.txId);
        packet->append(txid.begin(), 32);
        packet->append(entry.vout);
        packet->append(entry.rawAddress);
        packet->append(entry.signature);
    }

    packet->sign(ptr->mPubKey, ptr->mPrivKey);

    onSend(ptr->hubAddress, packet->body());

    return true;
}

//******************************************************************************
//******************************************************************************
Error App::acceptXBridgeTransaction(const uint256     & id,
                                    const std::string & from,
                                    const std::string & to)
{
    TransactionDescrPtr ptr;
    // TODO checkAcceptPrams can't be used after swap: uncovered bug, fix in progress (due to swap changing to/from)
//    const auto res = checkAcceptParams(id, ptr, from);
//    if(res != xbridge::SUCCESS)
//    {
//        return res;
//    }

    {
        boost::mutex::scoped_lock l(m_p->m_txLocker);
        if (!m_p->m_transactions.count(id))
        {

            WARN() << "transaction not found " << __FUNCTION__;
            return xbridge::TRANSACTION_NOT_FOUND;
        }
        ptr = m_p->m_transactions[id];
    }

    WalletConnectorPtr connFrom = connectorByCurrency(ptr->fromCurrency);
    WalletConnectorPtr connTo   = connectorByCurrency(ptr->toCurrency);
    if (!connFrom || !connTo)
    {
        // no session
        WARN() << "no session for <" << (connFrom ? ptr->toCurrency : ptr->fromCurrency) << "> " << __FUNCTION__;
        return xbridge::NO_SESSION;
    }

    // check dust
    if (connFrom->isDustAmount(static_cast<double>(ptr->fromAmount) / TransactionDescr::COIN))
    {
        return xbridge::Error::DUST;
    }
    if (connTo->isDustAmount(static_cast<double>(ptr->toAmount) / TransactionDescr::COIN))
    {
        return xbridge::Error::DUST;
    }

    if (pwalletMain->GetBalance() < connTo->serviceNodeFee)
    {
        return xbridge::Error::INSIFFICIENT_FUNDS_DX;
    }

    uint64_t utxoAmount = 0;
    uint64_t fee1       = 0;
    uint64_t fee2       = 0;

    std::vector<wallet::UtxoEntry> outputs;
    connFrom->getUnspent(outputs);

    // Select utxos
    std::vector<wallet::UtxoEntry> outputsForUse;
    if (!selectUtxos(from, outputs, connFrom, ptr->fromAmount, outputsForUse, utxoAmount, fee1, fee2))
    {
        WARN() << "insufficient funds for <" << ptr->fromCurrency << "> " << __FUNCTION__;
        return xbridge::Error::INSIFFICIENT_FUNDS;
    }

    // sign used coins
    for (wallet::UtxoEntry & entry : outputsForUse)
    {
        std::string signature;
        if (!connFrom->signMessage(entry.address, entry.toString(), signature))
        {
            WARN() << "funds not signed <" << ptr->fromCurrency << "> " << __FUNCTION__;
            return xbridge::Error::FUNDS_NOT_SIGNED;
        }

        bool isInvalid = false;
        entry.signature = DecodeBase64(signature.c_str(), &isInvalid);
        if (isInvalid)
        {
            WARN() << "invalid signature <" << ptr->fromCurrency << "> " << __FUNCTION__;
            return xbridge::Error::FUNDS_NOT_SIGNED;
        }

        entry.rawAddress = connFrom->toXAddr(entry.address);
        if(entry.signature.size() != 65)
        {
            ERR() << "incorrect signature length, need 65 bytes " << __FUNCTION__;
            return xbridge::Error::INVALID_SIGNATURE;
        }

        if(entry.rawAddress.size() != 20)
        {
            ERR() << "incorrect raw address length, need 20 bytes " << __FUNCTION__;
            return  xbridge::Error::INVALID_ADDRESS;
        }
    }

    ptr->from      = connFrom->toXAddr(from);
    ptr->to        = connTo->toXAddr(to);
    ptr->usedCoins = outputsForUse;
    ptr->role      = 'B';

    // m key
    connTo->newKeyPair(ptr->mPubKey, ptr->mPrivKey);
    assert(ptr->mPubKey.size() == 33 && "bad pubkey size");

#ifdef LOG_KEYPAIR_VALUES
    LOG() << "generated M keypair " << std::endl <<
             "    pub    " << HexStr(ptr->mPubKey) << std::endl <<
             "    pub id " << HexStr(connTo->getKeyId(ptr->mPubKey)) << std::endl <<
             "    priv   " << HexStr(ptr->mPrivKey);
#endif

    // try send immediatelly
    m_p->sendAcceptingTransaction(ptr);

//    LOG() << "accept transaction " << util::to_str(ptr->id) << std::endl
//          << "    from " << from << " (" << util::to_str(ptr->from) << ")" << std::endl
//          << "             " << ptr->fromCurrency << " : " << ptr->fromAmount << std::endl
//          << "    from " << to << " (" << util::to_str(ptr->to) << ")" << std::endl
//          << "             " << ptr->toCurrency << " : " << ptr->toAmount << std::endl;


    // lock used coins
    connTo->lockCoins(ptr->usedCoins, true);

    LOG() << "order accepted" << ptr << __FUNCTION__;

    return xbridge::Error::SUCCESS;
}

//******************************************************************************
//******************************************************************************
bool App::Impl::sendAcceptingTransaction(const TransactionDescrPtr & ptr)
{
    XBridgePacketPtr packet(new XBridgePacket(xbcTransactionAccepting));

    // field length must be 8 bytes
    std::vector<unsigned char> fc(8, 0);
    std::copy(ptr->fromCurrency.begin(), ptr->fromCurrency.end(), fc.begin());

    // field length must be 8 bytes
    std::vector<unsigned char> tc(8, 0);
    std::copy(ptr->toCurrency.begin(), ptr->toCurrency.end(), tc.begin());

    // 20 bytes - id of transaction
    // 2x
    // 20 bytes - address
    //  8 bytes - currency
    //  4 bytes - amount
    packet->append(ptr->hubAddress);
    packet->append(ptr->id.begin(), 32);
    packet->append(ptr->from);
    packet->append(fc);
    packet->append(ptr->fromAmount);
    packet->append(ptr->to);
    packet->append(tc);
    packet->append(ptr->toAmount);

    // utxo items
    packet->append(static_cast<uint32_t>(ptr->usedCoins.size()));
    for (const wallet::UtxoEntry & entry : ptr->usedCoins)
    {
        uint256 txid(entry.txId);
        packet->append(txid.begin(), 32);
        packet->append(entry.vout);
        packet->append(entry.rawAddress);
        packet->append(entry.signature);
    }

    packet->sign(ptr->mPubKey, ptr->mPrivKey);

    onSend(ptr->hubAddress, packet->body());

    ptr->state = TransactionDescr::trAccepting;
    xuiConnector.NotifyXBridgeTransactionChanged(ptr->id);

    return true;
}

//******************************************************************************
//******************************************************************************
xbridge::Error App::cancelXBridgeTransaction(const uint256 &id,
                                             const TxCancelReason &reason)
{
    TransactionDescrPtr ptr = transaction(id);
    if (!ptr || !ptr->isLocal())
    {
        LOG() << "order with id: " << id.GetHex() << " not found or order isn't local " << __FUNCTION__;
        return xbridge::TRANSACTION_NOT_FOUND;
    }

    if (ptr->state > TransactionDescr::trCreated)
    {
        LOG() << "order with id: " << id.GetHex() << " already in work " << __FUNCTION__;
        return xbridge::INVALID_STATE;
    }

    WalletConnectorPtr connFrom = connectorByCurrency(ptr->fromCurrency);
    if (!connFrom)
    {
        // no session
        WARN() << "no session for <" << ptr->fromCurrency << "> " << __FUNCTION__;
        return xbridge::NO_SESSION;
    }

    if (m_p->sendCancelTransaction(id, reason))
    {
        TransactionDescrPtr xtx = transaction(id);

        xtx->state  = TransactionDescr::trCancelled;
        xtx->reason = reason;

        connFrom->lockCoins(ptr->usedCoins, false);

        xuiConnector.NotifyXBridgeTransactionChanged(id);

        moveTransactionToHistory(id);
    }

    LOG() << "order cancelled" << ptr << __FUNCTION__;

    return xbridge::SUCCESS;
}

//******************************************************************************
//******************************************************************************
void App::cancelMyXBridgeTransactions()
{
    for(const auto &transaction : transactions())
    {
        if(transaction.second == nullptr)
            continue;

        if(transaction.second->isLocal())
            cancelXBridgeTransaction(transaction.second->id, crUserRequest);
    }
}

//******************************************************************************
//******************************************************************************
bool App::Impl::sendCancelTransaction(const uint256 & txid,
                                      const TxCancelReason & reason)
{
    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionCancel));
    reply->append(txid.begin(), 32);
    reply->append(static_cast<uint32_t>(reason));

    TransactionDescrPtr ptr;
    {
        boost::mutex::scoped_lock l(m_txLocker);
        if (m_transactions.count(txid))
        {
            ptr = m_transactions[txid];
        }
        else
        {
            return false;
        }
    }

    reply->sign(ptr->mPubKey, ptr->mPrivKey);

    static std::vector<unsigned char> addr(20, 0);
    onSend(addr, reply->body());

    // cancelled
    return true;
}

//******************************************************************************
//******************************************************************************
bool App::isValidAddress(const string & address) const
{
    // TODO need refactoring
    return ((address.size() >= 32) && (address.size() <= 36));
}

//******************************************************************************
//******************************************************************************
Error App::checkAcceptParams(const uint256       & id,
                             TransactionDescrPtr & ptr,
                             const std::string   & /*fromAddress*/)
{
    // TODO need refactoring
    ptr = transaction(id);

    if(!ptr) {
        WARN() << "transaction not found " << __FUNCTION__;
        return xbridge::TRANSACTION_NOT_FOUND;
    }

    // TODO enforce by address after improving addressbook
    return checkAmount(ptr->toCurrency, ptr->toAmount, "");
}

//******************************************************************************
//******************************************************************************
Error App::checkCreateParams(const string   & fromCurrency,
                             const string   & toCurrency,
                             const uint64_t & fromAmount,
                             const string   & /*fromAddress*/)
{
    // TODO need refactoring
    if (fromCurrency.size() > 8 || toCurrency.size() > 8)
    {
        WARN() << "invalid currency " << __FUNCTION__;
        return xbridge::INVALID_CURRENCY;
    }
    return checkAmount(fromCurrency, fromAmount, ""); // TODO enforce by address after improving addressbook
}

//******************************************************************************
//******************************************************************************
Error App::checkAmount(const string   & currency,
                       const uint64_t & amount,
                       const string   & address)
{
    // check amount
    WalletConnectorPtr conn = connectorByCurrency(currency);
    if (!conn) {
        // no session
        WARN() << "no session for <" << currency << "> " << __FUNCTION__;
        return xbridge::NO_SESSION;
    }

    // Check that wallet balance is larger than the smallest supported balance
    if (conn->getWalletBalance(address) < (static_cast<double>(amount) / TransactionDescr::COIN)) {
        WARN() << "insufficient funds for <" << currency << "> " << __FUNCTION__;
        return xbridge::INSIFFICIENT_FUNDS;
    }
    return xbridge::SUCCESS;
}

//******************************************************************************
/**
 * @brief Sends the service ping to the network on behalf of the current Servicenode.
 * @return
 */
//******************************************************************************
bool App::sendServicePing()
{
    if (activeServicenode.status != ACTIVE_SERVICENODE_STARTED)
    {
        ERR() << "This servicenode must be started in order to report report services to the network " << __FUNCTION__;
        return false;
    }

    CServicenode *pmn = mnodeman.Find(activeServicenode.vin);
    if (pmn == nullptr)
    {
        ERR() << "couldn't find the active servicenode " << __FUNCTION__;
        return false;
    }

    Exchange & e = Exchange::instance();
    std::map<std::string, bool> nodup;

    // TODO Add xrouter services
    std::vector<std::string> nonWalletServices;// = xrouter.services() {"xrSendTransaction","xrGetBlock","xrGetTransaction"};
    for (const auto &s : nonWalletServices)
    {
        nodup[s] = true;
    }

    // Add xbridge connected wallets
    for (const auto &wallet : e.connectedWallets())
    {
        nodup[wallet] = true;
    }

    // All services
    std::vector<std::string> services;
    services.reserve(nodup.size());
    for (const auto &item : nodup)
    {
        services.push_back(item.first);
    }

    std::string servicesStr = boost::algorithm::join(services, ",");

    // Create the ping packet
    XBridgePacketPtr ping(new XBridgePacket(xbcServicesPing));
    {
        ping->append(static_cast<uint32_t>(services.size()));
        ping->append(servicesStr);
        ping->sign(e.pubKey(), e.privKey());
    }

    if (ping->size() > 10000)
    {

        ERR() << "Service ping too big, too many services. Max of 10000 bytes supported " << __FUNCTION__;
        return false;
    }

    // Update node services on self
    m_p->addNodeServices(pmn->pubKeyServicenode, services);

    LOG() << "Sending service ping: " << servicesStr << __FUNCTION__;

    sendPacket(ping);
    return true;
}

//******************************************************************************
/**
 * @brief Returns true if the current node supports the specified service.
 * @param service Service to lookup
 * @return
 */
//******************************************************************************
bool App::hasNodeService(const std::string &service)
{
    return m_p->hasNodeService(activeServicenode.pubKeyServicenode, service);
}

//******************************************************************************
/**
 * @brief Returns true if the specified node supports the service.
 * @param nodePubKey Node to lookup
 * @param service Service to lookup
 * @return
 */
//******************************************************************************
bool App::hasNodeService(const ::CPubKey &nodePubKey, const std::string &service)
{
    return m_p->hasNodeService(nodePubKey, service);
}

//******************************************************************************
/**
 * @brief Returns all services across all nodes.
 * @return
 */
//******************************************************************************
std::map<::CPubKey, std::set<std::string> > App::allServices()
{
    boost::mutex::scoped_lock l(m_p->m_xwalletsLocker);
    return m_p->m_xwallets;
}

//******************************************************************************
/**
 * @brief Returns the node services supported by the specified node.
 * @return
 */
//******************************************************************************
std::set<std::string> App::nodeServices(const ::CPubKey &nodePubKey)
{
    boost::mutex::scoped_lock l(m_p->m_xwalletsLocker);
    return m_p->m_xwallets[nodePubKey];
}

//******************************************************************************
//******************************************************************************
bool App::addNodeServices(const ::CPubKey & nodePubKey,
                          const std::vector<std::string> & services)
{
    return m_p->addNodeServices(nodePubKey, services);
}

//******************************************************************************
//******************************************************************************
bool App::findNodeWithService(const std::set<std::string> & services,
                              CPubKey & node) const
{
    return m_p->findNodeWithService(services, node);
}

//******************************************************************************
//******************************************************************************
bool App::Impl::findNodeWithService(const std::set<std::string> & services,
                                    CPubKey & node) const
{
    boost::mutex::scoped_lock l(m_xwalletsLocker);

    for (const std::pair<CPubKey, std::set<std::string> > & n : m_xwallets)
    {
        uint32_t searchCounter = services.size();
        for (const std::string & serv : services)
        {
            if (n.second.count(serv))
            {
                if (--searchCounter == 0)
                {
                    node = n.first;
                    return true;
                }
            }
            else
            {
                break;
            }
        }
    }

    return false;
}

//******************************************************************************
/**
 * @brief Stores the specified services for the node.
 * @param nodePubKey Pubkey of the node
 * @param services List of supported services
 * @return True if success, otherwise false
 */
//******************************************************************************
bool App::Impl::addNodeServices(const ::CPubKey & nodePubKey,
                                const std::vector<std::string> & services)
{
    if (!services.empty())
    {
        std::set<std::string> map1;
        std::copy(services.begin(), services.end(), std::inserter(map1, map1.end()));

        {
            boost::mutex::scoped_lock l(m_xwalletsLocker);
            m_xwallets[nodePubKey] = map1;
        }
    }

    return true;
}

//******************************************************************************
/**
 * @brief Returns true if the service exists.
 * @param nodePubKey Pubkey of the node
 * @param service Service to search for
 * @return True if service is supported, otherwise false
 */
//******************************************************************************
bool App::Impl::hasNodeService(const ::CPubKey & nodePubKey,
                               const std::string & service)
{
    boost::mutex::scoped_lock l(m_xwalletsLocker);
    if (m_xwallets.count(nodePubKey))
    {
        return m_xwallets[nodePubKey].count(service) > 0;
    }

    return false;
}

//******************************************************************************
//******************************************************************************
template <typename T>
T random_element(T begin, T end)
{
    const unsigned long n = std::distance(begin, end);
    const unsigned long divisor = (RAND_MAX + 1) / n;

    unsigned long k;
    do { k = std::rand() / divisor; } while (k >= n);

    std::advance(begin, k);
    return begin;
}

//******************************************************************************
//******************************************************************************
bool App::selectUtxos(const std::string &addr, const std::vector<wallet::UtxoEntry> &outputs,
                      const WalletConnectorPtr &connFrom, const uint64_t &requiredAmount,
                      std::vector<wallet::UtxoEntry> &outputsForUse, uint64_t &utxoAmount,
                      uint64_t &fee1, uint64_t &fee2) const
{

    auto getUtxos = [&connFrom, &requiredAmount, &outputsForUse, &utxoAmount, &fee1, &fee2](const std::vector<wallet::UtxoEntry> & o) -> bool
    {
        fee2 = connFrom->minTxFee2(1, 1) * TransactionDescr::COIN;

        if(o.empty())
        {
            LOG() << "outputs list are empty " << __FUNCTION__;
            return false;
        }

        //sort entries from smaller to larger
        std::vector<wallet::UtxoEntry> outputsForSelection = o;
        std::sort(outputsForSelection.begin(), outputsForSelection.end(),
                  [](const wallet::UtxoEntry a, const wallet::UtxoEntry b) {
                      return (a.amount) < (b.amount);
                  });

        //one output that larger than target value
        std::vector<wallet::UtxoEntry> greaterThanTargetOutput;

        //try to find best matching one output or one larger output
        {
            fee1 = connFrom->minTxFee1(1, 3) * TransactionDescr::COIN;
            uint64_t fullAmount = requiredAmount + fee1 + fee2;

            for(const wallet::UtxoEntry & entry : outputsForSelection)
            {
                uint64_t utxosAmount = (entry.amount * TransactionDescr::COIN);

                if(utxosAmount == fullAmount)
                {
                    //we are lucky
                    outputsForUse.emplace_back(entry);
                    return true;
                }

                if (utxosAmount > fullAmount &&
                    !connFrom->isDustAmount(static_cast<double>(utxosAmount - fullAmount) / TransactionDescr::COIN))
                {
                    greaterThanTargetOutput.emplace_back(entry);
                    break;
                }
            }
        }


        //try to find sum of smaller outputs that match target
        std::vector<wallet::UtxoEntry> outputsSmallerThanTarget;
        std::copy_if(outputsForSelection.begin(), outputsForSelection.end(),
                     std::inserter(outputsSmallerThanTarget, outputsSmallerThanTarget.end()),
                     [&requiredAmount](const wallet::UtxoEntry & entry)
        {
            return requiredAmount > entry.amount * TransactionDescr::COIN;
        });

        bool sumOfSmallerOutputsLargerThanTarget = false;
        bool sumOfSmallerOutputsEqualTarget = false;
        {
            fee1 = connFrom->minTxFee1(outputsSmallerThanTarget.size(), 3) * TransactionDescr::COIN;

            uint64_t fullAmount = requiredAmount + fee1 + fee2;

            uint64_t utxosAmount = std::accumulate(outputsSmallerThanTarget.begin(), outputsSmallerThanTarget.end(), 0,
                                                  [](uint64_t accumulator, const wallet::UtxoEntry & entry)
            {
                return accumulator += (entry.amount * TransactionDescr::COIN);
            });

            if (utxosAmount == fullAmount)
            {
                sumOfSmallerOutputsEqualTarget = true;
            }

            if (utxosAmount > fullAmount &&
                !connFrom->isDustAmount(static_cast<double>(utxosAmount - fullAmount) / TransactionDescr::COIN))
            {
                sumOfSmallerOutputsLargerThanTarget = true;
            }
        }

        //best combination of smaller utxo's with lowest fee
        std::vector<wallet::UtxoEntry> bestSmallerOutputsCombination;

        //sum of all smaller outputs is lower than target, so return greater output
        if(!sumOfSmallerOutputsLargerThanTarget)
        {
            if(greaterThanTargetOutput.empty())
            {
                LOG() << "can't make any list of utxo's " << __FUNCTION__;
                return false;
            }

            outputsForUse = greaterThanTargetOutput;
            return true;
        }
        //sum of all smaller outputs is equal target, so best combination is all smaller outputs
        else if(sumOfSmallerOutputsEqualTarget)
        {
            bestSmallerOutputsCombination = outputsSmallerThanTarget;
        }
        //try to combine small outputs to target sum
        else
        {
            uint64_t smallestFee = std::numeric_limits<uint64_t>::max();
            const uint32_t iterations = 1000;
            for(uint32_t i = 0; i < iterations; ++i)
            {
                std::vector<wallet::UtxoEntry> uniqueOutputsSmallerThanTarget(outputsSmallerThanTarget);
                std::vector<wallet::UtxoEntry> outputsForUse;

                uint64_t utxosAmount = 0;

                while (!uniqueOutputsSmallerThanTarget.empty())
                {
                    const auto it = random_element(uniqueOutputsSmallerThanTarget.begin(),
                                                   uniqueOutputsSmallerThanTarget.end());
                    wallet::UtxoEntry entry = *it;

                    uniqueOutputsSmallerThanTarget.erase(it);

                    outputsForUse.emplace_back(entry);

                    fee1 = connFrom->minTxFee1(outputsForUse.size(), 3) * TransactionDescr::COIN;

                    uint64_t fullAmount = requiredAmount + fee1 + fee2;

                    utxosAmount += (entry.amount * TransactionDescr::COIN);

                    if (utxosAmount == fullAmount && fee1 < smallestFee)
                    {
                        smallestFee = fee1;
                        bestSmallerOutputsCombination = outputsForUse;
                        break;
                    }

                    if (utxosAmount > fullAmount && fee1 < smallestFee &&
                        !connFrom->isDustAmount(static_cast<double>(utxosAmount - fullAmount) / TransactionDescr::COIN))
                    {
                        smallestFee = fee1;
                        bestSmallerOutputsCombination = outputsForUse;
                        break;
                    }
                }
            }
        }

        if(greaterThanTargetOutput.empty() && bestSmallerOutputsCombination.empty())
        {
            LOG() << "all strategy are fail to create utxo's list " << __FUNCTION__;
            return false;
        }
        else if(greaterThanTargetOutput.empty())
            outputsForUse = bestSmallerOutputsCombination;
        else if(bestSmallerOutputsCombination.empty())
            outputsForUse = greaterThanTargetOutput;
        else
        {
            uint64_t times = 2; //differences in times
            uint64_t greaterThanTargetOutputAmount = 0;
            uint64_t bestSmallerOutputsCombinationAmount = 0;

            for(const wallet::UtxoEntry & entry : greaterThanTargetOutput)
                greaterThanTargetOutputAmount += (entry.amount * TransactionDescr::COIN);

            for(const wallet::UtxoEntry & entry : bestSmallerOutputsCombination)
                bestSmallerOutputsCombinationAmount += (entry.amount * TransactionDescr::COIN);

            //if one larger output bigger then sum of small outputs more then twice - better to use small outputs
            if(greaterThanTargetOutputAmount > bestSmallerOutputsCombinationAmount * times)
            {
                utxoAmount = bestSmallerOutputsCombinationAmount;
                outputsForUse = bestSmallerOutputsCombination;
            }
            else
            {
                utxoAmount = greaterThanTargetOutputAmount;
                outputsForUse = greaterThanTargetOutput;
            }
        }

        fee1 = connFrom->minTxFee1(outputsForUse.size(), 3) * TransactionDescr::COIN;

        return true;
    };

    std::vector<wallet::UtxoEntry> outputsFromRequiredAddress;
    std::copy_if(outputs.begin(), outputs.end(), std::inserter(outputsFromRequiredAddress, outputsFromRequiredAddress.end()),
                 [&addr](const wallet::UtxoEntry & entry){
        return entry.address == addr;
    });

    //try to fill outputs only from one address
    if(getUtxos(outputsFromRequiredAddress))
        return true;

    //try to fill outputs from any address
    return getUtxos(outputs);
}

//******************************************************************************
//******************************************************************************
void App::Impl::onTimer()
{
    // DEBUG_TRACE();
    {
        m_services.push_back(m_services.front());
        m_services.pop_front();

        xbridge::SessionPtr session = getSession();

        IoServicePtr io = m_services.front();

        // call check expired transactions
        io->post(boost::bind(&xbridge::Session::checkFinishedTransactions, session));

        // send transactions list
        {
            static uint32_t counter = 0;
            if (++counter == 20)
            {
                // 15 sec * 20 = 5 min
                counter = 0;
                io->post(boost::bind(&xbridge::Session::sendListOfTransactions, session));
            }
        }

        // erase expired tx
        io->post(boost::bind(&xbridge::Session::eraseExpiredPendingTransactions, session));

        // get addressbook
        io->post(boost::bind(&xbridge::Session::getAddressBook, session));

        // unprocessed packets
        {
            static uint32_t counter = 0;
            if (++counter == 2)
            {
                counter = 0;
                std::map<uint256, XBridgePacketPtr> map;
                {
                    boost::mutex::scoped_lock l(m_ppLocker);
                    map = m_pendingPackets;
                    m_pendingPackets.clear();
                }
                for (const std::pair<uint256, XBridgePacketPtr> & item : map)
                {

                    xbridge::SessionPtr s = getSession();
                    XBridgePacketPtr packet   = item.second;
                    io->post(boost::bind(&xbridge::Session::processPacket, s, packet, nullptr));

                }
            }
        }
    }

    m_timer.expires_at(m_timer.expires_at() + boost::posix_time::seconds(TIMER_INTERVAL));
    m_timer.async_wait(boost::bind(&Impl::onTimer, this));
}

} // namespace xbridge
