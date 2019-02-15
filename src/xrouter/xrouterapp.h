//*****************************************************************************
//*****************************************************************************

#ifndef XROUTERAPP_H
#define XROUTERAPP_H

#include "xrouterutils.h"
#include "xrouterpacket.h"
#include "xroutersettings.h"
#include "validationstate.h"
#include "xrouterserver.h"
#include "xrouterdef.h"
#include "xrouterpeer.h"
#include "net.h"
#include "servicenode.h"

#include <memory>
#include <chrono>
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
    friend class XRouterServer;

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

    std::unique_ptr<XRouterServer> server;

    boost::container::map<std::string, std::pair<boost::shared_ptr<boost::mutex>, boost::shared_ptr<boost::condition_variable> > > queriesLocks;
    boost::container::map<std::string, boost::container::map<CNode*, std::string> > queries;
    boost::container::map<std::string, CNode* > configQueries;
    boost::container::map<CNode*, std::chrono::time_point<std::chrono::system_clock> > lastConfigQueries;
    boost::container::map<std::string, std::chrono::time_point<std::chrono::system_clock> > lastConfigUpdates;
    boost::container::map<std::string, PaymentChannel> paymentChannels;
    boost::container::map<CNode*, boost::container::map<std::string, std::chrono::time_point<std::chrono::system_clock> > > lastPacketsSent;
    boost::container::map<std::string, XRouterSettings > snodeConfigs;
    static boost::container::map<CNode*, double > snodeScore;
    boost::container::map<std::string, std::string > snodeDomains;
    
    boost::container::map<std::string, XRouterPeer> peers;
    
    XRouterSettings xrouter_settings;
    std::string xrouterpath;
    
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
     * @brief xrouterSettings
     * @return local xrouter.conf settings
     */
    XRouterSettings& xrouterSettings() { return xrouter_settings; }

    /**
     * @brief start - start xrouter
     * run services, sessions,
     * @return true if all components run successfull
     */
    bool start();

    /**
     * @brief try to open connections to all service nodes in order
     * @param wallet
     * @param plugin
     */
    void openConnections(std::string wallet="", std::string plugin="");
    
    /**
     * @brief send config update requests to all nodes
     */
    std::string updateConfigs();
    
    /**
     * @brief prints xrouter configs
     */
    std::string printConfigs();
    
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
     * @brief this function is called when serviceping packet is generated (see details in Xbridge) 
     * @return list of XRouter services activated on this node
     */
    std::vector<std::string> getServicesList();
    
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
    std::string getTransactionsBloomFilter(const std::string & currency, const std::string & filter, const std::string & number, const std::string & confirmations);

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
     * @brief sends custom (plugin) call
     * @param name plugin name (taken from xrouter config)
     * @param params parameters list from command line. The function checks that the number and type of parameters matches the config
     * @return
     */
    std::string sendCustomCall(const std::string & name, std::vector<std::string> & params);
    
    /**
     * @brief reload xrouter.conf and plugin configs from disks
     */
    void reloadConfigs();
    
    /**
     * @brief returns status json object
     */
    std::string getStatus();
    
    std::string convertTimeToBlockCount(const std::string& currency, std::string time, const std::string& confirmations);
    
    /**
     * @brief gets address for comission payment
     * @param node service node
     * @return service node pubkey hash (address)
     */
    std::string getPaymentAddress(CNode* node);
    
    /**
     * @brief gets address for comission payment
     * @param node service node
     * @return service node public key
     */
    CPubKey getPaymentPubkey(CNode* node);
    
    /**
     * @brief prints all currently open payment channels
     * @return info string
     */
    std::string printPaymentChannels();
    
    /**
     * @brief fetches the xrouter config of a service node
     * @param node node object
     * @param addr "self" means return own xrouter.conf, any other address requests config for this address if present
     * @return config string
     */
    std::string getXrouterConfig(CNode* node, std::string addr="self");
    
    /**
     * @brief fetches the xrouter config of a service node
     * @param node node object
     * @return config string
     */
    std::string getXrouterConfigSync(CNode* node);
    
    /**
     * @brief process GetXrouterConfig call on service node side
     * @param packet Xrouter packet received over the network
     * @return
     */
    std::string processGetXrouterConfig(XRouterSettings cfg, std::string addr);

    /**
     * @brief process reply from service node on *client* side
     * @param packet Xrouter packet received over the network
     * @return
     */
    bool processReply(XRouterPacketPtr packet, CNode* node);
    
    /**
     * @brief process reply about xrouter config contents
     * @param packet Xrouter packet received over the network
     * @return
     */
    bool processConfigReply(XRouterPacketPtr packet);
    
    static bool cmpNodeScore(CNode* a, CNode* b) { return snodeScore[a] > snodeScore[b]; }
    
    /**
     * @brief get all nodes that support the command for a given chain
     * @param packet Xrouter packet formed and ready to be sendTransaction
     * @param wallet currency
     * @return
     */
    std::vector<CNode*> getAvailableNodes(enum XRouterCommand command, std::string wallet, int confirmations=1);
    
    /**
     * @brief find the node that supports a given plugin 
     * @param name pkugin name
     * @return
     */
    CNode* getNodeForService(std::string name);
    
    /**
     * @brief generates a payment transaction to given service node
     * @param pnode CNode corresponding to a service node
     * @param fee amount to pay
     * @return hex-encoded transaction
     * @throws std::runtime_error in case of errors
     */
    std::string generatePayment(CNode* pnode, CAmount fee);
    
    /**
     * @brief onMessageReceived  call when message from xrouter network received
     * @param node source CNode
     * @param message packet contents
     * @param state variable, used to ban misbehaving nodes
     */
    void onMessageReceived(CNode* node, const std::vector<unsigned char> & message, CValidationState & state);
    
    /**
     * @brief close the payment channel with the given id
     * @param id channel id
     */
    void closePaymentChannel(std::string id);
    
    /**
     * @brief close all payment channels
     */
    void closeAllPaymentChannels();
    
    /**
     * @brief save payment channels info to paymentchannels.json
     */
    void savePaymentChannels();
    
    /**
     * @brief load payment channels from paymentchannels.json
     */
    void loadPaymentChannels();
    
    /**
     * @brief run performance tests (xrTest)
     */
    void runTests();
    
    /**
     * @brief returns true if [Main]debug_on_client=1 is set
     */
    bool debug_on_client();
    
    /**
     * @brief returns true if [Main]debug=1 is set
     */
    bool isDebug();
    
    /**
     * @brief returns service node collateral address
     */
    std::string getMyPaymentAddress();
    
    /**
     * @brief register domain to given colalteral address
     * @param domain domain name
     * @param addr collateral address
     * @param update if true, write result to xrouter.conf
     * @return txid of the registered tx
     */
    std::string registerDomain(std::string domain, std::string addr, bool update=false);
    
    /**
     * @brief check if the given domain is registered
     * @param domain domain name
     * @return true if domain is registered
     */
    bool queryDomain(std::string domain);
    
    /**
     * @brief create deposit pubkey and address on service node
     * @param update if true, write result to xrouter.conf
     * @return deposit pubkey and address
     */
    std::string createDepositAddress(bool update=false);
};

} // namespace xrouter

#endif // XROUTERAPP_H
