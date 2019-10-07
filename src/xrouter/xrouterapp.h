// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_XROUTER_XROUTERAPP_H
#define BLOCKNET_XROUTER_XROUTERAPP_H

#include <servicenode/servicenode.h>
#include <xrouter/xrouterdef.h>
#include <xrouter/xrouterpacket.h>
#include <xrouter/xrouterserver.h>
#include <xrouter/xroutersettings.h>
#include <xrouter/xrouterutils.h>

#include <banman.h>
#include <hash.h>
#include <init.h>
#include <key_io.h>
#include <net.h>
#include <net_processing.h>
#include <sync.h>
#include <uint256.h>

#include <algorithm>
#include <chrono>
#include <memory>

#include <json/json_spirit.h>
#include <json/json_spirit_reader_template.h>
#include <json/json_spirit_writer_template.h>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/thread.hpp>

//*****************************************************************************
//*****************************************************************************
namespace xrouter
{

typedef std::shared_ptr<XRouterSettings> XRouterSettingsPtr;
typedef std::shared_ptr<XRouterServer> XRouterServerPtr;

template <typename T>
bool PushXRouterMessage(CNode *pnode, const T & message);

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
    bool init();

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
    void updatePaymentAddress(const CTxDestination & dest) {
        auto paymentAddress = EncodeDestination(dest);
        xrsettings->defaultPaymentAddress(paymentAddress);
    }

    /**
     * @brief Open connections to service nodes with specified services.
     * @param command XRouter command
     * @param service Wallet name or plugin name
     * @param count Number of nodes to open connections to
     * @param parameterCount Number of parameters used in the call
     * @param skipNodes avoids connecting to these nodes
     * @param foundCount number of nodes found
     */
    bool openConnections(enum XRouterCommand command, const std::string & service, const uint32_t & count,
                         const int & parameterCount, const std::vector<CNode*> & skipNodes, uint32_t & foundCount);
    
    /**
     * @brief send config update requests to all nodes
     */
    std::string updateConfigs(bool force = false);
    
    /**
     * @brief prints xrouter configs
     */
    std::string printConfigs();

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
    std::string getBlockHash(std::string & uuidRet, const std::string & currency, const int & confirmations, const unsigned int & block);

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
    std::string getBlocks(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::vector<std::string> & blockHashes);

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
    std::string getTransactions(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::vector<std::string> & txHashes);

    /**
     * @brief Decodes the specified raw transaction.
     * @param uuidRet uuid of the request
     * @param currency chain code (BTC, LTC etc)
     * @param confirmations number of service nodes to call (final result is selected from all answers by majority vote)
     * @param rawtx raw transaction as a hex string
     * @return json array of transaction data
     */
    std::string decodeRawTransaction(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & rawtx);

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
     * @param foundCount Number of service nodes that were found.
     * @return
     */
    std::map<NodeAddr, XRouterSettingsPtr> xrConnect(const std::string & service, const int & count, uint32_t & foundCount);

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
    std::string sendXRouterConfigRequest(CNode* node, const std::string & addr="self");
    
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
     * @brief process config message from sn::PING packet.
     * @param snode Service node to process config from
     * @return
     */
    bool processConfigMessage(const sn::ServiceNode & snode);

    /**
     * @brief get all nodes that support the command for a given chain
     * @param command XRouter command
     * @param service Wallet or currency name
     * @param parameterCount Number of parameters in request
     * @param count Number of nodes to fetch (default 1 node)
     * @return
     */
    std::vector<CNode*> availableNodesRetained(enum XRouterCommand command, const std::string & service,
                                               const int & parameterCount, const int & count = 1);
    
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
     * @brief returns service node collateral address
     */
    std::string getMyPaymentAddress();

