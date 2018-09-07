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
#include "validationstate.h"

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
class App
{
    class Impl;

private:
    /**
     * @brief App - default contructor,
     * initialized and run private implementation
     */
    App();
    /**
     * @brief ~App - destructor
     */
    virtual ~App();

public:
    /**
     * @brief instance - the classical implementation of singletone
     * @return
     */
    static App & instance();

    /**
     * @brief version
     * @return current version of application
     */
    static std::string version();

    /**
     * @brief isEnabled
     * @return enabled by default
     */
    static bool isEnabled();

    /**
     * @brief init init xbridge settings, secp256, exchange,
     * wallet sessions
     * @param argc count of arguments
     * @param argv values of arguments
     * @return true, if all paremeters load successfull
     * @throw std::runtime_error
     */
    bool init(int argc, char *argv[]);

    /**
     * @brief start - start xbrige
     * run services, sessions,
     * @return true if all components run successfull
     */
    bool start();
    /**
     * @brief stop - stopped services
     * @return
     */
    bool stop();

public:
    // classes
    /**
     * @brief summary info about old orders flushed by flushCancelledOrders()
     */
    class FlushedOrder {
    public:
        uint256 id;
        boost::posix_time::ptime txtime;
        int use_count;
        FlushedOrder() = delete;
        FlushedOrder(uint256 id, boost::posix_time::ptime txtime, int use_count)
            : id{id}, txtime{txtime}, use_count{use_count} {}
    };

    // transactions

    /**
     * @brief transaction - find transaction by id
     * @param id - id of transaction
     * @return - pointer to transaction or nullptr, if transaction not found
     */
    TransactionDescrPtr transaction(const uint256 & id) const;
    /**
     * @brief transactions
     * @return map of all transaction
     */
    std::map<uint256, xbridge::TransactionDescrPtr> transactions() const;
    /**
     * @brief history
     * @return map of historical transaction (local canceled and finished)
     */
    std::map<uint256, xbridge::TransactionDescrPtr> history() const;
    /**
     * @brief flushCancelledOrders with txtime older than minAge
     * @return list of all flushed orders
     */
    std::vector<FlushedOrder> flushCancelledOrders(boost::posix_time::time_duration minAge) const;

    /**
     * @brief appendTransaction append transaction into list (map) of transaction if not exits
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
    // TODO make protected
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
     * @brief cancelXBridgeTransaction - cancel xbridge transaction
     * @param id - id of transaction
     * @param reason reason of cancel
     * @return  status of operation
     */
    xbridge::Error cancelXBridgeTransaction(const uint256 &id, const TxCancelReason &reason);
    /**
     * @brief cancelMyXBridgeTransactions - canclel all local transactions
     */
    void cancelMyXBridgeTransactions();

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
     * @param fromAddress - address to pull utxo's from
     * @return xbridge::SUCCESS, if all parameters valid
     */
    xbridge::Error checkAcceptParams(const uint256 &id, TransactionDescrPtr &ptr, const std::string &fromAddress);

    /**
     * @brief checkCreateParams - checks parameter needs to success created transaction
     * @param fromCurrency - from currency
     * @param toCurrency - to currency
     * @param fromAmount -  amount
     * @param fromAddress - address to pull utxo's from
     * @return xbridge::SUCCES, if all parameters valid
     */
    xbridge::Error checkCreateParams(const std::string &fromCurrency, const std::string &toCurrency, const uint64_t &fromAmount, const std::string &fromAddress);

    /**
     * @brief checkAmount - checks wallet balance
     * @param currency - currency name
     * @param amount - amount
     * @param address - optional address to pull utxo's from
     * @return xbridge::SUCCES, if  the session currency is open and
     * on account has sufficient funds for operations
     */
    xbridge::Error checkAmount(const std::string &currency, const uint64_t &amount, const std::string &address = "");
public:
    // connectors

    /**
     * @brief availableCurrencies - list currencies available for the wallet
     * @return local currencies list
     */
    std::vector<std::string> availableCurrencies() const;
    /**
     * @brief networkCurrencies - list currencies supported by the network
     * @return all currencies list
     */
    std::vector<std::string> networkCurrencies() const;

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

    //
    /**
     * @brief sendPacket send packet btadcast to xbridge network
     * @param packet send message via xbridge
     */
    void sendPacket(const XBridgePacketPtr & packet);
    /**
     * @brief sendPacket send packet to xbridge network to specified id,
     * @param id
     * @param packet
     */
    void sendPacket(const std::vector<unsigned char> & id, const XBridgePacketPtr & packet);

    // call when message from xbridge network received
    /**
     * @brief onMessageReceived  call when message from xbridge network received
     * @param id packet id
     * @param message
     * @param state
     */
    void onMessageReceived(const std::vector<unsigned char> & id,
                           const std::vector<unsigned char> & message,
                           CValidationState & state);
    //
    /**
     * @brief onBroadcastReceived - processing recieved   broadcast message
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
     * @brief removePackets remove packet from pending packets (if added)
     * @param txid
     * @return true, if packet found and removed
     */
    bool removePackets(const uint256 & txid);

    /**
     * @brief Sends the services ping to the network (including supported xwallets).
     * @return
     */
    bool sendServicePing();

    /**
     * @brief Returns true if the current node supports the specified service.
     * @return
     */
    bool hasNodeService(const std::string &service);
    /**
     * @brief Returns true if the specified node supports the service.
     * @return
     */
    bool hasNodeService(const ::CPubKey &nodePubKey, const std::string &service);

    /**
     * @brief Returns the all services across all nodes.
     * @return
     */
    std::map<CPubKey, std::set<string> > allServices();
    /**
     * @brief Returns the node services supported by the specified node.
     * @return
     */
    std::set<std::string> nodeServices(const ::CPubKey & nodePubKey);
    bool addNodeServices(const ::CPubKey & nodePubKey,
                         const std::vector<std::string> & services);

    bool findNodeWithService(const std::set<std::string> & services, CPubKey & node) const;

private:
    std::unique_ptr<Impl> m_p;

    /**
     * @brief selectUtxos - Selects available utxos and writes to param outputsForUse.
     * @param addr - currency name
     * @param outputs - available outputs to search
     * @param connFrom - connector
     * @param requiredAmount - amount of required coins
     * @param outputsForUse - selected outputs for use
     * @param utxoAmount - total utxoAmount of selected outputs
     * @param fee1 - min tx fee for outputs
     * @param fee2
     */
    bool selectUtxos(const std::string &addr, const std::vector<wallet::UtxoEntry> &outputs, const WalletConnectorPtr &connFrom,
                     const uint64_t &requiredAmount, std::vector<wallet::UtxoEntry> &outputsForUse,
                     uint64_t &utxoAmount, uint64_t &fee1, uint64_t &fee2) const;
};

} // namespace xbridge

#endif // XBRIDGEAPP_H
