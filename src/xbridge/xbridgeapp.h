//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGEAPP_H
#define XBRIDGEAPP_H

#include "xbridgesession.h"
#include "xbridgepacket.h"
#include "uint256.h"
#include "xbridgetransactiondescr.h"
#include "xbridgewalletconnector.h"
#include "xbridgedef.h"

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

//*****************************************************************************
//*****************************************************************************
namespace xbridge
{

//*****************************************************************************
//*****************************************************************************
namespace rpc
{
class AcceptedConnection;
}

//*****************************************************************************
//*****************************************************************************
class App
{
    class Impl;

    typedef std::vector<unsigned char> UcharVector;

    friend void callback(void * closure, int event,
                         const unsigned char * info_hash,
                         const void * data, size_t data_len);

private:
    App();
    virtual ~App();

public:
    static App & instance();

    static std::string version();

    static bool isEnabled();

    bool init(int argc, char *argv[]);

    bool start();
    bool stop();

public:
    // transactions

    TransactionDescrPtr transaction(const uint256 & id);
    void appendTransactionToPending(const TransactionDescrPtr & ptr);
    void moveTransactionToHistory(const uint256 & id);

    uint256 sendXBridgeTransaction(const std::string & from,
                                   const std::string & fromCurrency,
                                   const uint64_t & fromAmount,
                                   const std::string & to,
                                   const std::string & toCurrency,
                                   const uint64_t & toAmount);
    bool sendPendingTransaction(const TransactionDescrPtr & ptr);

    uint256 acceptXBridgeTransaction(const uint256 & id,
                                     const std::string & from,
                                     const std::string & to);
    bool sendAcceptingTransaction(const TransactionDescrPtr & ptr);

    bool cancelXBridgeTransaction(const uint256 & id, const TxCancelReason & reason);
    bool sendCancelTransaction(const uint256 & txid, const TxCancelReason & reason);

    bool rollbackXBridgeTransaction(const uint256 & id);
    bool sendRollbackTransaction(const uint256 & txid);

public:
    // connectors

    std::vector<std::string> availableCurrencies() const;

    void addConnector(const WalletConnectorPtr & conn);
    void updateConnector(const WalletConnectorPtr & conn,
                         const std::vector<unsigned char> addr,
                         const std::string & currency);
    WalletConnectorPtr connectorByCurrency(const std::string & currency) const;
    std::vector<WalletConnectorPtr> connectors() const;

public:
    // network

    bool isKnownMessage(const std::vector<unsigned char> & message);
    void addToKnown(const std::vector<unsigned char> & message);

    // send messave via xbridge
    void sendPacket(const XBridgePacketPtr & packet);
    void sendPacket(const UcharVector & id, const XBridgePacketPtr & packet);

    // call when message from xbridge network received
    void onMessageReceived(const std::vector<unsigned char> & id,
                           const std::vector<unsigned char> & message,
                           CValidationState & state);
    // broadcast message
    void onBroadcastReceived(const std::vector<unsigned char> & message,
                             CValidationState & state);

    bool processLater(const uint256 & txid, const XBridgePacketPtr & packet);
    bool removePackets(const uint256 & txid);

public:
    // UTXO
    // TODO move to connector
    bool checkUtxoItems(const std::vector<wallet::UtxoEntry> & items);
    bool lockUtxoItems(const std::vector<wallet::UtxoEntry> & items);
    bool txOutIsLocked(const wallet::UtxoEntry & entry) const;

public:
    static boost::mutex                           m_txLocker;
    static std::map<uint256, TransactionDescrPtr> m_pendingTransactions;
    static std::map<uint256, TransactionDescrPtr> m_transactions;
    static std::map<uint256, TransactionDescrPtr> m_historicTransactions;

    static boost::mutex                           m_txUnconfirmedLocker;
    static std::map<uint256, TransactionDescrPtr> m_unconfirmed;

    static boost::mutex                           m_ppLocker;
    static std::map<uint256, XBridgePacketPtr>    m_pendingPackets;

    static boost::mutex                           m_utxoLocker;
    static std::set<wallet::UtxoEntry>            m_utxoItems;

private:
    std::unique_ptr<Impl> m_p;
};

} // namespace xbridge

#endif // XBRIDGEAPP_H
