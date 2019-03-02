//*****************************************************************************
//*****************************************************************************

#ifndef XROUTERAPP_H
#define XROUTERAPP_H

#include "xrouterutils.h"
#include "xrouterpacket.h"
#include "xroutersettings.h"
#include "xrouterserver.h"
#include "xrouterdef.h"

#include "xbridge/xbridgecryptoproviderbtc.h"

#include "init.h"
#include "uint256.h"
#include "hash.h"
#include "validationstate.h"
#include "servicenode.h"
#include "net.h"

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include <memory>
#include <chrono>
#include <algorithm>

//*****************************************************************************
//*****************************************************************************
namespace xrouter
{

typedef std::shared_ptr<XRouterSettings> XRouterSettingsPtr;

//*****************************************************************************
//*****************************************************************************
class App
{
    friend class XRouterServer;

public:
    /**
     * @brief instance - the classical implementation of singleton
     * @return
     */
    static App & instance();

    /**
     * @brief isEnabled
     * @return enabled by default
     */
    static bool isEnabled();
    
    /**
     * @brief XRouter settings
     * @return local xrouter.conf settings
     */
    XRouterSettingsPtr xrSettings() { return xrsettings; }

    /**
     * @brief start - start xrouter
     * run services, sessions,
     * @return true if all components run successfull
     */
    bool start();

    /**
     * @brief Open connections to service nodes with specified services.
     * @param wallet
     * @param plugin
     */
    void openConnections(std::string wallet="", std::string plugin="");
    
    /**
     * @brief send config update requests to all nodes
     */
    std::string updateConfigs(bool force = false);
    
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
     * @param uuidRet uuid of the request
     * @param currency chain code (BTC, LTC etc)
     * @param confirmations number of service nodes to call (final result is selected from all answers by majority vote)
     * @param param1 first additional param (command specific)
     * @param param2 second additional param (command specific)
     * @return reply from service node
     */
    std::string xrouterCall(enum XRouterCommand command, std::string & uuidRet, const std::string & currency, const int & confirmations, std::string param1="", std::string param2="");

    /**
     * @brief returns block count (highest tree) in the selected chain
     * @param uuidRet uuid of the request
     * @param currency chain code (BTC, LTC etc)
     * @return json reply of getblockcount
     */
    std::string getBlockCount(std::string & uuidRet, const std::string & currency, const int & confirmations);

    /**
     * @brief returns block hash for given block number
     * @param uuidRet uuid of the request
     * @param currency chain code (BTC, LTC etc)
     * @param blockId block number (integer converted to string)
     * @return json reply of getblockhash
     */
    std::string getBlockHash(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & blockId);

    /**
     * @brief returns block data by hash
     * @param uuidRet uuid of the request
     * @param currency chain code (BTC, LTC etc)
     * @param blockHash block hash string
     * @return json reply of getblock
     */
    std::string getBlock(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & blockHash);

    /**
     * @brief returns transaction by hash (requires tx idnex on server side)
     * @param uuidRet uuid of the request
     * @param currency chain code (BTC, LTC etc)
     * @param hash tx hash
     * @return json reply of decoderawtransaction
     */
    std::string getTransaction(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & hash);

    /**
     * @brief returns all blocks starting from given number
     * @param uuidRet uuid of the request
     * @param currency chain code (BTC, LTC etc)
     * @param number block number (int converted to string)
     * @return json array with block data
     */
    std::string getAllBlocks(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & number);

    /**
     * @brief returns transactions belonging to account after given block
     * @param uuidRet uuid of the request
     * @param currency chain code (BTC, LTC etc)
     * @param account address to search in transactionss
     * @param number block number where to start
     * @return json array of transaction data
     */
    std::string getAllTransactions(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & account, const std::string & number);

    /**
     * @brief returns balance for given account
     * @param uuidRet uuid of the request
     * @param currency chain code (BTC, LTC etc)
     * @param account address
     * @return balance (float converted to string)
     */
    std::string getBalance(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & account);

