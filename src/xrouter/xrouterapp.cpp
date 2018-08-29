//*****************************************************************************
//*****************************************************************************

#include "xrouterapp.h"
#include "init.h"
#include "keystore.h"
#include "main.h"
#include "net.h"
#include "servicenodeconfig.h"
#include "servicenodeman.h"
#include "addrman.h"
#include "script/standard.h"
#include "wallet.h"
#include "bloom.h"

#include "xbridge/util/settings.h"
#include "xbridge/bitcoinrpcconnector.h"
#include "xrouterlogger.h"

#include "xrouterconnector.h"
#include "xrouterconnectorbtc.h"
#include "xrouterconnectoreth.h"

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"
#include <assert.h>

#include <boost/chrono/chrono.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/range/adaptor/map.hpp>
#include <iostream>
#include <sstream>
#include <vector>
#include <chrono>

#define TEST_RUN_ON_CLIENT 0
#define DEFAULT_TIMEOUT 20000

#ifdef _WIN32
#include <objbase.h>
    
static std::string generateUUID()
{
    GUID guid;
	CoCreateGuid(&guid);
    char guid_string[37];
    sprintf(guid_string,
          "%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
          guid.Data1, guid.Data2, guid.Data3,
          guid.Data4[0], guid.Data4[1], guid.Data4[2],
          guid.Data4[3], guid.Data4[4], guid.Data4[5],
          guid.Data4[6], guid.Data4[7]);
    return guid_string;
}
    
#else

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

static std::string generateUUID()
{
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    return boost::uuids::to_string(uuid);
}

#endif 

