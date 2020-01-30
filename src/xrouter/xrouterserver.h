// Copyright (c) 2018-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_XROUTER_XROUTERSERVER_H
#define BLOCKNET_XROUTER_XROUTERSERVER_H

#include <xrouter/xrouterdef.h>
#include <xrouter/xrouterutils.h>
#include <xrouter/xrouterconnector.h>
#include <xrouter/xrouterconnectorbtc.h>
#include <xrouter/xrouterconnectoreth.h>

#include <consensus/validation.h>
#include <net.h>
#include <sync.h>
#include <validationinterface.h>

namespace xrouter
{

class WalletConnectorXRouter;
typedef std::shared_ptr<WalletConnectorXRouter> WalletConnectorXRouterPtr;

//*****************************************************************************
//*****************************************************************************
class XRouterServer
{
public:

    XRouterServer() = default;
    
    /**
     * @brief start - run sessions, threads and services
     * @return true, if run succesfull
     */
    bool start();

    /**
     * @brief Shutdown services.
     * @return true if successful
     */
    bool stop();

    /**
     * Returns true if the server has been started, otherwise false.
     * @return
     */
    bool isStarted() {
        LOCK(_lock);
        return started;
    }
    
    /**
     * @brief onMessageReceived  call when message from xrouter network received
     * @param node source CNode
     * @param message packet contents
     * @param state variable, used to ban misbehaving nodes
     */
    void onMessageReceived(CNode* node, XRouterPacketPtr packet, CValidationState & state);
    
       /**
     * @brief process xrGetBlockCount call on service node side
     * @param currency blockchain to query
     * @param params list of parameters
     * @return
     */
    std::string processGetBlockCount(const std::string & currency, const std::vector<std::string> & params);

    /**
     * @brief process xrGetBlockHash call on service node side
     * @param currency blockchain to query
     * @param params list of parameters
     * @return
     */
    std::string processGetBlockHash(const std::string & currency, const std::vector<std::string> & params);

    /**
     * @brief process xrGetBlock call on service node side
     * @param currency blockchain to query
     * @param params list of parameters
     * @return
     */
    std::string processGetBlock(const std::string & currency, const std::vector<std::string> & params);

    /**
     * @brief process xrGetBlocks call on service node side
     * @param currency blockchain to query
     * @param params list of parameters
     * @return
     */
    std::vector<std::string> processGetBlocks(const std::string & currency, const std::vector<std::string> & params);

    /**
     * @brief process xrGetTransaction call on service node side
     * @param currency blockchain to query
     * @param params list of parameters
     * @return
     */
    std::string processGetTransaction(const std::string & currency, const std::vector<std::string> & params);

    /**
     * @brief process xrGetAllTransactions call on service node side
     * @param currency blockchain to query
     * @param params list of parameters
     * @return
     */
    std::vector<std::string> processGetTransactions(const std::string & currency, const std::vector<std::string> & params);

    /**
     * @brief process xrDecodeRawTransaction call on service node side
     * @param currency blockchain to query
     * @param params list of parameters
     * @return
     */
    std::string processDecodeRawTransaction(const std::string & currency, const std::vector<std::string> & params);

    /**
     * @brief process xrGetTransactionsBloomFilter call on service node side
     * @param currency blockchain to query
     * @param params list of parameters
     * @return
     */
    std::vector<std::string> processGetTxBloomFilter(const std::string & currency, const std::vector<std::string> & params);

    /**
     * @brief process xrGenerateBloomFilter call on service node side
     * @param currency blockchain to query
     * @param params list of parameters
     * @return
     */
    std::string processGenerateBloomFilter(const std::string & currency, const std::vector<std::string> & params);

    /**
     * @brief process SendTransaction call on service node side
     * @param currency blockchain to send transaction on
     * @param params list of parameters
     * @return
     */
    std::string processSendTransaction(const std::string & currency, const std::vector<std::string> & params);

    /**
     * @brief process ConvertTimeToBlockCount call on service node side
     * @param currency blockchain to query
     * @param params list of parameters
     * @return
     */
    std::string processConvertTimeToBlockCount(const std::string & currency, const std::vector<std::string> & params);

    /**
     * @brief process xrGetBalance call on service node side
     * @param currency blockchain to query
     * @param params list of parameters
     * @return
     */
    std::string processGetBalance(const std::string & currency, const std::vector<std::string> & params);

    /**
     * @brief process xrService call on service node side
     * @param name plugin name
     * @param params plugin parameters
     * @return
     */
    std::string processServiceCall(const std::string & name, const std::vector<std::string> & params);
    
    /**
     * @brief process xrGetReply call on service node side
     * @param uuid query UUID
     * @return stored reply
     */
    std::string processFetchReply(const std::string & uuid);
    
    /**
     * @brief process payment transaction 
     * @param feetx hex-encoded payment tx and additional data
     */
    bool processPayment(const std::string & feetx);