    /**
     * @brief Create a change address return base58 version.
     * @return
     */
    std::string changeAddress() {
        auto wallets = GetWallets();
        auto wallet = wallets.front();
        if (!wallet->IsLocked())
            wallet->TopUpKeyPool();

        CReserveKey reservekey(wallet.get());
        CPubKey vchPubKey;
        if (!reservekey.GetReservedKey(vchPubKey))
            return "";

        reservekey.KeepKey();
        CKeyID keyID = vchPubKey.GetID();
        return EncodeDestination(CTxDestination(keyID));
    }

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
            CKey key; key.MakeNewKey(true);
            CPubKey pubkey = key.GetPubKey();
            cprivkey = ToByteVector(key);
            cpubkey = ToByteVector(pubkey);
            return true;
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
        LOCK(mu);
        return server->hasInFlightQuery(node);
    }

    /**
     * Return time of the last request to specified node.
     * @param node
     * @param command
     * @return
     */
    std::chrono::time_point<std::chrono::system_clock> getLastRequest(const NodeAddr & node, const std::string & command) {
        LOCK(mu);
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
        LOCK(mu);
        return lastPacketsSent.count(node) && lastPacketsSent[node].count(command);
    }

    /**
     * Updates (or adds) a request time for the specified node and command.
     * @param node
     * @param command
     */
    void updateSentRequest(const NodeAddr & node, const std::string & command) {
        LOCK(mu);
        lastPacketsSent[node][command] = std::chrono::system_clock::now();
    }

    /**
     * @brief Serialize configuration.
     * @param cfg XRouter settings obj
     * @return
     */
    std::string parseConfig(XRouterSettingsPtr cfg);

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
        LOCK(mu);
        return snodeScore.count(node);
    }
    int getScore(const NodeAddr & node) {
        LOCK(mu);
        return snodeScore[node];
    }
    void updateScore(const NodeAddr & node, const int score) {
        LOCK(mu);
        if (!snodeScore.count(node))
            snodeScore[node] = 0;
        snodeScore[node] += score;
        const auto scr = snodeScore[node];
        int banscore = gArgs.GetArg("-xrouterbanscore", -200);
        if (scr <= banscore) {
            g_connman->ForEachNode([&node,scr,this](CNode *pnode) {
                if (node == pnode->GetAddrName()) {
                    LOG() << strprintf("Banning XRouter Node %s because score is too low: %i", node, scr);
                    snodeScore[node] = -30; // default score when ban expires
                    LOCK(cs_main);
                    Misbehaving(pnode->GetId(), 100);
                }
            });
        }
    }
    bool bestNode(const NodeAddr & a, const NodeAddr & b, const XRouterCommand & command, const std::string & service) {
        const auto & a_score = getScore(a);
        const auto & b_score = getScore(b);
        if (a_score < 0)
            return a_score > b_score;
        if (b_score < 0)
            return true;
        auto sa = getConfig(a);
        auto sb = getConfig(b);
        if (!sa || !sb)
            return a_score > b_score;
        return sa->commandFee(command, service) < sb->commandFee(command, service);
    }

    std::map<NodeAddr, XRouterSettingsPtr> getConfigs() {
        LOCK(mu);
        return snodeConfigs;
    }
    bool hasDomain(const std::string & domain) {
        LOCK(mu);
        return snodeDomains.count(domain);
    }
    NodeAddr getDomainNode(const std::string & domain) {
        LOCK(mu);
        return snodeDomains[domain];
    }
    void updateDomainNode(const std::string & domain, const NodeAddr & node) {
        LOCK(mu);
        snodeDomains[domain] = node;
    }
    void addQuery(const std::string & queryId, const NodeAddr & node) {
        LOCK(mu);
        if (!configQueries.count(queryId))
            configQueries[queryId] = std::set<NodeAddr>{node};
        else
            configQueries[queryId].insert(node);
    }
    bool hasQuery(const std::string & queryId) {
        LOCK(mu);
        return configQueries.count(queryId);
    }
    bool completedQuery(const std::string & queryId) {
        LOCK(mu);
        return configQueries.count(queryId);
    }
    std::set<NodeAddr> getNodesForQuery(const std::string & queryId) {
        LOCK(mu);
        if (!configQueries.count(queryId))
            return std::set<NodeAddr>{};
        return configQueries[queryId];
    }
    bool hasConfig(const NodeAddr & node) {
        LOCK(mu);
        return snodeConfigs.count(node);
    }
    XRouterSettingsPtr getConfig(const NodeAddr & node) {
        LOCK(mu);
        if (snodeConfigs.count(node))
            return snodeConfigs[node];
        return nullptr;
    }
    void updateConfig(const NodeAddr & node, XRouterSettingsPtr config) {
        LOCK(mu);
        snodeConfigs[node] = config;
    }
    bool needConfigUpdate(const NodeAddr & node, const bool & isServer = false) {
        const auto & service = XRouterCommand_ToString(xrGetConfig);
        return !rateLimitExceeded(node, service, getLastRequest(node, service),
                isServer ? 10000 : 600000); // server default is 10 seconds, client default is 10 minutes
    }

    /**
     * Fills the specified containers with the latest active nodes.
     * @param snodes Servicenode list
     * @param nodes Connected node list
     * @param snodec Map of servicenodes
     * @param nodec Map of connected servicenodes node refs
     */
    void getLatestNodeContainers(std::vector<sn::ServiceNode> & snodes, std::vector<CNode*> & nodes,
                                 std::map<NodeAddr, sn::ServiceNode> & snodec, std::map<NodeAddr, CNode*> & nodec);
    /**
     * Decrements node references.
     * @param nodes
     */
    void releaseNodes(const std::vector<CNode*> & nodes) {
        for (auto & pnode : nodes)
            pnode->Release();
    }

    /**
     * DoS any bad nodes based on specified validation state.
     * @param state
     * @param pnode
     */
    void checkDoS(CValidationState & state, CNode *pnode);

    class PendingConnectionMgr {
    public:
        PendingConnectionMgr() = default;
        typedef std::pair<std::shared_ptr<boost::mutex>, std::shared_ptr<boost::condition_variable> > PendingConnection;
        /**
         * Add a pending connection for this node.
         * @param node
         * @param conn Pending connection returned.
         * @return true if pending connection was created, otherwise if there's already a connection returns false
         */
        bool addPendingConnection(const NodeAddr & node, PendingConnection & conn) {
            if (hasPendingConnection(node))
                return false; // skip adding duplilcate entries

            LOCK(mu);
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
            LOCK(mu);
            return pendingConnections.count(node);
        }
        /**
         * Return true if a pending connection exists.
         * @param node
         * @param conn Pending connection object.
         * @return
         */
        bool hasPendingConnection(const NodeAddr & node, PendingConnection & conn) {
            LOCK(mu);
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
            LOCK(mu);
            pendingConnections.erase(node);
        }
        /**
         * Returns the pending connection mutex or null if not found.
         * @param node
         * @return
         */
        std::shared_ptr<boost::mutex> connectionLock(const NodeAddr & node) {
            LOCK(mu);
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
            LOCK(mu);
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
                LOCK(mu);
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
        Mutex mu;
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

            LOCK(mu);

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
                LOCK(mu);

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

            LOCK(mu);
            return queries.count(id);
        }
        /**
         * Fetch a reply. This method returns the number of matching replies.
         * @param id
         * @param reply
         * @return
         */
        int reply(const std::string & id, const NodeAddr & node, std::string & reply) {
            LOCK(mu);

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
         * @param reply Most common reply
         * @param replies All replies
         * @param agree Set of nodes that provided most common replies
         * @param diff Set of nodes that provided non-common replies
         * @return
         */
        int mostCommonReply(const std::string & id, std::string & reply, std::map<NodeAddr, std::string> & replies,
                            std::set<NodeAddr> & agree, std::set<NodeAddr> & diff)
        {
            LOCK(mu);

            int consensus = queries.count(id);
            if (!consensus || queries[id].empty())
                return 0;

            // all replies
            replies = queries[id];

            std::map<uint256, std::string> hashes;
            std::map<uint256, int> counts;
            std::map<uint256, std::set<NodeAddr> > nodes;
            for (auto & item : queries[id]) {
                auto result = item.second;
                try {
                    Value j; read_string(result, j);
                    if (j.type() == obj_type)
                        result = write_string(j, false);
                } catch (...) {
                    result = item.second;
                }
                auto hash = Hash(result.begin(), result.end());
                hashes[hash] = item.second;
                counts[hash] = counts.count(hash) + 1; // update counts for common replies
                nodes[hash].insert(item.first);
            }

            // sort reply counts descending (most similar replies are more valuable)
            std::vector<std::pair<uint256, int> > tmp(counts.begin(), counts.end());
            std::sort(tmp.begin(), tmp.end(),
                      [](std::pair<uint256, int> & a, std::pair<uint256, int> & b) {
                          return a.second > b.second;
                      });

            diff.clear();
            if (tmp.size() > 1) {
                if (tmp[0].second == tmp[1].second) { // Check for errors and re-sort if there's a tie and highest rank has error
                    const auto &r = hashes[tmp[0].first];
                    if (hasError(r)) { // in tie arrangements we don't want errors to take precendence
                        std::sort(tmp.begin(), tmp.end(), // sort descending
                            [this, &hashes](std::pair<uint256, int> & a, std::pair<uint256, int> & b) {
                                const auto & ae = hasError(hashes[a.first]);
                                const auto & be = hasError(hashes[b.first]);
                                if ((!ae && !be) || (ae && be))
                                    return a.second > b.second;
                                return be;
                            });
                    }
                }
                // Filter nodes that responded with different results
                for (int i = 1; i < static_cast<int>(tmp.size()); ++i) {
                    const auto & hash = tmp[i].first;
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
            LOCK(mu);
            return queriesLocks.count(id);
        }
        /**
         * Returns true if the query with specified id and node address is valid.
         * @param id
         * @param node
         * @return
         */
        bool hasQuery(const std::string & id, const NodeAddr & node) {
            LOCK(mu);
            return queriesLocks.count(id) && queriesLocks[id].count(node);
        }
        /**
         * Returns true if a query for the specified node exists.
         * @param node
         * @return
         */
        bool hasNodeQuery(const NodeAddr & node) {
            LOCK(mu);
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
            LOCK(mu);
            return queries.count(id) && queries[id].count(node);
        }
        /**
         * Returns the query's mutex.
         * @param id
         * @param node
         * @return
         */
        std::shared_ptr<boost::mutex> queryLock(const std::string & id, const NodeAddr & node) {
            LOCK(mu);
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
            LOCK(mu);
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
            LOCK(mu);
            return queries[id];
        }
        /**
         * Return all query locks associated with an id.
         * @param id
         * @return
         */
        std::map<std::string, QueryCondition> allLocks(const std::string & id) {
            LOCK(mu);
            return queriesLocks[id];
        }
        /**
         * Purges the ephemeral state of a query with specified id.
         * @param id
         */
        void purge(const std::string & id) {
            LOCK(mu);
            queriesLocks.erase(id);
        }
        /**
         * Purges the ephemeral state of a query with specified id and node address.
         * @param id
         * @param node
         */
        void purge(const std::string & id, const NodeAddr & node) {
            LOCK(mu);
            if (queriesLocks.count(id))
                queriesLocks[id].erase(node);
        }
    private:
        bool hasError(const std::string & reply) {
            Value v; json_spirit::read_string(reply, v);
            if (v.type() != json_spirit::obj_type)
                return false;
            const auto & err_v = json_spirit::find_value(v.get_obj(), "error");
            return err_v.type() != json_spirit::null_type;
        }
    private:
        Mutex mu;
        std::map<std::string, std::map<NodeAddr, QueryCondition> > queriesLocks;
        std::map<std::string, std::map<NodeAddr, QueryReply> > queries;
    };

private:
    Mutex mu;

    XRouterSettingsPtr xrsettings;
    XRouterServerPtr server;

    std::map<NodeAddr, int> snodeScore;

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
    std::vector<unsigned char> cpubkey;
    std::vector<unsigned char> cprivkey;

    QueryMgr queryMgr;
    PendingConnectionMgr pendingConnMgr;
};

} // namespace xrouter

#endif // BLOCKNET_XROUTER_XROUTERAPP_H
