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
#include "sync.h"

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
typedef std::shared_ptr<XRouterServer> XRouterServerPtr;

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
     * @brief load xbridge.conf to get connection to xwallets
     * @param argc
     * @param argv
     * @return true if successful
     */
    bool init(int argc, char *argv[]);

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
     * @brief Returns true if XRouter configs have finished loading.
     * @return
     */
    bool isReady();

    /**
     * @brief Returns true if XRouter is ready to accept packets.
     * @return
     */
    bool canListen();

    /**
     * Activates the XRouter server node including updating its payment address.
     */
    void updatePaymentAddress(CPubKey & collateral) {
        auto paymentAddress = CBitcoinAddress(collateral.GetID()).ToString();
        xrsettings->defaultPaymentAddress(paymentAddress);
    }

    /**
     * @brief Open connections to service nodes with specified services.
     * @param command XRouter command
     * @param service Wallet name or plugin name
     * @param count Number of nodes to open connections to
     * @param skipNodes avoids connecting to these nodes
     */
    bool openConnections(enum XRouterCommand command, const std::string & service, const uint32_t & count,
                         const std::vector<CNode*> & skipNodes);
    
    /**
     * @brief send config update requests to all nodes
     */
    std::string updateConfigs(bool force = false);
    
    /**
     * @brief prints xrouter configs
     */
    std::string printConfigs();

    /**
     * @brief this function is called when serviceping packet is generated (see details in Xbridge) 
     * @return list of XRouter services activated on this node
     */
    std::vector<std::string> getServicesList();
    
    /**
     * @brief send packet from client side with the selected command
     * @param command XRouter command code
     * @param uuidRet uuid of the request
     * @param service chain code (BTC, LTC etc)
     * @param confirmations number of service nodes to call (final result is selected from all answers by majority vote)
     * @param param1 first additional param (command specific)
     * @param param2 second additional param (command specific)
     * @return reply from service node
     */
    std::string xrouterCall(enum XRouterCommand command, std::string & uuidRet, const std::string & service,
                            const int & confirmations, const std::vector<std::string> & params);

    /**
     * @brief returns block count (highest tree) in the selected chain
     * @param uuidRet uuid of the request
     * @param currency chain code (BTC, LTC etc)
     * @param confirmations number of service nodes to call (final result is selected from all answers by majority vote)
     * @return json reply of getblockcount
     */
    std::string getBlockCount(std::string & uuidRet, const std::string & currency, const int & confirmations);

    /**
     * @brief returns block hash for given block number
     * @param uuidRet uuid of the request
     * @param currency chain code (BTC, LTC etc)
     * @param confirmations number of service nodes to call (final result is selected from all answers by majority vote)
     * @param block block number
     * @return json reply of getblockhash
     */
    std::string getBlockHash(std::string & uuidRet, const std::string & currency, const int & confirmations, const int & block);

    /**
     * @brief returns block data by hash
     * @param uuidRet uuid of the request
     * @param currency chain code (BTC, LTC etc)
     * @param confirmations number of service nodes to call (final result is selected from all answers by majority vote)
     * @param blockHash block hash string
     * @return json reply of getblock
     */
    std::string getBlock(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & blockHash);

    /**
     * @brief returns all blocks starting from given number
     * @param uuidRet uuid of the request
     * @param currency chain code (BTC, LTC etc)
     * @param confirmations number of service nodes to call (final result is selected from all answers by majority vote)
     * @param blockHashes set of hashes to obtain block information for
     * @return json array with block data
     */
    std::string getBlocks(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::set<std::string> & blockHashes);

    /**
     * @brief returns transaction by hash (requires tx idnex on server side)
     * @param uuidRet uuid of the request
     * @param currency chain code (BTC, LTC etc)
     * @param confirmations number of service nodes to call (final result is selected from all answers by majority vote)
     * @param hash tx hash
     * @return json reply of decoderawtransaction
     */
    std::string getTransaction(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & hash);

    /**
     * @brief returns transactions belonging to account after given block
     * @param uuidRet uuid of the request
     * @param currency chain code (BTC, LTC etc)
     * @param confirmations number of service nodes to call (final result is selected from all answers by majority vote)
     * @param txs set of transaction hashes
     * @return json array of transaction data
     */
    std::string getTransactions(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::set<std::string> & txHashes);

    /**
     * @brief sends raw transaction to the given chain
     * @param currency chain code (BTC, LTC etc)
     * @param confirmations number of service nodes to call (final result is selected from all answers by majority vote)
     * @param transaction raw signed transaction
     * @return
     */
    std::string sendTransaction(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & transaction);

    /**
     * @brief returns all transactions using bloom filter
     * @param uuidRet uuid of the request
     * @param currency chain code (BTC, LTC etc)
     * @param confirmations consensus number of snodes (must be at least 1)
     * @param filter
     * @param number block number where to start
     * @return
     */
    std::string getTransactionsBloomFilter(std::string & uuidRet, const std::string & currency,
            const int & confirmations, const std::string & filter, const int & number);

    /**
     * @brief Returnst the block count at the specified time.
     * @param uuidRet uuid of the request
     * @param currency chain to query
     * @param confirmations consensus number (must be at least 1)
     * @param time Estimated block time
     * @return
     */
    std::string convertTimeToBlockCount(std::string & uuidRet, const std::string& currency,
            const int & confirmations, const int64_t & time);

    /**
     * @brief returns balance for given account
     * @param uuidRet uuid of the request
     * @param currency chain code (BTC, LTC etc)
     * @param address payment address
     * @return balance (float converted to string)
     */
    std::string getBalance(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & address);

    /**
     * Attempts to connects to at least number of indicated service nodes with the specified service and
     * returns a map of connections less than equal to count.
     * @param service
     * @param count Number of service nodes to query.
     * @return
     */
    std::map<NodeAddr, XRouterSettingsPtr> xrConnect(const std::string & service, const int & count);

    /**
     * @brief fetches the reply to the giver request
     * @param uuid UUID of the query
     * @return
     */
    std::string getReply(const std::string & uuid);

    /**
     * JSON output of specified configurations.
     * @param configs
     * @param data Array with json output.
     * @return
     */
    void snodeConfigJSON(const std::map<NodeAddr, XRouterSettingsPtr> & configs, json_spirit::Array & data);

    /**
     * Returns a map of connected node configurations.
     * @return
     */
    std::map<NodeAddr, XRouterSettingsPtr> getNodeConfigs() {
        return getConfigs();
    }

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
     * @brief process an "invalid" reply from an XRouter node. I.e. a valid reply indicating that the client
     *        sent an invalid packet.
     * @param node Connection to node
     * @param packet Xrouter packet received over the network
     * @param state DOS state
     * @return
     */
    bool processInvalid(CNode *node, XRouterPacketPtr packet, CValidationState & state);

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
     * @param command XRouter command
     * @param service Wallet or currency name
     * @param count Number of nodes to fetch (default 1 node)
     * @param nodes Currently connected nodes
     * @param nodec Nodes mapped by node address
     * @param snodes Servicenodes mapped by node address
     * @return
     */
    std::vector<CNode*> availableNodesRetained(enum XRouterCommand command, const std::string & service, const int & count = 1);
    
    /**
     * @brief find the node that supports a given plugin 
     * @param name plugin name
     * @return
     */
    CNode* getNodeForService(std::string name);
    
    /**
     * @brief generates a payment transaction to given service node
     * @param node Service node address
     * @param paymentAddress Payment address to use
     * @param fee amount to pay
     * @param address hex-encoded transaction
     * @return bool True if payment generated, otherwise false
     * @throws std::runtime_error in case of errors
     */
    bool generatePayment(const NodeAddr & pnode, const std::string & paymentAddress,
            const CAmount & fee, std::string & payment);
    
    /**
     * @brief onMessageReceived  call when message from xrouter network received
     * @param node source CNode
     * @param message packet contents
     */
    void onMessageReceived(CNode* node, const std::vector<unsigned char> & message);
    
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
     * Returns true if the rate limit has been exceeded on requests to the specified node.
     * @param node Node address
     * @param service Name of the command or service
     * @param lastRequest Time of last request
     * @param rateLimit Rate limit in milliseconds
     * @return
     */
    bool rateLimitExceeded(const NodeAddr & node, const std::string & service,
            const std::chrono::time_point<std::chrono::system_clock> lastRequest, const int rateLimit)
    {
        if (hasSentRequest(node, service)) {
            std::chrono::time_point<std::chrono::system_clock> time = std::chrono::system_clock::now();
            std::chrono::system_clock::duration diff = time - lastRequest;
            if (std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() < std::chrono::milliseconds(rateLimit).count())
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

    /**
     * Returns true if any queries are pending for the specified node.
     * @param node
     * @return
     */
    bool hasPendingQuery(const NodeAddr & node) {
        WaitableLock l(mu);
        return server->hasInFlightQuery(node);
    }

    /**
     * Return time of the last request to specified node.
     * @param node
     * @param command
     * @return
     */
    std::chrono::time_point<std::chrono::system_clock> getLastRequest(const NodeAddr & node, const std::string & command) {
        WaitableLock l(mu);
        if (lastPacketsSent.count(node) && lastPacketsSent[node].count(command))
            return lastPacketsSent[node][command];
        return std::chrono::system_clock::from_time_t(0);
    }

    /**
     * Returns true if a request to a node has been made previously.
     * @param node
     * @param command
     * @return
     */
    bool hasSentRequest(const NodeAddr & node, const std::string & command) {
        WaitableLock l(mu);
        return lastPacketsSent.count(node) && lastPacketsSent[node].count(command);
    }

    /**
     * Updates (or adds) a request time for the specified node and command.
     * @param node
     * @param command
     */
    void updateSentRequest(const NodeAddr & node, const std::string & command) {
        WaitableLock l(mu);
        lastPacketsSent[node][command] = std::chrono::system_clock::now();
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

    bool hasScore(const NodeAddr & node) {
        WaitableLock l(mu);
        return snodeScore.count(node);
    }
    int64_t getScore(const NodeAddr & node) {
        WaitableLock l(mu);
        return snodeScore[node];
    }
    void updateScore(const NodeAddr & node, const int score) {
        WaitableLock l(mu);
        if (!snodeScore.count(node))
            snodeScore[node] = 0;
        snodeScore[node] += score;
    }
    std::map<NodeAddr, XRouterSettingsPtr> getConfigs() {
        WaitableLock l(mu);
        return snodeConfigs;
    }
    bool hasDomain(const std::string & domain) {
        WaitableLock l(mu);
        return snodeDomains.count(domain);
    }
    NodeAddr getDomainNode(const std::string & domain) {
        WaitableLock l(mu);
        return snodeDomains[domain];
    }
    void updateDomainNode(const std::string & domain, const NodeAddr & node) {
        WaitableLock l(mu);
        snodeDomains[domain] = node;
    }
    void addQuery(const std::string & queryId, const NodeAddr & node) {
        WaitableLock l(mu);
        if (!configQueries.count(queryId))
            configQueries[queryId] = std::set<NodeAddr>{node};
        else
            configQueries[queryId].insert(node);
    }
    bool hasQuery(const std::string & queryId) {
        WaitableLock l(mu);
        return configQueries.count(queryId);
    }
    bool completedQuery(const std::string & queryId) {
        WaitableLock l(mu);
        return configQueries.count(queryId);
    }
    std::set<NodeAddr> getNodesForQuery(const std::string & queryId) {
        WaitableLock l(mu);
        if (!configQueries.count(queryId))
            return std::set<NodeAddr>{};
        return configQueries[queryId];
    }
    bool hasConfig(const NodeAddr & node) {
        WaitableLock l(mu);
        return snodeConfigs.count(node);
    }
    XRouterSettingsPtr getConfig(const NodeAddr & node) {
        WaitableLock l(mu);
        return snodeConfigs[node];
    }
    void updateConfig(const NodeAddr & node, XRouterSettingsPtr config) {
        WaitableLock l(mu);
        snodeConfigs[node] = config;
    }
    bool needConfigUpdate(const NodeAddr & node, const bool & isServer = false) {
        const auto & service = XRouterCommand_ToString(xrGetConfig);
        return !rateLimitExceeded(node, service, getLastRequest(node, service),
                isServer ? 10000 : 600000); // server default is 10 seconds, client default is 10 minutes
    }

    /**
     * @brief Serialize configuration.
     * @param cfg XRouter settings obj
     * @return
     */
    std::string parseConfig(XRouterSettingsPtr cfg);

    /**
     * Fills the specified containers with the latest active nodes.
     * @param snodes Servicenode list
     * @param nodes Connected node list
     * @param snodec Map of servicenodes
     * @param nodec Map of connected servicenodes node refs
     */
    void getLatestNodeContainers(std::vector<CServicenode> & snodes, std::vector<CNode*> & nodes,
                                 std::map<NodeAddr, CServicenode> & snodec, std::map<NodeAddr, CNode*> & nodec);
    /**
     * Decrements node references.
     * @param nodes
     */
    void releaseNodes(const std::vector<CNode*> & nodes) {
        LOCK(cs_vNodes);
        for (auto & pnode : nodes)
            pnode->Release();
    }

    class PendingConnectionMgr {
    public:
        PendingConnectionMgr() = default;
        typedef std::pair<std::shared_ptr<boost::mutex>, std::shared_ptr<boost::condition_variable> > PendingConnection;
        /**
         * Add a pending connection for this node.
         * @param node
         * @param conn Pending connection returned.
         * @return
         */
        bool addPendingConnection(const NodeAddr & node, PendingConnection & conn) {
            if (hasPendingConnection(node))
                return false; // skip adding duplilcate entries

            WaitableLock l(mu);
            auto m = std::make_shared<boost::mutex>();
            auto cond = std::make_shared<boost::condition_variable>();
            auto qc = PendingConnection{m, cond};
            pendingConnections[node] = qc;
            conn = qc;

            return true;
        }
        /**
         * Return true if a pending connection exists.
         * @param node
         * @return
         */
        bool hasPendingConnection(const NodeAddr & node) {
            WaitableLock l(mu);
            return pendingConnections.count(node);
        }
        /**
         * Return true if a pending connection exists.
         * @param node
         * @param conn Pending connection object.
         * @return
         */
        bool hasPendingConnection(const NodeAddr & node, PendingConnection & conn) {
            WaitableLock l(mu);
            bool found = pendingConnections.count(node);
            if (found)
                conn = pendingConnections[node];
            return found;
        }
        /**
         * Remove the pending connection for this node.
         * @param node
         */
        void removePendingConnection(const NodeAddr & node) {
            WaitableLock l(mu);
            pendingConnections.erase(node);
        }
        /**
         * Get the pending connection for this node.
         * @param node
         * @return
         */
        PendingConnection pendingConnection(const NodeAddr & node) {
            WaitableLock l(mu);
            return pendingConnections[node];
        }
        /**
         * Returns the pending connection mutex or null if not found.
         * @param node
         * @return
         */
        std::shared_ptr<boost::mutex> connectionLock(const NodeAddr & node) {
            WaitableLock l(mu);
            if (!pendingConnections.count(node))
                return nullptr;
            return pendingConnections[node].first;
        }
        /**
         * Returns the pending connection condition variable or null if not found.
         * @param node
         * @return
         */
        std::shared_ptr<boost::condition_variable> connectionCond(const NodeAddr & node) {
            WaitableLock l(mu);
            if (!pendingConnections.count(node))
                return nullptr;
            return pendingConnections[node].second;
        }
        /**
         * Return number of pending connections.
         * @return
         */
        int size() {
            return static_cast<int>(pendingConnections.size());
        }
        /**
         * Notify observers.
         * @param node
         */
        void notify(const NodeAddr & node) {
            PendingConnection conn;
            bool valid{false};

            {
                WaitableLock l(mu);
                if (pendingConnections.count(node)) {
                    conn = pendingConnections[node];
                    valid = true;
                }
            }

            if (valid) { // only handle locks if they exist
                boost::mutex::scoped_lock l(*conn.first);
                removePendingConnection(node); // remove the connection, we're done
                conn.second->notify_all();
            }
        }
    private:
        CWaitableCriticalSection mu;
        std::map<NodeAddr, PendingConnection> pendingConnections;
    };

    class QueryMgr {
    public:
        typedef std::string QueryReply;
        typedef std::pair<std::shared_ptr<boost::mutex>, std::shared_ptr<boost::condition_variable> > QueryCondition;
        QueryMgr() : queriesLocks(), queries() {}
        /**
         * Add a query. This stores interal state including condition variables and associated mutexes.
         * @param id uuid of query, can't be empty
         * @param node address of node associated with query, can't be empty
         */
        void addQuery(const std::string & id, const NodeAddr & node) {
            if (id.empty() || node.empty())
                return;

            WaitableLock l(mu);

            if (!queries.count(id))
                queries[id] = std::map<NodeAddr, std::string>{};

            std::shared_ptr<boost::mutex> m(new boost::mutex());
            std::shared_ptr<boost::condition_variable> cond(new boost::condition_variable());

            if (!queriesLocks.count(id))
                queriesLocks[id] = std::map<NodeAddr, QueryCondition>{};

            auto qc = QueryCondition{m, cond};
            queriesLocks[id][node] = qc;
        }
        /**
         * Store a query reply.
         * @param id
         * @param node
         * @param reply
         * @return Total number of replies for the query with specified id.
         */
        int addReply(const std::string & id, const NodeAddr & node, const std::string & reply) {
            if (id.empty() || node.empty())
                return 0;

            int replies{0};
            QueryCondition qcond;

            {
                WaitableLock l(mu);

                if (!queries.count(id))
                    return 0; // done, no query found with id

                // Total replies
                replies = queriesLocks.count(id);
                // Query condition
                if (replies)
                    qcond = queriesLocks[id][node];
            }

            if (replies) { // only handle locks if they exist for this query
                boost::mutex::scoped_lock l(*qcond.first);
                queries[id][node] = reply; // Assign reply
                qcond.second->notify_all();
            }

            WaitableLock l(mu);
            return queries.count(id);
        }
        /**
         * Fetch a reply. This method returns the number of matching replies.
         * @param id
         * @param reply
         * @return
         */
        int reply(const std::string & id, const NodeAddr & node, std::string & reply) {
            WaitableLock l(mu);

            int consensus = queries.count(id);
            if (!consensus)
                return 0;

            reply = queries[id][node];
            return consensus;
        }
        /**
         * Fetch the most common reply for a specific query. If a group of nodes return results and 2 of 3 are
         * matching, this will return the most common reply, i.e. the replies of the matching two.
         * @param id
         * @param reply
         * @param agree Set of nodes that provided most common replies
         * @param diff Set of nodes that provided non-common replies
         * @return
         */
        int mostCommonReply(const std::string & id, std::string & reply, std::set<NodeAddr> & agree, std::set<NodeAddr> & diff) {
            WaitableLock l(mu);

            int consensus = queries.count(id);
            if (!consensus)
                return 0;

            std::map<uint256, std::string> hashes;
            std::map<uint256, int> counts;
            std::map<uint256, std::set<NodeAddr> > nodes;
            for (auto & item : queries[id]) {
                auto hash = Hash(item.second.begin(), item.second.end());
                hashes[hash] = item.second;
                counts[hash] = counts.count(hash) + 1; // update counts for common replies
                nodes[hash].insert(item.first);
            }

            // sort replies counts descending (most similar replies are more valuable)
            std::vector<std::pair<uint256, int> > tmp(counts.begin(), counts.end());
            std::sort(tmp.begin(), tmp.end(),
                      [](std::pair<uint256, int> & a, std::pair<uint256, int> & b) {
                          return a.second > b.second;
                      });

            diff.clear();
            if (tmp.size() > 1) {
                for (int i = 1; i < tmp.size(); ++i) {
                    auto & hash = tmp[i].first;
                    if (!nodes.count(hash) || tmp[i].second >= tmp[0].second) // do not penalize equal counts, only fewer
                        continue;
                    auto ns = nodes[hash];
                    diff.insert(ns.begin(), ns.end());
                }
            }

            auto selhash = tmp[0].first;

            // store agreeing nodes
            agree.clear();
            agree = nodes[selhash];

            // select the most common replies
            reply = hashes[selhash];
            return tmp[0].second;
        }
        /**
         * Returns true if the query with specified id.
         * @param id
         * @return
         */
        bool hasQuery(const std::string & id) {
            WaitableLock l(mu);
            return queriesLocks.count(id);
        }
        /**
         * Returns true if the query with specified id and node address is valid.
         * @param id
         * @param node
         * @return
         */
        bool hasQuery(const std::string & id, const NodeAddr & node) {
            WaitableLock l(mu);
            return queriesLocks.count(id) && queriesLocks[id].count(node);
        }
        /**
         * Returns true if a query for the specified node exists.
         * @param node
         * @return
         */
        bool hasNodeQuery(const NodeAddr & node) {
            WaitableLock l(mu);
            for (const auto & item : queriesLocks) {
                if (item.second.count(node))
                    return true;
            }
            return false;
        }
        /**
         * Returns true if the reply exists for the specified node.
         * @param id
         * @param nodeAddr node address
         * @return
         */
        bool hasReply(const std::string & id, const NodeAddr & node) {
            WaitableLock l(mu);
            return queries.count(id) && queries[id].count(node);
        }
        /**
         * Returns the query's mutex.
         * @param id
         * @param node
         * @return
         */
        std::shared_ptr<boost::mutex> queryLock(const std::string & id, const NodeAddr & node) {
            WaitableLock l(mu);
            if (!queriesLocks.count(id))
                return nullptr;
            if (!queriesLocks[id].count(node))
                return nullptr;
            return queriesLocks[id][node].first;
        }
        /**
         * Returns the queries condition variable.
         * @param id
         * @param node
         * @return
         */
        std::shared_ptr<boost::condition_variable> queryCond(const std::string & id, const NodeAddr & node) {
            WaitableLock l(mu);
            if (!queriesLocks.count(id))
                return nullptr;
            if (!queriesLocks[id].count(node))
                return nullptr;
            return queriesLocks[id][node].second;
        }
        /**
         * Return all replies associated with a query.
         * @param id
         * @return
         */
        std::map<std::string, QueryReply> allReplies(const std::string & id) {
            WaitableLock l(mu);
            return queries[id];
        }
        /**
         * Return all query locks associated with an id.
         * @param id
         * @return
         */
        std::map<std::string, QueryCondition> allLocks(const std::string & id) {
            WaitableLock l(mu);
            return queriesLocks[id];
        }
        /**
         * Purges the ephemeral state of a query with specified id.
         * @param id
         */
        void purge(const std::string & id) {
            WaitableLock l(mu);
            queriesLocks.erase(id);
        }
        /**
         * Purges the ephemeral state of a query with specified id and node address.
         * @param id
         * @param node
         */
        void purge(const std::string & id, const NodeAddr & node) {
            WaitableLock l(mu);
            if (queriesLocks.count(id))
                queriesLocks[id].erase(node);
        }
    private:
        CWaitableCriticalSection mu;
        std::map<std::string, std::map<NodeAddr, QueryCondition> > queriesLocks;
        std::map<std::string, std::map<NodeAddr, QueryReply> > queries;
    };

private:
    CWaitableCriticalSection mu;

    XRouterSettingsPtr xrsettings;
    XRouterServerPtr server;

    std::map<std::string, int64_t> snodeScore;

    std::map<std::string, std::set<NodeAddr> > configQueries;
    std::map<NodeAddr, std::map<std::string, std::chrono::time_point<std::chrono::system_clock> > > lastPacketsSent;
    std::map<NodeAddr, XRouterSettingsPtr> snodeConfigs;
    std::map<std::string, NodeAddr> snodeDomains;

    boost::filesystem::path xrouterpath;
    bool xrouterIsReady{false};

    boost::thread_group requestHandlers;
    std::deque<std::shared_ptr<boost::asio::io_service> > ioservices;
    std::deque<std::shared_ptr<boost::asio::io_service::work> > ioworkers;

    // timer
    void onTimer();
    boost::asio::io_service timerIo;
    boost::thread timerThread;
    boost::asio::deadline_timer timer;

    // Key management
    xbridge::BtcCryptoProvider crypto;
    std::vector<unsigned char> cpubkey;
    std::vector<unsigned char> cprivkey;

    QueryMgr queryMgr;
    PendingConnectionMgr pendingConnMgr;
};

} // namespace xrouter

#endif // XROUTERAPP_H