    /**
     * @brief returns balance cange since given block number
     * @param uuidRet uuid of the request
     * @param currency chain code (BTC, LTC etc)
     * @param account address
     * @param number block number where to start
     * @return balance change (float converted to string)
     */
    std::string getBalanceUpdate(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & account, const std::string & number);

    /**
     * @brief returns all transactions using bloom filter
     * @param uuidRet uuid of the request
     * @param currency chain code (BTC, LTC etc)
     * @param account address
     * @param number block number where to start
     * @return balance change (float converted to string)
     */
    std::string getTransactionsBloomFilter(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & filter, const std::string & number);

    /**
     * @brief fetches the reply to the giver request
     * @param uuid UUID of the query
     * @return
     */
    std::string getReply(const std::string & uuid);

    /**
     * @brief sends raw transaction to the given chain
     * @param currency chain code (BTC, LTC etc)
     * @param transaction raw signed transaction
     * @return
     */
    std::string sendTransaction(std::string & uuidRet, const std::string & currency, const std::string & transaction);

    /**
     * @brief sends custom (plugin) call
     * @param uuidRet uuid of the request
     * @param name plugin name (taken from xrouter config)
     * @param params parameters list from command line. The function checks that the number and type of parameters matches the config
     * @return
     */
    std::string sendCustomCall(std::string & uuidRet, const std::string & name, std::vector<std::string> & params);

    /**
     * @brief Returnst the block count at the specified time.
     * @param uuidRet uuid of the request
     * @param name plugin name (taken from xrouter config)
     * @param params parameters list from command line. The function checks that the number and type of parameters matches the config
     * @return
     */
    std::string convertTimeToBlockCount(std::string & uuidRet, const std::string& currency, const int & confirmations, std::string time);
    
    /**
     * @brief reload xrouter.conf and plugin configs from disks
     */
    void reloadConfigs();

    /**
     * Loads the exchange wallets specified in settings.
     * @return true if wallets loaded, otherwise false
     */
    bool createConnectors();
    
    /**
     * @brief returns status json object
     */
    std::string getStatus();
    
    /**
     * @brief gets address for comission payment
     * @param nodeAddr service node
     * @param paymentAddress Service node payment address
     * @return true if found, otherwise false
     */
    bool getPaymentAddress(const NodeAddr & nodeAddr, std::string & paymentAddress);
    
    /**
     * @brief gets address for comission payment
     * @param node service node
     * @return service node public key
     */
    CPubKey getPaymentPubkey(CNode* node);
    
    /**
     * @brief fetches the xrouter config of a service node
     * @param node node object
     * @param addr "self" means return own xrouter.conf, any other address requests config for this address if present
     * @return config string
     */
    std::string sendXRouterConfigRequest(CNode* node, std::string addr="self");
    
    /**
     * @brief fetches the xrouter config of a service node
     * @param node node object
     * @return config string
     */
    std::string sendXRouterConfigRequestSync(CNode* node);
    
    /**
     * @brief process GetXrouterConfig call on service node side
     * @param cfg XRouter settings obj
     * @param node Node address
     * @return
     */
    std::string parseConfig(XRouterSettingsPtr cfg, const NodeAddr & node);

    /**
     * @brief process reply from service node on *client* side
     * @param node Connection to node
     * @param packet Xrouter packet received over the network
     * @param state DOS state
     * @return
     */
    bool processReply(CNode *node, XRouterPacketPtr packet, CValidationState & state);

    /**
     * @brief process reply about xrouter config contents
     * @param node Connection to node
     * @param packet Xrouter packet received over the network
     * @param state DOS state
     * @return
     */
    bool processConfigReply(CNode *node, XRouterPacketPtr packet, CValidationState & state);

    /**
     * @brief get all nodes that support the command for a given chain
     * @param packet Xrouter packet formed and ready to be sendTransaction
     * @param wallet currency
     * @return
     */
    std::vector<CNode*> getAvailableNodes(enum XRouterCommand command, std::string wallet, int confirmations=1);
    
