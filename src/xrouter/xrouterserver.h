//******************************************************************************
//******************************************************************************
#ifndef XROUTERSERVER_H
#define XROUTERSERVER_H

#include "xrouterconnector.h"
#include "xrouterconnectorbtc.h"
#include "xrouterconnectoreth.h"
#include "xrouterdef.h"
#include "xrouterutils.h"

#include "sync.h"

#include <vector>
#include <string>
#include <chrono>
#include <boost/container/map.hpp>

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
     * @brief onMessageReceived  call when message from xrouter network received
     * @param node source CNode
     * @param message packet contents
     * @param state variable, used to ban misbehaving nodes
     */
    void onMessageReceived(CNode* node, XRouterPacketPtr& packet, CValidationState & state);
    
       /**
     * @brief process GetBlockCount call on service node side
     * @param packet Xrouter packet received over the network
     * @return
     */
    std::string processGetBlockCount(XRouterPacketPtr packet, uint32_t offset, std::string currency);

    /**
     * @brief process GetBlockHash call on service node side
     * @param packet Xrouter packet received over the network
     * @param offset offset in the packet where to start reading additional parameters
     * @param currency chain id
     * @return
     */
    std::string processGetBlockHash(XRouterPacketPtr packet, uint32_t offset, std::string currency);

    /**
     * @brief process GetBlock call on service node side
     * @param packet Xrouter packet received over the network
     * @param offset offset in the packet where to start reading additional parameters
     * @param currency chain id
     * @return
     */
    std::string processGetBlock(XRouterPacketPtr packet, uint32_t offset, std::string currency);

    /**
     * @brief process GetTransaction call on service node side
     * @param packet Xrouter packet received over the network
     * @param offset offset in the packet where to start reading additional parameters
     * @param currency chain id
     * @return
     */
    std::string processGetTransaction(XRouterPacketPtr packet, uint32_t offset, std::string currency);

    /**
     * @brief process GetAllBlocks call on service node side
     * @param packet Xrouter packet received over the network
     * @param offset offset in the packet where to start reading additional parameters
     * @param currency chain id
     * @return
     */
    std::string processGetAllBlocks(XRouterPacketPtr packet, uint32_t offset, std::string currency);

    /**
     * @brief process GetAllTransactions call on service node side
     * @param packet Xrouter packet received over the network
     * @param offset offset in the packet where to start reading additional parameters
     * @param currency chain id
     * @return
     */
    std::string processGetAllTransactions(XRouterPacketPtr packet, uint32_t offset, std::string currency);

    /**
     * @brief process GetBalance call on service node side
     * @param packet Xrouter packet received over the network
     * @param offset offset in the packet where to start reading additional parameters
     * @param currency chain id
     * @return
     */
    std::string processGetBalance(XRouterPacketPtr packet, uint32_t offset, std::string currency);

    /**
     * @brief process GetBalanceUpdate call on service node side
     * @param packet Xrouter packet received over the network
     * @param offset offset in the packet where to start reading additional parameters
     * @param currency chain id
     * @return
     */
    std::string processGetBalanceUpdate(XRouterPacketPtr packet, uint32_t offset, std::string currency);

    /**
     * @brief process GetTransactionsBloomFilter call on service node side
     * @param packet Xrouter packet received over the network
     * @param offset offset in the packet where to start reading additional parameters
     * @param currency chain id
     * @return
     */
    std::string processGetTransactionsBloomFilter(XRouterPacketPtr packet, uint32_t offset, std::string currency);
    
    /**
     * @brief process ConvertTimeToBlockCount call on service node side
     * @param packet Xrouter packet received over the network
     * @param offset offset in the packet where to start reading additional parameters
     * @param currency chain id
     * @return
     */
    std::string processConvertTimeToBlockCount(XRouterPacketPtr packet, uint32_t offset, std::string currency);

    /**
     * @brief process SendTransaction call on service node side
     * @param packet Xrouter packet received over the network
     * @param offset offset in the packet where to start reading additional parameters
     * @param currency chain id
     * @return
     */
    std::string processSendTransaction(XRouterPacketPtr packet, uint32_t offset, std::string currency);

    /**
     * @brief process xrCustomCall call on service node side
     * @param name plugin name
     * @param params plugin parameters
     * @return
     */
    std::string processCustomCall(std::string name, std::vector<std::string> params);
    
    /**
     * @brief process xrFetchReply call on service node side
     * @param uuid query UUID
     * @return stored reply
     */
    std::string processFetchReply(const std::string & uuid);
    
    /**
     * @brief process payment transaction 
     * @param node node that paid the fee
     * @param feetx hex-encoded payment tx and additional data
     * @param requiredFee fee to be paid
     * @return true if a fee was processed, otherwise return false
     * @throws std::runtime_error in case of incorrect payment
     */
    bool processPayment(CNode* node, const std::string & feetx, CAmount requiredFee);
    
    /**
     * @brief returns own snode pubkey hash
     * @return blocknet address
     */
    std::string getMyPaymentAddress();
    
    /**
     * @brief returns own snode private key (used to sign transactions)
     * @return blocknet private key
     */
    CKey getMyPaymentAddressKey();
    
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
    void sendPacketToClient(std::string uuid, std::string reply, CNode* pnode);

    /**
     * Loads the servicenode key from config.
     * @return false on error, otherwise true
     */
    bool initKeyPair();

private:
    std::map<std::string, WalletConnectorXRouterPtr> connectors;
    std::map<std::string, std::shared_ptr<boost::mutex> > connectorLocks;

    std::map<NodeAddr, std::map<std::string, std::chrono::time_point<std::chrono::system_clock> > > lastPacketsReceived;

    std::map<std::string, std::pair<std::string, CAmount> > hashedQueries;
    std::map<std::string, std::chrono::time_point<std::chrono::system_clock> > hashedQueriesDeadlines;

    std::vector<unsigned char> spubkey;
    std::vector<unsigned char> sprivkey;

    mutable CCriticalSection _lock;

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
