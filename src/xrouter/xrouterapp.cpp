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
#include "rpcserver.h"

#include "xbridge/util/settings.h"
#include "xbridge/bitcoinrpcconnector.h"
#include "xrouterlogger.h"
#include "xrouterutils.h"
#include "xroutererror.h"

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

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
#include <assert.h>

//*****************************************************************************
//*****************************************************************************
namespace xrouter
{   

std::map<std::string, double> App::snodeScore;

//*****************************************************************************
//*****************************************************************************
App::App()
    : server(new XRouterServer)
{
}

//*****************************************************************************
//*****************************************************************************
App::~App()
{
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
    // Disabled by default
    return GetBoolArg("-xrouter", false);
}

bool App::init(int argc, char *argv[])
{
    if (!isEnabled())
        return true;
    std::string path(GetDataDir(false).string());
    this->xrouterpath = path + "/xrouter.conf";
    LOG() << "Loading xrouter config from file " << xrouterpath;
    this->xrouter_settings.read(xrouterpath.c_str());
    this->xrouter_settings.loadPlugins();

    // init xbridge settings
    Settings & s = settings();
    
    std::string xbridgepath = path + "/xbridge.conf";
    s.read(xbridgepath.c_str());
    s.parseCmdLine(argc, argv);
    LOG() << "Loading xbridge config from file " << xbridgepath;
    
    this->loadPaymentChannels();
    
    return true;
}

std::vector<std::string> App::getServicesList() 
{
    // We append "XRouter" if XRouter is activated at all, and "XRouter::service_name" for each activated plugin
    std::vector<std::string> result;
    if (!isEnabled())
        return result;
    result.push_back("XRouter");
    LOG() << "Adding XRouter to servicenode ping";
    for (const std::string & s : xrouter_settings.getPlugins()) {
        result.push_back("XRouter::" + s);
        LOG() << "Adding XRouter plugin " << s << " to servicenode ping";
    }
    return result;
}

static std::vector<CServicenode> getServiceNodes()
{
    return mnodeman.GetCurrentList();
}

//*****************************************************************************
//*****************************************************************************
bool App::start()
{
    if (!isEnabled())
        return true;
    
    updateConfigs();
    bool res = server->start();
    return res;
}

void App::openConnections(const std::string wallet, const std::string plugin)
{
    std::vector<CNode*> nodes;
    {
        LOCK(cs_vNodes);
        nodes = vNodes;
    }
    LOG() << "Current peers count = " << nodes.size();

    // Build node cache
    std::map<std::string, CNode*> nodec;
    for (auto & pnode : nodes)
        nodec[pnode->addr.ToString()] = pnode;

    // Open connections to all service nodes that are not already our peers
    std::vector<CServicenode> snodes = getServiceNodes();
    for (auto & s : snodes) {
        if (!s.HasService("XRouter"))
            continue;
        
        if (!wallet.empty())
            if (!s.HasService(wallet))
                continue;
        
        if (!plugin.empty())
            if (!s.HasService("XRouter::" + plugin))
                continue;

        const auto & snodeAddr = s.addr.ToString();

        // Do not connect if already a peer
        if (nodec.count(snodeAddr))
            continue;

        // Connect to snode
        CAddress addr;
        if (OpenNetworkConnection(addr, nullptr, snodeAddr.c_str())) { // OpenNetworkConnection filters out bad nodes (banned, etc)
            CNode *res = FindNode((CNetAddr)addr);
            if (res)
                LOG() << "Connected to servicenode " << CBitcoinAddress(s.pubKeyCollateralAddress.GetID()).ToString();
        } else LOG() << "Failed to connect to servicenode " << CBitcoinAddress(s.pubKeyCollateralAddress.GetID()).ToString();
    }

    LOG() << "Current peers count = " << vNodes.size();
}

std::string App::updateConfigs()
{
    if (!isEnabled())
        return "XRouter is turned off. Please set 'xrouter=1' in blocknetdx.conf to run XRouter.";
    
    std::vector<CServicenode> vServicenodes = getServiceNodes();
    std::vector<CNode*> nodes;
    {
        LOCK(cs_vNodes);
        nodes = vNodes;
    }

    // Build snode cache
    std::map<std::string, CServicenode> snodes;
    for (CServicenode & s : vServicenodes)
        snodes[s.addr.ToString()] = s;

    // Query servicenodes that haven't had configs updated recently
    for (CNode* pnode : nodes) {
        const auto & nodeAddr = pnode->addr.ToString();
        if (!snodes.count(nodeAddr) || !pnode->fSuccessfullyConnected || pnode->fDisconnect) // skip non-servicenodes and disconnecting nodes
            continue;

        std::chrono::time_point<std::chrono::system_clock> time = std::chrono::system_clock::now();
        if (lastConfigUpdates.count(nodeAddr)) {
            const auto prev_time = lastConfigUpdates[nodeAddr];
            std::chrono::system_clock::duration diff = time - prev_time;
            // There was a request to this node already, a new one will be sent only after 5 minutes
            if (std::chrono::duration_cast<std::chrono::seconds>(diff) < std::chrono::seconds(300)) 
                continue;
        }
         
        if (debug_on_client()) {
            std::string uuid = this->getXrouterConfig(pnode);
            LOG() << "Getting config from node " << nodeAddr  << " request id = " << uuid;
            lastConfigUpdates[nodeAddr] = time;
            continue;
        }

        // Request the config
        auto & snode = snodes[nodeAddr];
        lastConfigUpdates[nodeAddr] = time;
        std::string uuid = this->getXrouterConfig(pnode);
        LOG() << "Requesting config from snode " << CBitcoinAddress(snode.pubKeyCollateralAddress.GetID()).ToString() << " request id = " << uuid;
    }
    
    return "Config requests have been sent";
}

std::string App::printConfigs()
{
    Array result;
    
    for (const auto& it : this->snodeConfigs) {
        Object val;
        //val.emplace_back("node", it.first);
        val.emplace_back("config", it.second.rawText());
        result.push_back(Value(val));
    }
    
    return json_spirit::write_string(Value(result), true);
}

//*****************************************************************************
//*****************************************************************************
bool App::stop()
{
    if (!isEnabled())
        return false;
    server->closeAllPaymentChannels();
    return true;
}
 
//*****************************************************************************
//*****************************************************************************
std::vector<CNode*> App::getAvailableNodes(enum XRouterCommand command, std::string wallet, int confirmations)
{
    // Send only to the service nodes that have the required wallet
    std::vector<CServicenode> vServicenodes = getServiceNodes();

    openConnections(wallet);
    updateConfigs();
    
    std::vector<CNode*> selectedNodes;
    std::vector<CNode*> nodes;
    {
        LOCK(cs_vNodes);
        nodes = vNodes;
    }

    // Build snode cache
    std::map<std::string, CServicenode> snodes;
    for (CServicenode & s : vServicenodes)
        snodes[s.addr.ToString()] = s;

    // Build node cache
    std::map<std::string, CNode*> nodec;
    for (auto & pnode : nodes) {
        const auto addr = pnode->addr.ToString();
        if (snodes.count(addr))
            nodec[addr] = pnode;
    }

    double maxfee_d = xrouter_settings.getMaxFee(command, wallet);
    CAmount maxfee;
    if (maxfee_d >= 0)
        maxfee = to_amount(maxfee_d);
    else
        maxfee = -1;
    
    int supported = 0;
    int ready = 0;
    int below_maxfee = 0;
    for (const auto & item : snodeConfigs)
    {
        const auto & nodeAddr = item.first;
        XRouterSettings settings = snodeConfigs[nodeAddr];
        if (!settings.walletEnabled(wallet))
            continue;
        if (!settings.isAvailableCommand(command, wallet))
            continue;
        if (!snodes.count(nodeAddr)) // Ignore if not a snode
            continue;
        if (!snodes[nodeAddr].HasService(wallet)) // Ignore snodes that don't have the wallet
            continue;

        supported++;

        // This is the node whose config we are looking at now
        CNode *node = nullptr;
        if (nodec.count(nodeAddr))
            node = nodec[nodeAddr];

        // If the service node is not among peers, we try to connect to it right now
        if (!node) {
            CAddress addr;
            node = ConnectNode(addr, nodeAddr.c_str());
        }

        // Could not connect to service node
        if (!node)
            continue;

        const auto & snodeAddr = CBitcoinAddress(snodes[nodeAddr].pubKeyCollateralAddress.GetID()).ToString();

        if (maxfee >= 0) {
            CAmount fee = to_amount(settings.getCommandFee(command, wallet));
            if (fee > maxfee) {
                LOG() << "Skipping node " << snodeAddr << " because its fee=" << fee << " is higher than maxfee=" << maxfee;
                continue;
            }
        }

        below_maxfee++;

        std::chrono::time_point<std::chrono::system_clock> time = std::chrono::system_clock::now();
        const std::string commandStr(XRouterCommand_ToString(command));
        const auto & cmd = buildCommandKey(wallet, commandStr);
        double timeout = settings.getCommandTimeout(command, wallet);
        if (lastPacketsSent.count(nodeAddr) && lastPacketsSent[nodeAddr].count(cmd)) {
            auto prev_time = lastPacketsSent[nodeAddr][cmd];
            std::chrono::system_clock::duration diff = time - prev_time;
            const auto tout = static_cast<int>(timeout * 1000);
            if (std::chrono::duration_cast<std::chrono::milliseconds>(diff) < std::chrono::milliseconds(tout)) {
                LOG() << "Skipping node " << snodeAddr << " because not enough time passed since the last call";
                continue;
            }
        }
        
        ready++;
        selectedNodes.push_back(node);
    }
    
    for (CNode *node : selectedNodes) {
        const auto & addr = node->addr.ToString();
        if (!snodeScore.count(addr))
            snodeScore[addr] = 0;
    }
    
    std::sort(selectedNodes.begin(), selectedNodes.end(), cmpNodeScore);
    
    if (supported < confirmations)
        throw XRouterError("Could not find " + std::to_string(confirmations) + " Service Nodes supporting blockchain " + wallet, xrouter::UNSUPPORTED_BLOCKCHAIN);
    
    if (below_maxfee < confirmations)
        throw XRouterError("Could not find " + std::to_string(confirmations) + " Service Nodes with fee below maxfee.", xrouter::MAXFEE_TOO_LOW);
    
    if (ready < confirmations)
        throw XRouterError("Could not find " + std::to_string(confirmations) + " Service Nodes ready for query at the moment.", xrouter::NOT_ENOUGH_NODES);
    
    return selectedNodes;
}

CNode* App::getNodeForService(std::string name)
{
    openConnections();
    updateConfigs();

    double maxfee_d = xrouter_settings.getMaxFee(xrCustomCall, "");
    CAmount maxfee;
    if (maxfee_d >= 0)
        maxfee = to_amount(maxfee_d);
    else
        maxfee = -1;

    std::vector<CServicenode> vServicenodes = getServiceNodes();
    std::vector<CNode*> nodes;
    {
        LOCK(cs_vNodes);
        nodes = vNodes;
    }

    // Build snode cache
    std::map<std::string, CServicenode> snodes;
    for (CServicenode & s : vServicenodes)
        snodes[s.addr.ToString()] = s;

    // Build node cache
    std::map<std::string, CNode*> nodec;
    for (auto & pnode : nodes) {
        const auto addr = pnode->addr.ToString();
        if (snodes.count(addr))
            nodec[addr] = pnode;
    }

    CNode *res = nullptr;
    if (name.find("/") != string::npos) {
        std::vector<std::string> parts;
        boost::split(parts, name, boost::is_any_of("/"));
        std::string domain = parts[0];
        std::string name_part = parts[1];
        if (snodeDomains.count(domain) == 0)
            // Domain not found
            return nullptr;
        
        std::vector<CNode*> candidates;
        for (auto & item : nodec) {
            auto & pnode = item.second;
            XRouterSettings settings = snodeConfigs[snodeDomains[domain]];
            if (!settings.hasPlugin(name_part))
                continue;
            
            if (snodeDomains[domain] == pnode->addr.ToString()) {
                candidates.push_back(pnode);
            }
        }

        XRouterSettings settings = snodeConfigs[snodeDomains[domain]];
        
        if (candidates.empty())
            return nullptr;
        else if (candidates.size() == 1)
            res = candidates[0];
        else {
            // Perform verification check of domain names
            int best_block = -1;
            for (CNode* cand : candidates) {
                std::string addr = getPaymentAddress(cand);
                int block;

                std::string tx = settings.get<std::string>("Main.domain_tx", "");
                if (tx.empty())
                    continue;
                if (!verifyDomain(tx, domain, addr, block))
                    continue;

                if ((best_block < 0) || (block < best_block)) {
                    // TODO: what if both verification tx are in the same block?
                    best_block = block;
                    res = cand;
                }
            }
            
            if (!res)
                return nullptr;
        }

        const auto & nodeAddr = res->addr.ToString();
        
        std::chrono::time_point<std::chrono::system_clock> time = std::chrono::system_clock::now();
        double timeout = settings.getPluginSettings(name).get<double>("timeout", -1.0);
        if (lastPacketsSent.count(nodeAddr) && lastPacketsSent[nodeAddr].count(name)) {
            const auto prev_time = lastPacketsSent[nodeAddr][name];
            const auto tout = static_cast<int>(timeout * 1000);
            std::chrono::system_clock::duration diff = time - prev_time;
            if (std::chrono::duration_cast<std::chrono::milliseconds>(diff) < std::chrono::milliseconds(tout))
                return nullptr;
        }
        
        if (maxfee >= 0) {
            CAmount fee = to_amount(settings.getPluginSettings(name).getFee());
            if (fee > maxfee)
                return nullptr;
        }
        
        return res;
    }
    
    for (const auto & item : snodeConfigs)
    {
        const auto & nodeAddr = item.first;
        XRouterSettings settings = snodeConfigs[nodeAddr];
        if (!settings.hasPlugin(name))
            continue;
        if (!nodec.count(nodeAddr))
            continue; // not in snode list

        CNode *res = nodec[nodeAddr];
        if (!res) {
            CAddress addr;
            res = ConnectNode(addr, nodeAddr.c_str());
        }
        
        if (!res)
            continue;

        std::chrono::time_point<std::chrono::system_clock> time = std::chrono::system_clock::now();
        double timeout = settings.getPluginSettings(name).get<double>("timeout", -1.0);
        if (lastPacketsSent.count(nodeAddr) && lastPacketsSent[nodeAddr].count(name)) {
            const auto prev_time = lastPacketsSent[nodeAddr][name];
            const auto tout = static_cast<int>(timeout * 1000);
            std::chrono::system_clock::duration diff = time - prev_time;
            if (std::chrono::duration_cast<std::chrono::milliseconds>(diff) < std::chrono::milliseconds(tout))
                continue;
        }
        
        if (maxfee >= 0) {
            CAmount fee = to_amount(settings.getPluginSettings(name).getFee());
            if (fee > maxfee)
                continue;
        }
        
        return res;
    }
    
    return nullptr;
}

std::string App::processGetXrouterConfig(XRouterSettings cfg, std::string addr)
{
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
bool App::processReply(XRouterPacketPtr packet, CNode* node)
{
    uint32_t offset = 0;

    std::string uuid((const char *)packet->data()+offset);
    offset += uuid.size() + 1;
    std::string reply((const char *)packet->data()+offset);
    offset += reply.size() + 1;

    LOG() << "Received reply to query " << uuid;
    
    // check uuid is in queriesLock keys
    if (!queryMgr.hasQuery(uuid))
        return true; // done, nothing found
    
    Object ret;
    Value reply_val;
    read_string(reply, reply_val);
    
    if (reply_val.type() == obj_type) {
        Object reply_obj = reply_val.get_obj();
        const Value & code  = find_value(reply_obj, "code");
        if (code.type() == json_spirit::int_type && code.get_int() == xrouter::EXPIRED_PAYMENT_CHANNEL) {
            std::string addr = getPaymentAddress(node);
            if (paymentChannels.count(addr))
                paymentChannels.erase(this->paymentChannels.find(addr));
        }
    }
    
    // Store the reply
    queryMgr.addReply(uuid, node->addr.ToString(), reply);
    LOG() << reply;

    return true;
}

bool App::processConfigReply(XRouterPacketPtr packet)
{
    uint32_t offset = 0;

    std::string uuid((const char *)packet->data()+offset);
    offset += uuid.size() + 1;
    std::string reply((const char *)packet->data()+offset);
    offset += reply.size() + 1;

    LOG() << "Received reply to query " << uuid;
    LOG() << "Received xrouter config from node " << configQueries[uuid];
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
        // This is a reply from service node itself
        snodeConfigs[configQueries[uuid]] = settings;
        
        // Add IP and possibly domain name to table of domains
        snodeDomains[configQueries[uuid]] = configQueries[uuid];
        if (settings.get<std::string>("Main.domain", "") != "") {
            snodeDomains[settings.get<std::string>("Main.domain")] = configQueries[uuid];
        }
        return true;
    } else {
        // This is a reply from another client node
        
        std::string addr = find_value(reply_obj, "addr").get_str();
        snodeConfigs[addr] = settings;
        snodeDomains[addr] = addr;
        if (settings.get<std::string>("Main.domain", "") != "") {
            snodeDomains[settings.get<std::string>("Main.domain")] = addr;
        }
        return true;
    }
}

//*****************************************************************************
//*****************************************************************************
void App::onMessageReceived(CNode* node, const std::vector<unsigned char>& message, CValidationState& state)
{
    LOG() << "Received xrouter packet";

    // If xrouter == 0, xrouter is turned off on this snode
    if (!isEnabled())
        return;
    
    XRouterPacketPtr packet(new XRouterPacket);
    if (!packet->copyFrom(message)) {
        LOG() << "incorrect packet received " << __FUNCTION__;
        state.DoS(10, error("XRouter: invalid packet received"), REJECT_INVALID, "xrouter-error");
        return;
    }

    const auto & nodeAddr = node->addr.ToString();

    std::string reply;
    LOG() << "XRouter command: " << std::string(XRouterCommand_ToString(packet->command()));
    
    std::chrono::time_point<std::chrono::system_clock> time = std::chrono::system_clock::now();
    if (packet->command() == xrGetXrouterConfig) {
        uint32_t offset = 0;
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
        if (lastConfigQueries.count(nodeAddr)) {
            std::chrono::time_point<std::chrono::system_clock> prev_time = lastConfigQueries[nodeAddr];
            std::chrono::system_clock::duration diff = time - prev_time;
            if (std::chrono::duration_cast<std::chrono::seconds>(diff) < std::chrono::seconds(10))
                state.DoS(10, error("XRouter: too many config requests"), REJECT_INVALID, "xrouter-error");
            lastConfigQueries[nodeAddr] = time;
        } else {
            lastConfigQueries[nodeAddr] = time;
        }
        
        XRouterPacketPtr rpacket(new XRouterPacket(xrConfigReply));
        rpacket->append(uuid);
        rpacket->append(reply);
        node->PushMessage("xrouter", rpacket->body());
        return;
    } else if (packet->command() == xrReply) {
        processReply(packet, node);
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
std::string App::xrouterCall(enum XRouterCommand command, const std::string & currency,
                             const std::string param1, const std::string param2, const std::string confirmations)
{
    const std::string & id = generateUUID();

    try {
        if (!isEnabled())
            throw XRouterError("XRouter is turned off. Please set 'xrouter=1' in blocknetdx.conf to run XRouter.", xrouter::UNAUTHORIZED);
        
        // Check param1
        switch (command) {
            case xrGetBlockHash:
            case xrGetAllBlocks:
                if (!is_number(param1))
                    throw XRouterError("Incorrect block number: " + param1, xrouter::INVALID_PARAMETERS);
                break;
            case xrGetTransactionsBloomFilter:
                if (!is_hash(param1) || (param1.size() % 10 != 0))
                    throw XRouterError("Incorrect bloom filter: " + param1, xrouter::INVALID_PARAMETERS);
                break;
            case xrGetBlock:
            case xrGetTransaction:
                if (!is_hash(param1))
                    throw XRouterError("Incorrect hash: " + param1, xrouter::INVALID_PARAMETERS);
                break;
            case xrGetAllTransactions:
            case xrGetBalanceUpdate:
                if (!is_address(param1))
                    throw XRouterError("Incorrect address: " + param1, xrouter::INVALID_PARAMETERS);
                break;
            default:
                break;
        }
        
        // Check param2
        switch (command) {
            case xrGetAllTransactions:
            case xrGetBalanceUpdate:
            case xrGetTransactionsBloomFilter:
                if (!is_number(param2))
                    throw XRouterError("Incorrect block number: " + param2, xrouter::INVALID_PARAMETERS);
                break;
            default:
                break;
        }

        int confirmations_count = 0;
        if (!confirmations.empty()) {
            if (!is_number(confirmations))
                throw XRouterError("Incorrect number of service nodes for consensus: " + confirmations, xrouter::INVALID_PARAMETERS);
            confirmations_count = std::stoi(confirmations);
            if (confirmations_count < 1)
                throw XRouterError("Incorrect number of service nodes for consensus: " + confirmations, xrouter::INVALID_PARAMETERS);
        }
        if (confirmations_count < 1) {
            confirmations_count = xrouter_settings.get<int>("Main.consensus_nodes", 0);
            confirmations_count = XROUTER_DEFAULT_CONFIRMATIONS;
        }

        Object error;

        LOG() << "Sending query " << id;

        // Select available nodes
        std::vector<CNode*> selectedNodes = getAvailableNodes(command, currency, confirmations_count);
        
        if (static_cast<int>(selectedNodes.size()) < confirmations_count)
            throw XRouterError("Could not find " + std::to_string(confirmations_count) + " Service Nodes to query at the moment.", xrouter::NOT_ENOUGH_NODES);
        
        bool usehash = xrouter_settings.get<int>("Main.usehash", 0) != 0;
        if (confirmations_count == 1)
            usehash = false;
        
        usehash = false; // TODO usehash is always disabled here
        std::map<std::string, std::string> paytx_map;
        int snodeCount = 0;

        // Obtain all the snodes that meet our criteria
        for (CNode* pnode : selectedNodes) {
            const auto & addr = pnode->addr.ToString();
            CAmount fee = to_amount(snodeConfigs[addr].getCommandFee(command, currency));
            CAmount fee_part1 = fee;
            std::string payment_tx = usehash ? "hash;nofee" : "nohash;nofee";
            if (fee > 0) {
                try {
                    if (!usehash)
                        payment_tx = "nohash;" + generatePayment(pnode, fee);
                    else {
                        fee_part1 = fee / 2;
                        payment_tx = "hash;" + generatePayment(pnode, fee_part1);
                    }
                } catch (std::runtime_error & e) {
                    LOG() << "Failed to create payment to node " << addr;
                    continue;
                }
            }
            
            paytx_map[addr] = payment_tx;
            snodeCount++;
            if (snodeCount == confirmations_count)
                break;
        }

        // Do we have enough snodes? If not unlock utxos
        if (snodeCount < confirmations_count) {
            for (CNode* pnode : selectedNodes) {
                if (paytx_map.count(pnode->addr.ToString()))
                    unlockOutputs(paytx_map[pnode->addr.ToString()]);
            }
            throw XRouterError("Could not create payments to service nodes. Please check that your wallet is fully unlocked and you have at least " + std::to_string(confirmations_count) + " available unspent transaction outputs.", xrouter::INSUFFICIENT_FUNDS);
        }

        // Create query
        QueryMgr::QueryCondition qcond;
        queryMgr.addQuery(id, qcond);

        // Packet signing key
        std::vector<unsigned char> privkey;
        crypto.makeNewKey(privkey);
        CKey key; key.Set(privkey.begin(), privkey.end(), true);
        int timeout = GetArg("-rpcxroutertimeout", 60);

        // Send xrouter request to each selected node
        for (CNode* pnode : selectedNodes) {
            const auto & addr = pnode->addr.ToString();
            if (!paytx_map.count(addr))
                continue;

            XRouterPacketPtr packet(new XRouterPacket(command));
            packet->append(id);
            packet->append(currency);
            packet->append(paytx_map[addr]);
            if (!param1.empty())
                packet->append(param1);
            if (!param2.empty())
                packet->append(param2);
            packet->sign(key);

            // Send packet to xrouter node
            pnode->PushMessage("xrouter", packet->body());
            
            std::chrono::time_point<std::chrono::system_clock> time = std::chrono::system_clock::now();
            const std::string commandStr(XRouterCommand_ToString(command));
            const auto & cmd = buildCommandKey(currency, commandStr);
            if (!lastPacketsSent.count(addr))
                lastPacketsSent[addr] = std::map<std::string, std::chrono::time_point<std::chrono::system_clock> >();
            lastPacketsSent[addr][cmd] = time;

            LOG() << "Sent message to node " << pnode->addrName;
        }

        // At this point we need to wait for responses
        int confirmation_count = 0;

        {
            boost::mutex::scoped_lock lock(*qcond.first);
            while ((confirmation_count < confirmations_count) && qcond.second->timed_wait(lock, boost::posix_time::seconds(timeout)))
                confirmation_count++;
        }

        // Clean up
        queryMgr.purge(id);

        if (confirmation_count <= confirmations_count / 2)
            throw XRouterError("Failed to get response in time. Try xrGetReply command later.", xrouter::SERVER_TIMEOUT);

        // Handle the results
        std::string result;
        int c = queryMgr.reply(id, result);
        if (c <= confirmations_count / 2 || result.empty())
            throw XRouterError("No consensus between responses", xrouter::INTERNAL_SERVER_ERROR);

        if (!usehash)
            return result;

        // We reach here only if usehash == true

//        CNode* finalnode;
//        for (auto & i : queries[id]) {
//            if (result == i.second) {
//                finalnode = i.first;
//                break;
//            }
//        }
//
//        CAmount fee = to_amount(snodeConfigs[finalnode->addr.ToString()].getCommandFee(command, currency));
//        CAmount fee_part2 = fee - fee/2;
//        std::string payment_tx = "nofee";
//        if (fee > 0) {
//            try {
//                payment_tx = "nohash;" + generatePayment(finalnode, fee_part2);
//            } catch (std::runtime_error &) {
//                LOG() << "Failed to create payment to node " << finalnode->addr.ToString();
//                throw XRouterError("Could not create payment to service node", xrouter::INSUFFICIENT_FUNDS);
//            }
//        }
//
//        XRouterPacketPtr fpacket(new XRouterPacket(xrFetchReply));
//        fpacket->append(id);
//        fpacket->append(currency);
//        fpacket->append(payment_tx);
//        fpacket->sign(key);
//
//        finalnode->PushMessage("xrouter", fpacket->body());
//
//        LOG() << "Fetching reply from node " << finalnode->addrName;
//
//        boost::shared_ptr<boost::mutex> m2(new boost::mutex());
//        boost::shared_ptr<boost::condition_variable> cond2(new boost::condition_variable());
//        boost::mutex::scoped_lock lock2(*m2);
//        queriesLocks[id] = std::pair<boost::shared_ptr<boost::mutex>, boost::shared_ptr<boost::condition_variable> >(m2, cond2);
//        if (cond2->timed_wait(lock2, boost::posix_time::seconds(timeout))) {
//            std::string reply = queries[id][finalnode];
//            return reply;
//        } else {
//            throw XRouterError("Failed to fetch reply from service node", xrouter::SERVER_TIMEOUT);
//        }

    } catch (XRouterError & e) {
        Object error;
        error.emplace_back(Pair("error", e.msg));
        error.emplace_back(Pair("code", e.code));
        error.emplace_back(Pair("uuid", id));
        LOG() << e.msg;
        return json_spirit::write_string(Value(error), true);
    }
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

std::string App::getTransactionsBloomFilter(const std::string & currency, const std::string & filter, const std::string & number, const std::string & confirmations)
{
    return this->xrouterCall(xrGetTransactionsBloomFilter, currency, filter, number, confirmations);
}

std::string App::convertTimeToBlockCount(const std::string& currency, std::string time, const std::string& confirmations) {

    return this->xrouterCall(xrTimeToBlockNumber, currency, time, "", confirmations);
}

std::string App::getReply(const std::string & id)
{
    Object result;
    auto replies = queryMgr.allReplies(id);

    if (replies.empty()) {
        result.emplace_back(Pair("error", "No replies found"));
        result.emplace_back(Pair("uuid", id));
        return json_spirit::write_string(Value(result), true);
    }

    for (auto & it : replies) {
        std::string reply = it.second;
        result.emplace_back(Pair("reply", reply)); // TODO display node id?
    }

    return json_spirit::write_string(Value(result), true);
}

std::string App::sendTransaction(const std::string & currency, const std::string & transaction)
{
    return this->xrouterCall(xrSendTransaction, currency, transaction, "", "1");
}

std::string App::sendCustomCall(const std::string & name, std::vector<std::string> & params)
{
    std::string id = generateUUID();

    try {
        if (!isEnabled())
            throw XRouterError("XRouter is turned off. Please set 'xrouter=1' in blocknetdx.conf to run XRouter.", xrouter::UNAUTHORIZED);
        
        if (this->xrouter_settings.hasPlugin(name)) {
            // Run the plugin locally
            return server->processCustomCall(name, params);
        }
        
        updateConfigs();

        CNode* pnode = getNodeForService(name);
        if (!pnode)
            throw XRouterError("Plugin not found", xrouter::UNSUPPORTED_SERVICE);

        const auto & addr = pnode->addr.ToString();
        const auto psize = static_cast<int>(params.size());

        int min_count = snodeConfigs[addr].getPluginSettings(name).getMinParamCount();
        if (psize < min_count) {
            throw XRouterError("Not enough plugin parameters", xrouter::INVALID_PARAMETERS);
        }

        int max_count = snodeConfigs[addr].getPluginSettings(name).getMaxParamCount();
        if (psize > max_count) {
            throw XRouterError("Too many plugin parameters", xrouter::INVALID_PARAMETERS);
        }

        std::string strtxid;
        CAmount fee = to_amount(snodeConfigs[addr].getPluginSettings(name).getFee());
        std::string payment_tx = "nofee";
        if (fee > 0) {
            try {
                payment_tx = "nohash;" + generatePayment(pnode, fee);
                LOG() << "Payment transaction: " << payment_tx;
                //std::cout << "Payment transaction: " << payment_tx << std::endl << std::flush;
            } catch (std::runtime_error &) {
                LOG() << "Failed to create payment to node " << addr;
            }
        }

        // Create query
        QueryMgr::QueryCondition qcond;
        queryMgr.addQuery(id, qcond);

        // Key
        std::vector<unsigned char> privkey;
        crypto.makeNewKey(privkey);
        CKey key; key.Set(privkey.begin(), privkey.end(), true);
        int timeout = GetArg("-rpcxroutertimeout", 60);

        XRouterPacketPtr packet(new XRouterPacket(xrCustomCall));
        packet->append(id);
        packet->append(name);
        packet->append(payment_tx);
        for (std::string & param : params)
            packet->append(param);
        packet->sign(key);

        std::vector<unsigned char> msg;
        msg.insert(msg.end(), packet->body().begin(), packet->body().end());
        
        Object result;
        
        std::chrono::time_point<std::chrono::system_clock> time = std::chrono::system_clock::now();
        if (!lastPacketsSent.count(addr))
            lastPacketsSent[addr] = std::map<std::string, std::chrono::time_point<std::chrono::system_clock> >();
        lastPacketsSent[addr][name] = time;
        
        pnode->PushMessage("xrouter", msg);

        // Wait for reply
        boost::mutex::scoped_lock lock(*qcond.first);
        if (qcond.second->timed_wait(lock, boost::posix_time::milliseconds(timeout))) {
            std::string reply;
            int consensus = queryMgr.reply(id, reply);
            queryMgr.purge(id); // clean up
            // TODO Handle consensus
            return reply;
        }

        queryMgr.purge(id); // clean up

        // Error if we reach here
        throw XRouterError("Failed to get response in time. Try xrGetReply command later.", xrouter::SERVER_TIMEOUT);

    } catch (XRouterError & e) {
        Object error;
        error.emplace_back(Pair("error", e.msg));
        error.emplace_back(Pair("code", e.code));
        error.emplace_back(Pair("uuid", id));
        LOG() << e.msg;
        return json_spirit::write_string(Value(error), true);
    }
}

std::string App::generatePayment(CNode* pnode, CAmount fee)
{
    std::string strtxid;
    std::string dest = getPaymentAddress(pnode);
    CAmount deposit = to_amount(xrouter_settings.get<double>("Main.deposit", 0.0));
    int channeldate = xrouter_settings.get<int>("Main.channeldate", 100000);
    std::string deposit_pubkey_s = snodeConfigs[pnode->addr.ToString()].get<std::string>("depositpubkey", "");
    std::string payment_tx = "nofee";
    if (fee > 0) {
        if ((deposit == 0) || (deposit_pubkey_s.empty())) {
            bool res = createAndSignTransaction(dest, fee, payment_tx);
            payment_tx = "single;" + payment_tx;
            if(!res) {
                throw std::runtime_error("Failed to create payment transaction");
            }
        } else {
            // Create payment channel first
            std::string raw_tx, txid;
            payment_tx = "";
            PaymentChannel channel;
            std::string addr = getPaymentAddress(pnode);
            CPubKey deposit_pubkey = CPubKey(ParseHex(deposit_pubkey_s));
            
            // Clear expired channel
            if (this->paymentChannels.count(addr)) {
                if (std::time(0) >= this->paymentChannels[addr].deadline) {
                    this->paymentChannels.erase(this->paymentChannels.find(addr));
                }
            }
            
            if (!this->paymentChannels.count(addr)) {
                channel = createPaymentChannel(deposit_pubkey, deposit, channeldate);
                if (channel.txid == "")
                    throw std::runtime_error("Failed to create payment channel");
                this->paymentChannels[addr] = channel;
                payment_tx = channel.raw_tx + ";" + channel.txid + ";" + HexStr(channel.redeemScript.begin(), channel.redeemScript.end()) + ";";
            }
            
            // Submit payment via channel
            CAmount paid = this->paymentChannels[addr].value;
            
            std::string paytx;
            bool res = createAndSignChannelTransaction(this->paymentChannels[addr], dest, deposit, fee + paid, paytx);
            if (!res)
                throw std::runtime_error("Failed to pay to payment channel");
            this->paymentChannels[addr].latest_tx = paytx;
            this->paymentChannels[addr].value = fee + paid;
            
            // Send channel tx, channel tx id, payment tx in one string
            payment_tx += paytx;
            payment_tx = "channel;" + payment_tx;
        }
        
        LOG() << "Payment transaction: " << payment_tx;
        //std::cout << "Payment transaction: " << payment_tx << std::endl << std::flush;
        savePaymentChannels();
    }
    
    return payment_tx;
}

std::string App::getPaymentAddress(CNode* node)
{
    // Payment address = pubkey Collateral address of snode
    std::vector<CServicenode> vServicenodeRanks = getServiceNodes();
    for (CServicenode & s : vServicenodeRanks) {
        if (s.addr.ToString() == node->addr.ToString()) {
            return CBitcoinAddress(s.pubKeyCollateralAddress.GetID()).ToString();
        }
    }
    
    if (debug_on_client())
        return "yBW61mwkjuqFK1rVfm2Az2s2WU5Vubrhhw";
    
    return "";
}

CPubKey App::getPaymentPubkey(CNode* node)
{
    // Payment address = pubkey Collateral address of snode
    std::vector<CServicenode> vServicenodeRanks = getServiceNodes();
    for (CServicenode & s : vServicenodeRanks) {
        if (s.addr.ToString() == node->addr.ToString()) {
            return s.pubKeyCollateralAddress;
        }
    }
    
    if (debug_on_client()) {
        std::string test = "03872bfe748a5a3868c74c8f820ed1387a58d48c67a7c415c7b3fad1ca61803365";
        return CPubKey(ParseHex(test));
    }
    
    return CPubKey();
}

std::string App::printPaymentChannels() {
    Array client;
    
    for (const auto& it : this->paymentChannels) {
        Object val;
        val.emplace_back("Node id", it.first);
        val.emplace_back("Deposit transaction", it.second.raw_tx);
        val.emplace_back("Deposit transaction id", it.second.txid);
        val.emplace_back("Redeem transaction", it.second.latest_tx);
        val.emplace_back("Paid amount", it.second.value);
        val.emplace_back("Expires in (ms):", it.second.deadline - static_cast<int64_t>(std::time(0)));
        client.push_back(Value(val));
    }
    
    Object result;
    result.emplace_back("Client side", client);
    result.emplace_back("Server side", this->server->printPaymentChannels());
    
    return json_spirit::write_string(Value(result), true);
}

std::string App::getXrouterConfig(CNode* node, std::string addr) {
    XRouterPacketPtr packet(new XRouterPacket(xrGetXrouterConfig));

    uint256 txHash;
    uint32_t vout = 0;

    std::string id = generateUUID();
    packet->append(id);
    packet->append(addr);
    
    std::vector<unsigned char> msg;
    msg.insert(msg.end(), packet->body().begin(), packet->body().end());
    
    this->configQueries[id] = node->addr.ToString();
    node->PushMessage("xrouter", msg);
    return id;
}

std::string App::getXrouterConfigSync(CNode* node) {
    std::string id = generateUUID();
    const auto & addr = node->addr.ToString();

    XRouterPacketPtr packet(new XRouterPacket(xrGetXrouterConfig));
    packet->append(id);

    QueryMgr::QueryCondition qcond;
    queryMgr.addQuery(id, qcond);

    std::vector<unsigned char> msg;
    msg.insert(msg.end(), packet->body().begin(), packet->body().end());
    node->PushMessage("xrouter", msg);

    boost::mutex::scoped_lock lock(*qcond.first);
    int timeout = this->xrouter_settings.get<int>("Main.configsynctimeout", XROUTER_DEFAULT_WAIT);
    if (!qcond.second->timed_wait(lock, boost::posix_time::milliseconds(timeout))) {
        queryMgr.purge(id); // clean up
        return "Could not get XRouter config";
    }

    std::string reply;
    int consensus = queryMgr.reply(id, reply);
    queryMgr.purge(id); // clean up

    // Update settings for node
    XRouterSettings settings;
    settings.read(reply);
    this->snodeConfigs[addr] = settings;

    return reply;
}

void App::reloadConfigs() {
    LOG() << "Reloading xrouter config from file " << xrouterpath;
    this->xrouter_settings.read(xrouterpath.c_str());
    this->xrouter_settings.loadPlugins();
}

std::string App::getStatus() {
    Object result;
    result.emplace_back(Pair("enabled", isEnabled()));
    result.emplace_back(Pair("config", this->xrouter_settings.rawText()));
    Object myplugins;
    for (std::string s : this->xrouter_settings.getPlugins())
        myplugins.emplace_back(s, this->xrouter_settings.getPluginSettings(s).rawText());
    result.emplace_back(Pair("plugins", myplugins));
    
    Array nodes;
    for (auto& it : this->snodeConfigs) {
        Object val;
        //val.emplace_back("node", it.first);
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

void App::closePaymentChannel(std::string id) {
    server->closePaymentChannel(std::move(id));
}

void App::closeAllPaymentChannels() {
    server->closeAllPaymentChannels();
}

void App::savePaymentChannels() {
    std::string path(GetDataDir(true).string() + "/paymentchannels.json");
    Array client;
    
    for (const auto& it : this->paymentChannels) {
        Object val;
        val.emplace_back("id", it.first);
        val.emplace_back("raw_tx", it.second.raw_tx);
        val.emplace_back("txid", it.second.txid);
        val.emplace_back("vout", it.second.vout);
        val.emplace_back("latest_tx", it.second.latest_tx);
        val.emplace_back("value", it.second.value);
        val.emplace_back("deposit", it.second.deposit);
        val.emplace_back("deadline", it.second.deadline);
        val.emplace_back("keyid", it.second.keyid.ToString());
        val.emplace_back("redeemScript", HexStr(it.second.redeemScript.begin(), it.second.redeemScript.end()) );
        client.push_back(Value(val));
    }
    
    ofstream f(path);
    f << json_spirit::write_string(Value(client), true);
    f.close();
}

void App::loadPaymentChannels() {
    std::string path(GetDataDir(true).string() + "/paymentchannels.json");
    ifstream f(path);
    std::string config;
    f >> config;
    f.close();
    
    Value data;
    read_string(config, data);
    
    if (data.type() != array_type)
        return;
    
    Array channels = data.get_array();
    
    for (size_t i = 0; i < channels.size(); i++) {
        if (channels[i].type() != obj_type)
            continue;
        
        try {
            Object channel = channels[i].get_obj();
            std::string id = find_value(channel, "id").get_str();
            
            PaymentChannel c;
            c.raw_tx = find_value(channel, "raw_tx").get_str();
            c.txid = find_value(channel, "txid").get_str();
            c.vout = find_value(channel, "vout").get_int();
            c.latest_tx = find_value(channel, "latest_tx").get_str();
            c.value = to_amount(find_value(channel, "value").get_int());
            c.deposit = to_amount(find_value(channel, "deposit").get_int());
            c.deadline = find_value(channel, "deadline").get_int();
            std::vector<unsigned char> script = ParseHex(find_value(channel, "redeemScript").get_str());
            c.redeemScript = CScript(script.begin(), script.end());
            CBitcoinAddress addr = CBitcoinAddress(find_value(channel, "keyid").get_str());
            addr.GetKeyID(c.keyid);
            pwalletMain->GetKey(c.keyid, c.key);
                
            this->paymentChannels[id] = c;
        } catch (...) {
            continue;
        }
    }
}

std::string App::createDepositAddress(bool update) {
    CPubKey my_pubkey; 
    {
        LOCK(pwalletMain->cs_wallet);
        my_pubkey = pwalletMain->GenerateNewKey();
    }
    
    if (!my_pubkey.IsValid()) {
        Object error;
        error.emplace_back(Pair("error", "Could not generate deposit address. Make sure that your wallet is unlocked."));
        error.emplace_back(Pair("code", xrouter::INSUFFICIENT_FUNDS));
        error.emplace_back(Pair("uuid", ""));
        return json_spirit::write_string(Value(error), true);
    }
    
    CKeyID mykeyID = my_pubkey.GetID();
    CBitcoinAddress addr(mykeyID);
    Object result;
    result.emplace_back(Pair("depositpubkey", HexStr(my_pubkey)));
    result.emplace_back(Pair("depositaddress", addr.ToString()));
    if (update) {
        this->xrouter_settings.set("Main.depositaddress", addr.ToString());
        this->xrouter_settings.set("Main.depositpubkey", HexStr(my_pubkey));
        this->xrouter_settings.write();
    }
    return json_spirit::write_string(Value(result), true);
}

void App::runTests() {
    server->runPerformanceTests();
}

bool App::debug_on_client() {
    return xrouter_settings.get<int>("Main.debug_on_client", 0) != 0;
}

bool App::isDebug() {
    //int b;
    //verifyDomain("98fa59764df5d2022994ca98e8ad3ca795681920bb7cad0af8df07ab48539ac6", "antihype", "yBW61mwkjuqFK1rVfm2Az2s2WU5Vubrhhw", b);
    return xrouter_settings.get<int>("Main.debug", 0) != 0;
}

std::string App::getMyPaymentAddress() {
    return server->getMyPaymentAddress();
}

std::string App::registerDomain(std::string domain, std::string addr, bool update) {
    std::string result = generateDomainRegistrationTx(domain, addr);
    
    if (update) {
        this->xrouter_settings.set("Main.domain", domain);
        this->xrouter_settings.set("Main.domain_tx", result);
        this->xrouter_settings.write();
    }
    
    return result;
}
    

bool App::queryDomain(std::string domain) {
    openConnections();
    updateConfigs();
    
    if (snodeDomains.count(domain) == 0)
        return false;
    
    for (CNode* pnode : vNodes) {
        XRouterSettings settings = snodeConfigs[snodeDomains[domain]];
        
        if (snodeDomains[domain] != pnode->addr.ToString())
            continue;
        
        std::string addr = getPaymentAddress(pnode);
        int block;
        std::string tx = settings.get<std::string>("Main.domain_tx", "");
        if (tx.empty())
            continue;
        
        if (verifyDomain(tx, domain, addr, block))
            return true;
    }
    
    return false;
}

} // namespace xrouter