    /**
     * @brief find the node that supports a given plugin 
     * @param name plugin name
     * @return
     */
    CNode* getNodeForService(std::string name);
    
    /**
     * @brief generates a payment transaction to given service node
     * @param pnode CNode corresponding to a service node
     * @param fee amount to pay
     * @param address hex-encoded transaction
     * @return bool True if payment generated, otherwise false
     * @throws std::runtime_error in case of errors
     */
    bool generatePayment(CNode* pnode, CAmount fee, std::string & payment);
    
    /**
     * @brief onMessageReceived  call when message from xrouter network received
     * @param node source CNode
     * @param message packet contents
     * @param state variable, used to ban misbehaving nodes
     */
    void onMessageReceived(CNode* node, const std::vector<unsigned char> & message, CValidationState & state);
    
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
     * @brief Create a change address return base58 version.
     * @return
     */
    std::string changeAddress() {
        if (!pwalletMain->IsLocked())
            pwalletMain->TopUpKeyPool();

        CReserveKey reservekey(pwalletMain);
        CPubKey vchPubKey;
        if (!reservekey.GetReservedKey(vchPubKey))
            return "";

        reservekey.KeepKey();
        CKeyID keyID = vchPubKey.GetID();
        return CBitcoinAddress(keyID).ToString();
    }
    
    /**
     * @brief register domain to given colalteral address
     * @param domain domain name
     * @param addr collateral address
     * @param update if true, write result to xrouter.conf
     * @return txid of the registered tx
     */
    std::string registerDomain(std::string & uuidRet, const std::string & domain, const std::string & addr, bool update=false);
    
    /**
     * @brief check if the given domain is registered
     * @param uuidRet uuid of the request
     * @param domain domain name
     * @return true if domain is registered
     */
    bool checkDomain(std::string & uuidRet, const std::string & domain);
    
    /**
     * @brief create deposit pubkey and address on service node
     * @param uuidRet uuid of the request
     * @param update if true, write result to xrouter.conf
     * @return deposit pubkey and address
     */
    std::string createDepositAddress(std::string & uuidRet, bool update=false);

    /**
     * Helper to build key for use with lookups.
     * @param currency
     * @param command
     * @return
     */
    std::string buildCommandKey(const std::string & currency, const std::string & command) {
        return currency + "::" + command;
    }

    /**
     * Returns true if the rate limit has been exceeded on requests to the specified node.
     * @param node Node address
     * @param plugin Plugin name
     * @param lastRequest Time of last request
     * @param rateLimit Rate limit in milliseconds
     * @return
     */
    bool rateLimitExceeded(const NodeAddr & node, const std::string & plugin,
            const std::chrono::time_point<std::chrono::system_clock> lastRequest, const int rateLimit)
    {
        if (hasSentRequest(node, plugin)) {
            std::chrono::time_point<std::chrono::system_clock> time = std::chrono::system_clock::now();
            std::chrono::system_clock::duration diff = time - lastRequest;
            if (std::chrono::duration_cast<std::chrono::milliseconds>(diff) < std::chrono::milliseconds(rateLimit))
                return true;
        }
        return false;
    }

    /**
     * Return the servicenode for the specified query.
     * @param node Servicenode address
     * @param pubkey Return pubkey of specified servicenode
     * @return true if found, otherwise false
     */
    bool servicenodePubKey(const NodeAddr & node, std::vector<unsigned char> & pubkey);

    /**
     * Keypair initialization for client requests.
     * @return false on error, otherwise true
     */
    bool initKeyPair() {
        try {
            crypto.makeNewKey(cprivkey);
            return crypto.getPubKey(cprivkey, cpubkey);
        } catch (std::exception &) {
            return false;
        }
    }

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

    CCriticalSection _lock;

    XRouterSettingsPtr xrsettings;
    std::unique_ptr<XRouterServer> server;

