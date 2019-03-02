//*****************************************************************************
//*****************************************************************************

#include "xrouterapp.h"
#include "xrouterlogger.h"
#include "xrouterutils.h"
#include "xroutererror.h"

#include "xbridge/bitcoinrpcconnector.h"
#include "xbridge/util/settings.h"

#include "keystore.h"
#include "main.h"
#include "net.h"
#include "servicenodeconfig.h"
#include "servicenodeman.h"
#include "obfuscation.h"
#include "addrman.h"
#include "script/standard.h"
#include "wallet.h"
#include "bloom.h"
#include "rpcserver.h"

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

#include <boost/chrono/chrono.hpp>
#include <boost/thread/thread.hpp>
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

//*****************************************************************************
//*****************************************************************************
App::App()
    : xrsettings(new XRouterSettings)
        , server(new XRouterServer)
        , timerIoWork(new boost::asio::io_service::work(timerIo))
        , timerThread(boost::bind(&boost::asio::io_service::run, &timerIo))
        , timer(timerIo, boost::posix_time::seconds(XROUTER_TIMER_SECONDS))
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

    LOG() << "Loading xrouter config from file " << xrouterpath;
    std::string path(GetDataDir(false).string());
    xrouterpath = path + "/xrouter.conf";
    xrsettings->read(xrouterpath.c_str());
    xrsettings->loadPlugins();
    xrsettings->loadWallets();

    return true;
}

// We append "xr" if XRouter is activated at all, and "xr::service_name" for each activated plugin and wallet
std::vector<std::string> App::getServicesList() 
{
    std::vector<std::string> result;
    if (!isEnabled())
        return result;

    result.push_back(xr); // signal xrouter support
    std::string services = xr;

    // Add wallet services
    for (const std::string & s : xrsettings->getWallets()) {
        const auto w = buildCommandKey(xr, s);
        result.push_back(w);
        services += "," + w;
    }

    // Add plugin services
    for (const std::string & s : xrsettings->getPlugins()) {
        const auto p = buildCommandKey(xr, s);
        result.push_back(p);
        services += "," + p;
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

    // Only start server mode if we're a servicenode
    if (fServiceNode) {
        bool res = server->start();
        if (res) { // set outgoing xrouter requests to use snode key
            cpubkey = server->pubKey();
            cprivkey = server->privKey();
        }
    } else if (!initKeyPair()) // init on regular xrouter clients (non-snodes)
        return false;

    // Start the xrouter timer
    try {
        timer.expires_at(boost::posix_time::second_clock::universal_time() + boost::posix_time::seconds(XROUTER_TIMER_SECONDS));
        timer.async_wait(boost::bind(&App::onTimer, this));
    }
    catch (std::exception & e) {
        ERR() << "Failed to start the xrouter timer: " << e.what() << " "
              << __FUNCTION__;
        return false;
    }

    return true;
}

bool App::createConnectors() {
    if (fServiceNode && isEnabled())
        return server->createConnectors();
    ERR() << "Failed to load wallets: Must be a servicenode with xrouter=1 specified in config";
    return false;
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
        if (!s.HasService(xr))
            continue;

        if (!wallet.empty())
            if (!s.HasService(buildCommandKey(xr, wallet)))
                continue;

        if (!plugin.empty())
            if (!s.HasService(buildCommandKey(xr, plugin)))
                continue;

        const auto & snodeAddr = s.addr.ToString();

        // Do not connect if already a peer
        if (nodec.count(snodeAddr))
            continue;

        // Connect to snode
        CAddress addr;
        if (OpenNetworkConnection(addr, nullptr, snodeAddr.c_str())) { // OpenNetworkConnection filters out bad nodes (banned, etc)
            CNode *res = FindNode((CNetAddr)addr);
            if (res) {
                LOG() << "Connected to servicenode " << CBitcoinAddress(s.pubKeyCollateralAddress.GetID()).ToString();
                const auto & nodeAddr = res->addr.ToString();
                std::chrono::time_point<std::chrono::system_clock> time = std::chrono::system_clock::now();
                updateConfigTime(nodeAddr, time);
                std::string uuid = sendXRouterConfigRequest(res);
                LOG() << "Requesting config from snode " << CBitcoinAddress(s.pubKeyCollateralAddress.GetID()).ToString()
                      << " request id = " << uuid;
            }
        } else LOG() << "Failed to connect to servicenode " << CBitcoinAddress(s.pubKeyCollateralAddress.GetID()).ToString();
    }
}