//*****************************************************************************
//*****************************************************************************
namespace xrouter
{   
boost::container::map<CNode*, double > App::snodeScore = boost::container::map<CNode*, double >();    

//*****************************************************************************
//*****************************************************************************
App::App()
    : server(new XRouterServer), queries()
{
}

//*****************************************************************************
//*****************************************************************************
App::~App()
{
    stop();
}

//*****************************************************************************
//*****************************************************************************
// static
App& App::instance()
{
    static App app;
    return app;
}

//*****************************************************************************
//*****************************************************************************
// static
bool App::isEnabled()
{
    // enabled by default
    return true;
}

bool App::init(int argc, char *argv[])
{
    // init xbridge settings
    Settings & s = settings();
    
    std::string path(GetDataDir(false).string());
    std::string xbridgepath = path + "/xbridge.conf";
    s.read(xbridgepath.c_str());
    s.parseCmdLine(argc, argv);
    LOG() << "Loading xbridge config from file " << xbridgepath;

    this->xrouterpath = path + "/xrouter.conf";
    LOG() << "Loading xrouter config from file " << xrouterpath;
    this->xrouter_settings.read(xrouterpath.c_str());
    this->xrouter_settings.loadPlugins();

    return true;
}

std::vector<std::string> App::getServicesList() 
{
    std::vector<std::string> result;
    int xrouter_on = xrouter_settings.get<int>("Main.xrouter", 0);
    if (!xrouter_on)
        return result;
    result.push_back("XRouter");
    LOG() << "Adding XRouter to servicenode ping";
    for (std::string s : xrouter_settings.getPlugins()) {
        result.push_back("XRouter::" + s);
        LOG() << "Adding XRouter plugin " << s << " to servicenode ping";
    }
    return result;
}

static std::vector<pair<int, CServicenode> > getServiceNodes()
{
    int nHeight;
    {
        LOCK(cs_main);
        CBlockIndex* pindex = chainActive.Tip();
        if(!pindex) return std::vector<pair<int, CServicenode> >();
        nHeight = pindex->nHeight;
    }
    return mnodeman.GetServicenodeRanks(nHeight);
}

//*****************************************************************************
//*****************************************************************************
bool App::start()
{
    updateConfigs();
    bool res = server->start();
    //openConnections();
    return res;
}

void App::openConnections()
{
    //LOCK(cs_vNodes);
    LOG() << "Current peers count = " << vNodes.size();
    std::vector<pair<int, CServicenode> > vServicenodeRanks = getServiceNodes();
    BOOST_FOREACH (PAIRTYPE(int, CServicenode) & s, vServicenodeRanks) {
        if (!s.second.HasService("XRouter"))
            continue;
        
        // TODO: connect only to nodes with a specific service (specified in function parameters)
        std::string servicesList = s.second.GetServices();
        
        bool connected = false;
        for (CNode* pnode : vNodes) {
            if (s.second.addr.ToString() == pnode->addr.ToString()) {
                connected = true;
            }
        }
        
        if (!connected) {
            CAddress addr;
            CNode* res = ConnectNode(addr, s.second.addr.ToString().c_str());
            LOG() << "Trying to connect to " << CBitcoinAddress(s.second.pubKeyCollateralAddress.GetID()).ToString() << "; result=" << ((res == NULL) ? "fail" : "success");
            if (res)
                this->getXrouterConfig(res);
        }
    }

    LOG() << "Current peers count = " << vNodes.size();
}

std::string App::updateConfigs()
{
    int xrouter_on = xrouter_settings.get<int>("Main.xrouter", 0);
    if (!xrouter_on)
        return "XRouter is turned off. Please check that xrouter.conf is set up correctly.";
    
    std::vector<pair<int, CServicenode> > vServicenodeRanks = getServiceNodes();
    std::chrono::time_point<std::chrono::system_clock> time = std::chrono::system_clock::now();
    
    LOCK(cs_vNodes);
    for (CNode* pnode : vNodes) {
        if (snodeConfigs.count(pnode->addr.ToString())) {
            continue;
        }

        if (lastConfigUpdates.count(pnode)) {
            // There was a request to this node already, a new one will be sent only after 5 minutes
            std::chrono::time_point<std::chrono::system_clock> prev_time = lastConfigUpdates[pnode];
            std::chrono::system_clock::duration diff = time - prev_time;
            if (std::chrono::duration_cast<std::chrono::seconds>(diff) < std::chrono::seconds(300)) 
                continue;
        }
         
        if (TEST_RUN_ON_CLIENT) {
            std::string uuid = this->getXrouterConfig(pnode);
            LOG() << "Getting config from node " << pnode->addrName << " request id = " << uuid;
            lastConfigUpdates[pnode] = time;
            continue;
        }
        BOOST_FOREACH (PAIRTYPE(int, CServicenode) & s, vServicenodeRanks) {
            if (s.second.addr.ToString() == pnode->addr.ToString()) {
                // This node is a service node
                std::string uuid = this->getXrouterConfig(pnode);
                LOG() << "Getting config from node " << pnode->addrName << " request id = " << uuid;
                lastConfigUpdates[pnode] = time;
            }
        }
    }
    
    BOOST_FOREACH (PAIRTYPE(int, CServicenode) & s, vServicenodeRanks) {
        bool found = false;
        for (CNode* pnode : vNodes) {
            if (s.second.addr.ToString() == pnode->addr.ToString()) {
                found = true;
                break;
            }
        }
        
        // TODO: this code needs revision
        break;
        
        if (!found) {
            LOG() << "Broadcasting request for config of snode " << s.second.addr.ToString();
            for (CNode* pnode : vNodes) {
                if (lastConfigUpdates.count(pnode)) {
                    // There was a request to this node already, a new one will be sent only after 5 minutes
                    std::chrono::time_point<std::chrono::system_clock> prev_time = lastConfigUpdates[pnode];
                    std::chrono::system_clock::duration diff = time - prev_time;
                    if (std::chrono::duration_cast<std::chrono::seconds>(diff) < std::chrono::seconds(300)) 
                        continue;
                }
                this->getXrouterConfig(pnode, s.second.addr.ToString());
                break;
            }
        }
    }
    
    return "Config requests have been sent";
}

std::string App::printConfigs()
{
    Array result;
    
    for (const auto& it : this->snodeConfigs) {
        Object val;
        val.emplace_back("node", it.first);
        val.emplace_back("config", it.second.rawText());
        result.push_back(Value(val));
    }
    
    return json_spirit::write_string(Value(result), true);
}

//*****************************************************************************
//*****************************************************************************
bool App::stop()
{
    return true;
}
 
//*****************************************************************************
//*****************************************************************************

std::string App::sendPacketAndWait(const XRouterPacketPtr & packet, std::string id, std::string currency, int confirmations)
{
    Object error;
    boost::shared_ptr<boost::mutex> m(new boost::mutex());
    boost::shared_ptr<boost::condition_variable> cond(new boost::condition_variable());
    boost::mutex::scoped_lock lock(*m);
    int timeout = this->xrouter_settings.get<int>("Main.wait", DEFAULT_TIMEOUT);
    LOG() << "Sending query " << id;
    queriesLocks[id] = std::pair<boost::shared_ptr<boost::mutex>, boost::shared_ptr<boost::condition_variable> >(m, cond);
    if (!sendPacketToServer(packet, confirmations, currency)) {
        error.emplace_back(Pair("error", "Could not find available nodes for your request"));
        return json_spirit::write_string(Value(error), true);
    }

    int confirmation_count = 0;
    while ((confirmation_count < confirmations) && cond->timed_wait(lock, boost::posix_time::milliseconds(timeout)))
        confirmation_count++;

    if(confirmation_count <= confirmations / 2) {
        error.emplace_back(Pair("error", "Failed to get response"));
        error.emplace_back(Pair("uuid", id));
        return json_spirit::write_string(Value(error), true);
    }
    else
    {
        for (unsigned int i = 0; i < queries[id].size(); i++)
        {
            std::string cand = queries[id][i];
            int cnt = 0;
            for (unsigned int j = 0; j < queries[id].size(); j++)
            {
                if (queries[id][j] == cand)
                {
                    cnt++;
                    if (cnt > confirmations / 2)
                        return cand;
                }
            }
        }

        error.emplace_back(Pair("error", "No consensus between responses"));
        return json_spirit::write_string(Value(error), true);
    }
}

std::vector<CNode*> App::getAvailableNodes(const XRouterPacketPtr & packet, std::string wallet)
{
    // Send only to the service nodes that have the required wallet
    std::vector<pair<int, CServicenode> > vServicenodeRanks = getServiceNodes();

    openConnections();
    
    std::vector<CNode*> selectedNodes;
    
    LOCK(cs_vNodes);
    BOOST_FOREACH(const std::string key, snodeConfigs | boost::adaptors::map_keys)
    {
        XRouterSettings settings = snodeConfigs[key];
        if (!settings.walletEnabled(wallet))
            continue;
        if (!settings.isAvailableCommand(packet->command(), wallet))
            continue;
        
        CNode* res = NULL;
        for (CNode* pnode : vNodes) {
            if (key == pnode->addr.ToString()) {
                // This node is running xrouter
                res = pnode;
                break;
            }
        }
        
        if (!res) {
            CAddress addr;
            res = ConnectNode(addr, key.c_str());
        }
        
        if (!res)
            continue;
        
        std::chrono::time_point<std::chrono::system_clock> time = std::chrono::system_clock::now();
        std::string keystr = wallet + "::" + XRouterCommand_ToString(packet->command());
        double timeout = settings.getCommandTimeout(packet->command(), wallet);
        if (lastPacketsSent.count(res)) {
            if (lastPacketsSent[res].count(keystr)) {
                std::chrono::time_point<std::chrono::system_clock> prev_time = lastPacketsSent[res][keystr];
                std::chrono::system_clock::duration diff = time - prev_time;
                if (std::chrono::duration_cast<std::chrono::milliseconds>(diff) < std::chrono::milliseconds((int)(timeout * 1000))) {
                    continue;
                }
            }
        }
        
        selectedNodes.push_back(res);
    }
    
    for (CNode* node: selectedNodes) {
        if (!snodeScore.count(node))
            snodeScore[node] = 0;
    }
    
    std::sort(selectedNodes.begin(), selectedNodes.end(), cmpNodeScore);
    
    return selectedNodes;
}

CNode* App::getNodeForService(std::string name)
{
    // Send only to the service nodes that have the required wallet
    std::vector<pair<int, CServicenode> > vServicenodeRanks = getServiceNodes();
    openConnections();
    
    if (name.find("/") != string::npos) {
        std::vector<std::string> parts;
        boost::split(parts, name, boost::is_any_of("/"));
        std::string domain = parts[0];
        std::string name_part = parts[1];
        if (snodeDomains.count(domain)) {
            XRouterSettings settings = snodeConfigs[snodeDomains[domain]];
            if (!settings.hasPlugin(name_part))
                return NULL;
            
            CNode* res = NULL;
            for (CNode* pnode : vNodes) {
                if (snodeDomains[domain] == pnode->addr.ToString()) {
                    // This node is a running xrouter
                    res = pnode;
                    break;
                }
            }
            
            if (!res)
                return NULL;
            
            std::chrono::time_point<std::chrono::system_clock> time = std::chrono::system_clock::now();
            double timeout = settings.getPluginSettings(name).get<double>("timeout", -1.0);
            if (lastPacketsSent.count(res)) {
                if (lastPacketsSent[res].count(name)) {
                    std::chrono::time_point<std::chrono::system_clock> prev_time = lastPacketsSent[res][name];
                    std::chrono::system_clock::duration diff = time - prev_time;
                    if (std::chrono::duration_cast<std::chrono::milliseconds>(diff) < std::chrono::milliseconds((int)(timeout * 1000))) {
                        return NULL;
                    }
                }
            }
            
            return res;
        } else {
            return NULL;
        }
    }
    
    LOCK(cs_vNodes);
    BOOST_FOREACH(const std::string key, snodeConfigs | boost::adaptors::map_keys)
    {
        XRouterSettings settings = snodeConfigs[key];
        if (!settings.hasPlugin(name))
            continue;
        
        CNode* res = NULL;
        bool found = false;
        for (CNode* pnode : vNodes) {
            if (key == pnode->addr.ToString()) {
                // This node is a running xrouter
                if (found) {
                    LOG() << "Ambiguous plugin call";
                    return NULL;
                }
                res = pnode;
                found = true;
            }
        }
        
        if (!res) {
            CAddress addr;
            res = ConnectNode(addr, key.c_str());
        }
        
        if (!res)
            continue;
        
        std::chrono::time_point<std::chrono::system_clock> time = std::chrono::system_clock::now();
        double timeout = settings.getPluginSettings(name).get<double>("timeout", -1.0);
        if (lastPacketsSent.count(res)) {
            if (lastPacketsSent[res].count(name)) {
                std::chrono::time_point<std::chrono::system_clock> prev_time = lastPacketsSent[res][name];
                std::chrono::system_clock::duration diff = time - prev_time;
                if (std::chrono::duration_cast<std::chrono::milliseconds>(diff) < std::chrono::milliseconds((int)(timeout * 1000))) {
                    continue;
                }
            }
        }
        
        return res;
    }
    
    return NULL;
}

//*****************************************************************************
//*****************************************************************************
bool App::sendPacketToServer(const XRouterPacketPtr& packet, int confirmations, std::string wallet)
{
    // Send only to the service nodes that have the required wallet
    std::vector<CNode*> selectedNodes = getAvailableNodes(packet, wallet);
    
    if ((int)selectedNodes.size() < confirmations)
        return false;
    
    int sent = 0;
    for (CNode* pnode : selectedNodes) {
        pnode->PushMessage("xrouter", packet->body());
        sent++;
        
        std::chrono::time_point<std::chrono::system_clock> time = std::chrono::system_clock::now();
        std::string keystr = wallet + "::" + XRouterCommand_ToString(packet->command());
        if (!lastPacketsSent.count(pnode)) {
            lastPacketsSent[pnode] = boost::container::map<std::string, std::chrono::time_point<std::chrono::system_clock> >();
        }
        lastPacketsSent[pnode][keystr] = time;
        LOG() << "Sent message to node " << pnode->addrName;
        if (sent == confirmations)
            return true;
    }
    
    return false;
}

std::string App::processGetXrouterConfig(XRouterSettings cfg, std::string addr) {
    Object result;
    result.emplace_back(Pair("config", cfg.rawText()));
    Object plugins;
    for (std::string s : cfg.getPlugins())
        plugins.emplace_back(s, cfg.getPluginSettings(s).rawText());
    result.emplace_back(Pair("plugins", plugins));
    result.emplace_back(Pair("addr", addr));
    LOG() << "Sending config " << json_spirit::write_string(Value(result), true);
    return json_spirit::write_string(Value(result), true);
}

//*****************************************************************************
//*****************************************************************************
bool App::processReply(XRouterPacketPtr packet) {
    uint32_t offset = 0;

    std::string uuid((const char *)packet->data()+offset);
    offset += uuid.size() + 1;
    std::string reply((const char *)packet->data()+offset);
    offset += reply.size() + 1;

    LOG() << "Got reply to query " << uuid;
    
    // check uuid is in queriesLock keys
    if (!queriesLocks.count(uuid))
        return true;
    
    LOG() << reply;
    boost::mutex::scoped_lock l(*queriesLocks[uuid].first);
    if (!queries.count(uuid))
        queries[uuid] = vector<std::string>();
    queries[uuid].push_back(reply);
    queriesLocks[uuid].second->notify_all();
    return true;
}

bool App::processConfigReply(XRouterPacketPtr packet) {
    uint32_t offset = 0;

    std::string uuid((const char *)packet->data()+offset);
    offset += uuid.size() + 1;
    std::string reply((const char *)packet->data()+offset);
    offset += reply.size() + 1;

    LOG() << "Got reply to query " << uuid;
    
    
    LOG() << "Got xrouter config from node " << configQueries[uuid]->addrName;
    LOG() << reply;
    Value reply_val;
    read_string(reply, reply_val);
    Object reply_obj = reply_val.get_obj();
    std::string config = find_value(reply_obj, "config").get_str();
    Object plugins  = find_value(reply_obj, "plugins").get_obj();
    
    XRouterSettings settings;
    settings.read(config);
    
    for (Object::size_type i = 0; i != plugins.size(); i++ ) {
        XRouterPluginSettings psettings;
        psettings.read(std::string(plugins[i].value_.get_str()));
        settings.addPlugin(std::string(plugins[i].name_), psettings);
    }
     if (configQueries.count(uuid)) {   
        snodeConfigs[configQueries[uuid]->addr.ToString()] = settings;
        snodeDomains[configQueries[uuid]->addr.ToString()] = configQueries[uuid]->addr.ToString();
        if (settings.get<std::string>("domain", "") != "") {
            snodeDomains[settings.get<std::string>("domain")] = configQueries[uuid]->addr.ToString();
        }
        return true;
    } else {
        std::string addr = find_value(reply_obj, "addr").get_str();
        snodeConfigs[addr] = settings;
        snodeDomains[addr] = addr;
        if (settings.get<std::string>("domain", "") != "") {
            snodeDomains[settings.get<std::string>("domain")] = addr;
        }
        return true;
    }
}

//*****************************************************************************
//*****************************************************************************
void App::onMessageReceived(CNode* node, const std::vector<unsigned char>& message, CValidationState& state)
{
    LOG() << "Received xrouter packet";

    // If Main.xrouter == 0, xrouter is turned off on this snode
    int xrouter_on = xrouter_settings.get<int>("Main.xrouter", 0);
    if (!xrouter_on)
        return;
    
    XRouterPacketPtr packet(new XRouterPacket);
    if (!packet->copyFrom(message)) {
        LOG() << "incorrect packet received " << __FUNCTION__;
        state.DoS(10, error("XRouter: invalid packet received"), REJECT_INVALID, "xrouter-error");
        return;
    }

    std::string reply;
    LOG() << "XRouter command: " << std::string(XRouterCommand_ToString(packet->command()));
    
    std::chrono::time_point<std::chrono::system_clock> time = std::chrono::system_clock::now();
    if (packet->command() == xrGetXrouterConfig) {
        uint32_t offset = 36;
        std::string uuid((const char *)packet->data()+offset);
        offset += uuid.size() + 1;
        std::string addr((const char *)packet->data()+offset);
        XRouterSettings cfg;
        if (addr == "self")
            cfg = this->xrouter_settings;
        else {
            if (!this->snodeConfigs.count(addr))
                return;
            else
                cfg = this->snodeConfigs[addr];
        }
        
        reply = processGetXrouterConfig(cfg, addr);
        if (lastConfigQueries.count(node)) {
            std::chrono::time_point<std::chrono::system_clock> prev_time = lastConfigQueries[node];
            std::chrono::system_clock::duration diff = time - prev_time;
            if (std::chrono::duration_cast<std::chrono::seconds>(diff) < std::chrono::seconds(10))
                state.DoS(10, error("XRouter: too many config requests"), REJECT_INVALID, "xrouter-error");
            lastConfigQueries[node] = time;
        } else {
            lastConfigQueries[node] = time;
        }
        
        XRouterPacketPtr rpacket(new XRouterPacket(xrConfigReply));
        rpacket->append(uuid);
        rpacket->append(reply);
        node->PushMessage("xrouter", rpacket->body());
        return;
    } else if (packet->command() == xrReply) {
        processReply(packet);
        return;
    } else if (packet->command() == xrConfigReply) {
        processConfigReply(packet);
        return;
    } else {
        server->onMessageReceived(node, packet, state);
        return;
    }
}

//*****************************************************************************
//*****************************************************************************
static bool satisfyBlockRequirement(uint256& txHash, uint32_t& vout, CKey& key)
{
    if (!pwalletMain) {
        return false;
    }
    for (auto& addressCoins : pwalletMain->AvailableCoinsByAddress()) {
        for (auto& output : addressCoins.second) {
            if (output.Value() >= MIN_BLOCK) {
                CKeyID keyID;
                if (!addressCoins.first.GetKeyID(keyID)) {
                    //std::cerr << "GetKeyID failed\n";
                    continue;
                }
                if (!pwalletMain->GetKey(keyID, key)) {
                    //std::cerr << "GetKey failed\n";
                    continue;
                }
                txHash = output.tx->GetHash();
                vout = output.i;
                return true;
            }
        }
    }
    return false;
}

//*****************************************************************************
//*****************************************************************************
std::string App::xrouterCall(enum XRouterCommand command, const std::string & currency, std::string param1, std::string param2, std::string confirmations)
{
    int xrouter_on = xrouter_settings.get<int>("Main.xrouter", 0);
    if (!xrouter_on)
        return "XRouter is turned off. Please check that xrouter.conf is set up correctly.";
    
    updateConfigs();
    
    XRouterPacketPtr packet(new XRouterPacket(command));

    uint256 txHash;
    uint32_t vout = 0;
    CKey key;
    if ((command != xrGetXrouterConfig) && !satisfyBlockRequirement(txHash, vout, key)) {
        LOG() << "Minimum block requirement not satisfied";
        return "Minimum block requirement not satisfied. Make sure that your wallet is unlocked.";
    }

    std::string id = generateUUID();

    packet->append(txHash.begin(), 32);
    packet->append(vout);
    packet->append(id);
    packet->append(currency);
    if (!param1.empty())
        packet->append(param1);
    if (!param2.empty())
        packet->append(param2);
    packet->sign(key);

    if (!confirmations.empty())
        return sendPacketAndWait(packet, id, currency, std::stoi(confirmations));
    else
        return sendPacketAndWait(packet, id, currency);
}

std::string App::getBlockCount(const std::string & currency, const std::string & confirmations)
{
    return this->xrouterCall(xrGetBlockCount, currency, "", "", confirmations);
}

std::string App::getBlockHash(const std::string & currency, const std::string & blockId, const std::string & confirmations)
{
    return this->xrouterCall(xrGetBlockHash, currency, blockId, "", confirmations);
}

std::string App::getBlock(const std::string & currency, const std::string & blockHash, const std::string & confirmations)
{
    return this->xrouterCall(xrGetBlock, currency, blockHash, "", confirmations);
}

std::string App::getTransaction(const std::string & currency, const std::string & hash, const std::string & confirmations)
{
    return this->xrouterCall(xrGetTransaction, currency, hash, "", confirmations);
}

std::string App::getAllBlocks(const std::string & currency, const std::string & number, const std::string & confirmations)
{
    return this->xrouterCall(xrGetAllBlocks, currency, number, "", confirmations);
}

std::string App::getAllTransactions(const std::string & currency, const std::string & account, const std::string & number, const std::string & confirmations)
{
    return this->xrouterCall(xrGetAllTransactions, currency, account, number, confirmations);
}

std::string App::getBalance(const std::string & currency, const std::string & account, const std::string & confirmations)
{
    return this->xrouterCall(xrGetBalance, currency, account, "", confirmations);
}

std::string App::getBalanceUpdate(const std::string & currency, const std::string & account, const std::string & number, const std::string & confirmations)
{
    return this->xrouterCall(xrGetBalanceUpdate, currency, account, number, confirmations);
}

std::string App::getTransactionsBloomFilter(const std::string & currency, const std::string & number, const std::string & filter, const std::string & confirmations)
{
    return this->xrouterCall(xrGetTransactionsBloomFilter, currency, number, filter, confirmations);
}

std::string App::getReply(const std::string & id)
{
    Object result;

    if(queries[id].size() == 0) {
        result.emplace_back(Pair("error", "No replies found"));
        result.emplace_back(Pair("uuid", id));
        return json_spirit::write_string(Value(result), true);
    } else {
        for (unsigned int i = 0; i < queries[id].size(); i++) {
            std::string cand = queries[id][i];
            result.emplace_back(Pair("reply" + std::to_string(i+1), cand));
        }

        return json_spirit::write_string(Value(result), true);
    }
}

std::string App::sendTransaction(const std::string & currency, const std::string & transaction)
{
    int xrouter_on = xrouter_settings.get<int>("Main.xrouter", 0);
    if (!xrouter_on)
        return "XRouter is turned off. Please check that xrouter.conf is set up correctly.";
    
    updateConfigs();
    
    XRouterPacketPtr packet(new XRouterPacket(xrSendTransaction));

    uint256 txHash;
    uint32_t vout = 0;
    CKey key;
    if (!satisfyBlockRequirement(txHash, vout, key)) {
        return "Minimum block requirement not satisfied. Make sure that your wallet is unlocked.";
    }

    std::string id = generateUUID();

    packet->append(txHash.begin(), 32);
    packet->append(vout);
    packet->append(id);
    packet->append(currency);
    packet->append(transaction);
    packet->sign(key);
    
    boost::shared_ptr<boost::mutex> m(new boost::mutex());
    boost::shared_ptr<boost::condition_variable> cond(new boost::condition_variable());
    boost::mutex::scoped_lock lock(*m);
    queriesLocks[id] = std::pair<boost::shared_ptr<boost::mutex>, boost::shared_ptr<boost::condition_variable> >(m, cond);

    std::vector<unsigned char> msg;
    msg.insert(msg.end(), packet->body().begin(), packet->body().end());

    std::vector<CNode*> selectedNodes = getAvailableNodes(packet, currency);
    
    if ((int)selectedNodes.size() == 0)
        return "No available nodes";
    
    for (CNode* pnode : selectedNodes) {
        pnode->PushMessage("xrouter", msg);
        if (cond->timed_wait(lock, boost::posix_time::milliseconds(3000))) {
            std::string reply = queries[id][0];
            Value reply_val;
            read_string(reply, reply_val);
            Object reply_obj = reply_val.get_obj();
            const Value & errorcode  = find_value(reply_obj, "errorcode");
            if (errorcode.type() != null_type)
                if (errorcode.get_int() < 0) {
                    // Try sending to another node
                    queries[id].clear();
                    continue;
                }
        
            return reply;
        }
    }
    
    return "No available nodes";
}

std::string App::sendCustomCall(const std::string & name, std::vector<std::string> & params)
{
    int xrouter_on = xrouter_settings.get<int>("Main.xrouter", 0);
    if (!xrouter_on)
        return "XRouter is turned off. Please check that xrouter.conf is set up correctly.";
    
    if (this->xrouter_settings.hasPlugin(name)) {
        // Run the plugin locally
        return server->processCustomCall(name, params);
    }
    
    updateConfigs();
    
    XRouterPacketPtr packet(new XRouterPacket(xrCustomCall));

    uint256 txHash;
    uint32_t vout = 0;
    CKey key;
    if (!satisfyBlockRequirement(txHash, vout, key)) {
        return "Minimum block requirement not satisfied. Make sure that your wallet is unlocked.";
    }
    
    std::string id = generateUUID();

    boost::shared_ptr<boost::mutex> m(new boost::mutex());
    boost::shared_ptr<boost::condition_variable> cond(new boost::condition_variable());
    boost::mutex::scoped_lock lock(*m);
    queriesLocks[id] = std::pair<boost::shared_ptr<boost::mutex>, boost::shared_ptr<boost::condition_variable> >(m, cond);
    
    CNode* pnode = getNodeForService(name);
    if (!pnode)
        return "No available nodes";
    
    unsigned int min_count = snodeConfigs[pnode->addr.ToString()].getPluginSettings(name).getMinParamCount();
    if (params.size() < min_count) {
        return "Not enough plugin parameters";
    }
    
    unsigned int max_count = snodeConfigs[pnode->addr.ToString()].getPluginSettings(name).getMaxParamCount();
    if (params.size() > max_count) {
        return "Too many plugin parameters";
    }
    
    std::string strtxid;
    std::string dest = getPaymentAddress(pnode);
    xbridge::rpc::storeDataIntoBlockchain(std::vector<unsigned char>(dest.begin(), dest.end()), snodeConfigs[pnode->addr.ToString()].getPluginSettings(name).getFee(), std::vector<unsigned char>(), strtxid);
    
    packet->append(txHash.begin(), 32);
    packet->append(vout);
    packet->append(id);
    packet->append(name);
    packet->append(strtxid);
    for (std::string param: params)
        packet->append(param);
    packet->sign(key);
    std::vector<unsigned char> msg;
    msg.insert(msg.end(), packet->body().begin(), packet->body().end());
    
    Object result;
    
    std::chrono::time_point<std::chrono::system_clock> time = std::chrono::system_clock::now();
    if (!lastPacketsSent.count(pnode)) {
        lastPacketsSent[pnode] = boost::container::map<std::string, std::chrono::time_point<std::chrono::system_clock> >();
    }
    lastPacketsSent[pnode][name] = time;
    
    pnode->PushMessage("xrouter", msg);
    int timeout = this->xrouter_settings.get<int>("Main.wait", DEFAULT_TIMEOUT);
    if (cond->timed_wait(lock, boost::posix_time::milliseconds(timeout))) {
        std::string reply = queries[id][0];
        return reply;
    }
    
    result.emplace_back(Pair("error", "Failed to get response"));
    result.emplace_back(Pair("uuid", id));
    return json_spirit::write_string(Value(result), true);
}


std::string App::getPaymentAddress(CNode* node)
{
    std::vector<pair<int, CServicenode> > vServicenodeRanks = getServiceNodes();
    BOOST_FOREACH (PAIRTYPE(int, CServicenode) & s, vServicenodeRanks) {
        for (CNode* pnode : vNodes) {
            if (s.second.addr.ToString() == pnode->addr.ToString()) {
                return CBitcoinAddress(s.second.pubKeyCollateralAddress.GetID()).ToString();
            }
        }
    }
    
    return "";
}

std::string App::getXrouterConfig(CNode* node, std::string addr) {
    XRouterPacketPtr packet(new XRouterPacket(xrGetXrouterConfig));

    uint256 txHash;
    uint32_t vout = 0;

    std::string id = generateUUID();
    packet->append(txHash.begin(), 32);
    packet->append(vout);
    packet->append(id);
    packet->append(addr);
    
    std::vector<unsigned char> msg;
    msg.insert(msg.end(), packet->body().begin(), packet->body().end());
    
    this->configQueries[id] = node;
    node->PushMessage("xrouter", msg);
    return id;
}

std::string App::getXrouterConfigSync(CNode* node) {
    XRouterPacketPtr packet(new XRouterPacket(xrGetXrouterConfig));

    uint256 txHash;
    uint32_t vout = 0;

    std::string id = generateUUID();

    packet->append(txHash.begin(), 32);
    packet->append(vout);
    packet->append(id);

    boost::shared_ptr<boost::mutex> m(new boost::mutex());
    boost::shared_ptr<boost::condition_variable> cond(new boost::condition_variable());
    boost::mutex::scoped_lock lock(*m);
    queriesLocks[id] = std::pair<boost::shared_ptr<boost::mutex>, boost::shared_ptr<boost::condition_variable> >(m, cond);

    std::vector<unsigned char> msg;
    msg.insert(msg.end(), packet->body().begin(), packet->body().end());
    node->PushMessage("xrouter", msg);
    int timeout = this->xrouter_settings.get<int>("Main.wait", DEFAULT_TIMEOUT);
    if (!cond->timed_wait(lock, boost::posix_time::milliseconds(timeout)))
        return "Could not get XRouter config";

    std::string reply = queries[id][0];
    XRouterSettings settings;
    settings.read(reply);
    this->snodeConfigs[node->addr.ToString()] = settings;
    return reply;
}

void App::reloadConfigs() {
    LOG() << "Reloading xrouter config from file " << xrouterpath;
    this->xrouter_settings.read(xrouterpath.c_str());
    this->xrouter_settings.loadPlugins();
}

std::string App::getStatus() {
    Object result;
    result.emplace_back(Pair("enabled", xrouter_settings.get<int>("Main.xrouter", 0) != 0));
    result.emplace_back(Pair("config", this->xrouter_settings.rawText()));
    Object myplugins;
    for (std::string s : this->xrouter_settings.getPlugins())
        myplugins.emplace_back(s, this->xrouter_settings.getPluginSettings(s).rawText());
    result.emplace_back(Pair("plugins", myplugins));
    
    Array nodes;
    for (auto& it : this->snodeConfigs) {
        Object val;
        val.emplace_back("node", it.first);
        val.emplace_back("config", it.second.rawText());
        Object plugins;
        for (std::string s : it.second.getPlugins())
            plugins.emplace_back(s, it.second.getPluginSettings(s).rawText());
        val.emplace_back(Pair("plugins", plugins));
        nodes.push_back(Value(val));
    }
    
    result.emplace_back(Pair("nodes", nodes));
    
    return json_spirit::write_string(Value(result), true);
}

} // namespace xrouter
