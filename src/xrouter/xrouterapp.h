// Copyright (c) 2018-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_XROUTER_XROUTERAPP_H
#define BLOCKNET_XROUTER_XROUTERAPP_H

#include <xrouter/xrouterdef.h>
#include <xrouter/xrouterpacket.h>
#include <xrouter/xrouterquerymgr.h>
#include <xrouter/xrouterserver.h>
#include <xrouter/xroutersettings.h>
#include <xrouter/xrouterutils.h>

#include <banman.h>
#include <hash.h>
#include <key_io.h>
#include <net.h>
#include <servicenode/servicenode.h>
#include <sync.h>
#include <uint256.h>
#ifdef ENABLE_WALLET
#include <wallet/wallet.h>
#endif

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
     * @param confDir location of the directory containing xrouter.conf
     * @param skipPlugins If true the plugin samples are not created
     * @return true if created otherwise false
     */
    static bool createConf(const boost::filesystem::path & confDir, const bool & skipPlugins = false);

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
     * @param xrouterDir Directory containing xbridge.conf and xrouter.conf
     * @return true if successful
     */
    bool init(const boost::filesystem::path & xrouterDir);

    /**
     * @brief start - start xrouter
     * run services, sessions,
     * @return true if all components run successfull
     */
    bool start();

    /**
     * @brief stop - stopped services
     * @param safeCleanup specify false to indicate potential unsafe cleanup
     * @return
     */
    bool stop(const bool safeCleanup = true);

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
     * @param exrSnodes are selected nodes that do not accept requests on the default blocknet wallet port
     * @param foundCount number of nodes found
     */
    bool openConnections(enum XRouterCommand command, const std::string & service, const uint32_t & count,
                         const int & parameterCount, const std::vector<CNode*> & skipNodes,
                         std::vector<sn::ServiceNode> & exrSnodes, uint32_t & foundCount);
    
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
     * @param params json parameter list
     * @return reply from service node
     */
    std::string xrouterCall(enum XRouterCommand command, std::string & uuidRet, const std::string & service,
                            const int & confirmations, const UniValue & params);

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
    std::map<NodeAddr, std::pair<XRouterSettingsPtr, sn::ServiceNode::Tier>> xrConnect(const std::string & service, const int & count, uint32_t & foundCount);

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
    void snodeConfigJSON(const std::map<NodeAddr, std::pair<XRouterSettingsPtr, sn::ServiceNode::Tier>> & configs, json_spirit::Array & data);

    /**
     * Returns a map of connected node configurations.
     * @return
     */
    std::map<NodeAddr, std::pair<XRouterSettingsPtr, sn::ServiceNode::Tier>> getNodeConfigs() {
        return getConfigs();
    }

    /**
     * @brief reload xrouter.conf and plugin configs from disks
     * @return true on success
     */
    bool reloadConfigs();

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
     * @brief process config message from NetMsgType::SNPING packet.
     * @param snode Service node to process config from
     * @return
     */
    bool processConfigMessage(const sn::ServiceNode & snode);

    /**
     * Returns true if the specified service node matches all the criteria required for querying.
     * @param snode
     * @param settings
     * @param command
     * @param service
     * @param parameterCount
     * @return
     */
    bool snodeMatchesCriteria(const sn::ServiceNode & snode, xrouter::XRouterSettingsPtr settings,
            enum XRouterCommand command, const std::string & service, const int & parameterCount);

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
#ifndef ENABLE_WALLET
        return "";
#else
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
#endif // ENABLE_WALLET
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
     * @brief Serialize configuration.
     * @param cfg XRouter settings obj
     * @return
     */
    std::string parseConfig(XRouterSettingsPtr cfg);

    bool hasConfig(const NodeAddr & node) {
        LOCK(mu);
        return snodeConfigs.count(node);
    }

    XRouterSettingsPtr getConfig(const NodeAddr & node) {
        LOCK(mu);
        if (snodeConfigs.count(node))
            return snodeConfigs[node].first;
        return nullptr;
    }

    bool rateLimitExceeded(const NodeAddr & node, const std::string & service,
                           std::chrono::time_point<std::chrono::system_clock> lastRequest, int rateLimit) {
        return queryMgr.rateLimitExceeded(node, service, lastRequest, rateLimit);
    }

    void updateSentRequest(const NodeAddr & node, const std::string & command) {
        return queryMgr.updateSentRequest(node, command);
    }

    std::chrono::time_point<std::chrono::system_clock> getLastRequest(const NodeAddr & node, const std::string & command) {
        return queryMgr.getLastRequest(node, command);
    }

    int getScore(const NodeAddr & node) {
        return queryMgr.getScore(node);
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

    bool bestNode(const NodeAddr & a, const NodeAddr & b, const XRouterCommand & command, const std::string & service) {
        const auto & a_score = queryMgr.getScore(a);
        const auto & b_score = queryMgr.getScore(b);
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

    std::map<NodeAddr, std::pair<XRouterSettingsPtr, sn::ServiceNode::Tier>> getConfigs() {
        LOCK(mu);
        return snodeConfigs;
    }
    void addQuery(const std::string & queryId, const NodeAddr & node) {
        LOCK(mu);
        if (!configQueries.count(queryId))
            configQueries[queryId] = std::set<NodeAddr>{node};
        else
            configQueries[queryId].insert(node);
    }
    void updateConfig(const sn::ServiceNode & snode, XRouterSettingsPtr & config) {
        if (snode.isNull())
            return;
        LOCK(mu);
        // Remove existing configs that are associated with the snode pubkey
        for(auto it = snodeConfigs.begin(); it != snodeConfigs.end(); ) {
            if (it->second.first->getSnodePubKey() == snode.getSnodePubKey())
                snodeConfigs.erase(it++);
            else
                it++;
        }
        snodeConfigs[snode.getHostPort()] = std::make_pair(config, snode.getTier());
    }
    bool needConfigUpdate(const NodeAddr & node, const bool & isServer = false) {
        const auto & service = XRouterCommand_ToString(xrGetConfig);
        return !queryMgr.rateLimitExceeded(node, service, queryMgr.getLastRequest(node, service),
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

    /**
     * Bans the snode if required.
     * @param node
     * @param score
     */
    void checkSnodeBan(const NodeAddr & node, int score);

    /**
     * Manage xrouter connections on blockchain network. (default ports)
     */
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

private:
    Mutex mu;

    XRouterSettingsPtr xrsettings;
    XRouterServerPtr server;

    std::map<std::string, std::set<NodeAddr> > configQueries;
    std::map<NodeAddr, std::pair<XRouterSettingsPtr, sn::ServiceNode::Tier>> snodeConfigs;
    std::map<std::string, NodeAddr> snodeDomains;

    boost::filesystem::path xrouterpath;
    bool xrouterIsReady{false};

    boost::thread_group requestHandlers;
    std::deque<std::shared_ptr<boost::asio::io_service> > ioservices;
    std::deque<std::shared_ptr<boost::asio::io_service::work> > ioworkers;

    // timer
    boost::asio::io_service timerIo;
    boost::thread timerThread;
    boost::asio::deadline_timer timer;

    // Key management
    std::vector<unsigned char> cpubkey;
    std::vector<unsigned char> cprivkey;

    QueryMgr queryMgr;
    PendingConnectionMgr pendingConnMgr;
    std::atomic<bool> stopped{false};
};

} // namespace xrouter

#endif // BLOCKNET_XROUTER_XROUTERAPP_H
