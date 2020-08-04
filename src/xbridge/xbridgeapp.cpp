// Copyright (c) 2017-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//*****************************************************************************
//*****************************************************************************

#include <xbridge/xbridgeapp.h>

#include <xbridge/util/logger.h>
#include <xbridge/util/settings.h>
#include <xbridge/util/txlog.h>
#include <xbridge/util/xassert.h>
#include <xbridge/util/xbridgeerror.h>
#include <xbridge/util/xseries.h>
#include <xbridge/xbridgecryptoproviderbtc.h>
#include <xbridge/xbridgeexchange.h>
#include <xbridge/xbridgesession.h>
#include <xbridge/xbridgewalletconnector.h>
#include <xbridge/xbridgewalletconnectorbtc.h>
#include <xbridge/xbridgewalletconnectorbcd.h>
#include <xbridge/xbridgewalletconnectorbch.h>
#include <xbridge/xbridgewalletconnectordevault.h>
#include <xbridge/xbridgewalletconnectordgb.h>
#include <xbridge/xbridgewalletconnectorbtg.h>
#include <xbridge/xbridgewalletconnectorstealth.h>
#include <xbridge/xbridgepacket.h>
#include <xbridge/xuiconnector.h>
#include <xrouter/xrouterapp.h>

#include <net.h>
#include <netmessagemaker.h>
#include <rpc/server.h>
#include <servicenode/servicenodemgr.h>
#include <shutdown.h>
#include <sync.h>
#include <ui_interface.h>
#include <version.h>

#include <algorithm>
#include <assert.h>
#include <random>
#include <regex>
#include <string.h>

#include <boost/algorithm/string/join.hpp>
#include <boost/chrono/chrono.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>

#include <openssl/rand.h>
#include <openssl/md5.h>

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

bool CanAffordFeePayment(const CAmount & fee) {
    return App::instance().canAffordFeePayment(fee);
}

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
     * @param safeCleanup specify false to indicate potential unsafe cleanup, defaults to true
     * @return true
     */
    bool stop(const bool safeCleanup = true);

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
     * @param fromBlockHeight from token block height
     * @param toBlockHeight to token block height
     * @param fromBlockHash from token block hash
     * @param toBlockHash to token block hash
     * @return
     */
    bool sendAcceptingTransaction(const TransactionDescrPtr & ptr, uint32_t fromBlockHeight, uint32_t toBlockHeight,
                                  const std::string & fromBlockHash, const std::string & toBlockHash);

    /**
     * @brief findShuffledNodesWithService - finds nodes with given services
     *        that have the given protocol version
     * @param - requested services
     * @param - protocol version
     * @return - shuffled list of nodes with requested services
     */
    std::vector<CPubKey> findShuffledNodesWithService(
        const std::set<std::string>& requested_services,
        const uint32_t version,
        const std::set<CPubKey> & notIn) const;

    /**
     * @brief Checks the orders that are in the New and Pending states. If orders are stuck, rebroadcast to a
     *        different servicenode.
     */
    void checkAndRelayPendingOrders();

    /**
     * @brief Check orders on timer and erase if expired.
     */
    void checkAndEraseExpiredTransactions();

    /**
     * @brief Check for deposits that were spent by the counterparty.
     */
    void checkWatchesOnDepositSpends();

    /**
     * @brief Servicenodes watch for trader deposit locktimes to expire and when they do automatically
     *        submits the refund transaction for those orders that haven't reported completing.
     */
    void watchTraderDeposits();

    /**
     * @brief Returns true if the order's utxos are still valid and unspent.
     * @param order
     */
    bool orderUtxosAreStillValid(TransactionDescrPtr order);

    /**
     * @brief If a service node was found with the specified service, return true.
     * @param nodePubKey
     * @param service
     * @param checkRunning Default false
     */
    bool hasNodeService(const CPubKey & nodePubKey, const std::string & service, bool checkRunning=false);

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
    mutable CCriticalSection                           m_sessionsLock;
    SessionQueue                                       m_sessions;
    SessionsAddrMap                                    m_sessionAddressMap;

    // connectors
    mutable CCriticalSection                           m_connectorsLock;
    Connectors                                         m_connectors;
    ConnectorsAddrMap                                  m_connectorAddressMap;
    ConnectorsCurrencyMap                              m_connectorCurrencyMap;

    // pending messages (packet processing loop)
    CCriticalSection                                   m_messagesLock;
    typedef std::set<uint256> ProcessedMessages;
    ProcessedMessages                                  m_processedMessages;

    // address book
    CCriticalSection                                   m_addressBookLock;
    AddressBook                                        m_addressBook;
    std::set<std::string>                              m_addresses;

    // transactions
    CCriticalSection                                   m_txLocker;
    std::map<uint256, TransactionDescrPtr>             m_transactions;
    std::map<uint256, TransactionDescrPtr>             m_historicTransactions;
    xSeriesCache                                       m_xSeriesCache;

    // network packets queue
    CCriticalSection                                   m_ppLocker;
    std::map<uint256, XBridgePacketPtr>                m_pendingPackets;

    // store deposit watches
    CCriticalSection                                   m_watchDepositsLocker;
    std::map<uint256, TransactionDescrPtr>             m_watchDeposits;
    bool                                               m_watching{false};

    // store trader watches
    CCriticalSection                                   m_watchTradersLocker;
    std::map<uint256, TransactionPtr>                  m_watchTraders;
    bool                                               m_watchingTraders{false};

    std::atomic<bool>                                  m_stopped{false};
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
    : m_p(new Impl), m_disconnecting(false)
{
}