std::string App::updateConfigs(bool force)
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
        const auto nodeAddr = pnode->addr.ToString();
        // skip non-xrouter nodes, disconnecting nodes, and self
        if (!snodes.count(nodeAddr) || !snodes[nodeAddr].HasService(xr) || !pnode->fSuccessfullyConnected
            || pnode->fDisconnect || snodes[nodeAddr].pubKeyServicenode == activeServicenode.pubKeyServicenode)
            continue;

        std::chrono::time_point<std::chrono::system_clock> time = std::chrono::system_clock::now();
        // If not force check, rate limit config requests
        if (!force) {
            if (hasConfigTime(nodeAddr)) {
                const auto prev_time = getConfigTime(nodeAddr);
                std::chrono::system_clock::duration diff = time - prev_time;
                // There was a request to this node already, a new one will be sent only after 5 minutes
                if (std::chrono::duration_cast<std::chrono::seconds>(diff) < std::chrono::seconds(300))
                    continue;
            }
        }

        // Request the config
        auto & snode = snodes[nodeAddr]; // safe here due to check above
        updateConfigTime(nodeAddr, time);
        std::string uuid = sendXRouterConfigRequest(pnode);
        LOG() << "Requesting config from snode " << CBitcoinAddress(snode.pubKeyCollateralAddress.GetID()).ToString()
              << " request id = " << uuid;
    }
    
    return "Config requests have been sent";
}

