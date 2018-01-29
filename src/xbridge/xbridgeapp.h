//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGEAPP_H
#define XBRIDGEAPP_H

#include "xbridgesession.h"
#include "xbridgepacket.h"
#include "uint256.h"
#include "xbridgetransactiondescr.h"
#include "util/xbridgeerror.h"
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
/**
 * @brief The App class
 */
class App
{
    /**
     *
     */
    class Impl;

private:
    /**
     * @brief App - default constructor
     */
    App();
    virtual ~App();

public:
    /**
     * @brief instance - the classical implementation of Singletone
     * @return
     */
    static App & instance();

    /**
     * @brief version
     * @return version of application
     */
    static std::string version();

    /**
     * @brief isEnabled - enabled by default
     * @return true
     */
    static bool isEnabled();

    /**
     * @brief init init xbridge settings, secp256, exchange and sessions for currency
     * @param argc - count of arguments
     * @param argv - values of arguments
     * @return
     */
    bool init(int argc, char *argv[]);

    /**
     * @brief start -  start xbrige, services and threads, sessions
     * @return
     */
    bool start();
    /**
     * @brief stop - stop xbridge
     * @return
     */
    bool stop();

public:
    // transactions

    /**
     * @brief transaction
     * @param id - id of transaction
     * @return pointer to transaction if transaction found in list of opened or history transactions,
     * or nullptr
     */
    TransactionDescrPtr transaction(const uint256 & id) const;
    /**
     * @brief transactions
     * @return list (map) of opened transactions
     */
    std::map<uint256, xbridge::TransactionDescrPtr> transactions() const;
    /**
     * @brief history
     * @return list (map) of historycal transactions
     */
    std::map<uint256, xbridge::TransactionDescrPtr> history() const;

    /**
     * @brief appendTransaction - append transaction into list (map) of transaction if not exits
     * of update timestamp of transaction, if transaction exists
     * @param ptr
     */
    void appendTransaction(const TransactionDescrPtr & ptr);

    /**
     * @brief moveTransactionToHistory - move transaction from list of opened transactions
     * to list (map) historycal transactions,
     *  removed all pending packets associated with transaction
     * @param id - id of transaction
     */
    void moveTransactionToHistory(const uint256 & id);

    /**
     * @brief sendXBridgeTransaction - create new xbridge transaction and send to network
     * @param from - source address
     * @param fromCurrency - source currency
     * @param fromAmount - source amount
     * @param to - destionation amount
     * @param toCurrency - destionation currency
     * @param toAmount - destionation amount
     * @param id - id of transaction
     * @param blockHash
     * @return xbridge::SUCCES if success, else error code
     */
    Error sendXBridgeTransaction(const std::string & from,
                                 const std::string & fromCurrency,
                                 const uint64_t & fromAmount,
                                 const std::string & to,
                                 const std::string & toCurrency,
                                 const uint64_t & toAmount,
                                 uint256 & id,
                                 uint256& blockHash);

    /**
     * @brief sendPendingTransaction - send packet with data of pending transaction to network
     * @param ptr
     * @return true, if transaction data valid and sending to network
     */
    bool sendPendingTransaction(const TransactionDescrPtr & ptr);

    /**
     * @brief acceptXBridgeTransaction - accept transaction
     * @param id - id of  transaction
     * @param from - destionation address
     * @param to - source address
     * @return xbridge::SUCCESS, if transaction success accepted
     */
    Error acceptXBridgeTransaction(const uint256 & id,
                                   const std::string & from,
                                   const std::string & to);
    /**
     * @brief sendAcceptingTransaction - send packet with accepting transaction data
     * @param ptr - accepting transaction
     * @return status of operation
     */
    bool sendAcceptingTransaction(const TransactionDescrPtr & ptr);

    /**
     * @brief cancelXBridgeTransaction - cancel xbridge transaction
     * @param id - id of transactiob
     * @param reason reason of cancel
     * @return  status of operation
     */
    xbridge::Error cancelXBridgeTransaction(const uint256 &id, const TxCancelReason &reason);

    /**
     * @brief sendCancelTransaction - send packet with info about canceled transaction
     * @param txid - id of transaction
     * @param reason - reason of cancel
     * @return operation status
     */
    bool sendCancelTransaction(const uint256 &txid, const TxCancelReason &reason);

