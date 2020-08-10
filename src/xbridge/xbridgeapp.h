// Copyright (c) 2017-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//*****************************************************************************
//*****************************************************************************

#ifndef BLOCKNET_XBRIDGE_XBRIDGEAPP_H
#define BLOCKNET_XBRIDGE_XBRIDGEAPP_H

#include <xbridge/util/xbridgeerror.h>
#include <xbridge/util/xutil.h>
#include <xbridge/xbridgedb.h>
#include <xbridge/xbridgedef.h>
#include <xbridge/xbridgepacket.h>
#include <xbridge/xbridgetransactiondescr.h>
#include <xbridge/xbridgewalletconnector.h>

#include <amount.h>
#include <consensus/validation.h>
#include <primitives/transaction.h>
#include <uint256.h>
#ifdef ENABLE_WALLET
#include <wallet/wallet.h>
#endif // ENABLE_WALLET

#include <atomic>
#include <functional>
#include <map>
#include <queue>
#include <set>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include <boost/filesystem/fstream.hpp>

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

extern bool CanAffordFeePayment(const CAmount & fee);
extern WalletConnectorPtr ConnectorByCurrency(const std::string & currency);

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
    static uint32_t version();

    /**
     * @brief version
     * @return current version of application
     */
    static std::string versionStr();

    /**
     * @brief createConf creates an empty xrouter.conf if one is not found
     * @return true if created otherwise false
     */
    static bool createConf();

    /**
     * Save configuration files to the specified path.
     */
    static void saveConf(const boost::filesystem::path& p, const std::string& str) {
        boost::filesystem::ofstream file;
        file.exceptions(std::ofstream::failbit | std::ofstream::badbit);
        file.open(p, std::ios_base::binary);
        file.write(str.c_str(), str.size());
    }

    /**
     * @brief isEnabled
     * @return enabled by default
     */
    bool isEnabled();

    /**
     * @brief init init xbridge settings, secp256, exchange,
     * wallet sessions
     * @return true, if all paremeters load successfull
     * @throw std::runtime_error
     */
    bool init();

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
     * @param utxos - use these unspent transaction outputs (implies fees will be subtracted from this total)
     * @param partialOrder - partial order flag
     * @param partialMinimum - partial minimum amount
     * @param id - id of transaction
     * @param blockHash
     * @return xbridge::SUCCESS if success, else error code
     */
    Error sendXBridgeTransaction(const std::string & from,
                                 const std::string & fromCurrency,
                                 const CAmount & fromAmount,
                                 const std::string & to,
                                 const std::string & toCurrency,
                                 const CAmount & toAmount,
                                 const std::vector<wallet::UtxoEntry> utxos,
                                 bool partialOrder,
                                 bool repostOrder,
                                 CAmount partialMinimum,
                                 uint256 & id,
                                 uint256 & blockHash,
                                 const uint256 parentid=uint256());

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
     * @return xbridge::SUCCESS if success, else error code
     */
    Error sendXBridgeTransaction(const std::string & from,
                                 const std::string & fromCurrency,
                                 const CAmount & fromAmount,
                                 const std::string & to,
                                 const std::string & toCurrency,
                                 const CAmount & toAmount,
                                 uint256 & id,
                                 uint256 & blockHash);

    /**
     * @brief repostXBridgeTransaction - reposts a partial order with limited utxo set. Fees are subtracted
     * from the total utxo amount.
     * @param from - source address
     * @param fromCurrency - source currency
     * @param to - destionation amount
     * @param toCurrency - destionation currency
     * @param makerAmount - maker's from amount
     * @param takerAmount - maker's to amount
     * @param minFromAmount - the minimum size that can be taken from maker
     * @param utxos - use these unspent transaction outputs (implies fees will be subtracted from this total)
     * @param parentid - Parent order id
     * @return xbridge::SUCCESS if success, else error code
     */
    Error repostXBridgeTransaction(std::string from, std::string fromCurrency, std::string to, std::string toCurrency,
            CAmount makerAmount, CAmount takerAmount, uint64_t minFromAmount, const std::vector<wallet::UtxoEntry> utxos,
            const uint256 parentid=uint256());

    // TODO make protected
    /**
     * @brief sendPendingTransaction - send packet with data of pending transaction to network
     * @param ptr
     * @return true, if transaction data valid and sending to network
     */
    bool sendPendingTransaction(const TransactionDescrPtr & ptr);

    /**
     * @brief acceptXBridgeTransaction - accept order (supports partial orders)
     * @param id - id of  transaction
     * @param from - destionation address
     * @param to - source address
     * @param fromSize - new maker amount
     * @param toSize - new taker amount
     * @return xbridge::SUCCESS, if transaction success accepted
     */
    Error acceptXBridgeTransaction(const uint256 & id, const std::string & from, const std::string & to,
                                   uint64_t fromSize, uint64_t toSize);

    /**
     * @brief cancelXBridgeTransaction - cancel xbridge transaction
     * @param id - id of transaction
     * @param reason reason of cancel
     * @return  status of operation
     */
    xbridge::Error cancelXBridgeTransaction(const uint256 &id, const TxCancelReason &reason);

    /**
     * @brief cancelMyXBridgeTransactions - cancel all local transactions
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
     * @brief checkAcceptParams checks that the token wallet has enough to cover the balance.
     * @param fromCurrency - token to be taken
     * @param fromAmount - amount to be taken
     * @return xbridge::SUCCESS, if all parameters valid
     */
    xbridge::Error checkAcceptParams(std::string fromCurrency, uint64_t fromAmount);

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
     * Return all the services on this node.
     * @param includeXRouter defaults to true
     * @return
     */
    std::vector<std::string> myServices(bool includeXRouter = true) const;

    /**
     * Return all the services on this node in json format.
     * @return
     */
    std::string myServicesJSON() const;

    /**
     * @brief Returns the all services across all nodes.
     * @return
     */
    static std::map<CPubKey, XWallets> allServices();

    /**
     * @brief Returns the wallet specific services (non-xrouter services).
     * @return
     */
    static std::map<CPubKey, XWallets> walletServices();

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
     * @brief Clears the non-local orders.
     */
    void clearNonLocalOrders();

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

    /**
     * @brief Returns true if xbridge can afford to pay the specified BLOCK fee. i.e. there's
     * sufficient utxos available to cover the fee.
     * @param fee
     * @return true if can afford to pay fee, otherwise false
     */
    bool canAffordFeePayment(const CAmount & fee);

    /**
     * @brief selectUtxos - Selects available utxos and writes to param outputsForUse.
     * @param addr - currency name
     * @param outputs - available outputs to search
     * @param minTxFee1 - function calculating fee1
     * @param minTxFee2 - function calculating fee2
     * @param requiredAmount - amount of required coins
     * @param coinDenomination - int64_t coin denomination
     * @param outputsForUse - selected outputs for use
     * @param utxoAmount - total utxoAmount of selected outputs
     * @param fee1 - min tx fee for outputs
     * @param fee2
     */
    bool selectUtxos(const std::string &addr, const std::vector<wallet::UtxoEntry> &outputs,
            const std::function<double(uint32_t, uint32_t)> &minTxFee1,
            const std::function<double(uint32_t, uint32_t)> &minTxFee2,
            const uint64_t &requiredAmount, const int64_t &coinDenomination,
            std::vector<wallet::UtxoEntry> &outputsForUse,
            uint64_t &utxoAmount, uint64_t &fee1, uint64_t &fee2) const;

    /**
     * selectPartialUtxos - Selects utxos for use with the partial order.
     * @param addr Currency address
     * @param outputs Available utxos to search
     * @param minTxFee1 fee1 func
     * @param minTxFee2 fee2 func
     * @param requiredAmount total required amount (not including fees)
     * @param requiredUtxoCount number of utxos required
     * @param requiredFeePerUtxo fees per utxo required
     * @param requiredSplitSize size of each utxo not including fee
     * @param requiredRemainder size of the last utxo, the remainder
     * @param requiredPrepTxFees fees required to submit partial order prep tx
     * @param outputsForUse selected utxos
     * @param utxoAmount total amount of selected utxos
     * @param fees total amount of fees
     * @param exactUtxoMatch true if no prep tx is required
     * @return
     */
    bool selectPartialUtxos(const std::string & addr, const std::vector<wallet::UtxoEntry> & outputs,
            const CAmount requiredAmount, const int requiredUtxoCount, const CAmount requiredFeePerUtxo,
            const CAmount requiredPrepTxFees, const CAmount requiredSplitSize, const CAmount requiredRemainder,
            std::vector<wallet::UtxoEntry> & outputsForUse, CAmount & utxoAmount, CAmount & fees, bool & exactUtxoMatch) const;

    /**
     * Unit tests: add xwallets
     * @param services
     */
    void utAddXWallets(const std::vector<std::string> & services) {
        utxwallets = services;
    }

    /**
     * Processes pending partial orders, i.e. partial orders recently created but
     * waiting for prep transaction to confirm.
     */
    void processPendingPartialOrders();

    /**
     * Remove the partial order from the pending state.
     * @param ptr
     */
    void removePendingPartialOrder(TransactionDescrPtr ptr);

    /**
     * Load existing orders.
     */
    void loadOrders();

    /**
     * Save the orders to the persistent storage.
     */
    void saveOrders(bool force = false);

    /**
     * Returns the order that contains the specified utxo. If no order
     * contains the utxo, a null uint256 id is returned.
     * @param utxo
     * @return
     */
    uint256 orderWithUtxo(const wallet::UtxoEntry & utxo);

    /**
     * Returns the partial order chain for the specified order.
     * @param orderid
     * @return
     */
    std::vector<TransactionDescrPtr> getPartialOrderChain(uint256 orderid);