    /**
     * @brief Checks the payment
     * @param nodeAddr node address payment is intended for
     * @param paymentAddress the desired payment address
     * @param feetx hex-encoded payment tx and additional data
     * @param requiredFee fee to be paid
     * @return true if fee payment is valid, otherwise false
     * @throws std::runtime_error in case of incorrect payment
     */
    bool checkFeePayment(const NodeAddr & nodeAddr, const std::string & paymentAddress,
            const std::string & feetx, const CAmount & requiredFee);

    /**
     * @brief returns own snode pubkey hash
     * @return blocknet address
     */
    std::string getMyPaymentAddress();
    
    /**
     * @brief clears stored replies to queries periodically after timeout
     * @return 
     */
    void clearHashedQueries();

    /**
     * Get a raw change address from the wallet.
     * @return
     */
    std::string changeAddress();

    /**
     * Returns true if the rate limit has been exceeded by the specified node for the command.
     * @param nodeAddr
     * @param key
     * @param rateLimit
     * @return
     */
    bool rateLimitExceeded(const std::string & nodeAddr, const std::string & key, const int & rateLimit);

    /**
     * Loads the exchange wallets specified in settings.
     * @return true if wallets loaded, otherwise false
     */
    bool createConnectors();

    /**
     * Returns true if this server has a pending query.
     * @param node
     */
    bool hasInFlightQuery(const NodeAddr & node) {
        LOCK(_lock);
        return inFlightQueries.count(node) > 0;
    }

    /**
     * Watches the specified query.
     * @param node
     * @param uuid
     */
    void addInFlightQuery(const NodeAddr & node, const std::string & uuid) {
        LOCK(_lock);
        inFlightQueries[node].insert(uuid);
    }

    /**
     * Unwatches the specified query.
     * @param node
     * @param uuid
     */
    void removeInFlightQuery(const NodeAddr & node, const std::string & uuid) {
        LOCK(_lock);
        inFlightQueries[node].erase(uuid);
        if (inFlightQueries[node].empty())
            inFlightQueries.erase(node);
    }

    /**
     * Servicenode public key.
     * @return
     */
    const std::vector<unsigned char> & pubKey() const { return spubkey; }
    /**
     * Servicenode private key.
     * @return
     */
    const std::vector<unsigned char> & privKey() const { return sprivkey; }

    void runPerformanceTests();

private:
    /**
     * @brief load the connector (class used to communicate with other chains)
     * @param conn
     * @return
     */
    void addConnector(const WalletConnectorXRouterPtr & conn);

    /**
     * @brief return the connector (class used to communicate with other chains) for selected chain
     * @param currency chain code (BTC, LTC etc)
     * @return
     */
    WalletConnectorXRouterPtr connectorByCurrency(const std::string & currency) const;

    /**
     * @brief sendPacket send packet btadcast to xrouter network
     * @param packet send message via xrouter
     * @param wallet walletconnector ID = currency ID (BTC, LTC etc)
     */
    void sendPacketToClient(const std::string & uuid, const std::string & reply, CNode* pnode);

    /**
     * Loads the servicenode key from config.
     * @return false on error, otherwise true
     */
    bool initKeyPair();

    /**
     * Pulls the parameters out of the packet and adds to "parameters"
     * @param packet
     * @param params
     * @param offset position of the first byte following the parameters
     * @return
     */
    bool processParameters(XRouterPacketPtr packet, const int & paramsCount,
            std::vector<std::string> & params, uint32_t & offset);

    /**
     * Removes the {"result": ""} object wrapper.
     * @param res
     * @return
     */
    std::string parseResult(const std::string & res);

    /**
     * Removes the {"result": ""} object wrapper.
     * @param resv
     * @return
     */
    std::string parseResult(const std::vector<std::string> & resv);

private:
    bool started{false};

    std::map<std::string, WalletConnectorXRouterPtr> connectors;
    std::map<std::string, std::shared_ptr<boost::mutex> > connectorLocks;

    std::map<std::string, std::pair<std::string, CAmount> > hashedQueries;
    std::map<std::string, std::chrono::time_point<std::chrono::system_clock> > hashedQueriesDeadlines;
    std::map<NodeAddr, std::set<std::string> > inFlightQueries;

    std::vector<unsigned char> spubkey;
    std::vector<unsigned char> sprivkey;

    mutable Mutex _lock;

    std::string getQuery(const std::string & uuid) {
        LOCK(_lock);
        return hashedQueries[uuid].first;
    }
    CAmount getQueryFee(const std::string & uuid) {
        LOCK(_lock);
        return hashedQueries[uuid].second;
    }
    bool hasQuery(const std::string & uuid) {
        LOCK(_lock);
        return hashedQueries.count(uuid);
    }
    std::shared_ptr<boost::mutex> getConnectorLock(const std::string & currency) {
        LOCK(_lock);
        return connectorLocks[currency];
    }
    bool hasConnectorLock(const std::string & currency) {
        LOCK(_lock);
        return connectorLocks.count(currency);
    }

};

} // namespace

#endif // SERVER_H
