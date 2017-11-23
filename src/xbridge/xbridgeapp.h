//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGEAPP_H
#define XBRIDGEAPP_H

#include "xbridge.h"
#include "xbridgesession.h"
#include "xbridgepacket.h"
#include "uint256.h"
#include "xbridgetransactiondescr.h"
#include "xbridgewalletconnector.h"

#include <thread>
#include <atomic>
#include <vector>
#include <map>
#include <tuple>
#include <set>
#include <queue>

#ifdef WIN32
// #include <Ws2tcpip.h>
#endif

namespace rpc
{
class AcceptedConnection;
}

//*****************************************************************************
//*****************************************************************************
class XBridgeApp
{
    typedef std::vector<unsigned char> UcharVector;

    friend void callback(void * closure, int event,
                         const unsigned char * info_hash,
                         const void * data, size_t data_len);

private:
    XBridgeApp();
    virtual ~XBridgeApp();

public:
    static XBridgeApp & instance();

    static std::string version();

    static bool isEnabled();

    bool init(int argc, char *argv[]);

    bool start();
    bool stop();

public:
    uint256 sendXBridgeTransaction(const std::string & from,
                                   const std::string & fromCurrency,
                                   const uint64_t & fromAmount,
                                   const std::string & to,
                                   const std::string & toCurrency,
                                   const uint64_t & toAmount);
    bool sendPendingTransaction(const XBridgeTransactionDescrPtr & ptr);

    uint256 acceptXBridgeTransaction(const uint256 & id,
                                     const std::string & from,
                                     const std::string & to);
    bool sendAcceptingTransaction(const XBridgeTransactionDescrPtr & ptr);

    bool cancelXBridgeTransaction(const uint256 & id, const TxCancelReason & reason);
    bool sendCancelTransaction(const uint256 & txid, const TxCancelReason & reason);

    bool rollbackXBridgeTransaction(const uint256 & id);
    bool sendRollbackTransaction(const uint256 & txid);

public:
    XBridgeWalletConnectorPtr connectorByCurrency(const std::string & currency) const;

    std::vector<std::string> availableCurrencies() const;

    void addConnector(const XBridgeWalletConnectorPtr & conn);

    bool isKnownMessage(const std::vector<unsigned char> & message);
    void addToKnown(const std::vector<unsigned char> & message);

    XBridgeSessionPtr getSession();
    XBridgeSessionPtr getSession(const std::vector<unsigned char> & address);

    void storeAddressBookEntry(const std::string & currency,
                               const std::string & name,
                               const std::string & address);
    void getAddressBook();

    bool checkUtxoItems(const std::vector<wallet::UtxoEntry> & items);
    bool lockUtxoItems(const std::vector<wallet::UtxoEntry> & items);
    bool txOutIsLocked(const wallet::UtxoEntry & entry) const;

public:// slots:
    // send messave via xbridge
    void onSend(const XBridgePacketPtr & packet);
    void onSend(const UcharVector & id, const XBridgePacketPtr & packet);

    // call when message from xbridge network received
    void onMessageReceived(const std::vector<unsigned char> & id,
                           const std::vector<unsigned char> & message,
                           CValidationState & state);
    // broadcast message
    void onBroadcastReceived(const std::vector<unsigned char> & message,
                             CValidationState & state);

private:
    void onSend(const UcharVector & id, const UcharVector & message);

public:
    static void sleep(const unsigned int umilliseconds);

private:
    boost::thread_group m_threads;

    XBridgePtr        m_bridge;

    mutable boost::mutex m_connectorsLock;
    typedef std::vector<XBridgeWalletConnectorPtr> Connectors;
    Connectors m_connectors;
    typedef std::map<std::vector<unsigned char>, XBridgeWalletConnectorPtr> ConnectorsAddrMap;
    ConnectorsAddrMap m_connectorAddressMap;
    typedef std::map<std::string, XBridgeWalletConnectorPtr> ConnectorsCurrencyMap;
    ConnectorsCurrencyMap m_connectorCurrencyMap;

    mutable boost::mutex m_sessionsLock;
    typedef std::queue<XBridgeSessionPtr> SessionQueue;
    SessionQueue m_sessions;
    typedef std::map<std::vector<unsigned char>, XBridgeSessionPtr> SessionsAddrMap;
    SessionsAddrMap m_sessionAddressMap;

    boost::mutex m_messagesLock;
    typedef std::set<uint256> ProcessedMessages;
    ProcessedMessages m_processedMessages;

    boost::mutex m_addressBookLock;
    typedef std::tuple<std::string, std::string, std::string> AddressBookEntry;
    typedef std::vector<AddressBookEntry> AddressBook;
    AddressBook m_addressBook;
    std::set<std::string> m_addresses;

public:
    static boost::mutex                                  m_txLocker;
    static std::map<uint256, XBridgeTransactionDescrPtr> m_pendingTransactions;
    static std::map<uint256, XBridgeTransactionDescrPtr> m_transactions;
    static std::map<uint256, XBridgeTransactionDescrPtr> m_historicTransactions;

    static boost::mutex                                  m_txUnconfirmedLocker;
    static std::map<uint256, XBridgeTransactionDescrPtr> m_unconfirmed;

    static boost::mutex                                  m_ppLocker;
    static std::map<uint256, XBridgePacketPtr>           m_pendingPackets;

    static boost::mutex                                  m_utxoLocker;
    static std::set<wallet::UtxoEntry>                   m_utxoItems;
};

#endif // XBRIDGEAPP_H
