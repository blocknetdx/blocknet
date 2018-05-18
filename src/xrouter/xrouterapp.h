//*****************************************************************************
//*****************************************************************************

#ifndef XROUTERAPP_H
#define XROUTERAPP_H

#include "xrouterpacket.h"
#include "util/xroutererror.h"
#include "validationstate.h"
#include "xbridge/xbridgedef.h"
#include "net.h"

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
    
    /**
     * @brief load xbridge.conf to get connection to xwallets
     * @param argc
     * @param argv
     * @return true if successful
     */
    bool init(int argc, char *argv[]);
    
    /**
     * @brief load the connector (class used to communicate with other chains)
     * @param conn
     * @return 
     */
    void addConnector(const xbridge::WalletConnectorPtr & conn);
    
    /**
     * @brief return the connector (class used to communicate with other chains) for selected chain
     * @param currency chain code (BTC, LTC etc)
     * @return 
     */
    xbridge::WalletConnectorPtr connectorByCurrency(const std::string & currency) const;
    

    /**
     * @brief send packet from client side with the selected command
     * @param command XRouter command code
     * @param currency chain code (BTC, LTC etc)
     * @param param1 first additional param (command specific)
     * @param param2 second additional param (command specific)
     * @param confirmations number of service nodes to call (final result is selected from all answers by majority vote)
     * @return reply from service node
     */
    std::string xrouterCall(enum XRouterCommand command, const std::string & currency, std::string param1="", std::string param2="", std::string confirmations="");
    
    /**
     * @brief returns block count (highest tree) in the selected chain
     * @param currency chain code (BTC, LTC etc)
     * @return json reply of getblockcount
     */
    std::string getBlockCount(const std::string & currency, const std::string & confirmations);
    
    /**
     * @brief returns block hash for given block number
     * @param currency chain code (BTC, LTC etc)
     * @param blockId block number (integer converted to string)
     * @return json reply of getblockhash
     */
    std::string getBlockHash(const std::string & currency, const std::string & blockId, const std::string & confirmations);
    
    /**
     * @brief returns block data by hash
     * @param currency chain code (BTC, LTC etc)
     * @param blockHash block hash string
     * @return json reply of getblock
     */
    std::string getBlock(const std::string & currency, const std::string & blockHash, const std::string & confirmations);
    
    /**
     * @brief returns transaction by hash (requires tx idnex on server side)
     * @param currency chain code (BTC, LTC etc)
     * @param hash tx hash
     * @return json reply of decoderawtransaction
     */
    std::string getTransaction(const std::string & currency, const std::string & hash, const std::string & confirmations);
    
    /**
     * @brief returns all blocks starting from given number
     * @param currency chain code (BTC, LTC etc)
     * @param number block number (int converted to string)
     * @return json array with block data
     */
    std::string getAllBlocks(const std::string & currency, const std::string & number, const std::string & confirmations);
    
    /**
     * @brief returns transactions belonging to account after given block
     * @param currency chain code (BTC, LTC etc)
     * @param account address to search in transactionss
     * @param number block number where to start
     * @return json array of transaction data
     */
    std::string getAllTransactions(const std::string & currency, const std::string & account, const std::string & number, const std::string & confirmations);
    
    /**
     * @brief returns balance for given account
     * @param currency chain code (BTC, LTC etc)
     * @param account address
     * @return balance (float converted to string)
     */
    std::string getBalance(const std::string & currency, const std::string & account, const std::string & confirmations);
    
    /**
     * @brief returns balance cange since given block number
     * @param currency chain code (BTC, LTC etc)
     * @param account address
     * @param number block number where to start
     * @return balance change (float converted to string)
     */
    std::string getBalanceUpdate(const std::string & currency, const std::string & account, const std::string & number, const std::string & confirmations);
    
    /**
     * @brief returns all transactions using bloom filter
     * @param currency chain code (BTC, LTC etc)
     * @param account address
     * @param number block number where to start
     * @return balance change (float converted to string)
     */
    std::string getTransactionsBloomFilter(const std::string & currency, const std::string & account, const std::string & number, const std::string & confirmations);
    
    /**
     * @brief fetches the reply to the giver request
     * @param id UUID of the query
     * @return 
     */
    std::string getReply(const std::string & id);
    
    /**
     * @brief sends raw transaction to the given chain
     * @param currency chain code (BTC, LTC etc)
     * @param transaction raw signed transaction
     * @return 
     */
    std::string sendTransaction(const std::string & currency, const std::string & transaction);
    
    /**
     * @brief gets an account for comission payment
     * @param node service node
     * @return 
     */
    std::string getPaymentAddress(CNode* node);


    /**
     * @brief process GetBlockCount call on service node side
     * @param packet Xrouter packet received over the network
     * @return 
     */
    bool processGetBlockCount(XRouterPacketPtr packet);
    
    /**
     * @brief process GetBlockHash call on service node side
     * @param packet Xrouter packet received over the network
     * @return 
     */
    bool processGetBlockHash(XRouterPacketPtr packet);
    
    /**
     * @brief process GetBlock call on service node side
     * @param packet Xrouter packet received over the network
     * @return 
     */
    bool processGetBlock(XRouterPacketPtr packet);
    
    /**
     * @brief process GetTransaction call on service node side
     * @param packet Xrouter packet received over the network
     * @return 
     */
    bool processGetTransaction(XRouterPacketPtr packet);
    
    /**
     * @brief process GetAllBlocks call on service node side
     * @param packet Xrouter packet received over the network
     * @return 
     */
    bool processGetAllBlocks(XRouterPacketPtr packet);
    
    /**
     * @brief process GetAllTransactions call on service node side
     * @param packet Xrouter packet received over the network
     * @return 
     */
    bool processGetAllTransactions(XRouterPacketPtr packet);
    
    /**
     * @brief process GetBalance call on service node side
     * @param packet Xrouter packet received over the network
     * @return 
     */
    bool processGetBalance(XRouterPacketPtr packet);
    
    /**
     * @brief process GetBalanceUpdate call on service node side
     * @param packet Xrouter packet received over the network
     * @return 
     */
    bool processGetBalanceUpdate(XRouterPacketPtr packet);
    
    /**
     * @brief process GetBalanceUpdate call on service node side
     * @param packet Xrouter packet received over the network
     * @return 
     */
    bool processGetTransactionsBloomFilter(XRouterPacketPtr packet);
    
    /**
     * @brief process SendTransaction call on service node side
     * @param packet Xrouter packet received over the network
     * @return 
     */
    bool processSendTransaction(XRouterPacketPtr packet);
    
    /**
     * @brief process GetPaymentAddress call on service node side
     * @param packet Xrouter packet received over the network
     * @return 
     */
    bool processGetPaymentAddress(XRouterPacketPtr packet);
    
    /**
     * @brief process reply from service node on *client* side
     * @param packet Xrouter packet received over the network
     * @return 
     */
    bool processReply(XRouterPacketPtr packet);
    
    //
    /**
     * @brief sendPacket send packet btadcast to xrouter network
     * @param packet send message via xrouter
     * @param wallet walletconnector ID = currency ID (BTC, LTC etc)
     */
    void sendPacket(const XRouterPacketPtr & packet, std::string wallet="");

    /**
     * @brief sendPacket send packet to xrouter network to specified id,
     * @param id address
     * @param packet packet data
     * @param wallet
     */
    void sendPacket(const std::vector<unsigned char> & id, const XRouterPacketPtr & packet, std::string wallet="");

    /**
     * @brief sends packet to service node(s) and waits for replies in the same thread, then returns the result (or error if no reply came)
     * @param packet Xrouter packet received over the network
     * @param id address
     * @param currency chain id
     * @param confirmations number of packets to send and wait for reply (result is decided by majority vote)
     * @param timeout time period to wait
     * @return 
     */
    std::string sendPacketAndWait(const XRouterPacketPtr & packet, std::string id, std::string currency, int confirmations=3, int timeout=300000);

    // call when message from xrouter network received
    /**
     * @brief onMessageReceived  call when message from xrouter network received
     * @param id packet id
     * @param message
     * @param state
     */
    void onMessageReceived(const std::vector<unsigned char> & id, const std::vector<unsigned char> & message, CValidationState & state);
};

} // namespace xrouter

#endif // XROUTERAPP_H