    std::map<std::string, int> snodeScore;

    std::map<std::string, std::set<NodeAddr> > configQueries;
    std::map<NodeAddr, std::chrono::time_point<std::chrono::system_clock> > lastConfigQueries;
    std::map<NodeAddr, std::map<std::string, std::chrono::time_point<std::chrono::system_clock> > > lastPacketsSent;
    std::map<NodeAddr, XRouterSettingsPtr> snodeConfigs;
    std::map<std::string, NodeAddr> snodeDomains;

    std::string xrouterpath;

    // timer
    void onTimer();
    boost::asio::io_service timerIo;
    std::shared_ptr<boost::asio::io_service::work> timerIoWork;
    boost::thread timerThread;
    boost::asio::deadline_timer timer;

    // Key management
    xbridge::BtcCryptoProvider crypto;
    std::vector<unsigned char> cpubkey;
    std::vector<unsigned char> cprivkey;

    bool hasScore(const NodeAddr & node) {
        LOCK(_lock);
        return snodeScore.count(node);
    }
    int getScore(const NodeAddr & node) {
        LOCK(_lock);
        return snodeScore[node];
    }
    void updateScore(const NodeAddr & node, const int score) {
        LOCK(_lock);
        snodeScore[node] = score;
    }
    std::chrono::time_point<std::chrono::system_clock> getLastRequest(const NodeAddr & node, const std::string & command) {
        LOCK(_lock);
        if (lastPacketsSent.count(node) && lastPacketsSent[node].count(command))
            return lastPacketsSent[node][command];
        return std::chrono::system_clock::from_time_t(0);
    }
    bool hasSentRequest(const NodeAddr & node, const std::string & command) {
        LOCK(_lock);
        return lastPacketsSent.count(node) && lastPacketsSent[node].count(command);
    }
    std::map<NodeAddr, XRouterSettingsPtr> getConfigs() {
        LOCK(_lock);
        return snodeConfigs;
    }
    bool hasDomain(const std::string & domain) {
        LOCK(_lock);
        return snodeDomains.count(domain);
    }
    NodeAddr getDomainNode(const std::string & domain) {
        LOCK(_lock);
        return snodeDomains[domain];
    }
    void updateDomainNode(const std::string & domain, const NodeAddr & node) {
        LOCK(_lock);
        snodeDomains[domain] = node;
    }
    void addQuery(const std::string & queryId, const NodeAddr & node) {
        LOCK(_lock);
        if (!configQueries.count(queryId))
            configQueries[queryId] = std::set<NodeAddr>{node};
        else
            configQueries[queryId].insert(node);
    }
    bool hasQuery(const std::string & queryId) {
        LOCK(_lock);
        return configQueries.count(queryId);
    }
    bool completedQuery(const std::string & queryId) {
        LOCK(_lock);
        return configQueries.count(queryId);
    }
    std::set<NodeAddr> getNodesForQuery(const std::string & queryId) {
        LOCK(_lock);
        if (!configQueries.count(queryId))
            return std::set<NodeAddr>{};
        return configQueries[queryId];
    }
    bool hasConfig(const NodeAddr & node) {
        LOCK(_lock);
        return snodeConfigs.count(node);
    }
    XRouterSettingsPtr getConfig(const NodeAddr & node) {
        LOCK(_lock);
        return snodeConfigs[node];
    }
    void updateConfig(const NodeAddr & node, XRouterSettingsPtr config) {
        LOCK(_lock);
        snodeConfigs[node] = config;
    }
    bool hasConfigTime(const NodeAddr & node) {
        LOCK(_lock);
        return lastConfigQueries.count(node);
    }
    std::chrono::time_point<std::chrono::system_clock> getConfigTime(const NodeAddr & node) {
        LOCK(_lock);
        return lastConfigQueries[node];
    }
    void updateConfigTime(const NodeAddr & node, std::chrono::time_point<std::chrono::system_clock> & time) {
        LOCK(_lock);
        lastConfigQueries[node] = time;
    }

