//*****************************************************************************
//*****************************************************************************

#include "xbridgeapp.h"
#include "xbridgeexchange.h"
#include "util/xutil.h"
#include "util/logger.h"
#include "util/settings.h"
#include "util/xbridgeerror.h"
#include "version.h"
#include "config.h"
#include "xuiconnector.h"
#include "rpcserver.h"
#include "net.h"
#include "util.h"
#include "xkey.h"
#include "ui_interface.h"
#include "init.h"
#include "wallet.h"
#include "servicenodeman.h"
#include "xbridgewalletconnector.h"
#include "xbridgewalletconnectorbtc.h"
#include "xbridgewalletconnectorbcc.h"
#include "xbridgewalletconnectorsys.h"

#include <assert.h>

#include <boost/chrono/chrono.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <openssl/rand.h>
#include <openssl/md5.h>

#include "posixtimeconversion.h"

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
    Impl();

    bool start();
    bool stop();

protected:
    void onSend(const std::vector<unsigned char> & id, const std::vector<unsigned char> & message);

    void onTimer();

    SessionPtr getSession();
    SessionPtr getSession(const std::vector<unsigned char> & address);

protected:
    bool sendPendingTransaction(const TransactionDescrPtr & ptr);
    bool sendAcceptingTransaction(const TransactionDescrPtr & ptr);
    bool sendCancelTransaction(const uint256 &txid, const TxCancelReason &reason);

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
        // services and threas
        for (int i = 0; i < boost::thread::hardware_concurrency(); ++i)
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
                wp.m_ip                          = s.get<std::string>(*i + ".Ip");
                wp.m_port                        = s.get<std::string>(*i + ".Port");
                wp.m_user                        = s.get<std::string>(*i + ".Username");
                wp.m_passwd                      = s.get<std::string>(*i + ".Password");
                wp.addrPrefix[0]               = s.get<int>(*i + ".AddressPrefix", 0);
                wp.scriptPrefix[0]             = s.get<int>(*i + ".ScriptPrefix", 0);
                wp.secretPrefix[0]             = s.get<int>(*i + ".SecretPrefix", 0);
                wp.COIN                        = s.get<uint64_t>(*i + ".COIN", 0);
                wp.txVersion                   = s.get<uint32_t>(*i + ".TxVersion", 1);
                wp.minTxFee                    = s.get<uint64_t>(*i + ".MinTxFee", 0);
                wp.feePerByte                  = s.get<uint64_t>(*i + ".FeePerByte", 200);
                wp.m_minAmount                 = s.get<uint64_t>(*i + ".MinimumAmount", 0);
                wp.dustAmount                  = 3 * wp.minTxFee;
                wp.method                      = s.get<std::string>(*i + ".CreateTxMethod");
                wp.isGetNewPubKeySupported     = s.get<bool>(*i + ".GetNewKeySupported", false);
                wp.isImportWithNoScanSupported = s.get<bool>(*i + ".ImportWithNoScanSupported", false);
                wp.blockTime                   = s.get<int>(*i + ".BlockTime", 0);
                wp.requiredConfirmations       = s.get<int>(*i + ".Confirmations", 0);

                if (wp.m_ip.empty() || wp.m_port.empty() ||
                    wp.m_user.empty() || wp.m_passwd.empty() ||
                    wp.COIN == 0 || wp.blockTime == 0)
                {
                    LOG() << "read wallet " << *i << " with empty parameters>";
                    continue;
                }
                else
                {
                    LOG() << "read wallet " << *i << " [" << wp.title << "] " << wp.m_ip
                          << ":" << wp.m_port; // << " COIN=" << wp.COIN;
                }

                xbridge::WalletConnectorPtr conn;
                if (wp.method == "ETHER")
                {
                    LOG() << "wp.method ETHER not implemented" << __FUNCTION__;
                    // session.reset(new XBridgeSessionEthereum(wp));
                }
                else if (wp.method == "BTC")
                {
                    conn.reset(new BtcWalletConnector);
                    *conn = wp;
                }
                else if (wp.method == "BCC")
                {
                    conn.reset(new BccWalletConnector);
                    *conn = wp;
                }
                else if (wp.method == "SYS")
                {
                    conn.reset(new SysWalletConnector);
                    *conn = wp;
                }