std::string App::printConfigs()
{
    Array result;
    
    for (const auto& it : this->snodeConfigs) {
        Object val;
        val.emplace_back("config", it.second->rawText());
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

    timer.cancel();
    timerIo.stop();
    timerIoWork.reset();
    timerThread.join();

    return true;
}
 
//*****************************************************************************
//*****************************************************************************
std::vector<CNode*> App::getAvailableNodes(enum XRouterCommand command, std::string wallet, int confirmations)
{
    // Send only to the service nodes that have the required wallet
    std::vector<CServicenode> vServicenodes = getServiceNodes();

    // Open connections (at least number equal to confirmations)
    openConnections(wallet/*, confirmations*/); // TODO Only open enough connections to match confirmations

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

    auto maxfee = xrsettings->getMaxFee(command, wallet);

    int supported = 0;
    int ready = 0;

    auto configs = getConfigs();
    if (configs.empty()) // don't have any configs
        return selectedNodes;

    for (const auto & item : configs)
    {
        const auto nodeAddr = item.first;
        auto settings = item.second;
        if (!settings->walletEnabled(wallet))
            continue;
        if (!settings->isAvailableCommand(command, wallet))
            continue;
        if (!snodes.count(nodeAddr)) // Ignore if not a snode
            continue;
        if (!snodes[nodeAddr].HasService(buildCommandKey(xr, wallet))) // Ignore snodes that don't have the wallet
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

        // Only select nodes with a fee smaller than the max fee we're willing to pay
        if (maxfee > 0) {
            auto fee = settings->getCommandFee(command, wallet);
            if (fee > maxfee) {
                LOG() << "Skipping node " << snodeAddr << " because its fee=" << fee << " is higher than maxfee=" << maxfee;
                continue;
            }
        }

        const std::string commandStr(XRouterCommand_ToString(command));
        const auto & cmd = buildCommandKey(wallet, commandStr);
        auto rateLimit = settings->clientRequestLimit(command, wallet);
        if (hasSentRequest(nodeAddr, cmd) && rateLimitExceeded(nodeAddr, cmd, getLastRequest(nodeAddr, cmd), rateLimit)) {
            LOG() << "Skipping node " << snodeAddr << " because not enough time passed since the last call";
            continue;
        }
        
        ready++;
        selectedNodes.push_back(node);
    }
    
    for (CNode *node : selectedNodes) {
        const auto & addr = node->addr.ToString();
        if (!hasScore(addr))
            updateScore(addr, 0);
    }

    // Sort selected nodes descending by score
    std::sort(selectedNodes.begin(), selectedNodes.end(), [this](const CNode *a, const CNode *b) {
        return getScore(a->addr.ToString()) > getScore(b->addr.ToString());
    });

    if (ready < confirmations)
        throw XRouterError("Could not find " + std::to_string(confirmations) + " Service Node(s), only found " + std::to_string(ready), xrouter::NOT_ENOUGH_NODES);
    
    return selectedNodes;
}

CNode* App::getNodeForService(std::string name)
{
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

    double maxfee = xrsettings->getMaxFee(xrCustomCall, "");

    // Domains
    if (name.find("/") != string::npos) {
        std::vector<std::string> parts;
        boost::split(parts, name, boost::is_any_of("/"));
        std::string domain = parts[0];
        std::string name_part = parts[1];
        if (!hasDomain(domain))
            return nullptr; // Domain not found

        std::vector<CNode*> candidates; // TODO Multiple candidates?
        const auto nodeAddr = snodeDomains[domain];

        if (!nodec.count(nodeAddr))
            return nullptr; // unknown node

        CNode *pnode = nodec[nodeAddr];
        candidates.push_back(pnode);

        // Check if node has settings
        auto settings = getConfig(nodeAddr);
        if (!settings->hasPlugin(name_part))
            return nullptr;

        // Check if fee is within expected range
        if (maxfee >= 0) {
            CAmount fee = to_amount(settings->getPluginSettings(name)->getFee());
            if (fee > maxfee)
                return nullptr; // fee too expensive, ignore node
        }

        CNode *res = nullptr;

        if (candidates.empty())
            return nullptr;
        else if (candidates.size() == 1)
            res = candidates[0];
        else {
            // Perform verification check of domain names
            int best_block = -1;
            for (CNode *node : candidates) {
                const auto nodeAddr = node->addr.ToString();
                std::string addr;
                if (!getPaymentAddress(nodeAddr, addr))
                    continue; // couldn't find snode address, skip

                std::string tx = settings->get<std::string>("Main.domain_tx", "");
                if (tx.empty())
                    continue;

                int block;
                if (!verifyDomain(tx, domain, addr, block))
                    continue;

                if ((best_block < 0) || (block < best_block)) {
                    // TODO: what if both verification tx are in the same block?
                    best_block = block;
                    res = node;
                }
            }
            
            if (!res)
                return nullptr;
        }

        auto rateLimit = settings->getPluginSettings(name)->clientRequestLimit();
        if (rateLimitExceeded(nodeAddr, name, getLastRequest(nodeAddr, name), rateLimit))
            return nullptr; // exceeded rate limit on this plugin
        
        return res;
    }

    auto configs = getConfigs();
    for (const auto & item : configs)
    {
        const auto nodeAddr = item.first;

        if (!hasConfig(nodeAddr))
            continue; // no config, skipping

        auto settings = getConfig(nodeAddr);
        if (!settings->hasPlugin(name))
            continue; // no plugin, skipping

        CNode *res = nodec[nodeAddr];
        if (!res) {
            CAddress addr;
            res = ConnectNode(addr, nodeAddr.c_str());
        }
        
        if (!res)
            continue;

        auto rateLimit = settings->getPluginSettings(name)->clientRequestLimit();
        if (rateLimitExceeded(nodeAddr, name, getLastRequest(nodeAddr, name), rateLimit))
            continue; // exceeded rate limit on this plugin

        // Check if we're willing to pay this fee
        if (maxfee >= 0) {
            CAmount fee = to_amount(settings->getPluginSettings(name)->getFee());
            if (fee > maxfee)
                continue; // unwilling to pay fee, too much
        }
        
        return res;
    }
    
    return nullptr;
}

std::string App::parseConfig(XRouterSettingsPtr cfg, const NodeAddr & node)
{
    Object result;
    result.emplace_back("config", cfg->rawText());
    Object plugins;
    for (std::string s : cfg->getPlugins())
        plugins.emplace_back(s, cfg->getPluginSettings(s)->rawText());
    result.emplace_back("plugins", plugins);
    result.emplace_back("addr", node);
    LOG() << "Sending config to " << node;
    return json_spirit::write_string(Value(result), true);
}

//*****************************************************************************
//*****************************************************************************
bool App::processReply(CNode *node, XRouterPacketPtr packet, CValidationState & state)
{
    const auto & uuid = packet->suuid();
    const auto & nodeAddr = node->addr.ToString();

    // Do not process if we aren't expecting a result. Also prevent reply malleability (only first reply is accepted)
    if (!queryMgr.hasQuery(uuid) || queryMgr.hasReply(uuid, nodeAddr))
        return false;

    // Verify servicenode response
    std::vector<unsigned char> spubkey;
    if (!servicenodePubKey(nodeAddr, spubkey) || !packet->verify(spubkey)) {
        state.DoS(20, error("XRouter: unsigned packet or signature error"), REJECT_INVALID, "xrouter-error");
        return false;
    }

    uint32_t offset = 0;
    std::string reply((const char *)packet->data()+offset);
    offset += reply.size() + 1;

    LOG() << "Received reply to query " << uuid;
    
    // check uuid is in queriesLock keys
    if (!queryMgr.hasQuery(uuid))
        return true; // done, nothing found

    Object ret;
    Value reply_val;
    read_string(reply, reply_val);

    // Store the reply
    queryMgr.addReply(uuid, nodeAddr, reply);
    LOG() << reply;

    return true;
}

bool App::processConfigReply(CNode *node, XRouterPacketPtr packet, CValidationState & state)
{
    const auto & uuid = packet->suuid();
    const auto & nodeAddr = node->addr.ToString();

    // Do not process if we aren't expecting a result. Also prevent reply malleability (only first reply is accepted)
    if (!queryMgr.hasQuery(uuid) || queryMgr.hasReply(uuid, nodeAddr))
        return false;

    // Verify servicenode response
    std::vector<unsigned char> spubkey;
    if (!servicenodePubKey(nodeAddr, spubkey) || !packet->verify(spubkey)) {
        state.DoS(20, error("XRouter: unsigned packet or signature error"), REJECT_INVALID, "xrouter-error");
        return false;
    }

    uint32_t offset = 0;
    std::string reply((const char *)packet->data()+offset);
    offset += reply.size() + 1;

    LOG() << "Received reply to query " << uuid << " from node " << nodeAddr;
    LOG() << reply;

    Value reply_val;
    read_string(reply, reply_val);
    Object reply_obj = reply_val.get_obj();
    std::string config = find_value(reply_obj, "config").get_str();
    Object plugins = find_value(reply_obj, "plugins").get_obj();
    
    auto settings = std::make_shared<XRouterSettings>(config);

    for (Object::size_type i = 0; i != plugins.size(); i++ ) {
        auto psettings = std::make_shared<XRouterPluginSettings>();
        psettings->read(plugins[i].value_.get_str());
        settings->addPlugin(plugins[i].name_, psettings);
    }

    updateConfig(nodeAddr, settings);

    // TODO Handle domain requests
//    auto domain = settings->get<std::string>("Main.domain");
//    if (!addr.empty()) // Add node to table of domains
//        updateDomainNode(domain, addr);

    return true;
}

//*****************************************************************************
//*****************************************************************************
void App::onMessageReceived(CNode* node, const std::vector<unsigned char> & message, CValidationState & state)
{
    // If xrouter == 0, xrouter is turned off on this node
    if (!isEnabled())
        return;
    
    XRouterPacketPtr packet(new XRouterPacket);
    if (!packet->copyFrom(message)) {
        LOG() << "incorrect packet received " << __FUNCTION__;
        state.DoS(10, error("XRouter: invalid packet received"), REJECT_INVALID, "xrouter-error");
        return;
    }

    const auto & uuid = packet->suuid();
    const auto nodeAddr = node->addr.ToString();
    const auto command = std::string(XRouterCommand_ToString(packet->command()));
    LOG() << "XRouter command: " << command << " from node " << nodeAddr;

    if (packet->command() == xrReply) {
        processReply(node, packet, state);
        return;
    } else if (packet->command() == xrConfigReply) {
        processConfigReply(node, packet, state);
        return;
    } else {
        server->onMessageReceived(node, packet, state);
        return;
    }
}

//*****************************************************************************
//*****************************************************************************
std::string App::xrouterCall(enum XRouterCommand command, std::string & uuidRet, const std::string & currency,
                             const int & confirmations, const std::string param1, const std::string param2)
{
    const std::string & uuid = generateUUID();
    uuidRet = uuid; // set uuid

    try {
        if (!isEnabled())
            throw XRouterError("XRouter is turned off. Please set 'xrouter=1' in blocknetdx.conf", xrouter::UNAUTHORIZED);
        
        // Check param1
        switch (command) {
            case xrGetBlockHash:
            case xrGetAllBlocks:
                if (!is_number(param1))
                    throw XRouterError("Incorrect block number: " + param1, xrouter::INVALID_PARAMETERS);
                break;
            case xrGetTxBloomFilter:
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
            case xrGetTxBloomFilter:
                if (!is_number(param2))
                    throw XRouterError("Incorrect block number: " + param2, xrouter::INVALID_PARAMETERS);
                break;
            default:
                break;
        }

        int confirmations_count = confirmations;
        if (confirmations_count < 1)
            confirmations_count = xrsettings->get<int>("Main.consensus", XROUTER_DEFAULT_CONFIRMATIONS);

        // Select available nodes
        std::vector<CNode*> selectedNodes = getAvailableNodes(command, currency, confirmations_count);
        auto selected = static_cast<int>(selectedNodes.size());
        
        if (selected < confirmations_count)
            throw XRouterError("Failed to find " + std::to_string(confirmations_count) + " service node(s) supporting " +
                               buildCommandKey(currency, XRouterCommand_ToString(command)) + " found " + std::to_string(selected), xrouter::NOT_ENOUGH_NODES);
        
        std::map<std::string, std::string> paytx_map;
        int snodeCount = 0;
        std::string fundErr{"Could not create payments to service nodes. Please check that your wallet "
                            "is fully unlocked and you have at least " + std::to_string(confirmations_count) +
                            " available unspent transaction outputs."};

        // Obtain all the snodes that meet our criteria
        for (CNode* pnode : selectedNodes) {
            const auto & addr = pnode->addr.ToString();
            if (!hasConfig(addr))
                continue; // skip nodes that do not have configs

            // Create the fee payment
            std::string feePayment;
            CAmount fee = to_amount(getConfig(addr)->getCommandFee(command, currency));
            if (fee > 0) {
                try {
                    if (!generatePayment(pnode, fee, feePayment))
                        throw XRouterError(fundErr, xrouter::INSUFFICIENT_FUNDS);
                } catch (XRouterError & e) {
                    ERR() << "Failed to create payment to node " << addr << " " << e.msg;
                    continue;
                }
            }
            
            paytx_map[addr] = feePayment;
            snodeCount++;
            if (snodeCount == confirmations_count)
                break;
        }

        // Do we have enough snodes? If not unlock utxos
        if (snodeCount < confirmations_count) {
            for (const auto & item : paytx_map) {
                const std::string & tx = item.second;
                unlockOutputs(tx);
            }
            throw XRouterError("Unable to find " + std::to_string(confirmations_count) +
                               " service node(s) to process request", xrouter::NOT_ENOUGH_NODES);
        }

        // Create query
        QueryMgr::QueryCondition qcond;
        queryMgr.addQuery(uuid, qcond);

        int timeout = xrsettings->commandTimeout(command, currency);

        // Send xrouter request to each selected node
        for (CNode* pnode : selectedNodes) {
            const auto & addr = pnode->addr.ToString();
            if (!paytx_map.count(addr))
                continue;

            // Record the node sending request to
            addQuery(uuid, addr);

            json_spirit::Object jbody;
            jbody.emplace_back("feetx", paytx_map[addr]);

            // Send packet to xrouter node
            XRouterPacketPtr packet(new XRouterPacket(command, uuid));
            packet->append(currency);
            packet->append(json_spirit::write_string(Value(jbody)));
            if (!param1.empty())
                packet->append(param1);
            if (!param2.empty())
                packet->append(param2);
            packet->sign(cpubkey, cprivkey);
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
        queryMgr.purge(uuid);

        if (confirmation_count <= confirmations_count / 2)
            throw XRouterError("Failed to get response in time. Try xrGetReply command later.", xrouter::SERVER_TIMEOUT);

        // Handle the results
        std::string result;
        int c = queryMgr.reply(uuid, result);
        if (c <= confirmations_count / 2 || result.empty())
            throw XRouterError("No consensus between responses", xrouter::INTERNAL_SERVER_ERROR);

        return result;

    } catch (XRouterError & e) {
        Object error;
        error.emplace_back(Pair("error", e.msg));
        error.emplace_back(Pair("code", e.code));
        error.emplace_back(Pair("uuid", uuid));
        LOG() << e.msg;
        return json_spirit::write_string(Value(error), true);
    }
}

std::string App::getBlockCount(std::string & uuidRet, const std::string & currency, const int & confirmations)
{
    return this->xrouterCall(xrGetBlockCount, uuidRet, currency, confirmations, "", "");
}

std::string App::getBlockHash(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & blockId)
{
    return this->xrouterCall(xrGetBlockHash, uuidRet, currency, confirmations, blockId, "");
}

std::string App::getBlock(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & blockHash)
{
    return this->xrouterCall(xrGetBlock, uuidRet, currency, confirmations, blockHash, "");
}

std::string App::getTransaction(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & hash)
{
    return this->xrouterCall(xrGetTransaction, uuidRet, currency, confirmations, hash, "");
}

std::string App::getAllBlocks(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & number)
{
    return this->xrouterCall(xrGetAllBlocks, uuidRet, currency, confirmations, number, "");
}

std::string App::getAllTransactions(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & account, const std::string & number)
{
    return this->xrouterCall(xrGetAllTransactions, uuidRet, currency, confirmations, account, number);
}

std::string App::getBalance(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & account)
{
    return this->xrouterCall(xrGetBalance, uuidRet, currency, confirmations, account, "");
}

std::string App::getBalanceUpdate(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & account, const std::string & number)
{
    return this->xrouterCall(xrGetBalanceUpdate, uuidRet, currency, confirmations, account, number);
}

std::string App::getTransactionsBloomFilter(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & filter, const std::string & number)
{
    return this->xrouterCall(xrGetTxBloomFilter, uuidRet, currency, confirmations, filter, number);
}

std::string App::convertTimeToBlockCount(std::string & uuidRet, const std::string& currency, const int & confirmations, std::string time) {

    return this->xrouterCall(xrGetBlockForTime, uuidRet, currency, confirmations, time, "");
}

std::string App::sendTransaction(std::string & uuidRet, const std::string & currency, const std::string & transaction)
{
    int confirmations{1}; // default 1 on this call
    return this->xrouterCall(xrSendTransaction, uuidRet, currency, confirmations, transaction, "");
}

std::string App::sendCustomCall(std::string & uuidRet, const std::string & name, std::vector<std::string> & params)
{
    std::string uuid = generateUUID();
    uuidRet = uuid;

    try {
        if (!isEnabled())
            throw XRouterError("XRouter is turned off. Please set 'xrouter=1' in blocknetdx.conf to run XRouter.", xrouter::UNAUTHORIZED);
        
        if (xrsettings->hasPlugin(name)) // Run the plugin locally
            return server->processCustomCall(name, params);

        CNode* pnode = getNodeForService(name);
        if (!pnode)
            throw XRouterError("Plugin not found", xrouter::UNSUPPORTED_SERVICE);

        const auto & addr = pnode->addr.ToString();
        const auto psize = static_cast<int>(params.size());

        auto pluginSettings = snodeConfigs[addr]->getPluginSettings(name);
        int min_count = pluginSettings->minParamCount();
        if (psize < min_count)
            throw XRouterError("Not enough plugin parameters", xrouter::INVALID_PARAMETERS);

        int max_count = snodeConfigs[addr]->getPluginSettings(name)->maxParamCount();
        if (psize > max_count)
            throw XRouterError("Too many plugin parameters", xrouter::INVALID_PARAMETERS);

        CAmount fee = to_amount(snodeConfigs[addr]->getPluginSettings(name)->getFee());
        std::string payment_tx = "nofee";
        if (fee > 0) {
            try {
                if (!generatePayment(pnode, fee, payment_tx))
                    throw XRouterError("Failed to create servicenode payment", xrouter::INSUFFICIENT_FEE);
                payment_tx = "nohash;" + payment_tx;
                LOG() << "Payment transaction: " << payment_tx;
            } catch (std::runtime_error &) {
                LOG() << "Failed to create payment to node " << addr;
            }
        }

        addQuery(uuid, addr);

        // Create query
        QueryMgr::QueryCondition qcond;
        queryMgr.addQuery(uuid, qcond);

        Object result;
        int timeout = pluginSettings->commandTimeout();

        std::chrono::time_point<std::chrono::system_clock> time = std::chrono::system_clock::now();
        if (!lastPacketsSent.count(addr))
            lastPacketsSent[addr] = std::map<std::string, std::chrono::time_point<std::chrono::system_clock> >();
        lastPacketsSent[addr][name] = time;

        // Send packet
        XRouterPacketPtr packet(new XRouterPacket(xrCustomCall, uuid));
        packet->append(name);
        packet->append(payment_tx);
        for (std::string & param : params)
            packet->append(param);
        packet->sign(cpubkey, cprivkey);
        pnode->PushMessage("xrouter", packet->body());

        // Wait for reply
        boost::mutex::scoped_lock lock(*qcond.first);
        if (qcond.second->timed_wait(lock, boost::posix_time::seconds(timeout))) {
            std::string reply;
            int consensus = queryMgr.reply(uuid, reply); // TODO Handle consensus
            queryMgr.purge(uuid); // clean up
            return reply;
        }

        queryMgr.purge(uuid); // clean up

        // Error if we reach here
        throw XRouterError("Failed to get response in time. Try xrGetReply command later.", xrouter::SERVER_TIMEOUT);

    } catch (XRouterError & e) {
        Object error;
        error.emplace_back(Pair("error", e.msg));
        error.emplace_back(Pair("code", e.code));
        error.emplace_back(Pair("uuid", uuid));
        LOG() << e.msg;
        return json_spirit::write_string(Value(error), true);
    }
}

std::string App::getReply(const std::string & id)
{
    auto replies = queryMgr.allReplies(id);

    if (replies.empty()) {
        Object result;
        result.emplace_back("error", "No replies found");
        result.emplace_back("code", xrouter::NO_REPLIES);
        return json_spirit::write_string(Value(result), true);
    }

    Value result;

    if (replies.size() == 1) {
        std::string reply = replies.begin()->second;
        try {
            Value reply_val;
            static const std::string nouuid;
            read_string(write_string(Value(form_reply(nouuid, reply))), reply_val);
            result = reply_val;
        } catch (...) { }
    } else {
        Array arr;
        for (auto & it : replies) {
            std::string reply = it.second;
            try {
                Value reply_val;
                static const std::string nouuid;
                read_string(write_string(Value(form_reply(nouuid, reply))), reply_val);
                arr.emplace_back(reply_val);
            } catch (...) { }
        }
        result = arr;
    }

    return json_spirit::write_string(Value(result), true);
}

bool App::generatePayment(CNode *pnode, const CAmount fee, std::string & payment)
{
    const auto nodeAddr = pnode->addr.ToString();
    if (!hasConfig(nodeAddr))
        throw std::runtime_error("No config found for servicenode: " + nodeAddr);

    if (fee <= 0)
        return true;

    // Get payment address
    std::string snodeAddress;
    if (!getPaymentAddress(nodeAddr, snodeAddress))
        return false;

    bool res = createAndSignTransaction(snodeAddress, fee, payment);
    if (!res)
        return false;

    return true;
}

bool App::getPaymentAddress(const NodeAddr & nodeAddr, std::string & paymentAddress)
{
    // Payment address = pubkey Collateral address of snode
    std::vector<CServicenode> snodes = getServiceNodes();
    for (CServicenode & s : snodes) {
        if (s.addr.ToString() == nodeAddr) {
            paymentAddress = CBitcoinAddress(s.pubKeyCollateralAddress.GetID()).ToString();
            return true;
        }
    }
    
    if (debug_on_client()) {
        paymentAddress = changeAddress();
        return true;
    }

    return false;
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

std::string App::sendXRouterConfigRequest(CNode* node, std::string addr) {
    const auto & uuid = generateUUID();
    addQuery(uuid, node->addr.ToString());
    queryMgr.addQuery(uuid);

    XRouterPacketPtr packet(new XRouterPacket(xrGetConfig, uuid));
    packet->append(addr);
    packet->sign(cpubkey, cprivkey);
    node->PushMessage("xrouter", packet->body());

    return uuid;
}

std::string App::sendXRouterConfigRequestSync(CNode* node) {
    const auto & uuid = generateUUID();
    addQuery(uuid, node->addr.ToString());
    QueryMgr::QueryCondition qcond;
    queryMgr.addQuery(uuid, qcond);

    XRouterPacketPtr packet(new XRouterPacket(xrGetConfig, uuid));
    packet->sign(cpubkey, cprivkey);
    node->PushMessage("xrouter", packet->body());

    // Wait for response
    boost::mutex::scoped_lock lock(*qcond.first);
    int timeout = xrsettings->configSyncTimeout();
    if (!qcond.second->timed_wait(lock, boost::posix_time::seconds(timeout))) {
        queryMgr.purge(uuid); // clean up
        return "Could not get XRouter config";
    }

    std::string reply;
    int consensus = queryMgr.reply(uuid, reply); // TODO Handle consensus
    queryMgr.purge(uuid); // clean up

    // Update settings for node
    auto settings = std::make_shared<XRouterSettings>(reply);
    updateConfig(node->addr.ToString(), settings);

    return reply;
}

void App::reloadConfigs() {
    LOG() << "Reloading xrouter config from file " << xrouterpath;
    xrsettings->read(xrouterpath.c_str());
    xrsettings->loadPlugins();
    xrsettings->loadWallets();
    createConnectors();
}

std::string App::getStatus() {
    Object result;
    result.emplace_back(Pair("enabled", isEnabled()));
    result.emplace_back(Pair("config", xrsettings->rawText()));
    Object myplugins;
    for (std::string s : xrsettings->getPlugins())
        myplugins.emplace_back(s, xrsettings->getPluginSettings(s)->rawText());
    result.emplace_back(Pair("plugins", myplugins));

    LOCK(_lock);

    Array nodes;
    for (auto& it : this->snodeConfigs) {
        Object val;
        //val.emplace_back("node", it.first);
        val.emplace_back("config", it.second->rawText());
        Object plugins;
        for (std::string s : it.second->getPlugins())
            plugins.emplace_back(s, it.second->getPluginSettings(s)->rawText());
        val.emplace_back(Pair("plugins", plugins));
        nodes.push_back(Value(val));
    }
    
    result.emplace_back(Pair("nodes", nodes));
    
    return json_spirit::write_string(Value(result), true);
}

std::string App::createDepositAddress(std::string & uuidRet, bool update) {
    uuidRet = generateUUID();

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
        xrsettings->set("Main.depositaddress", addr.ToString());
        xrsettings->set("Main.depositpubkey", HexStr(my_pubkey));
        xrsettings->write();
    }

    return json_spirit::write_string(Value(result), true);
}

void App::runTests() {
    server->runPerformanceTests();
}

bool App::debug_on_client() {
    return xrsettings->get<int>("Main.debug_on_client", 0) != 0;
}

bool App::isDebug() {
    //int b;
    //verifyDomain("98fa59764df5d2022994ca98e8ad3ca795681920bb7cad0af8df07ab48539ac6", "antihype", "yBW61mwkjuqFK1rVfm2Az2s2WU5Vubrhhw", b);
    return xrsettings->get<int>("Main.debug", 0) != 0;
}

std::string App::getMyPaymentAddress() {
    return server->getMyPaymentAddress();
}

std::string App::registerDomain(std::string & uuidRet, const std::string & domain, const std::string & addr, bool update) {
    uuidRet = generateUUID();
    std::string result = generateDomainRegistrationTx(domain, addr);
    
    if (update) {
        xrsettings->set("Main.domain", domain);
        xrsettings->set("Main.domain_tx", result);
        xrsettings->write();
    }
    
    return result;
}

bool App::checkDomain(std::string & uuidRet, const std::string & domain) {
    uuidRet = generateUUID();

    if (!hasDomain(domain))
        return false;

    const auto nodeAddr = getDomainNode(domain);
    auto settings = getConfig(nodeAddr);

    std::string tx = settings->get<std::string>("Main.domain_tx", "");
    if (tx.empty())
        return false;

    std::string addr;
    if (!getPaymentAddress(nodeAddr, addr))
        return false;

    int block;
    if (verifyDomain(tx, domain, addr, block))
        return true;

    return false;
}

void App::onTimer() {
    static uint32_t counter = -1;
    ++counter;

    // Update configs after set time
    if ((counter * XROUTER_TIMER_SECONDS) % 300 == 0) {
        counter = 0;
        updateConfigs();
    }

    timer.expires_at(boost::posix_time::second_clock::universal_time() + boost::posix_time::seconds(XROUTER_TIMER_SECONDS));
    timer.async_wait(boost::bind(&App::onTimer, this));
}

bool App::servicenodePubKey(const NodeAddr & node, std::vector<unsigned char> & pubkey)  {
    std::vector<CServicenode> vServicenodes = getServiceNodes();
    for (const auto & snode : vServicenodes) {
        if (node != snode.addr.ToString())
            continue;
        auto key = snode.pubKeyServicenode;
        if (!key.IsCompressed() && !key.Compress())
            return false;
        pubkey = std::vector<unsigned char>{key.begin(), key.end()};
        return true;
    }
    return false;
}

} // namespace xrouter
