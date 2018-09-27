//******************************************************************************
//******************************************************************************
#ifndef XROUTERSERVER_H
#define XROUTERSERVER_H

#include <vector>
#include <string>
#include "xrouterconnector.h"
#include "xrouterconnectorbtc.h"
#include "xrouterconnectoreth.h"
#include "xrouterdef.h"
#include <chrono>
#include <boost/container/map.hpp>

namespace xrouter
{

//*****************************************************************************
//*****************************************************************************
class XRouterServer
{
    friend class App;

    mutable boost::mutex m_connectorsLock;
    xrouter::Connectors m_connectors;
    xrouter::ConnectorsAddrMap m_connectorAddressMap;
    xrouter::ConnectorsCurrencyMap m_connectorCurrencyMap;

    boost::container::map<CNode*, boost::container::map<std::string, std::chrono::time_point<std::chrono::system_clock> > > lastPacketsReceived;
    
    boost::container::map<CNode*, std::pair<std::string, double> > paymentChannels;
    
protected:
    /**
     * @brief Impl - default constructor, init
     * services and timer
     */
    XRouterServer() { }

    /**
     * @brief start - run sessions, threads and services
     * @return true, if run succesfull
     */
    bool start();
    
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
    void sendPacketToClient(const XRouterPacketPtr & packet, CNode* pnode);
    
    inline void sendReply(CNode* node, std::string uuid, std::string reply);
    
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
};

} // namespace

#endif // SERVER_H
