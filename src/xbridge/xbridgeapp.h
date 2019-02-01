#include <utility>

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
#include <functional>
#include <map>
#include <tuple>
#include <set>
#include <queue>

#ifdef WIN32
// #include <Ws2tcpip.h>
#endif

class xQuery;
class CurrencyPair;
class xAggregate;
class xSeriesCache;

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
    bool isEnabled();

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
     * @brief Stores the supported services on servicenodes (xwallets).
     */
    class XWallets {
    public:
        XWallets() :  _version(0), _nodePubKey(::CPubKey()), _services(std::set<std::string>()) {}
        XWallets(const uint32_t version, const ::CPubKey & nodePubKey, const std::set<std::string> services)
            :  _version(version), _nodePubKey(nodePubKey), _services(std::move(services)) {}
        uint32_t version() const { return _version; };
        ::CPubKey nodePubKey() const { return _nodePubKey; };
        std::set<std::string> services() const { return _services; };
    private:
        uint32_t _version;
        ::CPubKey _nodePubKey;
        std::set<std::string> _services;
    };

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

    // Settings
    /**
     * @brief Load xbridge.conf settings file.
     */
    bool loadSettings();

    // Shutdown
    /**
     * @brief Disconnects all wallets loaded by this node and notifies the network of the empty service list.
     */
    bool disconnectWallets();

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
     * @brief history_matches returns details of local transactions that match given filter,
     * it is like the history() call but instead of copying the entire map container it
     * includes only transactions matching TransactionFilter and xQuery
     * @param - filter to apply and other query parameters
     * @return - list of individual matching local transactions
     */
    using TransactionFilter = std::function<void(std::vector<CurrencyPair>& matches,
                                                 const TransactionDescr& tr,
                                                 const xQuery& query)>;
    std::vector<CurrencyPair> history_matches(const TransactionFilter& filter, const xQuery& query);

    /**
     * @brief getXSeriesCache
     * @return - reference to cache of open,high,low,close transaction aggregated series
     */
    xSeriesCache& getXSeriesCache();

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
     * @param wallet connection to check against
     * @return true, if address valid
     */
    bool isValidAddress(const std::string &address, WalletConnectorPtr &conn) const;

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

    /**
     * Store list of orders to watch for counterparty spent deposit.
     * @param tr
     * @return true if watching, false if not watching
     */
    bool watchForSpentDeposit(TransactionDescrPtr tr);

    /**
     * Stop watching for a spent deposit.
     * @param tr
     */
    void unwatchSpentDeposit(TransactionDescrPtr tr);

    /**
     * Store list of orders to watch for redeeming refund on behalf of trader.
     * @param tr
     * @return true if watching, false if not watching
     */
    bool watchTraderDeposit(TransactionPtr tr);

    /**
     * Stop watching for a trader redeeming.
     * @param tr
     */
    void unwatchTraderDeposit(TransactionPtr tr);

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
     * @brief Removes the specified connector.
     * @param conn connector to remove
     */
    void removeConnector(const std::string & currency);

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
     * Updates the active wallets list. Active wallets are those that are running and responding
     * to rpc calls.
     */
    void updateActiveWallets();

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
    bool isKnownMessage(const uint256 & hash);
    /**
     * @brief addToKnown - add message to queue of processed messages
     * @param message
     */
    void addToKnown(const std::vector<unsigned char> & message);
    void addToKnown(const uint256 & hash);

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
    bool sendServicePing(std::vector<std::string> &nonWalletServices);

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
    std::map<CPubKey, XWallets> allServices();
    /**
     * @brief Returns the node services supported by the specified node.
     * @return
     */
    std::set<std::string> nodeServices(const ::CPubKey & nodePubKey);
    bool addNodeServices(const ::CPubKey & nodePubKey,
                         const std::vector<std::string> & services,
                         const uint32_t version);

    /**
     * @brief Finds a servicenode with the specified services that is also not in an "excluded" set.
     * @param services
     * @param node
     * @param notIn This set contains servicenode CPubKey's that we are ignoring
     * @return
     */
    bool findNodeWithService(const std::set<std::string> & services, CPubKey & node, const std::set<CPubKey> & notIn) const;

    /**
     * @brief Clears the bad wallet designations.
     */
    void clearBadWallets() {
        LOCK(m_updatingWalletsLock);
        m_badWallets.clear();
    }

    /**
     * @brief Returns true if wallet update checks are already in progress, otherwise returns false.
     * @return
     */
    bool isUpdatingWallets() {
        LOCK(m_updatingWalletsLock);
        return m_updatingWallets;
    }

    /**
     * @brief Returns a copy of the locked fee utxos.
     * @return
     */
    const std::set<xbridge::wallet::UtxoEntry> getFeeUtxos();

    /**
     * @brief Lock the specified fee utxos. This prevents fee utxos from being used in orders.
     * @param feeUtxos
     */
    void lockFeeUtxos(std::set<xbridge::wallet::UtxoEntry> & feeUtxos);

    /**
     * @brief Unlocks the fee utxos, allowing them to be used in orders.
     * @param feeUtxos
     */
    void unlockFeeUtxos(std::set<xbridge::wallet::UtxoEntry> & feeUtxos);

    /**
     * @brief Returns a copy of the locked non-fee utxos.
     * @return
     */
    const std::set<xbridge::wallet::UtxoEntry> getLockedUtxos(const std::string & token);

    /**
     * @brief Returns a copy of both the locked fee and non-fee utxos.
     * @return
     */
    const std::set<xbridge::wallet::UtxoEntry> getAllLockedUtxos(const std::string & token);

    /**
     * @brief Lock the specified utxos. Returns false if specified utxos are already locked.
     * @param utxos
     * @return
     */
    bool lockCoins(const std::string & token, const std::vector<wallet::UtxoEntry> & utxos);

    /**
     * @brief Unlock the specified utxos.
     * @param utxos
     */
    void unlockCoins(const std::string & token, const std::vector<wallet::UtxoEntry> & utxos);

protected:
    void clearMempool();

private:
    std::unique_ptr<Impl> m_p;
    bool m_disconnecting;
    CCriticalSection m_lock;
    std::map<std::string, boost::posix_time::ptime> m_badWallets;
    bool m_updatingWallets{false};
    CCriticalSection m_updatingWalletsLock;

    std::set<xbridge::wallet::UtxoEntry> m_feeUtxos;
    std::map<std::string, std::set<xbridge::wallet::UtxoEntry> > m_utxosDict;
    CCriticalSection m_utxosLock;
    CCriticalSection m_utxosOrderLock;

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