    class QueryMgr {
    public:
        typedef std::string QueryReply;
        typedef std::pair<boost::shared_ptr<boost::mutex>, boost::shared_ptr<boost::condition_variable> > QueryCondition;
        QueryMgr() : queriesLocks(), queries() {}
        /**
         * Add a query. This stores interal state including condition variables and associated mutexes.
         * @param id
         * @param qc
         */
        void addQuery(const std::string & id, QueryCondition & qc) {
            LOCK(mu);

            if (!queries.count(id))
                queries[id] = std::map<NodeAddr, std::string>{};

            boost::shared_ptr<boost::mutex> m(new boost::mutex());
            boost::shared_ptr<boost::condition_variable> cond(new boost::condition_variable());

            qc = std::pair<boost::shared_ptr<boost::mutex>, boost::shared_ptr<boost::condition_variable> >(m, cond);
            queriesLocks[id] = qc;
        }
        /**
         * Add a query. This stores interal state for non-sync queries.
         * @param id
         */
        void addQuery(const std::string & id) {
            LOCK(mu);

            if (!queries.count(id))
                queries[id] = std::map<NodeAddr, std::string>{};
        }
        /**
         * Store a query reply.
         * @param id
         * @param node
         * @param reply
         * @return Total number of replies for the query with specified id.
         */
        int addReply(const std::string & id, const std::string & node, const std::string & reply) {
            LOCK(mu);
            if (!queries.count(id))
                return 0; // done, no query found with id
            queries[id][node] = reply;
            if (queriesLocks.count(id)) { // only handle locks if they exist for this query
                boost::mutex::scoped_lock l(*queriesLocks[id].first);
                queriesLocks[id].second->notify_all();
            }
            return queries.count(id);
        }
        /**
         * Fetch a reply. This method returns the number of matching replies.
         * @param id
         * @param reply
         * @return
         */
        int reply(const std::string & id, std::string & reply) {
            LOCK(mu);

            int consensus = queries.count(id);
            if (!consensus)
                return 0;

            std::map<uint256, std::string> hashes;
            std::map<uint256, int> counts;
            for (auto & item : queries[id]) {
                auto hash = Hash(item.second.begin(), item.second.end());
                hashes[hash] = item.second;
                counts[hash] = counts.count(hash) + 1; // update counts for common replies
            }

            // sort replies descending
            std::vector<std::pair<uint256, int> > tmp(counts.begin(), counts.end());
            std::sort(tmp.begin(), tmp.end(),
                      [](std::pair<uint256, int> & a, std::pair<uint256, int> & b) {
                          return a.second > b.second;
                      });

            // select the most common replies
            reply = hashes[tmp[0].first];
            return tmp[0].second;
        }
        /**
         * Returns true if the query with specified id is valid.
         * @param id
         * @return
         */
        bool hasQuery(const std::string & id) {
            LOCK(mu);
            return queries.count(id);
        }
        /**
         * Returns true if the reply exists for the specified node.
         * @param id
         * @param nodeAddr node address
         * @return
         */
        bool hasReply(const std::string & id, const NodeAddr & node) {
            LOCK(mu);
            return queries.count(id) && queries[id].count(node);
        }
        /**
         * Return all replies associated with a query.
         * @param id
         * @return
         */
        std::map<std::string, QueryReply> allReplies(const std::string & id) {
            LOCK(mu);
            return queries[id];
        }
        /**
         * Purges the ephemeral state of a query with specified id.
         * @param id
         */
        void purge(const std::string & id) {
            LOCK(mu);
            queriesLocks.erase(id);
        }
    private:
        CCriticalSection mu;
        std::map<std::string, QueryCondition> queriesLocks;
        std::map<std::string, std::map<std::string, QueryReply> > queries;
    };

    QueryMgr queryMgr;
};

} // namespace xrouter

#endif // XROUTERAPP_H