    /**
     * @brief rollbackXBridgeTransaction - rollback transaction
     * @param id - id of transaction
     * @return xbridge::SUCCES or error code
     */
    xbridge::Error rollbackXBridgeTransaction(const uint256 &id);
    /**
     * @brief sendRollbackTransaction  - send packet with info about rollback transaction
     * @param txid - id of transaction
     * @return true, if success
     */
    bool sendRollbackTransaction(const uint256 &txid);

    /**
     * @brief isValidAddress checks the correctness of the address
     * @param address checked address
     * @return true, if address valid
     */
    bool isValidAddress(const std::string &address) const;

    /**
     * @brief checkAcceptParams checks the correctness of the parameters
     * @param id - id accepted transaction
     * @param ptr - smart pointer to accepted transaction
     * @return xbridge::SUCCESS, if all parameters valid
     */
    xbridge::Error checkAcceptParams(const uint256 &id, TransactionDescrPtr &ptr);

    /**
     * @brief checkCreateParams - checks parameter needs to success created transaction
     * @param fromCurrency - from currency
     * @param toCurrency - to currency
     * @param fromAmount -  amount
     * @return xbridge::SUCCES, if all parameters valid
     */
    xbridge::Error checkCreateParams(const std::string &fromCurrency, const std::string &toCurrency, const uint64_t &fromAmount);

    /**
     * @brief checkAmount - checks wallet balance
     * @param currency - currency name
     * @param amount - amount
     * @return xbridge::SUCCES, if  the session currency is open and
     * on account has sufficient funds for operations
     */
    xbridge::Error checkAmount(const std::string &currency, const uint64_t &amount);
public:
    // connectors

    /**
     * @brief availableCurrencies
     * @return list of available currencies
     */
    std::vector<std::string> availableCurrencies() const;
    /**
     * @brief hasCurrency  - checks connector with currency
     * @param currency - needs currency
     * @return true, if app has connect to currency
     */
    bool hasCurrency(const std::string & currency) const;

    /**
     * @brief addConnector - added new connector to list available connectors to currenies
     * @param conn - new connector
     */
    void addConnector(const WalletConnectorPtr & conn);
    /**
     * @brief updateConnector - update connector params
     * @param conn - pointer to connector
     * @param addr - new currency name address
     * @param currency - currency name
     */
    void updateConnector(const WalletConnectorPtr & conn,
                         const std::vector<unsigned char> addr,
                         const std::string & currency);
    /**
     * @brief connectorByCurrency
     * @param currency - currency name
     * @return exists connector or new instance of WalletConnectorPtr
     */
    WalletConnectorPtr connectorByCurrency(const std::string & currency) const;
    /**
     * @brief connectors
     * @return list of available connectors
     */
    std::vector<WalletConnectorPtr> connectors() const;

public:
    // network

    /**
     * @brief isKnownMessage - checks message status
     * @param message - message
     * @return true, if message known and processing
     */
    bool isKnownMessage(const std::vector<unsigned char> & message);
    /**
     * @brief addToKnown - add message to queue of processed messages
     * @param message
     */
    void addToKnown(const std::vector<unsigned char> & message);

    // send messave via xbridge
    /**
     * @brief sendPacket
     * @param packet
     */
    void sendPacket(const XBridgePacketPtr & packet);
    /**
     * @brief sendPacket send packet to xbridge network to specified id,
     *  or broadcast, when id is empty
     * @param id - id of transaction
     * @param packet
     */
    void sendPacket(const std::vector<unsigned char> & id, const XBridgePacketPtr & packet);

    //
    /**
     * @brief onMessageReceived  call when message from xbridge network received
     * @param id - id of transaction
     * @param message - message from network
     * @param state
     */
    void onMessageReceived(const std::vector<unsigned char> & id,
                           const std::vector<unsigned char> & message,
                           CValidationState & state);

    /**
     * @brief onBroadcastReceived  broadcast message
     * @param message
     * @param state
     */
    void onBroadcastReceived(const std::vector<unsigned char> & message,
                             CValidationState & state);

    /**
     * @brief processLater
     * @param txid
     * @param packet
     * @return
     */
    bool processLater(const uint256 & txid, const XBridgePacketPtr & packet);
    /**
     * @brief removePackets remove from pending packets (if added)
     * @param txid
     * @return
     */
    bool removePackets(const uint256 & txid);

private:
    std::unique_ptr<Impl> m_p;
};

} // namespace xbridge

#endif // XBRIDGEAPP_H