protected:
    void clearMempool();

private:
    std::unique_ptr<Impl> m_p;
    bool m_disconnecting;
    CCriticalSection m_lock;
    std::map<std::string, boost::posix_time::ptime> m_badWallets;
    bool m_updatingWallets{false};
    CCriticalSection m_updatingWalletsLock;
    bool m_stopped{false};

    std::vector<TransactionDescrPtr> m_partialOrders;
    std::set<xbridge::wallet::UtxoEntry> m_feeUtxos;
    std::map<std::string, std::set<xbridge::wallet::UtxoEntry> > m_utxosDict;
    CCriticalSection m_utxosLock;
    CCriticalSection m_utxosOrderLock;

    XBridgeDB xdb;
    std::vector<std::string> utxwallets; // unit tests only
};

/**
 * Returns all available coins across all wallets.
 * @param onlySafe
 * @param minDepth
 * @param maxDepth
 * @return
 */
static std::vector<std::pair<COutPoint,CTxOut>> availableCoins(const bool & onlySafe = true, const int & minDepth = 0,
        const int & maxDepth = 9999999)
{
    std::vector<std::pair<COutPoint,CTxOut>> r;
#ifdef ENABLE_WALLET
    const auto wallets = GetWallets();
    for (const auto & wallet : wallets) {
        auto locked_chain = wallet->chain().lock();
        LOCK2(cs_main, wallet->cs_wallet);
        std::vector<COutput> coins;
        wallet->AvailableCoins(*locked_chain, coins, onlySafe, nullptr, 1, MAX_MONEY, MAX_MONEY, 0, minDepth, maxDepth);
        if (coins.empty())
            continue;
        for (auto & coin : coins) {
            if (coin.fSpendable)
                r.emplace_back(coin.GetInputCoin().outpoint, coin.GetInputCoin().txout);
        }
    }
#endif // ENABLE_WALLET
    return r;
}

/**
 * Returns all available coins across all wallets.
 * @param onlySafe
 * @param minDepth
 * @param maxDepth
 * @return
 */
static CAmount availableBalance() {
    CAmount balance{0};
#ifdef ENABLE_WALLET
    const auto wallets = GetWallets();
    for (const auto & wallet : wallets)
        balance += wallet->GetBalance();
#endif
    return balance;
}

/**
 * Display the string representation of the enum.
 * @param reason
 * @return
 */
extern std::string TxCancelReasonText(uint32_t reason);

} // namespace xbridge

#endif // BLOCKNET_XBRIDGE_XBRIDGEAPP_H
