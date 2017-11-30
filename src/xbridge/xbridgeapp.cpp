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
#include "init.h"
#include "wallet.h"

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
std::map<uint256, XBridgePacketPtr>           XBridgeApp::m_pendingPackets;
boost::mutex                                  XBridgeApp::m_utxoLocker;
std::set<wallet::UtxoEntry>                   XBridgeApp::m_utxoItems;

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
{
    boost::mutex::scoped_lock l(m_sessionsLock);

    for (uint32_t i = 0; i < boost::thread::hardware_concurrency(); ++i)
    {
        XBridgeSessionPtr ptr(new XBridgeSession());
        m_sessions.push(ptr);
        m_sessionAddressMap[ptr->sessionAddr()] = ptr;
    }
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
bool XBridgeApp::stop()
{
    LOG() << "stopping threads...";

    m_bridge->stop();

    m_threads.join_all();

    // secp stop
    xbridge::ECC_Stop();

    return true;
}

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
XBridgeSessionPtr XBridgeApp::getSession()
{
    XBridgeSessionPtr ptr;

    boost::mutex::scoped_lock l(m_sessionsLock);
    ptr = m_sessions.front();
    m_sessions.pop();
    m_sessions.push(ptr);

    return ptr;
}

//*****************************************************************************
//*****************************************************************************
XBridgeSessionPtr XBridgeApp::getSession(const std::vector<unsigned char> & address)
{
    boost::mutex::scoped_lock l(m_sessionsLock);
    if (m_sessionAddressMap.count(address))
    {
        return m_sessionAddressMap[address];
    }

    return XBridgeSessionPtr();
}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::onMessageReceived(const UcharVector & id,
                                   const UcharVector & message,
                                   CValidationState & /*state*/)
{
    if (isKnownMessage(message))
    {
        return;
    }

    addToKnown(message);

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
        // ERR() << "incorrect protocol version <" << packet->version() << "> " << __FUNCTION__;
        return;
    }

    // check direct session address
    XBridgeSessionPtr ptr = getSession(id);
    if (ptr)
    {
        ptr->processPacket(packet);
    }

    else
    {
        {
            // if no session address - find connector address
            boost::mutex::scoped_lock l(m_connectorsLock);
            if (m_connectorAddressMap.count(id))
            {
                ptr = getSession();
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
void XBridgeApp::onBroadcastReceived(const std::vector<unsigned char> & message,
                                     CValidationState & /*state*/)
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
        // ERR() << "incorrect protocol version <" << packet->version() << "> " << __FUNCTION__;
        return;
    }

    XBridgeSessionPtr ptr = getSession();
    if (ptr)
    {
        ptr->processPacket(packet);
    }
}

//*****************************************************************************
//*****************************************************************************
// static
void XBridgeApp::sleep(const unsigned int umilliseconds)
{
    boost::this_thread::sleep_for(boost::chrono::milliseconds(umilliseconds));
}

//*****************************************************************************
//*****************************************************************************
XBridgeWalletConnectorPtr XBridgeApp::connectorByCurrency(const std::string & currency) const
{
    boost::mutex::scoped_lock l(m_connectorsLock);
    if (m_connectorCurrencyMap.count(currency))
    {
        return m_connectorCurrencyMap.at(currency);
    }

    return XBridgeWalletConnectorPtr();
}

//*****************************************************************************
//*****************************************************************************
std::vector<std::string> XBridgeApp::availableCurrencies() const
{
    boost::mutex::scoped_lock l(m_connectorsLock);

    std::vector<std::string> currencies;

    for(auto i = m_connectorCurrencyMap.begin(); i != m_connectorCurrencyMap.end();)
    {
        currencies.push_back(i->first);
        ++i;
    }

    return currencies;
}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::addConnector(const XBridgeWalletConnectorPtr & conn)
{
    boost::mutex::scoped_lock l(m_sessionsLock);
    m_connectors.push_back(conn);
    m_connectorCurrencyMap[conn->currency] = conn;
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
void XBridgeApp::getAddressBook()
{
    boost::mutex::scoped_lock l(m_connectorsLock);

    for (Connectors::iterator i = m_connectors.begin(); i != m_connectors.end(); ++i)
    {
        std::vector<wallet::AddressBookEntry> entries;
        (*i)->requestAddressBook(entries);

        for (const wallet::AddressBookEntry & e : entries)
        {
            for (const std::string & addr : e.second)
            {
                std::vector<unsigned char> vaddr = (*i)->toXAddr(addr);
                // m_addressBook.insert(vaddr);
                m_connectorAddressMap[vaddr] = (*i);
                m_connectorCurrencyMap[(*i)->currency] = (*i);

                xuiConnector.NotifyXBridgeAddressBookEntryReceived
                        ((*i)->currency, e.first, addr);
            }
        }
    }
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeApp::checkUtxoItems(const std::vector<wallet::UtxoEntry> & items)
{
    for (const wallet::UtxoEntry & item : items)
    {
        if (txOutIsLocked(item))
        {
            return false;
        }
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeApp::lockUtxoItems(const std::vector<wallet::UtxoEntry> & items)
{
    bool hasDuplicate = false;
    boost::mutex::scoped_lock l(m_utxoLocker);
    for (const wallet::UtxoEntry & item : items)
    {
        if (!m_utxoItems.insert(item).second)
        {
            // duplicate items
            hasDuplicate = true;
            break;
        }
    }

    if (hasDuplicate)
    {
        // TODO remove items?
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeApp::txOutIsLocked(const wallet::UtxoEntry & entry) const
{
    boost::mutex::scoped_lock l(m_utxoLocker);
    if (m_utxoItems.count(entry))
    {
        return true;
    }
    return false;
}

//******************************************************************************
//******************************************************************************
uint256 XBridgeApp::sendXBridgeTransaction(const std::string & from,
                                           const std::string & fromCurrency,
                                           const uint64_t    & fromAmount,
                                           const std::string & to,
                                           const std::string & toCurrency,
                                           const uint64_t    & toAmount)
{
    if (fromCurrency.size() > 8 || toCurrency.size() > 8)
    {
        LOG() << "invalid currency" << __FUNCTION__;
        return uint256();
    }

    XBridgeWalletConnectorPtr connFrom = connectorByCurrency(fromCurrency);
    XBridgeWalletConnectorPtr connTo   = connectorByCurrency(toCurrency);
    if (!connFrom || !connTo)
    {
        uiInterface.ThreadSafeMessageBox(_("No connector for ") + (!connFrom ? fromCurrency : toCurrency),
                                         "blocknet",
                                         CClientUIInterface::BTN_OK | CClientUIInterface::ICON_INFORMATION | CClientUIInterface::MODAL);

        WARN() << "no connector for <" << (!connFrom ? fromCurrency : toCurrency) << "> " << __FUNCTION__;
        return uint256();
    }

    // check amount
    std::vector<wallet::UtxoEntry> outputs;
    connFrom->getUnspent(outputs);

    double utxoAmount = 0;
    std::vector<wallet::UtxoEntry> outputsForUse;
    for (const wallet::UtxoEntry & entry : outputs)
    {
        if (!txOutIsLocked(entry))
        {
            utxoAmount += entry.amount;
            outputsForUse.push_back(entry);

            // TODO calculate fee for outputsForUse.count()

            if (utxoAmount > fromAmount)
            {
                break;
            }
        }
    }

    if ((utxoAmount * XBridgeTransactionDescr::COIN) < fromAmount)
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
    ptr->from         = connFrom->toXAddr(from);
    ptr->fromCurrency = fromCurrency;
    ptr->fromAmount   = fromAmount;
    ptr->to           = connTo->toXAddr(to);
    ptr->toCurrency   = toCurrency;
    ptr->toAmount     = toAmount;
    ptr->usedCoins    = outputsForUse;

    // try send immediatelly
    if (!sendPendingTransaction(ptr))
    {
        uiInterface.ThreadSafeMessageBox(_("Transaction send error "),
                                         "blocknet",
                                         CClientUIInterface::BTN_OK | CClientUIInterface::ICON_INFORMATION | CClientUIInterface::MODAL);

        LOG() << "Transaction failed <" << fromCurrency << "> " << __FUNCTION__;
        return uint256();
    }

//    LOG() << "accept transaction " << util::to_str(ptr->id) << std::endl
//          << "    from " << from << " (" << util::to_str(ptr->from) << ")" << std::endl
//          << "             " << ptr->fromCurrency << " : " << ptr->fromAmount << std::endl
//          << "    from " << to << " (" << util::to_str(ptr->to) << ")" << std::endl
//          << "             " << ptr->toCurrency << " : " << ptr->toAmount << std::endl;

    // lock used coins
    // connFrom->lockUnspent(ptr->usedCoins, true);

    {
        boost::mutex::scoped_lock l(m_txLocker);
        m_pendingTransactions[id] = ptr;
    }

    return id;
}

//******************************************************************************
//******************************************************************************
bool XBridgeApp::sendPendingTransaction(const XBridgeTransactionDescrPtr & ptr)
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
        }
    }

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

    XBridgeWalletConnectorPtr connFrom = connectorByCurrency(ptr->fromCurrency);
    XBridgeWalletConnectorPtr connTo   = connectorByCurrency(ptr->toCurrency);
    if (!connFrom || !connTo)
    {
        uiInterface.ThreadSafeMessageBox(_("No connector for ") + (!connFrom ? ptr->fromCurrency : ptr->toCurrency),
                                         "blocknet",
                                         CClientUIInterface::BTN_OK | CClientUIInterface::ICON_INFORMATION | CClientUIInterface::MODAL);

        WARN() << "no connector for <" << (!connFrom ? ptr->fromCurrency : ptr->toCurrency) << "> " << __FUNCTION__;
        return false;
    }

    // check amount
    std::vector<wallet::UtxoEntry> outputs;
    connTo->getUnspent(outputs);

    double utxoAmount = 0;
    std::vector<wallet::UtxoEntry> outputsForUse;
    for (const wallet::UtxoEntry & entry : outputs)
    {
        if (!txOutIsLocked(entry))
        {
            utxoAmount += entry.amount;
            outputsForUse.push_back(entry);

            // TODO calculate fee for outputsForUse.count()

            if (utxoAmount > ptr->toAmount)
            {
                break;
            }
        }
    }

    if ((utxoAmount * XBridgeTransactionDescr::COIN) < ptr->toAmount)
    {
        uiInterface.ThreadSafeMessageBox(_("Insufficient funds for ") + ptr->toCurrency,
                                         "blocknet",
                                         CClientUIInterface::BTN_OK | CClientUIInterface::ICON_INFORMATION | CClientUIInterface::MODAL);

        LOG() << "insufficient funds for <" << ptr->toCurrency << "> " << __FUNCTION__;
        return uint256();
    }

    ptr->from = connTo->toXAddr(from);
    ptr->to   = connFrom->toXAddr(to);
    std::swap(ptr->fromCurrency, ptr->toCurrency);
    std::swap(ptr->fromAmount,   ptr->toAmount);
    ptr->usedCoins = outputsForUse;

    // try send immediatelly
    if (!sendAcceptingTransaction(ptr))
    {
        uiInterface.ThreadSafeMessageBox(_("Transaction send error"),
                                         "blocknet",
                                         CClientUIInterface::BTN_OK | CClientUIInterface::ICON_INFORMATION | CClientUIInterface::MODAL);
        return uint256();
    }

//    LOG() << "accept transaction " << util::to_str(ptr->id) << std::endl
//          << "    from " << from << " (" << util::to_str(ptr->from) << ")" << std::endl
//          << "             " << ptr->fromCurrency << " : " << ptr->fromAmount << std::endl
//          << "    from " << to << " (" << util::to_str(ptr->to) << ")" << std::endl
//          << "             " << ptr->toCurrency << " : " << ptr->toAmount << std::endl;


    // lock used coins
    // connTo->lockUnspent(ptr->usedCoins, true);

    return id;
}

//******************************************************************************
//******************************************************************************
bool XBridgeApp::sendAcceptingTransaction(const XBridgeTransactionDescrPtr & ptr)
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
    }

    onSend(ptr->hubAddress, ptr->packet);

    ptr->state = XBridgeTransactionDescr::trAccepting;
    xuiConnector.NotifyXBridgeTransactionStateChanged(ptr->id, XBridgeTransactionDescr::trAccepting);

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeApp::cancelXBridgeTransaction(const uint256 & id,
                                          const TxCancelReason & reason)
{
    if (sendCancelTransaction(id, reason))
    {
        boost::mutex::scoped_lock l(m_txLocker);

        m_pendingTransactions.erase(id);
        if (m_transactions.count(id))
        {
            m_transactions[id]->state = XBridgeTransactionDescr::trCancelled;
            xuiConnector.NotifyXBridgeTransactionStateChanged(id, XBridgeTransactionDescr::trCancelled);
        }
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeApp::rollbackXBridgeTransaction(const uint256 & id)
{
    XBridgeWalletConnectorPtr conn;
    {
        boost::mutex::scoped_lock l(m_txLocker);

        if (m_transactions.count(id))
        {
            XBridgeTransactionDescrPtr ptr = m_transactions[id];
            if (!ptr->refTx.empty())
            {
                conn = connectorByCurrency(ptr->fromCurrency);
                if (!conn)
                {
                    ERR() << "no connector for currency " << ptr->fromCurrency;
                    return false;
                }
            }
        }
    }

//    if (conn)
//    {
//        // session use m_txLocker, must be unlocked because not recursive
//        if (!conn->rollbacktXBridgeTransaction(id))
//        {
//            LOG() << "revert tx failed for " << id.ToString();
//            return false;
//        }

//        sendRollbackTransaction(id);
//    }

    return true;
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