//                else if (wp.method == "RPC")
//                {
//                    LOG() << "wp.method RPC not implemented" << __FUNCTION__;
//                    // session.reset(new XBridgeSessionRpc(wp));
//                }
                else
                {
                    // session.reset(new XBridgeSession(wp));
                    ERR() << "unknown session type " << __FUNCTION__;
                }
                if (conn)
                {
                    app.addConnector(conn);
                }
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

    // init secp256
    if(!ECC_Start()) {

        ERR() << "can't start secp256, xbridgeApp not started " << __FUNCTION__;
        throw  std::runtime_error("can't start secp256, xbridgeApp not started ");

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

    // secp stop
    ECC_Stop();

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

    LOG() << "received message to " << util::base64_encode(std::string((char *)&id[0], 20)).c_str()
             << " command " << packet->command();

    // check direct session address
    SessionPtr ptr = m_p->getSession(id);
    if (ptr)
    {
        ptr->processPacket(packet);
    }

    else
    {
        {
            // if no session address - find connector address
            boost::mutex::scoped_lock l(m_p->m_connectorsLock);
            if (m_p->m_connectorAddressMap.count(id))
            {
                ptr = m_p->getSession();
            }
        }

        if (ptr)
        {
            ptr->processPacket(packet);
        }
    }
}

//*****************************************************************************
//*****************************************************************************
void App::onBroadcastReceived(const std::vector<unsigned char> & message,
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
//            assert(counter < 2 && "duplicate transaction");
        }

        if (xtx)
        {
            if(m_p->m_historicTransactions.count(id) != 0) {
                ERR() << "duplicate tx " << id.GetHex() << " in tx list and history " << __FUNCTION__;
                return;
            }
//            assert(m_p->m_historicTransactions.count(id) == 0 && "duplicate tx in tx list and history");
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
//    LOG() <<
    const auto statusCode = checkCreateParams(fromCurrency, toCurrency, fromAmount);
    if(statusCode != xbridge::SUCCESS) {
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

    // check amount
    std::vector<wallet::UtxoEntry> outputs;
    connFrom->getUnspent(outputs);

    double utxoAmount = 0;
    double fee1       = 0;
    double fee2       = connFrom->minTxFee2(1, 1);

    std::vector<wallet::UtxoEntry> outputsForUse;
    for (const wallet::UtxoEntry & entry : outputs)
    {
        utxoAmount += entry.amount;
        outputsForUse.push_back(entry);

        fee1 = connFrom->minTxFee1(outputsForUse.size(), 3);

        LOG() << "USED FOR TX <" << entry.txId << "> amount " << entry.amount << " " << entry.vout << " fee " << fee1;

        if ((utxoAmount * TransactionDescr::COIN) > fromAmount + ((fee1 + fee2) * TransactionDescr::COIN))
        {
            break;
        }
    }

    if ((utxoAmount * TransactionDescr::COIN) < fromAmount + ((fee1 + fee2) * TransactionDescr::COIN))
    {
        WARN() << "insufficient funds for <" << fromCurrency << "> " << __FUNCTION__;
        return xbridge::Error::INSIFFICIENT_FUNDS;
    }

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

        if(entry.signature.size() != 65) {

            ERR() << "incorrect signature length, need 20 bytes " << __FUNCTION__;
            return xbridge::Error::INVALID_SIGNATURE;

        }
//        assert(entry.signature.size() == 65 && "incorrect signature length, need 20 bytes");
        if(entry.rawAddress.size() != 20) {

            ERR() << "incorrect raw address length, need 20 bytes " << __FUNCTION__;
            return  xbridge::Error::INVALID_ADDRESS;

        }
//        assert(entry.rawAddress.size() == 20 && "incorrect raw address length, need 20 bytes");
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

    LOG() << "created order with id " << id.GetHex();

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
            return false;
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
        // 20 bytes - address
        //  8 bytes - currency
        //  8 bytes - amount
        // 32 bytes - hash of block when tr created
        ptr->packet->append(ptr->id.begin(), 32);
        ptr->packet->append(ptr->from);
        ptr->packet->append(fc);
        ptr->packet->append(ptr->fromAmount);
        ptr->packet->append(ptr->to);
        ptr->packet->append(tc);
        ptr->packet->append(ptr->toAmount);
        ptr->packet->append(util::timeToInt(ptr->created));
        ptr->packet->append(ptr->blockHash.begin(), 32);


        // utxo items
        ptr->packet->append(static_cast<uint32_t>(ptr->usedCoins.size()));
        for (const wallet::UtxoEntry & entry : ptr->usedCoins)
        {
            uint256 txid(entry.txId);
            ptr->packet->append(txid.begin(), 32);
            ptr->packet->append(entry.vout);
            ptr->packet->append(entry.rawAddress);
            ptr->packet->append(entry.signature);
        }

    }

    ptr->packet->sign(ptr->mPubKey, ptr->mPrivKey);

    static std::vector<unsigned char> addr(20, 0);
    onSend(addr, ptr->packet->body());

    ptr->state = TransactionDescr::trPending;
    xuiConnector.NotifyXBridgeTransactionChanged(ptr->id);

    return true;
}