//*****************************************************************************
//*****************************************************************************
App::~App()
{
    m_p->stop(false);
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
uint32_t App::version() {
    return static_cast<uint32_t>(XBRIDGE_PROTOCOL_VERSION);
}
std::string App::versionStr() {
    std::ostringstream o;
    o << XBRIDGE_PROTOCOL_VERSION;
    return o.str();
}
bool App::createConf()
{
    try {
        std::string eol = "\n";
#ifdef WIN32
        eol = "\r\n";
#endif
        auto p = GetDataDir(false) / "xbridge.conf";
        if (!boost::filesystem::exists(p)) {
            saveConf(p,
                "# For a complete list of configuration files for each supported token"        + eol +
                "# please visit: https://github.com/blocknetdx/blockchain-configuration-files" + eol +
                ""                                                                             + eol +
                "[Main]"                                                                       + eol +
                "ExchangeWallets="                                                             + eol +
                "FullLog=true"                                                                 + eol +
                "# Show all orders across the network regardless of whether wallets are "      + eol +
                "# installed locally, set to \"true\". -dxnowallets in blocknet.conf "         + eol +
                "# overrides this setting"                                                     + eol +
                "ShowAllOrders=false"                                                          + eol +
                ""                                                                             + eol +
                "# Sample configuration:"                                                      + eol +
                "# [BLOCK]"                                                                    + eol +
                "# Title=Blocknet"                                                             + eol +
                "# Address="                                                                   + eol +
                "# Ip=127.0.0.1"                                                               + eol +
                "# Port=41414"                                                                 + eol +
                "# Username=test"                                                              + eol +
                "# Password=testpassword"                                                      + eol +
                "# AddressPrefix=26"                                                           + eol +
                "# ScriptPrefix=28"                                                            + eol +
                "# SecretPrefix=154"                                                           + eol +
                "# COIN=100000000"                                                             + eol +
                "# MinimumAmount=0"                                                            + eol +
                "# TxVersion=1"                                                                + eol +
                "# DustAmount=0"                                                               + eol +
                "# CreateTxMethod=BTC"                                                         + eol +
                "# GetNewKeySupported=true"                                                    + eol +
                "# ImportWithNoScanSupported=true"                                             + eol +
                "# MinTxFee=10000"                                                             + eol +
                "# BlockTime=60"                                                               + eol +
                "# FeePerByte=20"                                                              + eol +
                "# Confirmations=2"                                                            + eol +
                "# TxWithTimeField=false"                                                      + eol +
                "# LockCoinsSupported=false"                                                   + eol +
                "# JSONVersion="                                                               + eol +
                "# ContentType="                                                               + eol
            );
        }

        return true;

    } catch (...) {
        ERR() << "XBridge failed to create default xbridge.conf";
    }
    return false;
}

//*****************************************************************************
//*****************************************************************************
bool App::isEnabled()
{
    return connectors().size() > 0 || xbridge::Exchange::instance().isEnabled() || gArgs.GetBoolArg("-dxnowallets", settings().showAllOrders());
}

//*****************************************************************************
//*****************************************************************************
bool App::start()
{
    auto s = m_p->start();

    // This will update the wallet connectors on both the app & exchange
    updateActiveWallets();

    if (xbridge::Exchange::instance().isEnabled())
        LOG() << "XBridge exchange enabled";

    if (xbridge::Exchange::instance().isStarted())
        LOG() << "XBridge exchange started";

    // Restore local orders
    loadOrders();

    return s;
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
    }
    catch (std::exception & e)
    {
        ERR() << e.what() << " " << __FUNCTION__;
    }

    m_stopped = false;
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool App::init()
{
    // init xbridge settings
    Settings & s = settings();
    s.parseCmdLine(GetDataDir());
    loadSettings();

    // init exchange
    Exchange & e = Exchange::instance();
    e.init();

    // sessions
    {
        LOCK(m_p->m_sessionsLock);

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
    if (m_stopped)
        return true;
    m_stopped = true;

    // Save db state
    saveOrders(true);

    bool s = m_p->stop();
    return s;
}

//*****************************************************************************
//*****************************************************************************
bool App::disconnectWallets()
{
    {
        LOCK(m_lock);
        if (m_disconnecting || !sn::ServiceNodeMgr::instance().hasActiveSn())
            return false; // not a servicenode or not started
        m_disconnecting = true;
    }

    // Notify the network all wallets are going offline
    std::set<std::string> wallets;
    {
        LOCK(m_p->m_connectorsLock);
        for (auto & conn : m_p->m_connectors)
            wallets.insert(conn->currency);
    }
    // Remove all connectors
    for (auto & wallet : wallets)
        removeConnector(wallet);

    std::set<std::string> noWallets;
    xbridge::Exchange::instance().loadWallets(noWallets);
    // TODO Blocknet XRouter notify network wallets are going offline
//    std::vector<std::string> nonWalletServices = xrouter::App::instance().getServicesList();
//    sendServicePing(nonWalletServices);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool App::loadSettings()
{
    LOCK(m_lock);

    Settings & s = settings();
    try {
        std::string path(GetDataDir(false).string());
        path += "/xbridge.conf";
        s.read(path.c_str());
        LOG() << "Finished loading config " << path;
    } catch (...) {
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool App::Impl::stop(const bool log)
{
    if (m_stopped)
        return true;
    m_stopped = true;

    if (log)
        LOG() << "stopping xbridge threads...";

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
    uint64_t timestampValue = timeToInt(timestamp);
    unsigned char * ptr = reinterpret_cast<unsigned char *>(&timestampValue);
    msg.insert(msg.end(), ptr, ptr + sizeof(uint64_t));

    // body
    msg.insert(msg.end(), message.begin(), message.end());

    uint256 hash = Hash(msg.begin(), msg.end());

    App::instance().addToKnown(hash);

    // Relay
    const CNetMsgMaker msgMaker(PROTOCOL_VERSION);
    g_connman->ForEachNode([&](CNode* pnode) {
        if (!pnode->fSuccessfullyConnected)
            return;
        if (pnode->fSuccessfullyConnected && !pnode->fDisconnect && !pnode->fXRouter) // do not relay to xrouter nodes
            g_connman->PushMessage(pnode, msgMaker.Make(NetMsgType::XBRIDGE, msg));
    });
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

    LOCK(m_sessionsLock);

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
    LOCK(m_sessionsLock);
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
            LOCK(m_p->m_connectorsLock);
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

    // If Servicenode w/ exchange, process packets for this snode only
    Exchange & e = Exchange::instance();
    if (e.isStarted() && sn::ServiceNodeMgr::instance().hasActiveSn())
    {
        auto snodeID = sn::ServiceNodeMgr::instance().getActiveSn().keyId();
        std::vector<unsigned char> snodeAddr(20);
        std::copy(snodeID.begin(), snodeID.end(), snodeAddr.begin());

        // check that ids match
        if (memcmp(&snodeAddr[0], &id[0], 20) != 0)
            return;

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
    LOCK(m_p->m_ppLocker);
    m_p->m_pendingPackets[txid] = packet;
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool App::removePackets(const uint256 & txid)
{
    // remove from pending packets (if added)

    LOCK(m_p->m_ppLocker);
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
    LOCK(m_p->m_connectorsLock);
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
    LOCK(m_p->m_connectorsLock);

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
    std::set<std::string> coins;
    auto snodes = sn::ServiceNodeMgr::instance().list();
    // Obtain unique xwallets supported across network
    for (auto & sn : snodes) {
        if (!sn.running())
            continue;
        for (auto &w : sn.serviceList()) {
            if (!coins.count(w))
                coins.insert(w);
        }
    }
    if (!coins.empty()) {
        std::vector<std::string> result(coins.size());
        std::copy(coins.begin(), coins.end(), std::back_inserter(result));
        return result;
    }
    return std::vector<std::string>();
}

//*****************************************************************************
//*****************************************************************************
bool App::hasCurrency(const std::string & currency) const
{
    LOCK(m_p->m_connectorsLock);
    return m_p->m_connectorCurrencyMap.count(currency);
}

//*****************************************************************************
//*****************************************************************************
// The connector is not added if it already exists.
void App::addConnector(const WalletConnectorPtr & conn)
{
    LOCK(m_p->m_connectorsLock);

    // Remove existing connector
    bool found = false;
    for (int i = m_p->m_connectors.size() - 1; i >= 0; --i) {
        if (m_p->m_connectors[i]->currency == conn->currency) {
            found = true;
            m_p->m_connectors.erase(m_p->m_connectors.begin() + i);
        }
    }

    // Add new connector
    m_p->m_connectors.push_back(conn);
    m_p->m_connectorCurrencyMap[conn->currency] = conn;

    // Update address connectors
    for (auto iter = m_p->m_connectorAddressMap.rbegin(); iter != m_p->m_connectorAddressMap.rend(); ++iter) {
        if (iter->second->currency == conn->currency)
            m_p->m_connectorAddressMap[iter->first] = conn;
    }
}

//*****************************************************************************
//*****************************************************************************
void App::removeConnector(const std::string & currency)
{
    LOCK(m_p->m_connectorsLock);

    for (int i = m_p->m_connectors.size() - 1; i >= 0; --i) {
        if (m_p->m_connectors[i]->currency == currency)
            m_p->m_connectors.erase(m_p->m_connectors.begin() + i);
    }

    // Remove currency connector
    m_p->m_connectorCurrencyMap.erase(currency);

    // Remove addresses linked to this connector (delete in reverse iterator)
    for (auto iter = m_p->m_connectorAddressMap.cbegin(); iter != m_p->m_connectorAddressMap.cend();) {
        if (iter->second && iter->second->currency == currency)
            m_p->m_connectorAddressMap.erase(iter++);
        else ++iter;
    }
}

//*****************************************************************************
//*****************************************************************************
void App::updateConnector(const WalletConnectorPtr & conn,
                          const std::vector<unsigned char> addr,
                          const std::string & currency)
{
    LOCK(m_p->m_connectorsLock);

    m_p->m_connectorAddressMap[addr]      = conn;
    m_p->m_connectorCurrencyMap[currency] = conn;
}

//*****************************************************************************
//*****************************************************************************
void App::updateActiveWallets()
{
    {
        LOCK(m_updatingWalletsLock);
        if (m_updatingWallets)
            return;
        m_updatingWallets = true;
    }
    if (ShutdownRequested())
        return;

    Settings & s = settings();
    std::vector<std::string> wallets = s.exchangeWallets();

    // Disconnect any wallets not in the exchange list
    std::set<std::string> toRemove;
    {
        LOCK(m_p->m_connectorsLock);
        for (auto & item : m_p->m_connectorCurrencyMap) {
            bool found = false;
            for (auto & currency : wallets) {
                if (item.first == currency) {
                    found = true;
                    break;
                }
            }
            if (!found)
                toRemove.insert(item.first);
        }
    } // do not deadlock removeConnector(), it must be outside lock scope
    for (auto & currency : toRemove)
        removeConnector(currency);

    // Store connectors from config
    std::vector<WalletConnectorPtr> conns;

    // Copy bad wallets
    std::map<std::string, boost::posix_time::ptime> badWallets;
    {
        LOCK(m_updatingWalletsLock);
        badWallets = m_badWallets;
    }

    for (std::vector<std::string>::iterator i = wallets.begin(); i != wallets.end(); ++i)
    {
        // Ignore bad wallets until expiry
        if (badWallets.count(*i)) {
            const auto last_time = badWallets[*i];
            const auto current_time = boost::posix_time::second_clock::universal_time();
            // Wait ~5 minutes before doing wallet check on bad wallet
            if (static_cast<boost::posix_time::time_duration>(current_time - last_time).total_seconds() >= 300) {
                LOCK(m_updatingWalletsLock);
                m_badWallets.erase(*i);
            } else // not ready to do wallet check
                continue;
        }

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
        wp.feePerByte                  = s.get<uint64_t>   (*i + ".FeePerByte", 0);
        wp.method                      = s.get<std::string>(*i + ".CreateTxMethod");
        wp.blockTime                   = s.get<int>        (*i + ".BlockTime", 0);
//        wp.blockSize                   = s.get<int>        (*i + ".BlockSize", 0);
        wp.requiredConfirmations       = s.get<int>        (*i + ".Confirmations", 0);
        wp.txWithTimeField             = s.get<bool>       (*i + ".TxWithTimeField", false);
        wp.isLockCoinsSupported        = s.get<bool>       (*i + ".LockCoinsSupported", false);
        wp.jsonver                     = s.get<std::string>(*i + ".JSONVersion", "");
        wp.contenttype                 = s.get<std::string>(*i + ".ContentType", "");
        wp.cashAddrPrefix              = s.get<std::string>(*i + ".CashAddrPrefix", "");

        if (wp.m_user.empty() || wp.m_passwd.empty())
            WARN() << wp.currency << " \"" << wp.title << "\"" << " has empty credentials";

        if (wp.m_ip.empty() || wp.m_port.empty() || wp.COIN == 0 || wp.blockTime == 0) {
            ERR() << wp.currency << " \"" << wp.title << "\"" << " Failed to connect, check the config";
            removeConnector(wp.currency);
            continue;
        }

        // Check maker locktime reqs
        if (wp.blockTime * XMIN_LOCKTIME_BLOCKS > XMAKER_LOCKTIME_TARGET_SECONDS) {
            ERR() << wp.currency << " \"" << wp.title << "\"" << " Failed maker locktime requirements";
            removeConnector(wp.currency);
            continue;
        }
        // Check taker locktime reqs (non-slow chains)
        if (wp.blockTime < XSLOW_BLOCKTIME_SECONDS && wp.blockTime * XMIN_LOCKTIME_BLOCKS > XTAKER_LOCKTIME_TARGET_SECONDS) {
            ERR() << wp.currency << " \"" << wp.title << "\"" << " Failed taker locktime requirements";
            removeConnector(wp.currency);
            continue;
        }
        // If this coin is a slow blockchain check to make sure locktime drift checks
        // are compatible with this chain. If not then ignore loading the token.
        // locktime calc should be less than taker locktime target
        if (wp.blockTime >= XSLOW_BLOCKTIME_SECONDS && wp.blockTime * XMIN_LOCKTIME_BLOCKS > XSLOW_TAKER_LOCKTIME_TARGET_SECONDS) {
            ERR() << wp.currency << " \"" << wp.title << "\"" << " Failed taker locktime requirements";
            removeConnector(wp.currency);
            continue;
        }

        // Confirmation compatibility check
        const auto maxConfirmations = std::max<uint32_t>(XLOCKTIME_DRIFT_SECONDS/wp.blockTime, XMAX_LOCKTIME_DRIFT_BLOCKS);
        if (wp.requiredConfirmations > maxConfirmations) {
            ERR() << wp.currency << " \"" << wp.title << "\"" << " Failed confirmation check, max allowed for this token is " << maxConfirmations;
            removeConnector(wp.currency);
            continue;
        }

        if (wp.blockSize < 1024) {
            wp.blockSize = 1024;
            WARN() << wp.currency << " \"" << wp.title << "\"" << " Minimum block size required is 1024 kb";
        }

        xbridge::WalletConnectorPtr conn;
        if (wp.method == "ETH" || wp.method == "ETHER" || wp.method == "ETHEREUM") {
            LOG() << "ETH connector is not supported on XBridge at this time";
            continue;
        }
        else if (wp.method == "BTC" || wp.method == "SYS")
        {
            conn.reset(new BtcWalletConnector<BtcCryptoProvider>);
            *conn = wp;
        }
        else if (wp.method == "BCD")
        {
            conn.reset(new BCDWalletConnector);
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
        else if (wp.method == "BTG")
        {
            conn.reset(new BTGWalletConnector);
            *conn = wp;
        }
        else if (wp.method == "DEVAULT")
        {
            conn.reset(new DevaultWalletConnector);
            *conn = wp;
        }
        else if (wp.method == "STEALTH" || wp.method == "XST")
        {
            conn.reset(new StealthWalletConnector);
            *conn = wp;
        }
        else
        {
            ERR() << "unknown session type " << __FUNCTION__;
        }

        // If the wallet is invalid, remove it from the list
        if (!conn)
        {
            ERR() << wp.currency << " \"" << wp.title << "\"" << " Failed to connect, check the config";
            removeConnector(wp.currency);
            continue;
        }

        conns.push_back(conn);
    }

    // Valid connections
    std::vector<WalletConnectorPtr> validConnections;
    // Invalid connections
    std::vector<WalletConnectorPtr> badConnections;
    // All valid wallets
    std::set<std::string> validWallets;

    // Process connections
    if (!conns.empty()) {
        // TLDR: Multithreaded connection checks
        // The code below utilizes boost async to spawn threads up to the reported hardware concurrency
        // capabilities of the host. All of the wallets loaded into xbridge.conf will be checked here,
        // specifically for valid connections. This implementation also supports being interrupted via
        // a boost interruption point. A mutex is used to synchronize checks across async threads.
        boost::mutex muJobs;
        auto rpcThreads = static_cast<int32_t>(gArgs.GetArg("-rpcthreads", 4));
        if (rpcThreads <= 0)
            rpcThreads = 4;
        const uint32_t maxPendingJobs = static_cast<uint32_t>(rpcThreads);
        uint32_t pendingJobs = 0;
        uint32_t allJobs = static_cast<uint32_t>(conns.size());
        boost::thread_group tg;
        auto check = [&muJobs,&pendingJobs,&allJobs,&badConnections,&validConnections](WalletConnectorPtr conn) {
            if (ShutdownRequested())
                return;
            // Check that wallet is reachable
            if (!conn->init()) {
                {
                    boost::mutex::scoped_lock l(muJobs);
                    --pendingJobs;
                    --allJobs;
                    badConnections.push_back(conn);
                }
                return;
            }
            {
                boost::mutex::scoped_lock l(muJobs);
                --pendingJobs;
                --allJobs;
                validConnections.push_back(conn);
            }
        };

        // Synchronize all connection checks
        try {
            while (true) {
                boost::this_thread::interruption_point();
                if (ShutdownRequested())
                    break;

                const int32_t size = conns.size();
                for (int32_t i = size - 1; i >= 0; --i) {
                    {
                        boost::mutex::scoped_lock l(muJobs);
                        if (pendingJobs >= maxPendingJobs)
                            break;
                        ++pendingJobs;
                    }
                    WalletConnectorPtr conn = conns.back();
                    conns.pop_back();
                    // Asynchronously check connection
                    try {
                        tg.create_thread([&check, conn]() {
                            RenameThread("blocknet-xbridgewalletcheck");
                            check(conn);
                        });
                    } catch (...) {
                        // try single threaded on error creating thread
                        check(conn);
                    }
                }

                {
                    boost::mutex::scoped_lock l(muJobs);
                    if (allJobs == 0)
                        break;
                }

                boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
            }
            tg.join_all();
        } catch (...) { // bail on error (possible thread error etc)
            tg.interrupt_all();
            tg.join_all();
            LOCK(m_updatingWalletsLock);
            m_updatingWallets = false;
            WARN() << "Potential issue with active xbridge wallets checks (unknown threading error). If issue persists please notify the dev team";
            return;
        }

        // Check for shutdown
        if (!ShutdownRequested()) {
            // Add valid connections
            for (auto & conn : validConnections) {
                addConnector(conn);
                validWallets.insert(conn->currency);
                LOG() << conn->currency << " \"" << conn->title << "\"" << " connected " << conn->m_ip << ":" << conn->m_port;
            }

            // Remove bad connections
            for (auto & conn : badConnections) {
                removeConnector(conn->currency);
                {
                    LOCK(m_updatingWalletsLock);
                    boost::posix_time::ptime time{boost::posix_time::second_clock::universal_time()};
                    m_badWallets[conn->currency] = time;
                }
                WARN() << conn->currency << " \"" << conn->title << "\"" << " Failed to connect, check the config";
            }
        }
    }

    // Let the exchange know about the new wallet list
    if (!ShutdownRequested())
        xbridge::Exchange::instance().loadWallets(validWallets);

    {
        LOCK(m_updatingWalletsLock);
        m_updatingWallets = false;
    }
}

//*****************************************************************************
//*****************************************************************************
std::vector<WalletConnectorPtr> App::connectors() const
{
    LOCK(m_p->m_connectorsLock);
    return m_p->m_connectors;
}

//*****************************************************************************
//*****************************************************************************
bool App::isKnownMessage(const std::vector<unsigned char> & message)
{
    LOCK(m_p->m_messagesLock);
    return m_p->m_processedMessages.count(Hash(message.begin(), message.end())) > 0;
}

//*****************************************************************************
//*****************************************************************************
bool App::isKnownMessage(const uint256 & hash)
{
    LOCK(m_p->m_messagesLock);
    return m_p->m_processedMessages.count(hash) > 0;
}

//*****************************************************************************
//*****************************************************************************
void App::addToKnown(const std::vector<unsigned char> & message)
{
    // add to known
    LOCK(m_p->m_messagesLock);
    // clear memory if it's larger than mempool threshold
    clearMempool();
    m_p->m_processedMessages.insert(Hash(message.begin(), message.end()));
}

//*****************************************************************************
//*****************************************************************************
void App::addToKnown(const uint256 & hash)
{
    // add to known
    LOCK(m_p->m_messagesLock);
    // clear memory if it's larger than mempool threshold
    clearMempool();
    m_p->m_processedMessages.insert(hash);
}

//******************************************************************************
//******************************************************************************
TransactionDescrPtr App::transaction(const uint256 & id) const
{
    TransactionDescrPtr result;

    LOCK(m_p->m_txLocker);

    if (m_p->m_transactions.count(id))
    {
        result = m_p->m_transactions[id];
    }

    if (m_p->m_historicTransactions.count(id))
    {
        if(result != nullptr) {
            ERR() << "duplicate order " << __FUNCTION__;
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
    LOCK(m_p->m_txLocker);
    return m_p->m_transactions;
}

//******************************************************************************
//******************************************************************************
std::map<uint256, xbridge::TransactionDescrPtr> App::history() const
{
    LOCK(m_p->m_txLocker);
    return m_p->m_historicTransactions;
}

//******************************************************************************
//******************************************************************************
std::vector<CurrencyPair> App::history_matches(const App::TransactionFilter& filter,
                                          const xQuery& query)
{
    std::vector<CurrencyPair> matches{};
    {
        LOCK(m_p->m_txLocker);
        for(const auto& it : m_p->m_historicTransactions) {
            filter(matches, *it.second, query);
        }
    }
    return matches;
}

xSeriesCache& App::getXSeriesCache() { return m_p->m_xSeriesCache; }

//******************************************************************************
//******************************************************************************
std::vector<App::FlushedOrder>
App::flushCancelledOrders(bpt::time_duration minAge) const
{
    std::vector<App::FlushedOrder> list{};
    const bpt::ptime keepTime{bpt::microsec_clock::universal_time() - minAge};
    const std::vector<TransactionMap*> maps{&m_p->m_transactions, &m_p->m_historicTransactions};

    LOCK(m_p->m_txLocker);

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
    LOCK(m_p->m_txLocker);

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
        LOCK(m_p->m_txLocker);

        size_t counter = 0;

        if (m_p->m_transactions.count(id))
        {
            xtx = m_p->m_transactions[id];

            counter = m_p->m_transactions.erase(id);
            if(counter > 1) {
                ERR() << "duplicate order id = " << id.GetHex() << " " << __FUNCTION__;
            }
        }

        if (xtx)
        {
            if(m_p->m_historicTransactions.count(id) != 0) {
                ERR() << "duplicate order " << id.GetHex() << " in history " << __FUNCTION__;
                return;
            }
            xtx->moveToHistory();
            m_p->m_historicTransactions[id] = xtx;
        }
    }

    if (xtx)
    {
        // unlock tx coins
        xbridge::App::instance().unlockCoins(xtx->fromCurrency, xtx->usedCoins);
    }

    // remove pending packets for this tx
    removePackets(id);
}

//******************************************************************************
//******************************************************************************
xbridge::Error App::repostXBridgeTransaction(const std::string from, const std::string fromCurrency,
        const std::string to, const std::string toCurrency, const CAmount makerAmount, const CAmount takerAmount,
        const uint64_t minFromAmount, const std::vector<wallet::UtxoEntry> utxos, const uint256 parentid)
{
    double repostAmount{0};
    for (const auto & utxo : utxos)
        repostAmount += utxo.amount;

    if (utxos.empty() || repostAmount > makerAmount)
        return xbridge::Error::INSIFFICIENT_FUNDS; // do not repost an order with insufficient funds

    WalletConnectorPtr connFrom = connectorByCurrency(fromCurrency);
    WalletConnectorPtr connTo = connectorByCurrency(toCurrency);
    if (!connFrom || !connTo) {
        WARN() << "no session for <" << (connFrom ? toCurrency : fromCurrency) << "> " << __FUNCTION__;
        return xbridge::Error::NO_SESSION;
    }
    if (connFrom->isDustAmount(repostAmount))
        return xbridge::Error::DUST;

    auto newRepostAmount = xBridgeAmountFromReal(repostAmount);
    const auto fee1 = xBridgeAmountFromReal(connFrom->minTxFee1(1, 3));
    const auto fee2 = xBridgeAmountFromReal(connFrom->minTxFee2(1, 1));
    newRepostAmount -= (fee1 + fee2) * utxos.size();
    const bool usePartial = newRepostAmount > minFromAmount;

    // Calculate new to amount (destination amount).
    const CAmount toAmount = xBridgeDestAmountFromPrice(newRepostAmount, makerAmount, takerAmount);

    // Check the params (checks for valid amount)
    const auto statusCode = checkCreateParams(fromCurrency, toCurrency, newRepostAmount, from);
    if (statusCode != xbridge::SUCCESS)
        return statusCode;

    uint256 id, blockHash;
    return sendXBridgeTransaction(from, fromCurrency, newRepostAmount, to, toCurrency, toAmount, utxos,
                                  usePartial, usePartial, minFromAmount, id, blockHash, parentid);
}

//******************************************************************************
//******************************************************************************
xbridge::Error App::sendXBridgeTransaction(const std::string & from,
                                           const std::string & fromCurrency,
                                           const CAmount & fromAmount,
                                           const std::string & to,
                                           const std::string & toCurrency,
                                           const CAmount & toAmount,
                                           uint256 & id,
                                           uint256 & blockHash)
{
    return sendXBridgeTransaction(from, fromCurrency, fromAmount, to, toCurrency, toAmount, std::vector<wallet::UtxoEntry>{}, false, false, 0, id, blockHash);
}

//******************************************************************************
//******************************************************************************
xbridge::Error App::sendXBridgeTransaction(const std::string & from,
                                           const std::string & fromCurrency,
                                           const CAmount & fromAmount,
                                           const std::string & to,
                                           const std::string & toCurrency,
                                           const CAmount & toAmount,
                                           const std::vector<wallet::UtxoEntry> utxos,
                                           const bool partialOrder,
                                           const bool repostOrder,
                                           const CAmount partialMinimum,
                                           uint256 & id,
                                           uint256 & blockHash,
                                           const uint256 parentid)
{
    // search for service node
    std::set<std::string> currencies{fromCurrency, toCurrency};
    CPubKey snode;
    std::set<CPubKey> notIn;
    if (!findNodeWithService(currencies, snode, notIn))
    {
        ERR() << "Failed to find servicenode for pair " << boost::algorithm::join(currencies, ",") << " "
              << __FUNCTION__;
        return xbridge::Error::NO_SERVICE_NODE;
    }

    auto pmn = sn::ServiceNodeMgr::instance().getSn(snode);
    if (pmn.isNull()) {
        if (snode.Decompress()) // try to uncompress pubkey and search
            pmn = sn::ServiceNodeMgr::instance().getSn(snode);
        if (pmn.isNull()) {
            ERR() << "Failed to find servicenode for pair " << boost::algorithm::join(currencies, ",") << " "
                  << " servicenode in xwallets is not in servicenode list " << __FUNCTION__;
            return xbridge::NO_SERVICE_NODE;
        }
    }

    std::vector<unsigned char> snodeAddress(20);
    std::vector<unsigned char> sPubKey(33);
    CKeyID snodeID = snode.GetID();
    std::copy(snodeID.begin(), snodeID.end(), snodeAddress.begin());

    if (!snode.IsCompressed()) {
        snode.Compress();
    }
    sPubKey = std::vector<unsigned char>(snode.begin(), snode.end());

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

    if (connFrom->isDustAmount(xBridgeValueFromAmount(fromAmount)))
        return xbridge::Error::DUST;

    if (connTo->isDustAmount(xBridgeValueFromAmount(toAmount)))
        return xbridge::Error::DUST;

    if (partialOrder && connFrom->isDustAmount(xBridgeValueFromAmount(partialMinimum))) {
        WARN() << "partial order minimum is dust <" << fromCurrency << "> " << __FUNCTION__;
        return xbridge::Error::DUST;
    }

    int partialUtxosRequiredForMinimum{0};
    bool partialRemainderRequired{false};
    CAmount partialVoutsTotal{0};
    CAmount partialPerUtxoFees{0};
    CAmount partialRemainderVoutTotal{0};
    bool partialRemainderIsDust{false};
    int partialOrderVouts{0};
    bool partialExactUtxoMatch{false};
    if (partialOrder && utxos.empty()) {
        // Partial order support
        partialUtxosRequiredForMinimum = static_cast<int>(fromAmount / partialMinimum);
        if (partialUtxosRequiredForMinimum > xBridgePartialOrderMaxUtxos) {
            partialUtxosRequiredForMinimum = xBridgePartialOrderMaxUtxos - 1; // support 1 utxo for excess remainder
            partialRemainderRequired = true;
        } else if (fromAmount % partialMinimum != 0)
            partialRemainderRequired = true;

        // Estimated fees if taker were to take the minimum order
        // (i.e. if 1 utxo were to be used to fulfill the partial order)
        CAmount partialFee1 = xBridgeIntFromReal(connFrom->minTxFee1(1, 3));
        CAmount partialFee2 = xBridgeIntFromReal(connFrom->minTxFee2(1, 1));
        partialPerUtxoFees = partialFee1 + partialFee2;
        CAmount partialFees = (partialUtxosRequiredForMinimum + (partialRemainderRequired ? 1 : 0)) * partialPerUtxoFees;

        CAmount partialSplitVoutsTotal = partialUtxosRequiredForMinimum * partialMinimum;
        if (fromAmount - partialSplitVoutsTotal < 0) {
            WARN() << "insufficient funds for partial order <" << fromCurrency << "> " << __FUNCTION__;
            return xbridge::Error::INSIFFICIENT_FUNDS;
        }
        partialRemainderVoutTotal = fromAmount - partialSplitVoutsTotal;
        if (partialRemainderVoutTotal < 0)
            partialRemainderVoutTotal = 0;
        partialRemainderIsDust = connFrom->isDustAmount(xBridgeValueFromAmount(partialRemainderVoutTotal + partialPerUtxoFees));
        partialVoutsTotal = partialFees + partialSplitVoutsTotal + (partialRemainderRequired && !partialRemainderIsDust ? partialRemainderVoutTotal : 0);
        partialOrderVouts = partialUtxosRequiredForMinimum + (partialRemainderRequired && !partialRemainderIsDust ? 1 : 0);
    }

    TransactionDescrPtr ptr(new TransactionDescr);
    std::vector<wallet::UtxoEntry> outputsForUse;

    // Utxo selection only if supplied utxos are empty
    if (utxos.empty()) {
        LOCK(m_utxosOrderLock);

        // Exclude the used uxtos
        const auto excludedUtxos = getAllLockedUtxos(connFrom->currency);

        // Available utxos from from wallet
        std::vector<wallet::UtxoEntry> outputs;
        connFrom->getUnspent(outputs, excludedUtxos);

        if (partialOrder) {
            const CAmount prepTxFee = xBridgeIntFromReal(connFrom->minTxFee1(10, partialOrderVouts + 1));
            CAmount utxoAmount{0};
            CAmount fees{0};

            // Select utxos
            if (!selectPartialUtxos(from, outputs, fromAmount, partialUtxosRequiredForMinimum, partialPerUtxoFees, prepTxFee,
                    partialMinimum, partialRemainderVoutTotal, outputsForUse, utxoAmount, fees, partialExactUtxoMatch)) {
                WARN() << "partial order insufficient funds for <" << fromCurrency << "> " << __FUNCTION__;
                return xbridge::Error::INSIFFICIENT_FUNDS;
            }

            {
                UniValue log_obj(UniValue::VOBJ);
                log_obj.pushKV("currency", from);
                log_obj.pushKV("partial_fees", xBridgeValueFromAmount(fees));
                log_obj.pushKV("utxos_amount", xBridgeValueFromAmount(utxoAmount));
                log_obj.pushKV("required_amount", xBridgeValueFromAmount(fromAmount + fees));
                log_obj.pushKV("utxo_count", (int)outputsForUse.size());
                xbridge::LogOrderMsg(log_obj, "partial order utxo selection details for order", __FUNCTION__);
            }

        } else {
            uint64_t utxoAmount = 0;
            uint64_t fee1 = 0;
            uint64_t fee2 = 0;

            // fee calcs should incorporate partial order splitting (i.e. number of splits * fee for single order)
            auto minTxFee1 = [&connFrom](const uint32_t & inputs, const uint32_t & outputs) -> double {
                return connFrom->minTxFee1(inputs, outputs);
            };
            auto minTxFee2 = [&connFrom](const uint32_t & inputs, const uint32_t & outputs) -> double {
                return connFrom->minTxFee2(inputs, outputs);
            };

            // Select utxos
            if (!selectUtxos(from, outputs, minTxFee1, minTxFee2, fromAmount,
                             TransactionDescr::COIN, outputsForUse, utxoAmount, fee1, fee2))
            {
                WARN() << "insufficient funds for <" << fromCurrency << "> " << __FUNCTION__;
                return xbridge::Error::INSIFFICIENT_FUNDS;
            }

            {
                UniValue log_obj(UniValue::VOBJ);
                log_obj.pushKV("currency", from);
                log_obj.pushKV("fee1", (static_cast<double>(fee1) / TransactionDescr::COIN));
                log_obj.pushKV("fee2", (static_cast<double>(fee2) / TransactionDescr::COIN));
                log_obj.pushKV("utxos_amount", (static_cast<double>(utxoAmount) / TransactionDescr::COIN));
                log_obj.pushKV("required_amount", (static_cast<double>(fromAmount + fee1 + fee2) / TransactionDescr::COIN));
                xbridge::LogOrderMsg(log_obj, "utxo selection details for order", __FUNCTION__);
            }
        }

    } else {
        outputsForUse = utxos;
    }

    // sign used coins
    for (auto & entry : outputsForUse) {
        std::string signature;
        if (!connFrom->signMessage(entry.address, entry.toString(), signature)) {
            WARN() << "funds not signed <" << fromCurrency << "> " << __FUNCTION__;
            return xbridge::Error::FUNDS_NOT_SIGNED;
        }

        bool isInvalid = false;
        entry.signature = DecodeBase64(signature.c_str(), &isInvalid);
        if (isInvalid) {
            WARN() << "invalid signature <" << fromCurrency << "> " << __FUNCTION__;
            return xbridge::Error::FUNDS_NOT_SIGNED;
        }

        entry.rawAddress = connFrom->toXAddr(entry.address);

        if (entry.signature.size() != 65) {
            ERR() << "incorrect signature length, need 65 bytes " << __FUNCTION__;
            return xbridge::Error::INVALID_SIGNATURE;
        }
        xassert(entry.signature.size() == 65 && "incorrect signature length, need 20 bytes");
        if (entry.rawAddress.size() != 20) {
            ERR() << "incorrect raw address length, need 20 bytes " << __FUNCTION__;
            return xbridge::Error::INVALID_ADDRESS;
        }
        xassert(entry.rawAddress.size() == 20 && "incorrect raw address length, need 20 bytes");
    }

    ptr->usedCoins = outputsForUse;

    // lock used coins only if specified utxos is empty (assuming those are already locked)
    if (ptr->usedCoins.empty() || (utxos.empty() && !lockCoins(connFrom->currency, ptr->usedCoins))) {
        ERR() << "failed to create order, cannot reuse utxo inputs for " << connFrom->currency
              << " across multiple orders " << __FUNCTION__;
        return xbridge::Error::INSIFFICIENT_FUNDS;
    }

    boost::posix_time::ptime timestamp = boost::posix_time::microsec_clock::universal_time();
    uint64_t timestampValue = timeToInt(timestamp);

    {
        LOCK(cs_main);
        blockHash = chainActive.Tip()->pprev->GetBlockHash();
    }

    ptr->hubAddress   = snodeAddress;
    ptr->sPubKey      = sPubKey;
    ptr->created      = timestamp;
    ptr->txtime       = timestamp;
    ptr->fromAddr     = from;
    ptr->from         = connFrom->toXAddr(from);
    ptr->fromCurrency = fromCurrency;
    ptr->fromAmount   = fromAmount;
    ptr->origFromCurrency = fromCurrency;
    ptr->origFromAmount = fromAmount;
    ptr->toAddr       = to;
    ptr->to           = connTo->toXAddr(to);
    ptr->toCurrency   = toCurrency;
    ptr->toAmount     = toAmount;
    ptr->origToCurrency = toCurrency;
    ptr->origToAmount = toAmount;
    ptr->blockHash    = blockHash;
    ptr->role         = 'A';

    CHashWriter ss(SER_GETHASH, 0);
    ss << ptr->from
       << ptr->fromCurrency
       << ptr->fromAmount
       << ptr->to
       << ptr->toCurrency
       << ptr->toAmount
       << timestampValue
       << ptr->blockHash
       << outputsForUse.at(0).signature;
    id = ss.GetHash();
    ptr->id = id; // overwritten by partial order designation below
    ptr->setParentOrder(parentid);

    // Create partial order utxos
    if (partialOrder) {
        ptr->minFromAmount = partialMinimum;
        ptr->repostOrder = repostOrder;
        ptr->allowPartialOrders();

        if (utxos.empty()) {
            if (partialExactUtxoMatch) {
                if (ptr->usedCoins.size() > xBridgePartialOrderMaxUtxos) {
                    unlockCoins(ptr->fromCurrency, ptr->usedCoins);
                    UniValue log_obj(UniValue::VOBJ);
                    log_obj.pushKV("orderid", "unknown");
                    log_obj.pushKV("from_currency", connFrom->currency);
                    xbridge::LogOrderMsg(log_obj, "failed to create order, the maximum number of utxos on the order was exceeded", __FUNCTION__);
                    return xbridge::Error::INVALID_AMOUNT;
                }
            } else { // If no user supplied utxos, create the partial order prep transaction
                std::vector<wallet::UtxoEntry> existingUtxos;
                double vinsTotal{0};
                std::vector<xbridge::XTxIn> vins;
                for (const auto & vin : ptr->usedCoins) {
                    // If we already have exact utxos, skip consuming those and subtract from expected total
                    if (vin.camount() == partialMinimum + partialPerUtxoFees && partialUtxosRequiredForMinimum > 0) {
                        existingUtxos.push_back(vin);
                        partialUtxosRequiredForMinimum--;
                        partialVoutsTotal -= partialMinimum + partialPerUtxoFees;
                        continue;
                    }
                    vinsTotal += vin.amount;
                    vins.emplace_back(vin.txId, vin.vout, vin.amount);
                }

                std::vector<std::pair<std::string, double>> vouts;
                for (int i = 0; i < partialUtxosRequiredForMinimum; ++i)
                    vouts.emplace_back(ptr->fromAddr, xBridgeValueFromAmount(partialMinimum + partialPerUtxoFees));
                // add remainder vout if not dust
                if (partialRemainderRequired && !partialRemainderIsDust)
                    vouts.emplace_back(ptr->fromAddr, xBridgeValueFromAmount(partialRemainderVoutTotal + partialPerUtxoFees));
                // Change
                const double changeAmount = vinsTotal - xBridgeValueFromAmount(partialVoutsTotal) - connFrom->minTxFee1(vins.size(), vouts.size()+1); // vouts + 1 for change
                if (changeAmount < std::numeric_limits<double>::epsilon()) {
                    unlockCoins(ptr->fromCurrency, ptr->usedCoins);
                    UniValue log_obj(UniValue::VOBJ);
                    log_obj.pushKV("orderid", "unknown");
                    log_obj.pushKV("change_amount", xBridgeStringValueFromPrice(changeAmount, connFrom->COIN));
                    log_obj.pushKV("from_currency", connFrom->currency);
                    xbridge::LogOrderMsg(log_obj, "failed to create order, insufficient funds on partial order", __FUNCTION__);
                    return xbridge::Error::INVALID_AMOUNT;
                }
                if (!connFrom->isDustAmount(changeAmount))
                    vouts.emplace_back(ptr->fromAddr, changeAmount);

                std::string txid, rawtx;
                if (!connFrom->createPartialTransaction(vins, vouts, txid, rawtx)) {
                    unlockCoins(ptr->fromCurrency, ptr->usedCoins);
                    UniValue log_obj(UniValue::VOBJ);
                    log_obj.pushKV("orderid", "unknown");
                    log_obj.pushKV("from_currency", connFrom->currency);
                    log_obj.pushKV("partial_prep_tx", rawtx);
                    xbridge::LogOrderMsg(log_obj, "failed to create order, cannot create partial order utxos",
                                         __FUNCTION__);
                    return xbridge::Error::UNKNOWN_ERROR;
                }
                std::string sentid;
                int32_t errCode{0};
                std::string errMessage;
                if (!connFrom->sendRawTransaction(rawtx, sentid, errCode, errMessage)) {
                    unlockCoins(ptr->fromCurrency, ptr->usedCoins);
                    UniValue log_obj(UniValue::VOBJ);
                    log_obj.pushKV("orderid", "unknown");
                    log_obj.pushKV("from_currency", connFrom->currency);
                    log_obj.pushKV("partial_prep_tx", rawtx);
                    xbridge::LogOrderMsg(log_obj, "failed to create order, cannot submit partial order utxos transaction",
                                         __FUNCTION__);
                    return xbridge::Error::UNKNOWN_ERROR;
                }
                // Assign the prep tx id
                ptr->partialOrderPrepTx = txid;

                unlockCoins(ptr->fromCurrency, ptr->usedCoins);
                ptr->clearUsedCoins();
                ptr->usedCoins = existingUtxos; // add existing utxos

                CAmount partialNewTotalUtxosAmount{0};
                for (int i = 0; i < vouts.size(); ++i) {
                    xbridge::wallet::UtxoEntry entry;
                    entry.txId = txid;
                    entry.vout = i;
                    entry.amount = vouts[i].second;
                    entry.address = connFrom->fromXAddr(connFrom->toXAddr(vouts[i].first));
                    ptr->usedCoins.push_back(entry);
                    partialNewTotalUtxosAmount += entry.camount();
                    if (partialVoutsTotal - partialNewTotalUtxosAmount <= 0)
                        break; // only need enough utxos to cover partial order (use 1 sat for rounding errors)
                }

                if (ptr->usedCoins.size() > xBridgePartialOrderMaxUtxos) {
                    unlockCoins(ptr->fromCurrency, ptr->usedCoins);
                    UniValue log_obj(UniValue::VOBJ);
                    log_obj.pushKV("orderid", "unknown");
                    log_obj.pushKV("from_currency", connFrom->currency);
                    xbridge::LogOrderMsg(log_obj, "failed to create order, the maximum number of utxos on the order was exceeded", __FUNCTION__);
                    return xbridge::Error::INVALID_AMOUNT;
                }

                if (ptr->usedCoins.empty() || !lockCoins(ptr->fromCurrency, ptr->usedCoins)) {
                    unlockCoins(ptr->fromCurrency, ptr->usedCoins);
                    UniValue log_obj(UniValue::VOBJ);
                    log_obj.pushKV("orderid", "unknown");
                    log_obj.pushKV("from_currency", connFrom->currency);
                    xbridge::LogOrderMsg(log_obj, "failed to create order, cannot lock partial order utxos", __FUNCTION__);
                    return xbridge::Error::INVALID_PARTIAL_ORDER;
                }

                // sign used coins
                for (auto & entry : ptr->usedCoins) {
                    std::string signature;
                    if (!connFrom->signMessage(entry.address, entry.toString(), signature)) {
                        unlockCoins(ptr->fromCurrency, ptr->usedCoins);
                        UniValue log_obj(UniValue::VOBJ);
                        log_obj.pushKV("orderid", "unknown");
                        log_obj.pushKV("from_currency", fromCurrency);
                        xbridge::LogOrderMsg(log_obj, "funds not signed", __FUNCTION__);
                        return xbridge::Error::FUNDS_NOT_SIGNED;
                    }

                    bool isInvalid = false;
                    entry.signature = DecodeBase64(signature.c_str(), &isInvalid);
                    if (isInvalid) {
                        unlockCoins(ptr->fromCurrency, ptr->usedCoins);
                        UniValue log_obj(UniValue::VOBJ);
                        log_obj.pushKV("orderid", "unknown");
                        log_obj.pushKV("from_currency", fromCurrency);
                        xbridge::LogOrderMsg(log_obj, "invalid signature", __FUNCTION__);
                        return xbridge::Error::FUNDS_NOT_SIGNED;
                    }

                    entry.rawAddress = connFrom->toXAddr(entry.address);

                    if (entry.signature.size() != 65) {
                        unlockCoins(ptr->fromCurrency, ptr->usedCoins);
                        UniValue log_obj(UniValue::VOBJ);
                        log_obj.pushKV("orderid", "unknown");
                        log_obj.pushKV("from_currency", fromCurrency);
                        xbridge::LogOrderMsg(log_obj, "incorrect signature length, need 65 bytes", __FUNCTION__);
                        return xbridge::Error::INVALID_SIGNATURE;
                    }
                    if (entry.rawAddress.size() != 20) {
                        unlockCoins(ptr->fromCurrency, ptr->usedCoins);
                        UniValue log_obj(UniValue::VOBJ);
                        log_obj.pushKV("orderid", "unknown");
                        log_obj.pushKV("from_currency", fromCurrency);
                        xbridge::LogOrderMsg(log_obj, "incorrect raw address length, need 20 bytes", __FUNCTION__);
                        return xbridge::Error::INVALID_ADDRESS;
                    }
                }

                CHashWriter ss2(SER_GETHASH, 0);
                ss2 << ptr->from
                    << ptr->fromCurrency
                    << ptr->fromAmount
                    << ptr->to
                    << ptr->toCurrency
                    << ptr->toAmount
                    << timestampValue
                    << ptr->blockHash
                    << ptr->usedCoins.at(0).signature;
                id = ss2.GetHash();

                ptr->id = id;
                ptr->setPartialOrderPending(true);
                {
                    LOCK(m_lock);
                    m_partialOrders.push_back(ptr);
                }
            }
        }
    }

    {
        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", id.GetHex());
        log_obj.pushKV("snode_pubkey", HexStr(pmn.getSnodePubKey()));
        if (!ptr->partialOrderPrepTx.empty())
            log_obj.pushKV("partial_prep_tx", ptr->partialOrderPrepTx);
        xbridge::LogOrderMsg(log_obj, "using servicenode for order", __FUNCTION__);
    }

    // m key
    connTo->newKeyPair(ptr->mPubKey, ptr->mPrivKey);
    assert(ptr->mPubKey.size() == 33 && "bad pubkey size");

    // x key
    connTo->newKeyPair(ptr->xPubKey, ptr->xPrivKey);
    assert(ptr->xPubKey.size() == 33 && "bad pubkey size");

#ifdef LOG_KEYPAIR_VALUES
    TXLOG() << "generated M keypair for order " << ptr->id.ToString() << std::endl <<
             "    pub    " << HexStr(ptr->mPubKey) << std::endl <<
             "    pub id " << HexStr(connTo->getKeyId(ptr->mPubKey)) << std::endl <<
             "    priv   " << HexStr(ptr->mPrivKey);
    TXLOG() << "generated X keypair for order " << ptr->id.ToString() << std::endl <<
             "    pub    " << HexStr(ptr->xPubKey) << std::endl <<
             "    pub id " << HexStr(connTo->getKeyId(ptr->xPubKey)) << std::endl <<
             "    priv   " << HexStr(ptr->xPrivKey);
#endif

    // Add destination address
    updateConnector(connFrom, ptr->from, ptr->fromCurrency);
    updateConnector(connTo, ptr->to, ptr->toCurrency);

    if (!partialOrder || !utxos.empty() || partialExactUtxoMatch) {
        // notify ui about new order
        xuiConnector.NotifyXBridgeTransactionReceived(ptr);

        // try send immediatelly
        m_p->sendPendingTransaction(ptr);

        xbridge::LogOrderMsg(ptr, std::string(__FUNCTION__) + " order created");
    }

    {
        LOCK(m_p->m_txLocker);
        m_p->m_transactions[id] = ptr;
    }

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

    // order details
    packet->append(ptr->id.begin(), 32);
    packet->append(ptr->from);
    packet->append(fc);
    packet->append(ptr->fromAmount);
    packet->append(ptr->to);
    packet->append(tc);
    packet->append(ptr->toAmount);
    packet->append(timeToInt(ptr->created));
    packet->append(ptr->blockHash.begin(), 32);
    // partial order details
    packet->append(uint16_t(ptr->isPartialOrderAllowed()));
    packet->append(ptr->minFromAmount);

    // utxo items
    packet->append(static_cast<uint32_t>(ptr->usedCoins.size()));
    for (const wallet::UtxoEntry & entry : ptr->usedCoins)
    {
        uint256 txid = uint256S(entry.txId);
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
Error App::acceptXBridgeTransaction(const uint256 & id, const std::string & from, const std::string & to,
                                    const uint64_t fromSize, const uint64_t toSize)
{
    TransactionDescrPtr ptr;

    {
        LOCK(m_p->m_txLocker);
        if (!m_p->m_transactions.count(id))
        {
            xbridge::LogOrderMsg(id.GetHex(), "order not found", __FUNCTION__);
            return xbridge::TRANSACTION_NOT_FOUND;
        }
        ptr = m_p->m_transactions[id];
    }

    if (ptr->state >= TransactionDescr::trAccepting) {
        xbridge::LogOrderMsg(id.GetHex(), "not accepting, order already accepted", __FUNCTION__);
        return xbridge::BAD_REQUEST;
    }
    const auto priorState = ptr->state;
    ptr->state = TransactionDescr::trAccepting;
    ptr->fromAmount = fromSize;
    ptr->toAmount = toSize;

    auto revertOrder = [priorState](TransactionDescrPtr & ptr){
        ptr->state = priorState;
        ptr->fromAmount = ptr->origFromAmount;
        ptr->toAmount = ptr->origToAmount;
    };

    WalletConnectorPtr connFrom = connectorByCurrency(ptr->fromCurrency);
    WalletConnectorPtr connTo   = connectorByCurrency(ptr->toCurrency);
    if (!connFrom || !connTo)
    {
        revertOrder(ptr);
        // no session
        xbridge::LogOrderMsg(id.GetHex(), "no wallet session for " + (connFrom ? ptr->fromCurrency : ptr->toCurrency), __FUNCTION__);
        return xbridge::NO_SESSION;
    }

    // check dust
    if (connFrom->isDustAmount(xBridgeValueFromAmount(ptr->fromAmount)))
    {
        revertOrder(ptr);
        return xbridge::Error::DUST;
    }
    if (connTo->isDustAmount(xBridgeValueFromAmount(ptr->toAmount)))
    {
        revertOrder(ptr);
        return xbridge::Error::DUST;
    }

    if (availableBalance() < connTo->serviceNodeFee)
    {
        revertOrder(ptr);
        return xbridge::Error::INSIFFICIENT_FUNDS_DX;
    }

    // service node pub key
    ::CPubKey pksnode;
    {
        uint32_t len = ptr->sPubKey.size();
        if (len != 33) {
            revertOrder(ptr);
            xbridge::LogOrderMsg(id.GetHex(), "not accepting order, bad service node public key length (" + std::to_string(len) + ")", __FUNCTION__);
            return xbridge::Error::NO_SERVICE_NODE;
        }
        pksnode.Set(ptr->sPubKey.begin(), ptr->sPubKey.end());
    }
    // Get servicenode collateral address
    CKeyID snodeCollateralAddress;
    {
        sn::ServiceNode snode = sn::ServiceNodeMgr::instance().getSn(pksnode);
        if (snode.isNull())
        {
            // try to uncompress pubkey and search
            if (pksnode.Decompress())
            {
                snode = sn::ServiceNodeMgr::instance().getSn(pksnode);
            }
            if (snode.isNull())
            {
                revertOrder(ptr);
                // bad service node, no more
                xbridge::LogOrderMsg(id.GetHex(), "not accepting order, unknown service node " + pksnode.GetID().ToString(), __FUNCTION__);
                return xbridge::Error::NO_SERVICE_NODE;
            }
        }

        snodeCollateralAddress = snode.getPaymentAddress();

        UniValue log_obj(UniValue::VOBJ);
        log_obj.pushKV("orderid", id.GetHex());
        log_obj.pushKV("from_currency", ptr->fromCurrency);
        log_obj.pushKV("to_currency", ptr->toCurrency);
        log_obj.pushKV("snode_pubkey", HexStr(snode.getSnodePubKey()));
        xbridge::LogOrderMsg(log_obj, "using service node for order", __FUNCTION__);
    }

    // transaction info
    size_t maxBytes = nMaxDatacarrierBytes-3;

    json_spirit::Array info;
    info.push_back("");
    info.push_back(ptr->fromCurrency);
    info.push_back(ptr->fromAmount);
    info.push_back(ptr->toCurrency);
    info.push_back(ptr->toAmount);
    std::string strInfo = write_string(json_spirit::Value(info));
    info.erase(info.begin());

    // Truncate the order id in situations where we don't have enough space in the tx
    std::string orderId{ptr->id.GetHex()};
    if (strInfo.size() + orderId.size() > maxBytes) {
        auto leftOver = maxBytes - strInfo.size();
        orderId.erase(leftOver, std::string::npos);
    }
    info.insert(info.begin(), orderId); // add order id to the front
    strInfo = write_string(json_spirit::Value(info));
    if (strInfo.size() > maxBytes) { // make sure we're not too large
        revertOrder(ptr);
        return xbridge::Error::INVALID_ONCHAIN_HISTORY;
    }

    auto destScript = GetScriptForDestination(CTxDestination(snodeCollateralAddress));
    auto data = ToByteVector(strInfo);

    // Utxo selection
    {
        LOCK(m_utxosOrderLock);

        // BLOCK available p2pkh utxos
        std::vector<wallet::UtxoEntry> feeOutputs;
        if (!rpc::unspentP2PKH(feeOutputs)) {
            revertOrder(ptr);
            xbridge::LogOrderMsg(id.GetHex(), "insufficient BLOCK funds for service node fee payment", __FUNCTION__);
            return xbridge::Error::INSIFFICIENT_FUNDS;
        }

        // Exclude the used uxtos
        auto excludedUtxos = getAllLockedUtxos(connFrom->currency);

        // Exclude utxos matching fee inputs
        feeOutputs.erase(
            std::remove_if(feeOutputs.begin(), feeOutputs.end(), [&excludedUtxos](const xbridge::wallet::UtxoEntry & u) {
                return excludedUtxos.count(u);
            }),
            feeOutputs.end()
        );

        double blockFeePerByte = 40 / static_cast<double>(COIN);
        if (!rpc::createFeeTransaction(destScript, connFrom->serviceNodeFee, blockFeePerByte,
                data, feeOutputs, ptr->feeUtxos, ptr->rawFeeTx))
        {
            revertOrder(ptr);
            xbridge::LogOrderMsg(id.GetHex(), "order not accepted, failed to prepare the service node fee", __FUNCTION__);
            return xbridge::Error::INSIFFICIENT_FUNDS;
        }

        // Lock the fee utxos
        lockFeeUtxos(ptr->feeUtxos);

        // Exclude the used uxtos
        excludedUtxos = getAllLockedUtxos(connFrom->currency);

//        std::cout << "All utxos: " << outputs.size() << std::endl;
//        std::cout << "Exclude utxos: " << excludeUtxos.size() << std::endl;
//        for (const auto & utxo : excludeUtxos)
//            std::cout << "    Exclude: " << utxo.txId << " " << utxo.vout << std::endl;

        // Available utxos from from wallet
        std::vector<wallet::UtxoEntry> outputs;
        connFrom->getUnspent(outputs, excludedUtxos);

//        std::cout << "After exclude: " << outputs.size() << std::endl;
//        for (const auto & utxo : outputs)
//            std::cout << "    After: " << utxo.txId << " " << utxo.vout << std::endl;

        uint64_t utxoAmount = 0;
        uint64_t fee1       = 0;
        uint64_t fee2       = 0;

        auto minTxFee1 = [&connFrom](const uint32_t & inputs, const uint32_t & outputs) -> double {
            return connFrom->minTxFee1(inputs, outputs);
        };
        auto minTxFee2 = [&connFrom](const uint32_t & inputs, const uint32_t & outputs) -> double {
            return connFrom->minTxFee2(inputs, outputs);
        };

        // Select utxos
        std::vector<wallet::UtxoEntry> outputsForUse;
        if (!selectUtxos(from, outputs, minTxFee1, minTxFee2, ptr->fromAmount,
                         TransactionDescr::COIN, outputsForUse, utxoAmount, fee1, fee2))
        {
            revertOrder(ptr);
            xbridge::LogOrderMsg(id.GetHex(), "not accepting order, insufficient funds for <" + ptr->fromCurrency + ">", __FUNCTION__);
            unlockFeeUtxos(ptr->feeUtxos);
            return xbridge::Error::INSIFFICIENT_FUNDS;
        }

        // sign used coins
        for (wallet::UtxoEntry & entry : outputsForUse)
        {
            xbridge::Error err = xbridge::Error::SUCCESS;
            std::string signature;
            if (!connFrom->signMessage(entry.address, entry.toString(), signature))
            {
                xbridge::LogOrderMsg(id.GetHex(), "not accepting order, funds not signed <" + ptr->fromCurrency + ">", __FUNCTION__);
                err = xbridge::Error::FUNDS_NOT_SIGNED;
            }

            bool isInvalid = false;
            entry.signature = DecodeBase64(signature.c_str(), &isInvalid);
            if (isInvalid)
            {
                xbridge::LogOrderMsg(id.GetHex(), "not accepting order, invalid signature <" + ptr->fromCurrency + ">", __FUNCTION__);
                err = xbridge::Error::FUNDS_NOT_SIGNED;
            }

            entry.rawAddress = connFrom->toXAddr(entry.address);
            if(entry.signature.size() != 65)
            {
                xbridge::LogOrderMsg(id.GetHex(), "not accepting order, incorrect signature length, need 65 bytes", __FUNCTION__);
                err = xbridge::Error::INVALID_SIGNATURE;
            }

            if(entry.rawAddress.size() != 20)
            {
                xbridge::LogOrderMsg(id.GetHex(), "not accepting order, incorrect raw address length, need 20 bytes", __FUNCTION__);
                err = xbridge::Error::INVALID_ADDRESS;
            }

            if (err) {
                revertOrder(ptr);
                // unlock fee utxos on error
                unlockFeeUtxos(ptr->feeUtxos);
                return err;
            }
        }

//        std::cout << "Selected Order: " << ptr->id.ToString() << std::endl;
//        for (const auto & utxo : outputsForUse)
//            std::cout << "    Selected: " << utxo.txId << " " << utxo.vout << std::endl;
        ptr->usedCoins = outputsForUse;

        // lock used coins
        if (!lockCoins(connFrom->currency, ptr->usedCoins)) {
            revertOrder(ptr);
            xbridge::LogOrderMsg(id.GetHex(), "not accepting order, cannot reuse utxo inputs for " + connFrom->currency +
                                     " across multiple orders ", __FUNCTION__);
            return xbridge::Error::INSIFFICIENT_FUNDS;
        }
    }

    // Obtain the block heights and hashes from both tokens involved in the order.
    uint32_t fromBlockHeight;
    std::string fromBlockHash;
    uint32_t toBlockHeight;
    std::string toBlockHash;
    if (!connFrom->getBlockCount(fromBlockHeight) || !connFrom->getBlockHash(fromBlockHeight, fromBlockHash)
        || !connTo->getBlockCount(toBlockHeight) || !connTo->getBlockHash(toBlockHeight, toBlockHash))
    {
        revertOrder(ptr);
        unlockCoins(connFrom->currency, ptr->usedCoins);
        unlockFeeUtxos(ptr->feeUtxos);
        ptr->clearUsedCoins();
        return xbridge::Error::NO_SESSION;
    }

    ptr->fromAddr  = from;
    ptr->from      = connFrom->toXAddr(from);
    ptr->toAddr    = to;
    ptr->to        = connTo->toXAddr(to);
    ptr->role      = 'B';

    // m key
    connTo->newKeyPair(ptr->mPubKey, ptr->mPrivKey);
    assert(ptr->mPubKey.size() == 33 && "bad pubkey size");

#ifdef LOG_KEYPAIR_VALUES
    TXLOG() << "generated M keypair for order " << ptr->id.ToString() << std::endl <<
             "    pub    " << HexStr(ptr->mPubKey) << std::endl <<
             "    pub id " << HexStr(connTo->getKeyId(ptr->mPubKey)) << std::endl <<
             "    priv   " << HexStr(ptr->mPrivKey);
#endif

    // Add destination address
    updateConnector(connFrom, ptr->from, ptr->fromCurrency);
    updateConnector(connTo, ptr->to, ptr->toCurrency);

    // try send immediatelly
    m_p->sendAcceptingTransaction(ptr, fromBlockHeight, toBlockHeight, fromBlockHash, toBlockHash);
    xbridge::LogOrderMsg(ptr, std::string(__FUNCTION__) + " order accepted");

    return xbridge::Error::SUCCESS;
}

//******************************************************************************
//******************************************************************************
bool App::Impl::sendAcceptingTransaction(const TransactionDescrPtr & ptr, uint32_t fromBlockHeight, uint32_t toBlockHeight,
                                         const std::string & fromBlockHash, const std::string & toBlockHash)
{
    XBridgePacketPtr packet(new XBridgePacket(xbcTransactionAccepting));

    // field length must be 8 bytes
    std::vector<unsigned char> fc(8, 0);
    std::copy(ptr->fromCurrency.begin(), ptr->fromCurrency.end(), fc.begin());

    // field length must be 8 bytes
    std::vector<unsigned char> tc(8, 0);
    std::copy(ptr->toCurrency.begin(), ptr->toCurrency.end(), tc.begin());

    // first 8 bytes of block hash
    std::vector<unsigned char> fromhash(8, 0);
    std::memcpy(&fromhash[0], &fromBlockHash[0], fromhash.size());
    // first 8 bytes of block hash
    std::vector<unsigned char> tohash(8, 0);
    std::memcpy(&tohash[0], &toBlockHash[0], tohash.size());

    // 20 bytes - id of transaction
    // 2x
    //  4 bytes - snode fee tx size
    //  n bytes - snode fee tx hex
    // 20 bytes - address
    //  8 bytes - currency
    //  4 bytes - amount
    //  4 bytes - block height
    //  8 bytes - block hash
    packet->append(ptr->hubAddress);
    packet->append(ptr->id.begin(), 32);
    auto sfeetx = ParseHex(ptr->rawFeeTx);
    packet->append(uint32_t(sfeetx.size())); // snode fee tx size
    packet->append(sfeetx); // snode fee tx
    packet->append(ptr->from);
    packet->append(fc);
    packet->append(ptr->fromAmount);
    packet->append(fromBlockHeight);
    packet->append(fromhash);
    packet->append(ptr->to);
    packet->append(tc);
    packet->append(ptr->toAmount);
    packet->append(toBlockHeight);
    packet->append(tohash);

    // utxo items
    packet->append(static_cast<uint32_t>(ptr->usedCoins.size()));
    for (const wallet::UtxoEntry & entry : ptr->usedCoins)
    {
        uint256 txid = uint256S(entry.txId);
        packet->append(txid.begin(), 32);
        packet->append(entry.vout);
        packet->append(entry.rawAddress);
        packet->append(entry.signature);
    }

    packet->sign(ptr->mPubKey, ptr->mPrivKey);

    onSend(ptr->hubAddress, packet->body());

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

    xbridge::SessionPtr session = m_p->getSession();
    session->sendCancelTransaction(ptr, reason);

    return xbridge::SUCCESS;
}

//******************************************************************************
//******************************************************************************
void App::cancelMyXBridgeTransactions()
{
    // If service node cancel all open orders
    Exchange & e = Exchange::instance();
    if (e.isStarted()) {
        xbridge::SessionPtr session = m_p->getSession();
        if (!session)
            return;
        auto txs = e.pendingTransactions();
        for (auto & tx : txs)
            session->sendCancelTransaction(tx, crTimeout);
        return;
    }

    // Local orders (traders)
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
bool App::isValidAddress(const std::string & address, WalletConnectorPtr & conn) const
{
    return (address.size() >= 32 && conn->isValidAddress(address));
}

//******************************************************************************
//******************************************************************************
Error App::checkAcceptParams(const std::string fromCurrency, const uint64_t fromAmount) {
    return checkAmount(fromCurrency, fromAmount, "");
}

//******************************************************************************
//******************************************************************************
Error App::checkCreateParams(const std::string & fromCurrency,
                             const std::string & toCurrency,
                             const uint64_t    & fromAmount,
                             const std::string & /*fromAddress*/)
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
Error App::checkAmount(const std::string & currency,
                       const uint64_t    & amount,
                       const std::string & address)
{
    // check amount
    WalletConnectorPtr conn = connectorByCurrency(currency);
    if (!conn) {
        // no session
        WARN() << "no session for <" << currency << "> " << __FUNCTION__;
        return xbridge::NO_SESSION;
    }

    // Check that wallet balance is larger than the smallest supported balance
    const auto & excluded = getAllLockedUtxos(currency);
    if (conn->getWalletBalance(excluded, address) < (static_cast<double>(amount) / TransactionDescr::COIN)) {
        WARN() << "insufficient funds for <" << currency << "> " << __FUNCTION__;
        return xbridge::INSIFFICIENT_FUNDS;
    }
    return xbridge::SUCCESS;
}

//******************************************************************************
//******************************************************************************
/**
 * Store list of orders to watch for counterparty spent deposit.
 * @param tr
 * @return true if watching, false if not watching
 */
bool App::watchForSpentDeposit(TransactionDescrPtr tr) {
    if (tr == nullptr)
        return false;
    LOCK(m_p->m_watchDepositsLocker);
    tr->setWatchingForSpentDeposit(true);
    m_p->m_watchDeposits[tr->id] = tr;
    return true;
}

//******************************************************************************
//******************************************************************************
/**
 * Stop watching for a spent deposit.
 * @param tr
 */
void App::unwatchSpentDeposit(TransactionDescrPtr tr) {
    if (tr == nullptr)
        return;
    LOCK(m_p->m_watchDepositsLocker);
    m_p->m_watchDeposits.erase(tr->id);
    tr->setWatchingForSpentDeposit(false);
}

//******************************************************************************
//******************************************************************************
/**
 * Store list of orders to watch for redeeming refund on behalf of trader.
 * @param tr
 * @return true if watching, false if not watching
 */
bool App::watchTraderDeposit(TransactionPtr tr) {
    if (tr == nullptr)
        return false;
    LOCK(m_p->m_watchTradersLocker);
    m_p->m_watchTraders[tr->id()] = tr;
    return true;
}

//******************************************************************************
//******************************************************************************
/**
 * Stop watching for a trader redeeming.
 * @param tr
 */
void App::unwatchTraderDeposit(TransactionPtr tr) {
    if (tr == nullptr)
        return;
    LOCK(m_p->m_watchTradersLocker);
    m_p->m_watchTraders.erase(tr->id());
}

/**
 * Return this node's services.
 * @param includeXRouter defaults to true
 * @return
 */
std::vector<std::string> App::myServices(const bool includeXRouter) const {
    Exchange & e = Exchange::instance();
    std::set<std::string> services; // weed out duplicates

    // Add xbridge connected wallets
    if (e.isStarted()) {
        std::map<std::string, bool> nodup;
        const auto & wallets = e.connectedWallets();
        for (const auto & wallet : wallets)
            nodup[wallet] = hasCurrency(wallet);
        // All services
        for (const auto &item : nodup) {
            if (item.second) // only show enabled wallets
                services.insert(item.first);
        }
    }

    // Add xrouter wallets and plugins
    if (includeXRouter && xrouter::App::isEnabled() && xrouter::App::instance().isReady()) {
        auto & xrapp = xrouter::App::instance();
        const auto & wallets = xrapp.xrSettings()->getWallets();
        for (const auto & wallet : wallets)
            services.insert(xrouter::walletCommandKey(wallet));
        const auto & plugins = xrapp.xrSettings()->getPlugins();
        for (const auto & plugin : plugins)
            services.insert(xrouter::pluginCommandKey(plugin));
    }

    return std::move(std::vector<std::string>{services.begin(), services.end()});
}

/**
 * Return this node's services.
 * @return
 */
std::string App::myServicesJSON() const {
    json_spirit::Array xwallets;
    const auto & services = myServices(false); // do not include xrouter here (xrouter included below)
    for (const auto & service : services)
        xwallets.push_back(service);

    for (const auto & service : utxwallets) // add unit test supplied services
        xwallets.push_back(service);

    json_spirit::Object result;
    json_spirit::Value xrouterConfigVal;
    if (xrouter::App::isEnabled() && xrouter::App::instance().isReady()) {
        auto & xrapp = xrouter::App::instance();
        const std::string & xrouterConfig = xrapp.parseConfig(xrapp.xrSettings());
        json_spirit::read_string(xrouterConfig, xrouterConfigVal);
    }
    result.emplace_back("xrouterversion", static_cast<int>(XROUTER_PROTOCOL_VERSION));
    result.emplace_back("xbridgeversion", static_cast<int>(version()));
    result.emplace_back("xrouter", xrouterConfigVal);
    result.emplace_back("xbridge", xwallets);
    return json_spirit::write_string(json_spirit::Value(result), json_spirit::none, 8);
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
    if (!sn::ServiceNodeMgr::instance().hasActiveSn())
        return false;
    return m_p->hasNodeService(sn::ServiceNodeMgr::instance().getActiveSn().key.GetPubKey(), service);
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
std::map<::CPubKey, App::XWallets> App::allServices()
{
    std::map<::CPubKey, App::XWallets> ws;
    const auto & snodes = sn::ServiceNodeMgr::instance().list();
    for (const auto & snode : snodes) {
        if (!snode.running())
            continue;
        ws[snode.getSnodePubKey()] = XWallets{
            snode.getXBridgeVersion(), snode.getSnodePubKey(),
            std::set<std::string>{snode.serviceList().begin(), snode.serviceList().end()}
        };
    }
    return std::move(ws);
}

//******************************************************************************
/**
 * @brief Returns all wallets supported by the network. This excludes other non-wallet
 * services (i.e. excludes xrouter services).
 * @return
 */
//******************************************************************************
std::map<::CPubKey, App::XWallets> App::walletServices()
{
    std::regex rwallet("^[^:]+$"); // match wallets
    std::smatch m;
    std::map<::CPubKey, App::XWallets> ws;

    const auto & snodes = sn::ServiceNodeMgr::instance().list();
    for (const auto & snode : snodes) {
        if (!snode.running())
            continue;
        const auto & services = snode.serviceList();
        std::set<std::string> xwallets;
        for (const auto & s : services) {
            if (!std::regex_match(s, m, rwallet) || s == xrouter::xr || s == xrouter::xrs)
                continue;
            xwallets.insert(s);
        }
        App::XWallets & x = ws[snode.getSnodePubKey()];
        ws[snode.getSnodePubKey()] = XWallets{x.version(), x.nodePubKey(), xwallets};
    }

    return std::move(ws);
}

//******************************************************************************
//******************************************************************************
bool App::findNodeWithService(const std::set<std::string> & services,
        CPubKey & node, const std::set<CPubKey> & notIn) const
{
    const uint32_t ver = version();
    auto list = m_p->findShuffledNodesWithService(services, ver, notIn);
    if (list.empty())
        return false;
    node = list.front();
    return true;
}

//******************************************************************************
//******************************************************************************
const std::set<xbridge::wallet::UtxoEntry> App::getFeeUtxos() {
    LOCK(m_utxosLock);
    return m_feeUtxos;
}

//******************************************************************************
//******************************************************************************
void App::lockFeeUtxos(std::set<xbridge::wallet::UtxoEntry> & feeUtxos) {
    LOCK(m_utxosLock);
    m_feeUtxos.insert(feeUtxos.begin(), feeUtxos.end());
}

//******************************************************************************
//******************************************************************************
void App::unlockFeeUtxos(std::set<xbridge::wallet::UtxoEntry> & feeUtxos) {
    LOCK(m_utxosLock);
    for (const auto & utxo : feeUtxos)
        m_feeUtxos.erase(utxo);
}

//******************************************************************************
//******************************************************************************
const std::set<xbridge::wallet::UtxoEntry> App::getLockedUtxos(const std::string & token) {
    LOCK(m_utxosLock);
    const auto & utxos = m_utxosDict[token];
    return utxos;
}

//******************************************************************************
//******************************************************************************
const std::set<xbridge::wallet::UtxoEntry> App::getAllLockedUtxos(const std::string & token) {
    auto & fees = getFeeUtxos();
    auto & other = getLockedUtxos(token);
    std::set<xbridge::wallet::UtxoEntry> all;
    all.insert(fees.begin(), fees.end());
    all.insert(other.begin(), other.end());
    return all;
}

//******************************************************************************
//******************************************************************************
bool App::lockCoins(const std::string & token, const std::vector<wallet::UtxoEntry> & utxos) {
    LOCK(m_utxosLock);

    // Get existing coins
    if (!m_utxosDict.count(token)) {
        std::set<wallet::UtxoEntry> o(utxos.begin(), utxos.end());
        m_utxosDict[token] = o;
        return true;
    }

    // Check if existing utxos are already locked, don't accept this request if so
    auto & o = m_utxosDict[token];
    for (const wallet::UtxoEntry & u : utxos) {
        if (o.count(u))
            return false;
    }

    // Add new utxos
    o.insert(utxos.begin(), utxos.end());

    return true;
}

//******************************************************************************
//******************************************************************************
void App::unlockCoins(const std::string & token, const std::vector<wallet::UtxoEntry> & utxos) {
    LOCK(m_utxosLock);

    // If no existing, ignore
    if (!m_utxosDict.count(token))
        return;

    // Remove utxos if they exist
    auto & o = m_utxosDict[token];
    for (const wallet::UtxoEntry & u : utxos)
        if (o.count(u))
            o.erase(u);
}

//******************************************************************************
//******************************************************************************
bool App::canAffordFeePayment(const CAmount & fee) {
#ifdef ENABLE_WALLET
    const auto & lockedUtxos = getAllLockedUtxos("BLOCK");
    auto coins = availableCoins(true, 1); // at least 1-conf

    CAmount running{0};
    for (const auto & out : coins) {
        wallet::UtxoEntry entry;
        entry.txId = out.first.hash.ToString();
        entry.vout = out.first.n;
        if (!lockedUtxos.count(entry)) {
            running += out.second.nValue;
            if (running >= fee)
                return true;
        }
    }
#endif
    return false;
}

//******************************************************************************
//******************************************************************************
std::vector<CPubKey> App::Impl::findShuffledNodesWithService(
    const std::set<std::string>& requested_services,
    const uint32_t version,
    const std::set<CPubKey> & notIn) const
{
    std::vector<CPubKey> list;
    const auto & snodes = sn::ServiceNodeMgr::instance().list();
    for (const auto& x : snodes)
    {
        if (x.getXBridgeVersion() != version || notIn.count(x.getSnodePubKey()) || !x.running())
            continue;

        // Make sure this xwallet entry is in the servicenode list
        auto pmn = sn::ServiceNodeMgr::instance().getSn(x.getSnodePubKey());
        if (pmn.isNull()) {
            auto k = x.getSnodePubKey();
            if (k.Decompress()) // try to uncompress pubkey and search
                pmn = sn::ServiceNodeMgr::instance().getSn(k);
            if (pmn.isNull())
                continue;
        }

        const auto& wallet_services = std::set<std::string>{x.serviceList().begin(), x.serviceList().end()};
        auto searchCounter = requested_services.size();
        for (const std::string & serv : requested_services)
        {
            if (not wallet_services.count(serv))
                break;
            if (--searchCounter == 0)
                list.push_back(x.getSnodePubKey());
        }
    }
    static std::default_random_engine rng{0};
    std::shuffle(list.begin(), list.end(), rng);
    return list;
}

//******************************************************************************
/**
 * @brief Returns true if the service exists.
 * @param nodePubKey Pubkey of the node
 * @param service Service to search for
 * @param checkRunning Will only return true if the service node is also running.
 * @return True if service is supported, otherwise false
 */
//******************************************************************************
bool App::Impl::hasNodeService(const CPubKey & nodePubKey, const std::string & service, bool checkRunning)
{
    const auto & snode = sn::ServiceNodeMgr::instance().getSn(nodePubKey);
    if (snode.isNull() || (checkRunning && !snode.running()))
        return false;
    return snode.hasService(service);
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
                      const std::function<double(uint32_t, uint32_t)> &minTxFee1,
                      const std::function<double(uint32_t, uint32_t)> &minTxFee2,
                      const uint64_t &requiredAmount, const int64_t &coinDenomination,
                      std::vector<wallet::UtxoEntry> &outputsForUse,
                      uint64_t &utxoAmount, uint64_t &fee1, uint64_t &fee2) const
{
    auto feeAmount = [&minTxFee1,&minTxFee2](const double amt, const uint32_t inputs, const uint32_t outputs) -> double {
        return amt + minTxFee1(inputs, outputs) + minTxFee2(1, 1);
    };

    // Fee utxo selector
    auto selUtxos = [&minTxFee1,&minTxFee2, &addr, &feeAmount](std::vector<xbridge::wallet::UtxoEntry> & a,
                        std::vector<xbridge::wallet::UtxoEntry> & o,
                        const double amt) -> void
    {
        bool done{false};
        std::vector<xbridge::wallet::UtxoEntry> gt;
        std::vector<xbridge::wallet::UtxoEntry> lt;

        // Check ideal, find input that is larger than min amount and within range
        double minAmount{feeAmount(amt, 1, 3)};
        for (const auto & utxo : a) {
            if (utxo.amount >= minAmount
               && utxo.amount < minAmount + (minTxFee1(1, 3) + minTxFee2(1, 1)) * 1000
               && (utxo.address == addr || addr.empty()))
            {
                o.push_back(utxo);
                done = true;
                break;
            }
            else if (utxo.amount >= minAmount)
                gt.push_back(utxo);
            else if (utxo.amount < minAmount)
                lt.push_back(utxo);
        }

        if (done)
            return;

        // Find the smallest input > min amount
        // - or -
        // Find the biggest inputs smaller than min amount that when added is >= min amount
        // - otherwise fail -

        if (gt.size() == 1)
            o.push_back(gt[0]);
        else if (gt.size() > 1) {
            // sort utxos greater than amount (ascending) and pick first
            sort(gt.begin(), gt.end(),
                 [](const xbridge::wallet::UtxoEntry & a, const xbridge::wallet::UtxoEntry & b) {
                     return a.amount < b.amount;
                 });
            o.push_back(gt[0]);
        } else if (lt.size() < 2)
            return; // fail (not enough inputs)
        else {
            // sort inputs less than amount (descending)
            sort(lt.begin(), lt.end(),
                 [](const xbridge::wallet::UtxoEntry & a, const xbridge::wallet::UtxoEntry & b) {
                     return a.amount > b.amount;
                 });

            std::vector<xbridge::wallet::UtxoEntry> sel; // store all selected inputs
            for (const auto & utxo : lt) {
                sel.push_back(utxo);

                // Add amount and incorporate fee calc
                double fee1 = minTxFee1(sel.size(), 3);
                double fee2 = minTxFee2(1, 1);
                double runningAmount{(fee1 + fee2) * -1}; // subtract the fees

                for (auto & u : sel)
                    runningAmount += u.amount;

                if (runningAmount >= minAmount) {
                    o.insert(o.end(), sel.begin(), sel.end()); // only add utxos if we pass threshold
                    break;
                }
            }
        }
    };

    // Find inputs
    std::vector<xbridge::wallet::UtxoEntry> utxos(outputs.begin(), outputs.end());

    // Sort available utxos by amount (descending)
    sort(utxos.begin(), utxos.end(),
         [](const xbridge::wallet::UtxoEntry & a, const xbridge::wallet::UtxoEntry & b) {
             return a.amount > b.amount;
         });

    selUtxos(utxos, outputsForUse, static_cast<double>(requiredAmount)/static_cast<double>(coinDenomination));
    if (outputsForUse.empty())
        return false;

    // Add up all selected utxos in COIN denomination
    for (const auto & utxo : outputsForUse)
        utxoAmount += utxo.amount * coinDenomination;

    // Fees in COIN denomination
    fee1 = minTxFee1(outputsForUse.size(), 3) * coinDenomination;
    fee2 = minTxFee2(1, 1) * coinDenomination;

    return true;
}

bool App::selectPartialUtxos(const std::string & addr, const std::vector<wallet::UtxoEntry> & outputs,
        const CAmount requiredAmount, const int requiredUtxoCount, const CAmount requiredFeePerUtxo,
        const CAmount requiredPrepTxFees, const CAmount requiredSplitSize, const CAmount requiredRemainder,
        std::vector<wallet::UtxoEntry> & outputsForUse, CAmount & utxoAmount, CAmount & fees, bool & exactUtxoMatch) const
{
    utxoAmount = 0;
    fees = 0;
    exactUtxoMatch = false;

    std::vector<wallet::UtxoEntry> utxos(outputs.begin(), outputs.end()); // copy
    CAmount totalAmountNeeded = requiredAmount + fees;
    CAmount totalExactSplitSizeNeeded = (requiredSplitSize + requiredFeePerUtxo) * requiredUtxoCount;

    // Find all ideal utxos (i.e. those matching split size and fees)
    const CAmount requiredSplitSizeAmt = requiredSplitSize + requiredFeePerUtxo;
    for (auto it = utxos.begin(); it != utxos.end(); ) {
        auto & utxo = *it;
        if (utxo.camount() == requiredSplitSizeAmt && utxoAmount < totalExactSplitSizeNeeded) {
            utxoAmount += utxo.camount();
            fees += requiredFeePerUtxo;
            outputsForUse.push_back(utxo);
            it = utxos.erase(it); // remove selected utxo
            continue;
        }
        if (requiredRemainder > 0 && utxo.camount() == requiredRemainder) {
            utxoAmount += utxo.camount();
            fees += requiredFeePerUtxo;
            outputsForUse.push_back(utxo);
            it = utxos.erase(it); // remove selected utxo
            continue;
        }
        if (totalAmountNeeded <= utxoAmount)
            break;
        ++it;
    }

    // Check to see if we've the exact number of utxos we're looking for.
    // Prep tx fees not required at this point because exact match of
    // required utxos exists.
    totalAmountNeeded = requiredAmount + fees;
    // The <= 1 is the margin of error allowance
    if ((outputsForUse.size() == requiredUtxoCount || (requiredRemainder > 0 && outputsForUse.size() == requiredUtxoCount + 1)) && totalAmountNeeded - utxoAmount <= 0) {
        exactUtxoMatch = true;
        return true;
    }

    // Sort available utxos by amount (ascending)
    sort(utxos.begin(), utxos.end(),
         [](const xbridge::wallet::UtxoEntry & a, const xbridge::wallet::UtxoEntry & b) {
             return a.camount() < b.camount();
         });

    if (outputsForUse.size() == requiredUtxoCount) { // Find a utxo that matches the exact remainder amount
        for (auto it = utxos.begin(); it != utxos.end(); ) {
            auto & utxo = *it;
            totalAmountNeeded = requiredAmount + fees + requiredFeePerUtxo;
            if (utxo.camount() == totalAmountNeeded - utxoAmount) {
                utxoAmount += utxo.camount();
                fees += requiredFeePerUtxo;
                outputsForUse.push_back(utxo);
                it = utxos.erase(it); // remove selected utxo
                break;
            }
            ++it;
        }

        // Check if we're done (prep tx fees not required yet)
        totalAmountNeeded = requiredAmount + fees;
        // The <= 1 is the margin of error allowance
        if (outputsForUse.size() >= requiredUtxoCount && totalAmountNeeded - utxoAmount <= 0) {
            exactUtxoMatch = true;
            return true;
        }

    } else { // Find enough utxos to cover remaining partial order amount
        // At this point a prep tx will be required to support the order, include those fees here
        for (auto it = utxos.begin(); it != utxos.end(); ) {
            auto & utxo = *it;
            totalAmountNeeded = requiredAmount + fees + requiredPrepTxFees;
            if (totalAmountNeeded - utxoAmount <= 0)
                break; // Stop searching when we've reached the required amount

            // At this point we want to pick utxos that are larger than the required split amount to limit
            // total utxos selected. If all the required split utxos have been selected then make sure
            // we have enough inputs to cover change.
            if (utxo.camount() >= requiredSplitSize + requiredFeePerUtxo) {
                utxoAmount += utxo.camount();
                fees += requiredFeePerUtxo;
                outputsForUse.push_back(utxo);
                it = utxos.erase(it); // remove selected utxo
                continue;
            }

            ++it;
        }
    }

    // Incorporate prep fees here
    totalAmountNeeded = requiredAmount + fees + requiredPrepTxFees;

    // Find the largest utxo to cover the remainder
    if (utxoAmount < totalAmountNeeded) {
        for (auto it = utxos.begin(); it != utxos.end(); ) {
            auto & utxo = *it;
            if (utxo.camount() + utxoAmount >= totalAmountNeeded) {
                utxoAmount += utxo.camount();
                outputsForUse.push_back(utxo);
                it = utxos.erase(it); // remove selected utxo
                break;
            }
            ++it;
        }
    }

    // Find the largest utxos to cover the remainder
    if (utxoAmount < totalAmountNeeded) {
        // sort largest first (descending)
        sort(utxos.begin(), utxos.end(),
             [](const xbridge::wallet::UtxoEntry & a, const xbridge::wallet::UtxoEntry & b) {
                 return a.camount() > b.camount();
             });
        for (auto it = utxos.begin(); it != utxos.end(); ) {
            auto & utxo = *it;
            if (totalAmountNeeded - utxoAmount <= 0)
                break; // Stop searching when we've reached the required amount
            utxoAmount += utxo.camount();
            outputsForUse.push_back(utxo);
            it = utxos.erase(it); // remove selected utxo
        }
    }

    if (outputsForUse.empty() || utxoAmount - totalAmountNeeded <= 0)
        return false;

    return true;
}

//******************************************************************************
//******************************************************************************
void App::Impl::checkAndRelayPendingOrders() {
    // Try and rebroadcast my orders older than N seconds (see below)
    auto currentTime = boost::posix_time::second_clock::universal_time();
    std::map<uint256, TransactionDescrPtr> txs;
    {
        LOCK(m_txLocker);
        txs = m_transactions;
    }
    if (txs.empty())
        return;

    auto & xapp = xbridge::App::instance();

    for (const auto & i : txs) {
        TransactionDescrPtr order = i.second;
        if (!order->isLocal()) // only process local orders
            continue;

        auto pendingOrderShouldRebroadcast = (currentTime - order->txtime).total_seconds() >= 240; // 4min
        auto newOrderShouldRebroadcast = (currentTime - order->txtime).total_seconds() >= 15; // 15sec

        if (newOrderShouldRebroadcast && order->state == xbridge::TransactionDescr::trNew && !order->isPartialOrderPending())
        {
            // exclude the old snode
            CPubKey oldsnode;
            oldsnode.Set(order->sPubKey.begin(), order->sPubKey.end());

            // Pick new servicenode
            std::set<std::string> currencies{order->fromCurrency, order->toCurrency};
            CPubKey snode;
            auto notIn = order->excludedNodes();
            notIn.insert(oldsnode); // exclude the current snode
            if (!xapp.findNodeWithService(currencies, snode, notIn)) {
                UniValue log_obj(UniValue::VOBJ);
                log_obj.pushKV("orderid", order->id.GetHex());
                log_obj.pushKV("from_currency", order->fromCurrency);
                log_obj.pushKV("to_currency", order->toCurrency);
                xbridge::LogOrderMsg(log_obj, "order may be stuck, trying to submit order to previous snode", __FUNCTION__);
                // do not fail here, let the order be broadcasted on existing snode just in case (also may avoid stalling the order)
            } else {
                // assign new snode
                order->excludeNode(oldsnode);
                order->assignServicenode(snode);
            }

            order->updateTimestamp();
            // Only broadcast the order if the utxos are still valid
            if (orderUtxosAreStillValid(order))
                sendPendingTransaction(order);
            else
                xbridge::App::instance().cancelXBridgeTransaction(order->id, crBadAUtxo);
        }
        else if (pendingOrderShouldRebroadcast && order->state == xbridge::TransactionDescr::trPending) {
            order->updateTimestamp();

            // Check that snode order is assigned to is still valid
            CPubKey oldsnode;
            oldsnode.Set(order->sPubKey.begin(), order->sPubKey.end());
            if (!hasNodeService(oldsnode, order->fromCurrency, true) || !hasNodeService(oldsnode, order->toCurrency, true)) {
                // Pick new servicenode
                std::set<std::string> currencies{order->fromCurrency, order->toCurrency};
                CPubKey newsnode;
                auto notIn = order->excludedNodes();
                notIn.insert(oldsnode); // exclude the current snode
                if (!xapp.findNodeWithService(currencies, newsnode, notIn)) {
                    UniValue log_obj(UniValue::VOBJ);
                    log_obj.pushKV("orderid", order->id.GetHex());
                    log_obj.pushKV("from_currency", order->fromCurrency);
                    log_obj.pushKV("to_currency", order->toCurrency);
                    xbridge::LogOrderMsg(log_obj, "failed to find service node, order may be stuck: trying to submit order to another snode", __FUNCTION__);
                    // do not fail here, let the order be broadcasted on existing snode just in case (also may avoid stalling the order)
                } else {
                    // assign new snode
                    order->excludeNode(oldsnode);
                    order->assignServicenode(newsnode);
                }
            }

            // Only broadcast the order if the utxos are still valid
            if (orderUtxosAreStillValid(order))
                sendPendingTransaction(order);
            else
                xapp.cancelXBridgeTransaction(order->id, crBadAUtxo);
        }
    }
}

//******************************************************************************
//******************************************************************************
/**
 * @brief Checks the blockchain for the spent deposit. Only when 
 *        the Maker spends the deposit can the Taker proceed with 
 *        the swap. Note that the Taker must watch the "from" 
 *        chain, since this is the chain the Maker is submitting 
 *        the pay tx on.
 */
void App::Impl::checkWatchesOnDepositSpends()
{
    std::map<uint256, TransactionDescrPtr> watches;
    {
        LOCK(m_watchDepositsLocker);
        if (m_watching) // ignore if we're still processing from previous request
            return;
        m_watching = true;
        watches = m_watchDeposits;
    }

    // Check blockchain for spends
    xbridge::App & app = xbridge::App::instance();
    for (auto & item : watches) {
        auto & xtx = item.second;
        if (xtx->isWatching())
            continue;

        if (xtx->role == 'A' && xtx->state == TransactionDescr::trFinished) { // maker can stop checking after p2sh redeem
            app.unwatchSpentDeposit(xtx);
            continue;
        }

        WalletConnectorPtr connFrom = app.connectorByCurrency(xtx->fromCurrency);
        if (!connFrom)
            continue; // skip (maybe wallet went offline)

        xtx->setWatching(true);

        uint32_t blockCount{0};
        if (!connFrom->getBlockCount(blockCount)) {
            xtx->setWatching(false);
            continue;
        }

        if (xtx->role == 'B') { // This section only applies to taker looking for secret

        // If we don't have the secret yet, look for the pay tx
        if (!xtx->hasSecret()) {
            // Obtain the transactions to search (current mempool or current block)
            std::vector<std::string> txids;
            if (xtx->getWatchStartBlock() == blockCount) {
                if (!connFrom->getRawMempool(txids)) {
                    xtx->setWatching(false);
                    continue;
                }
            } else { // check in next block to search
                uint32_t blocks = xtx->getWatchCurrentBlock();
                bool failure = false;

                // Search all tx in blocks up to current block
                while (blocks <= blockCount) {
                    std::string blockHash;
                    std::vector<std::string> txs;
                    if (!connFrom->getBlockHash(blocks, blockHash)) {
                        failure = true;
                        break;
                    }
                    if (!connFrom->getTransactionsInBlock(blockHash, txs)) {
                        failure = true;
                        break;
                    }
                    txids.insert(txids.end(), txs.begin(), txs.end());
                    xtx->setWatchBlock(++blocks); // mark that we've processed current block
                }

                // If any failure, skip
                if (failure) {
                    xtx->setWatching(false);
                    continue;
                }
            }

            // Look for the spent pay tx
            for (auto & txid : txids) {
                bool isSpent = false;
                if (connFrom->isUTXOSpentInTx(txid, xtx->binTxId, xtx->binTxVout, isSpent) && isSpent) {
                    // Found valid spent pay tx, now assign
                    xtx->setOtherPayTxId(txid);
                    xtx->doneWatching(); // report that we're done looking
                    break;
                }
            }
        }
        }

        // If a redeem of origin deposit or pay tx is successful
        bool done = false;

        // If lockTime has expired on original deposit, attempt to redeem it
        if (xtx->lockTime <= blockCount) {
            xbridge::SessionPtr session = getSession();
            int32_t errCode = 0;
            if (session->redeemOrderDeposit(xtx, errCode)) {
                xtx->state = TransactionDescr::trRollback;
                done = true;
            }
        }

        // If we've found the spent paytx and haven't redeemed it yet, do that now
        if (xtx->isDoneWatching() && !xtx->hasRedeemedCounterpartyDeposit()) {
            xbridge::SessionPtr session = getSession();
            int32_t errCode = 0;
            if (session->redeemOrderCounterpartyDeposit(xtx, errCode)) {
                xtx->state = TransactionDescr::trFinished;
                done = true;
            }
        }

        if (done) {
            xtx->doneWatching();
            xbridge::App & xapp = xbridge::App::instance();
            xapp.unwatchSpentDeposit(xtx);
            xapp.saveOrders(true);
        }

        xtx->setWatching(false);
    }

    {
        LOCK(m_watchDepositsLocker);
        m_watching = false;
    }
}

//******************************************************************************
//******************************************************************************
/**
 * @brief Checks the blockchain for the current block and if the block matches
 *        the trader's locktime (indicating that it has expired), the
 *        servicenode will attempt to redeem the traders deposit on their
 *        behalf. Normally traders will redeem their own funds, however, this is
 *        a backup in case the trader node goes offline or is disconnected.
 */
void App::Impl::watchTraderDeposits()
{
    std::map<uint256, TransactionPtr> watches;
    {
        LOCK(m_watchTradersLocker);
        if (m_watchingTraders) // ignore if we're still processing from previous request
            return;
        m_watchingTraders = true;
        watches = m_watchTraders;
    }

    // Checks the trader's chain for locktime and submits refund transaction if necessary
    auto check = [](xbridge::SessionPtr session, const std::string & orderId, const WalletConnectorPtr & conn,
                    const uint32_t & lockTime, const std::string & refTx) -> bool
    {
        uint32_t blockCount{0};
        if (!conn->getBlockCount(blockCount))
            return false;

        // If a redeem of trader deposit is successful
        bool done = false;

        // If lockTime has expired on trader deposit, attempt to redeem it
        if (lockTime <= blockCount) {
            int32_t errCode = 0;
            if (session->refundTraderDeposit(orderId, conn->currency, lockTime, refTx, errCode))
                done = true;
            else if (errCode == RPCErrorCode::RPC_VERIFY_ALREADY_IN_CHAIN
                  || errCode == RPCErrorCode::RPC_INVALID_ADDRESS_OR_KEY
                  || errCode == RPCErrorCode::RPC_VERIFY_REJECTED)
                done = true;

            if (!done && (blockCount - lockTime) * conn->blockTime > 3600) // if locktime has expired for more than 1 hr, we're done
                done = true;
        }

        return done;
    };

    // Check blockchain for spends
    xbridge::App & app = xbridge::App::instance();
    for (auto & item : watches) {
        auto & tr = item.second;

        xbridge::App & xapp = xbridge::App::instance();
        xbridge::SessionPtr session = getSession();

        // Trader A check (if not refunded, has valid refund tx, and order not marked finished)
        if (!tr->a_refunded() && !tr->a_refTx().empty() && tr->state() != xbridge::Transaction::trFinished) {
            WalletConnectorPtr connA = xapp.connectorByCurrency(tr->a_currency());
            if (connA && check(session, tr->id().ToString(), connA, tr->a_lockTime(), tr->a_refTx())) {
                tr->a_setRefunded(true);
            }
        }

        // Trader B check (if not refunded, has valid refund tx, and order not marked finished)
        if (!tr->b_refunded() && !tr->b_refTx().empty() && tr->state() != xbridge::Transaction::trFinished) {
            WalletConnectorPtr connB = app.connectorByCurrency(tr->b_currency());
            if (connB && check(session, tr->id().ToString(), connB, tr->b_lockTime(), tr->b_refTx())) {
                tr->b_setRefunded(true);
            }
        }

        if ((tr->a_refunded() && tr->b_refunded()) || tr->state() == xbridge::Transaction::trFinished)
            xapp.unwatchTraderDeposit(tr);
    }

    {
        LOCK(m_watchTradersLocker);
        m_watchingTraders = false;
    }
}

//******************************************************************************
//******************************************************************************
bool App::Impl::orderUtxosAreStillValid(TransactionDescrPtr order) {
    WalletConnectorPtr makerConn = ConnectorByCurrency(order->fromCurrency);
    if (!makerConn)
        return false;

    auto makerUtxos = order->usedCoins;
    for (auto & entry : makerUtxos) {
        if (!makerConn->getTxOut(entry))
            return false;
    }

    return true; // done
}

//*****************************************************************************
//*****************************************************************************
void App::Impl::checkAndEraseExpiredTransactions()
{
    // check xbridge transactions
    Exchange & e = Exchange::instance();
    e.eraseExpiredTransactions();

    // check client transactions
    auto currentTime = boost::posix_time::microsec_clock::universal_time();
    std::map<uint256, TransactionDescrPtr> txs;
    std::set<uint256> forErase;
    {
        LOCK(m_txLocker);
        txs = m_transactions;
    }
    if (txs.empty())
    {
        return;
    }
    // check...
    for (const std::pair<uint256, TransactionDescrPtr> & i : txs)
    {
        TransactionDescrPtr tx = i.second;
        bool stateChanged = false;
        {
            TRY_LOCK(tx->_lock, txlock);
            if (!txlock)
            {
                continue;
            }
            boost::posix_time::time_duration td = currentTime - tx->txtime;
            boost::posix_time::time_duration tc = currentTime - tx->created;
            if (tx->state == xbridge::TransactionDescr::trNew &&
                td.total_seconds() > xbridge::Transaction::pendingTTL &&
                !tx->isPartialOrderPending()) // do not expire a pending partial order
            {
                tx->state = xbridge::TransactionDescr::trOffline;
                stateChanged = true;
            }
            else if (tx->state == xbridge::TransactionDescr::trPending &&
                     td.total_seconds() > xbridge::Transaction::pendingTTL)
            {
                tx->state = xbridge::TransactionDescr::trExpired;
                stateChanged = true;
            }
            else if ((tx->state == xbridge::TransactionDescr::trExpired ||
                      tx->state == xbridge::TransactionDescr::trOffline) &&
                     td.total_seconds() < xbridge::Transaction::pendingTTL)
            {
                tx->state = xbridge::TransactionDescr::trPending;
                stateChanged = true;
            }
            else if ((tx->state == xbridge::TransactionDescr::trExpired ||
                      tx->state == xbridge::TransactionDescr::trOffline) &&
                     td.total_seconds() > xbridge::Transaction::TTL)
            {
                forErase.insert(i.first);
            }
            else if (tx->state == xbridge::TransactionDescr::trPending &&
                     tc.total_seconds() > xbridge::Transaction::deadlineTTL)
            {
                forErase.insert(i.first);
            }
        }
        if (stateChanged)
        {
            xuiConnector.NotifyXBridgeTransactionChanged(tx->id);
        }
    }
    // ...erase expired...
    {
        LOCK(m_txLocker);
        for (const uint256 & id : forErase)
        {
            m_transactions.erase(id);
        }
    }
    // ...and notify
//    for (const uint256 & id : forErase)
//    {
//        xuiConnector.NotifyXBridgeTransactionRemoved(id);
//    }
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

        // update active xwallets (in case a wallet goes offline)
        auto app = &xbridge::App::instance();
        static uint32_t updateActiveWallets_c = 0;
        if (++updateActiveWallets_c == 2) { // every ~30 seconds
            updateActiveWallets_c = 0;
            io->post(boost::bind(&xbridge::App::updateActiveWallets, app));
        }

        // Check orders
        io->post(boost::bind(&Impl::checkAndRelayPendingOrders, this));

        // erase expired tx
        io->post(boost::bind(&Impl::checkAndEraseExpiredTransactions, this));

        Exchange & e = Exchange::instance();
        auto isServicenode = e.isStarted();

        // Check for deposit spends
        if (!isServicenode) // if not servicenode, watch deposits
            io->post(boost::bind(&Impl::checkWatchesOnDepositSpends, this));

        if (isServicenode) {
            // If servicenode, watch trader deposits
            static uint32_t watchCounter = 0;
            if (++watchCounter == 40) { // ~10 min
                watchCounter = 0;
                io->post(boost::bind(&Impl::watchTraderDeposits, this));
            }
        }

        if (sn::ServiceNodeMgr::instance().hasActiveSn()) { // send ping if active snode
            // Send service ping every 3 minutes
            static int pingCounter{0};
            if (++pingCounter % 12 == 0) {
                auto smgr = &sn::ServiceNodeMgr::instance();
                io->post(boost::bind(&sn::ServiceNodeMgr::sendPing, smgr, XROUTER_PROTOCOL_VERSION,
                                     app->myServicesJSON(), g_connman.get()));
            }
        }

        // unprocessed packets
        {
            static uint32_t counter = 0;
            if (++counter == 2)
            {
                counter = 0;
                std::map<uint256, XBridgePacketPtr> map;
                {
                    LOCK(m_ppLocker);
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

        // Process partial orders that are waiting for prep tx confirmation
        {
            LOCK(app->m_lock);
            if (!app->m_partialOrders.empty())
                io->post(boost::bind(&xbridge::App::processPendingPartialOrders, app));
        }

        // Save orders states every so often
        {
            static uint32_t counter{0};
            if (++counter % 4 == 0)
                app->saveOrders();
        }
    }

    m_timer.expires_at(m_timer.expires_at() + boost::posix_time::seconds(TIMER_INTERVAL));
    m_timer.async_wait(boost::bind(&Impl::onTimer, this));
}

void App::processPendingPartialOrders() {
    std::vector<TransactionDescrPtr> pendingOrders;
    {
        LOCK(m_lock);
        pendingOrders = m_partialOrders;
    }

    // Iterate over all partial orders and submit the ones that are no longer
    // pending. Prior to submitting the partial order's prep tx is checked
    // for valid confirmations (at least 1 conf is required for submission).
    for (auto ptr : pendingOrders) {
        auto connFrom = connectorByCurrency(ptr->fromCurrency);
        if (!connFrom)
            continue;

        wallet::UtxoEntry entry;
        entry.txId = ptr->partialOrderPrepTx;
        entry.vout = 0;
        if (!connFrom->getTxOut(entry))
            continue;
        if (!entry.hasConfirmations || entry.confirmations <= 0)
            continue; // skip if no confs yet

        // Submit order to the network now that partial order prep tx
        // has confirmations.
        if (!m_p->sendPendingTransaction(ptr))
            xbridge::LogOrderMsg(ptr, std::string(__FUNCTION__) + " failed to submit partial order");
        else
            xbridge::LogOrderMsg(ptr, std::string(__FUNCTION__) + " partial order created");

        removePendingPartialOrder(ptr);
    }
}

void App::removePendingPartialOrder(TransactionDescrPtr ptr) {
    LOCK(m_lock);
    auto it = m_partialOrders.begin();
    while (it != m_partialOrders.end()) {
        if (ptr->id == it->get()->id) {
            ptr->setPartialOrderPending(false);
            m_partialOrders.erase(it);
            break;
        }
        ++it;
    }
}

/**
 * Clears the xbridge message mempool. This is not threadsafe, locks required outside this func.
 */
void App::clearMempool() {
    auto count = m_p->m_processedMessages.size();
    auto maxMBytes = static_cast<unsigned int>(gArgs.GetArg("-maxmempoolxbridge", 128)) * 1000000;
    if (count * 64 > maxMBytes) // estimated 64 bytes per hash
        m_p->m_processedMessages.clear();
}

void App::clearNonLocalOrders() {
    LOCK(m_p->m_txLocker);
    for (auto it = m_p->m_transactions.begin(); it != m_p->m_transactions.end(); ) {
        const TransactionDescrPtr & ptr = it->second;
        if (ptr->isLocal()) {
            ++it;
            continue; // do not remove any orders that belong to us
        }
        LOCK(m_p->m_connectorsLock);
        if (!m_p->m_connectorCurrencyMap.count(ptr->fromCurrency) || !m_p->m_connectorCurrencyMap.count(ptr->toCurrency)) {
            m_p->m_transactions.erase(it++);
        } else {
            ++it;
        }
    }
}

void App::loadOrders() {
    LOCK(m_lock);

    XOrderSet orders;
    if (!xdb.Exists()) {
        UniValue info(UniValue::VOBJ);
        if (xdb.Create())
            LogOrderMsg(info, "Creating orders database", __FUNCTION__);
        else
            LogOrderMsg(info, "Failed to create orders database", __FUNCTION__);
        return;
    }
    if (!xdb.Read(orders)) {
        UniValue erro(UniValue::VOBJ);
        LogOrderMsg(erro, "Failed to load existing orders database", __FUNCTION__);
        return;
    }

    LOCK(m_p->m_txLocker);
    for (auto & order : orders) {
        auto tr = std::make_shared<TransactionDescr>(order.second);
        if (!tr)
            continue;

        // Restore all transactions
        if (tr->state == TransactionDescr::trCancelled || tr->state == TransactionDescr::trFinished || tr->isHistorical())
            m_p->m_historicTransactions.insert(std::make_pair(tr->id, tr));
        else
            m_p->m_transactions.insert(std::make_pair(tr->id, tr));

        // Restore spent deposit watches
        if (tr->isWatchingForSpentDeposit())
            watchForSpentDeposit(tr);

        // Restore pending partial orders
        if (tr->isPartialOrderPending())
            m_partialOrders.push_back(tr);
    }
}

void App::saveOrders(bool force) {
    LOCK(m_lock);

    {
        LOCK(m_p->m_txLocker);
        if (m_p->m_transactions.empty() && m_p->m_historicTransactions.empty() && m_partialOrders.empty())
            return;
    }

    XOrderSet orders;
    if (!xdb.Read(orders)) {
        UniValue erro(UniValue::VOBJ);
        LogOrderMsg(erro, "Failed to load existing orders database", __FUNCTION__);
    }

    {
        LOCK(m_p->m_txLocker);
        for (auto & order : m_p->m_transactions) {
            if (order.second->isLocal())
                orders[order.first] = *order.second;
        }
        for (auto & order : m_p->m_historicTransactions) {
            if (order.second->isLocal())
                orders[order.first] = *order.second;
        }
        for (auto & order : m_partialOrders) {
            orders[order->id] = *order;
        }
    }

    xdb.Write(orders, force);
}

uint256 App::orderWithUtxo(const wallet::UtxoEntry & utxo) {
    LOCK(m_p->m_txLocker);
    for (const auto & order : m_p->m_transactions) {
        if (order.second->isLocal()) {
            auto utxos = order.second->utxos();
            if (utxos.count(utxo))
                return order.first;
        }
    }
    return uint256();
}

std::vector<TransactionDescrPtr> App::getPartialOrderChain(const uint256 orderid) {
    xbridge::TransactionDescrPtr rorder = nullptr;
    std::vector<TransactionDescrPtr> orders;

    // Filter local partial orders
    TransactionMap trList = transactions();
    for (const auto & i : trList) {
        const auto & t = i.second;
        // Associate exact orders that are child orders of partial orders
        if (!t->isLocal() || (t->getParentOrder().IsNull() && !t->isPartialOrderAllowed()))
            continue;
        if (t->id == orderid)
            rorder = t;
        orders.push_back(t);
    }

    // Add historical partial orders
    TransactionMap histOrders = history();
    for (auto & item : histOrders) {
        const auto & t = item.second;
        // Associate exact orders that are child orders of partial orders
        if (!t->isLocal() || (t->getParentOrder().IsNull() && !t->isPartialOrderAllowed()))
            continue;
        if (t->id == orderid)
            rorder = t;
        orders.push_back(t);
    }

    if (orders.empty() || !rorder) // If no orders then return
        return {};

    // sort ascending by utxo sizes
    std::sort(orders.begin(), orders.end(),
          [](const xbridge::TransactionDescrPtr & a,  const xbridge::TransactionDescrPtr & b) {
              return (a->utxoCount()) < (b->utxoCount());
          });

    // Find the partial order chain:
    // 1) Find all partial order children
    // 2) Find all partial order parents
    std::vector<xbridge::TransactionDescrPtr> orderChain{rorder};
    for (int i = 0; i < orders.size(); ++i) {
        auto & t = orders[i];
        if (t == rorder) {
            // Search orders with fewer utxos than user specified order id
            auto currentid = t->id;
            auto currentptr = t;
            for (int j = i-1; j >= 0; --j) {
                auto & ptrChild = orders[j];
                if (ptrChild->getParentOrder() == currentid) {
                    orderChain.insert(orderChain.begin(), ptrChild);
                    if (ptrChild->utxoCount() < currentptr->utxoCount())
                        currentid = currentptr->id;
                }
            }
            // Search orders with more utxos than user specified order id
            currentid = t->id;
            currentptr = t;
            for (int k = i+1; k < (int)orders.size(); ++k) {
                auto & ptrParent = orders[k];
                if (currentptr->getParentOrder() == ptrParent->id) {
                    orderChain.insert(orderChain.end(), ptrParent);
                    if (ptrParent->utxoCount() > currentptr->utxoCount())
                        currentptr = ptrParent;
                }
            }
            break;
        }
    }

    // Sort ascending by created time so that the first order in the chain
    // is displayed first.
    std::sort(orderChain.begin(), orderChain.end(),
          [](const xbridge::TransactionDescrPtr & a,  const xbridge::TransactionDescrPtr & b) {
              return a->created < b->created;
          });

    return std::move(orderChain);
}

std::ostream & operator << (std::ostream& out, const TransactionDescrPtr& tx)
{
    UniValue log_obj(UniValue::VOBJ);
    std::string errMsg;

    log_obj.pushKV("orderid", tx->id.GetHex());

    if (!settings().isFullLog()) {
        out << log_obj.write();
        return out;
    }

    xbridge::WalletConnectorPtr connFrom = xbridge::App::instance().connectorByCurrency(tx->fromCurrency);
    xbridge::WalletConnectorPtr connTo   = xbridge::App::instance().connectorByCurrency(tx->toCurrency);

    if (!connFrom || !connTo)
        errMsg = (!connFrom ? tx->fromCurrency : tx->toCurrency) + " connector missing";

    UniValue log_utxos(UniValue::VARR);
    uint32_t count = 0;
    for (const auto & entry : tx->usedCoins) {
        UniValue log_utxo(UniValue::VOBJ);
        log_utxo.pushKV("index", static_cast<int>(count));
        log_utxo.pushKV("txid", entry.txId);
        log_utxo.pushKV("vout", static_cast<int>(entry.vout));
        log_utxo.pushKV("amount", xBridgeStringValueFromPrice(entry.amount, COIN));
        log_utxo.pushKV("address", entry.address);
        log_utxos.push_back(log_utxo);
        ++count;
    }

    log_obj.pushKV("maker", tx->fromCurrency);
    log_obj.pushKV("maker_size", xbridge::xBridgeStringValueFromAmount(tx->fromAmount));
    log_obj.pushKV("maker_addr", (!tx->from.empty() && connFrom ? connFrom->fromXAddr(tx->from) : ""));
    log_obj.pushKV("taker", tx->toCurrency);
    log_obj.pushKV("taker_size", xbridge::xBridgeStringValueFromAmount(tx->toAmount));
    log_obj.pushKV("taker_addr", (!tx->to.empty() && connTo ? connTo->fromXAddr(tx->to) : ""));
    log_obj.pushKV("partial_allowed", tx->isPartialOrderAllowed());
    log_obj.pushKV("partial_repost", tx->isPartialRepost());
    log_obj.pushKV("partial_minimum", xbridge::xBridgeStringValueFromAmount(tx->minFromAmount));
    log_obj.pushKV("partial_orig_maker_size", xbridge::xBridgeStringValueFromAmount(tx->origFromAmount));
    log_obj.pushKV("partial_orig_taker_size", xbridge::xBridgeStringValueFromAmount(tx->origToAmount));
    log_obj.pushKV("partial_parent_id", tx->getParentOrder().GetHex());
    log_obj.pushKV("state", tx->strState());
    log_obj.pushKV("block_hash", tx->blockHash.GetHex());
    log_obj.pushKV("updated_at", iso8601(tx->txtime));
    log_obj.pushKV("created_at", iso8601(tx->created));
    log_obj.pushKV("err_msg", errMsg);
    log_obj.pushKV("cancel_reason", TxCancelReasonText(tx->reason));
    log_obj.pushKV("utxos", log_utxos);

    out << log_obj.write();
    return out;
}

WalletConnectorPtr ConnectorByCurrency(const std::string & currency) {
    return App::instance().connectorByCurrency(currency);
}

std::string TxCancelReasonText(uint32_t reason) {
    const auto creason = static_cast<TxCancelReason>(reason);
    switch (creason) {
        case TxCancelReason::crBadSettings:
            return "crUnknown";
        case TxCancelReason::crUserRequest:
            return "crUserRequest";
        case TxCancelReason::crNoMoney:
            return "crNoMoney";
        case TxCancelReason::crBadUtxo:
            return "crBadUtxo";
        case TxCancelReason::crDust:
            return "crDust";
        case TxCancelReason::crRpcError:
            return "crRpcError";
        case TxCancelReason::crNotSigned:
            return "crNotSigned";
        case TxCancelReason::crNotAccepted:
            return "crNotAccepted";
        case TxCancelReason::crRollback:
            return "crRollback";
        case TxCancelReason::crRpcRequest:
            return "crRpcRequest";
        case TxCancelReason::crXbridgeRejected:
            return "crXbridgeRejected";
        case TxCancelReason::crInvalidAddress:
            return "crInvalidAddress";
        case TxCancelReason::crBlocknetError:
            return "crBlocknetError";
        case TxCancelReason::crBadADepositTx:
            return "crBadADepositTx";
        case TxCancelReason::crBadBDepositTx:
            return "crBadBDepositTx";
        case TxCancelReason::crTimeout:
            return "crTimeout";
        case TxCancelReason::crBadLockTime:
            return "crBadLockTime";
        case TxCancelReason::crBadALockTime:
            return "crBadALockTime";
        case TxCancelReason::crBadBLockTime:
            return "crBadBLockTime";
        case TxCancelReason::crBadAUtxo:
            return "crBadAUtxo";
        case TxCancelReason::crBadBUtxo:
            return "crBadBUtxo";
        case TxCancelReason::crBadARefundTx:
            return "crBadARefundTx";
        case TxCancelReason::crBadBRefundTx:
            return "crBadBRefundTx";
        case TxCancelReason::crBadFeeTx:
            return "crBadFeeTx";
        case TxCancelReason::crUnknown:
        default:
            return "crNone";
    }
}

} // namespace xbridge
