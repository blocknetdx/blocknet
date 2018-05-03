//*****************************************************************************
//*****************************************************************************

#ifndef XROUTERAPP_H
#define XROUTERAPP_H

#include "xrouterpacket.h"
#include "util/xroutererror.h"
#include "validationstate.h"
#include "xbridge/xbridgedef.h"

#include <memory>
#include <boost/container/map.hpp>

#include "uint256.h"
//*****************************************************************************
//*****************************************************************************
namespace xrouter
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

    std::unique_ptr<Impl> m_p;

    boost::container::map<std::string, std::pair<boost::shared_ptr<boost::mutex>, boost::shared_ptr<boost::condition_variable> > > queriesLocks;
    boost::container::map<std::string, std::vector<std::string> > queries;

public:
    /**
     * @brief instance - the classical implementation of singletone
     * @return
     */
    static App & instance();

    /**
     * @brief isEnabled
     * @return enabled by default
     */
    static bool isEnabled();

    /**
     * @brief start - start xrouter
     * run services, sessions,
     * @return true if all components run successfull
     */
    bool start();

    /**
     * @brief stop - stopped services
     * @return
     */
    bool stop();
    
    bool init(int argc, char *argv[]);
    
    void addConnector(const xbridge::WalletConnectorPtr & conn);
    xbridge::WalletConnectorPtr connectorByCurrency(const std::string & currency) const;
    

    /**
     * @brief sendXRouterTransaction - create new xrouter transaction and send to network
     */
    std::string xrouterCall(enum XRouterCommand command, const std::string & currency, std::string param1="", std::string param2="");
    std::string getBlockCount(const std::string & currency);
    std::string getBlockHash(const std::string & currency, const std::string & blockId);
    std::string getBlock(const std::string & currency, const std::string & blockHash);
    std::string getTransaction(const std::string & currency, const std::string & hash);
    std::string getAllBlocks(const std::string & currency, const std::string & number);
    std::string getAllTransactions(const std::string & currency, const std::string & account, const std::string & number);
    std::string getBalance(const std::string & currency, const std::string & account);
    std::string getBalanceUpdate(const std::string & currency, const std::string & account, const std::string & number);
    std::string sendTransaction(const std::string & currency, const std::string & transaction);

    
    bool processGetBlockCount(XRouterPacketPtr packet);
    bool processGetBlockHash(XRouterPacketPtr packet);
    bool processGetBlock(XRouterPacketPtr packet);
    bool processGetTransaction(XRouterPacketPtr packet);
    bool processGetAllBlocks(XRouterPacketPtr packet);
    bool processGetAllTransactions(XRouterPacketPtr packet);
    bool processGetBalance(XRouterPacketPtr packet);
    bool processGetBalanceUpdate(XRouterPacketPtr packet);
    bool processSendTransaction(XRouterPacketPtr packet);
    
    bool processReply(XRouterPacketPtr packet);
    
    //
    /**
     * @brief sendPacket send packet btadcast to xrouter network
     * @param packet send message via xrouter
     */
    void sendPacket(const XRouterPacketPtr & packet, std::string wallet="");

    /**
     * @brief sendPacket send packet to xrouter network to specified id,
     * @param id
     * @param packet
     */
    void sendPacket(const std::vector<unsigned char> & id, const XRouterPacketPtr & packet, std::string wallet="");

    std::string sendPacketAndWait(const XRouterPacketPtr & packet, std::string id, std::string currency, int timeout=3000);

    // call when message from xrouter network received
    /**
     * @brief onMessageReceived  call when message from xrouter network received
     * @param id packet id
     * @param message
     * @param state
     */
    void onMessageReceived(const std::vector<unsigned char> & id,
                           const std::vector<unsigned char> & message,
                           CValidationState & state);
};

} // namespace xrouter

#endif // XROUTERAPP_H