//******************************************************************************
//******************************************************************************
Error App::acceptXBridgeTransaction(const uint256     & id,
                                    const std::string & from,
                                    const std::string & to)
{
    TransactionDescrPtr ptr;
    const auto res = checkAcceptParams(id, ptr);
    if(res != xbridge::SUCCESS) {
        return res;
    }

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

    // check amount
    std::vector<wallet::UtxoEntry> outputs;
    connFrom->getUnspent(outputs);

    double utxoAmount = 0;
    std::vector<wallet::UtxoEntry> outputsForUse;
    for (const wallet::UtxoEntry & entry : outputs)
    {
        utxoAmount += entry.amount;
        outputsForUse.push_back(entry);

        // TODO calculate fee for outputsForUse.count()

        if ((utxoAmount * TransactionDescr::COIN) > ptr->fromAmount)
        {
            break;
        }
    }

    if ((utxoAmount * TransactionDescr::COIN) < ptr->fromAmount)
    {
        WARN() << "insufficient funds for <" << ptr->fromCurrency << "> " << __FUNCTION__;
        return xbridge::INSIFFICIENT_FUNDS;
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
        if(entry.signature.size() != 65) {

            ERR() << "incorrect signature length, need 20 bytes " << __FUNCTION__;
            return xbridge::Error::INVALID_SIGNATURE;

        }

        if(entry.rawAddress.size() != 20) {

            ERR() << "incorrect raw address length, need 20 bytes " << __FUNCTION__;
            return  xbridge::Error::INVALID_ADDRESS;

        }
    }

    ptr->from = connFrom->toXAddr(from);
    ptr->to   = connTo->toXAddr(to);
    ptr->usedCoins = outputsForUse;

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

    return xbridge::Error::SUCCESS;
}

//******************************************************************************
//******************************************************************************
bool App::Impl::sendAcceptingTransaction(const TransactionDescrPtr & ptr)
{
    ptr->packet.reset(new XBridgePacket(xbcTransactionAccepting));

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
    ptr->packet->append(ptr->hubAddress);
    ptr->packet->append(ptr->id.begin(), 32);
    ptr->packet->append(ptr->from);
    ptr->packet->append(fc);
    ptr->packet->append(ptr->fromAmount);
    ptr->packet->append(ptr->to);
    ptr->packet->append(tc);
    ptr->packet->append(ptr->toAmount);

    // utxo items
    ptr->packet->append(static_cast<uint32_t>(ptr->usedCoins.size()));
    for (const wallet::UtxoEntry & entry : ptr->usedCoins)
    {
        uint256 txid(entry.txId);
        ptr->packet->append(txid.begin(), 32);
        ptr->packet->append(entry.vout);
        ptr->packet->append(entry.rawAddress);
        ptr->packet->append(entry.signature);
    }

    ptr->packet->sign(ptr->mPubKey, ptr->mPrivKey);

    onSend(ptr->hubAddress, ptr->packet->body());

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
        return xbridge::TRANSACTION_NOT_FOUND;
    }

    if (ptr->state > TransactionDescr::trPending)
    {
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

    return xbridge::SUCCESS;
}

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
bool App::isValidAddress(const string &address) const
{
    // TODO need refactoring
    return ((address.size() >= 32) && (address.size() <= 36));
}

//******************************************************************************
//******************************************************************************
Error App::checkAcceptParams(const uint256 &id, TransactionDescrPtr &ptr)
{
    // TODO need refactoring
    ptr = transaction(id);

    if(!ptr) {
        WARN() << "transaction not found " << __FUNCTION__;
        return xbridge::TRANSACTION_NOT_FOUND;
    }

    return checkAmount(ptr->toCurrency, ptr->toAmount);
}

//******************************************************************************
//******************************************************************************
Error App::checkCreateParams(const string &fromCurrency,
                             const string &toCurrency,
                             const uint64_t &fromAmount)
{
    // TODO need refactoring
    if (fromCurrency.size() > 8 || toCurrency.size() > 8) {
        WARN() << "invalid currency " << __FUNCTION__;
        return xbridge::INVALID_CURRENCY;
    }
    return  checkAmount(fromCurrency, fromAmount);
}

//******************************************************************************
//******************************************************************************
Error App::checkAmount(const string &currency, const uint64_t &amount)
{
    // check amount
    WalletConnectorPtr conn = connectorByCurrency(currency);
    if (!conn) {
        // no session
        WARN() << "no session for <" << currency << "> " << __FUNCTION__;
        return xbridge::NO_SESSION;
    }

    if (conn->getWalletBalance() < (amount / conn->COIN)) {
        WARN() << "insufficient funds for <" << currency << "> " << __FUNCTION__;
        return xbridge::INSIFFICIENT_FUNDS;
    }
    return xbridge::SUCCESS;
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
                    io->post(boost::bind(&xbridge::Session::processPacket, s, packet));

                }
            }
        }
    }

    m_timer.expires_at(m_timer.expires_at() + boost::posix_time::seconds(TIMER_INTERVAL));
    m_timer.async_wait(boost::bind(&Impl::onTimer, this));
}

} // namespace xbridge
