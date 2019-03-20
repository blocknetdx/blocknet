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
App::App() : timerThread(boost::bind(&boost::asio::io_service::run, &timerIo))
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

    xrsettings = std::make_shared<XRouterSettings>();
    server = std::make_shared<XRouterServer>();

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

bool App::openConnections(const std::string & fqService, const uint32_t & count)
{
    if (fqService.empty())
        return false;
    if (count < 1) {
        ERR() << "Cannot open connections: " << std::to_string(count) << ", possibly bad config entry for "
              << fqService;
        return false;
    }

    std::vector<CNode*> nodes;
    {
        LOCK(cs_vNodes);
        nodes = vNodes;
    }

    // Build node cache
    std::map<std::string, CNode*> nodec;
    for (auto & pnode : nodes) {
        if (!pnode->addr.ToString().empty())
            nodec[pnode->addr.ToString()] = pnode;
    }

    uint32_t connected{0};

    // Manages fetching the config from specified nodes
    auto fetchConfig = [this](CNode *node, CServicenode & snode, uint32_t & connected) {
        const std::string & nodeAddr = node->addr.ToString();
        std::chrono::time_point<std::chrono::system_clock> time = std::chrono::system_clock::now();
        updateConfigTime(nodeAddr, time);

        // fetch config
        std::string uuid = sendXRouterConfigRequest(node);
        LOG() << "Requesting config from snode " << CBitcoinAddress(snode.pubKeyCollateralAddress.GetID()).ToString()
              << " request id = " << uuid;
        auto qcond = queryMgr.queryCond(uuid, nodeAddr);
        if (qcond) {
            auto l = queryMgr.queryLock(uuid, nodeAddr);
            boost::mutex::scoped_lock lock(*l);
            if (qcond->timed_wait(lock, boost::posix_time::milliseconds(3500),
                [this,&uuid,&nodeAddr]() { return ShutdownRequested() || queryMgr.hasReply(uuid, nodeAddr); }))
            {
                if (queryMgr.hasReply(uuid, nodeAddr))
                    ++connected;
            }
            queryMgr.purge(uuid); // clean up
        }
    };

    std::vector<CServicenode> snodes = getServiceNodes();

    // Check if existing snode connections have what we need
    for (auto & s : snodes) {
        if (connected >= count)
            break; // done, found enough nodes

        const auto & snodeAddr = s.addr.ToString();
        if (!nodec.count(snodeAddr))
            continue;

        if (!s.HasService(xr)) // has xrouter
            continue;

        if (!s.HasService(fqService)) // has the service
            continue;

        fetchConfig(nodec[snodeAddr], s, connected);
    }

    if (connected >= count)
        return true; // done if we have enough existing connections

    // Open connections to all service nodes up to the desired number that are not already our peers
    for (auto & s : snodes) {
        if (connected >= count)
            break; // done, found enough nodes

        if (!s.HasService(xr)) // has xrouter
            continue;

        if (!s.HasService(fqService)) // has the service
            continue;

        const auto & snodeAddr = s.addr.ToString();

        // Do not connect if already a peer
        if (snodeAddr.empty() || nodec.count(snodeAddr))
            continue;

        // Connect to snode
        CAddress addr;
        CNode *node = OpenXRouterConnection(addr, snodeAddr.c_str());
        if (node) { // Filters out bad nodes (banned, etc)
            LOG() << "Connected to servicenode " << CBitcoinAddress(s.pubKeyCollateralAddress.GetID()).ToString();
            fetchConfig(node, s, connected);
        } else
            LOG() << "Failed to connect to servicenode " << CBitcoinAddress(s.pubKeyCollateralAddress.GetID()).ToString();
    }

    return connected >= count;
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
    for (CServicenode & s : vServicenodes) {
        if (!s.addr.ToString().empty())
            snodes[s.addr.ToString()] = s;
    }

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
    timer.cancel();
    timerIo.stop();
    timerThread.join();

    if (!isEnabled())
        return false;

    if (!server->stop())
        return false;

    return true;
}
 
//*****************************************************************************
//*****************************************************************************
std::vector<CNode*> App::getAvailableNodes(enum XRouterCommand command, const std::string & service, int count)
{
    // Send only to the service nodes that have the required wallet
    std::vector<CServicenode> vServicenodes = getServiceNodes();

    std::vector<CNode*> selectedNodes;
    std::vector<CNode*> nodes;
    {
        LOCK(cs_vNodes);
        nodes = vNodes;
    }

    // Build snode cache
    std::map<std::string, CServicenode> snodes;
    for (CServicenode & s : vServicenodes) {
        if (!s.addr.ToString().empty())
            snodes[s.addr.ToString()] = s;
    }

    // Build node cache
    std::map<std::string, CNode*> nodec;
    for (auto & pnode : nodes) {
        const auto addr = pnode->addr.ToString();
        if (snodes.count(addr))
            nodec[addr] = pnode;
    }

    auto maxfee = xrsettings->getMaxFee(command, service);
    int supported = 0;
    int ready = 0;

    auto configs = getConfigs();
    if (configs.empty()) // don't have any configs
        return selectedNodes;

    for (const auto & item : configs)
    {
        const auto nodeAddr = item.first;
        auto settings = item.second;
        const std::string commandStr(XRouterCommand_ToString(command));
        const auto & fqCmd = // fully qualified command e.g. xr::ServiceName
                (command == xrService ?buildCommandKey(service) : buildCommandKey(service, commandStr));

        if (command != xrService && !settings->walletEnabled(service))
            continue;
        if (!settings->isAvailableCommand(command, service))
            continue;
        if (!snodes.count(nodeAddr)) // Ignore if not a snode
            continue;
        if (!snodes[nodeAddr].HasService(fqCmd)) // Ignore snodes that don't have the service
            continue;

        supported++;

        // This is the node whose config we are looking at now
        CNode *node = nullptr;
        if (nodec.count(nodeAddr))
            node = nodec[nodeAddr];

        // If the service node is not among peers skip
        if (!node)
            continue;

        const auto & snodeAddr = CBitcoinAddress(snodes[nodeAddr].pubKeyCollateralAddress.GetID()).ToString();

        // Only select nodes with a fee smaller than the max fee we're willing to pay
        if (maxfee > 0) {
            auto fee = settings->getCommandFee(command, service);
            if (fee > maxfee) {
                LOG() << "Skipping node " << snodeAddr << " because its fee=" << fee << " is higher than maxfee=" << maxfee;
                continue;
            }
        }

        auto rateLimit = settings->clientRequestLimit(command, service);
        if (hasSentRequest(nodeAddr, fqCmd) && rateLimitExceeded(nodeAddr, fqCmd, getLastRequest(nodeAddr, fqCmd), rateLimit)) {
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

    if (ready < count)
        throw XRouterError("Could not find " + std::to_string(count) + " Service Node(s), only found " + std::to_string(ready), xrouter::NOT_ENOUGH_NODES);
    
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
    for (CServicenode & s : vServicenodes) {
        if (!s.addr.ToString().empty())
            snodes[s.addr.ToString()] = s;
    }

    // Build node cache
    std::map<std::string, CNode*> nodec;
    for (auto & pnode : nodes) {
        const auto addr = pnode->addr.ToString();
        if (snodes.count(addr))
            nodec[addr] = pnode;
    }

    double maxfee = xrsettings->getMaxFee(xrService, "");

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
    if (!queryMgr.hasQuery(uuid, nodeAddr) || queryMgr.hasReply(uuid, nodeAddr))
        return false; // done, nothing found

    // Verify servicenode response
    std::vector<unsigned char> spubkey;
    if (!servicenodePubKey(nodeAddr, spubkey) || !packet->verify(spubkey)) {
        state.DoS(20, error("XRouter: unsigned packet or signature error"), REJECT_INVALID, "xrouter-error");
        return false;
    }

    uint32_t offset = 0;
    std::string reply((const char *)packet->data()+offset);
    offset += reply.size() + 1;

    // Store the reply
    queryMgr.addReply(uuid, nodeAddr, reply);
    queryMgr.purge(uuid, nodeAddr);

    LOG() << "Received reply to query " << uuid;
    LOG() << reply;

    return true;
}

bool App::processConfigReply(CNode *node, XRouterPacketPtr packet, CValidationState & state)
{
    const auto & uuid = packet->suuid();
    const auto & nodeAddr = node->addr.ToString();

    // Do not process if we aren't expecting a result. Also prevent reply malleability (only first reply is accepted)
    if (!queryMgr.hasQuery(uuid, nodeAddr) || queryMgr.hasReply(uuid, nodeAddr))
        return false; // done, nothing found

    // Verify servicenode response
    std::vector<unsigned char> spubkey;
    if (!servicenodePubKey(nodeAddr, spubkey) || !packet->verify(spubkey)) {
        state.DoS(20, error("XRouter: unsigned packet or signature error"), REJECT_INVALID, "xrouter-error");
        return false;
    }

    uint32_t offset = 0;
    std::string reply((const char *)packet->data()+offset);
    offset += reply.size() + 1;

    Value reply_val;
    read_string(reply, reply_val);
    Object reply_obj = reply_val.get_obj();
    std::string config = find_value(reply_obj, "config").get_str();
    Object plugins = find_value(reply_obj, "plugins").get_obj();
    
    auto settings = std::make_shared<XRouterSettings>(config);
    settings->assignNode(nodeAddr);

    for (Object::size_type i = 0; i != plugins.size(); i++ ) {
        auto psettings = std::make_shared<XRouterPluginSettings>();
        psettings->read(plugins[i].value_.get_str());
        settings->addPlugin(plugins[i].name_, psettings);
    }

    // Update settings for node
    updateConfig(nodeAddr, settings);
    queryMgr.addReply(uuid, nodeAddr, reply);
    queryMgr.purge(uuid, nodeAddr);

    LOG() << "Received reply to query " << uuid << " from node " << nodeAddr;
    LOG() << reply;

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
    LOG() << "XRouter command: " << command << " from node " << nodeAddr << " with uuid " << uuid;

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
std::string App::xrouterCall(enum XRouterCommand command, std::string & uuidRet, const std::string & service,
                             const int & confirmations, const std::vector<std::string> & params)
{
    const std::string & uuid = generateUUID();
    uuidRet = uuid; // set uuid

    try {
        if (!isEnabled())
            throw XRouterError("XRouter is turned off. Please set 'xrouter=1' in blocknetdx.conf", xrouter::UNAUTHORIZED);

        if (command != xrService) {
            // Check param1
            switch (command) {
                case xrGetBlockHash:
                case xrGetAllBlocks:
                    if (params.size() > 0 && !is_number(params[0]))
                        throw XRouterError("Incorrect block number: " + params[0], xrouter::INVALID_PARAMETERS);
                    break;
                case xrGetTxBloomFilter:
                    if (params.size() > 0 && (!is_hash(params[0]) || (params[0].size() % 10 != 0)))
                        throw XRouterError("Incorrect bloom filter: " + params[0], xrouter::INVALID_PARAMETERS);
                    break;
                case xrGetBlock:
                case xrGetTransaction:
                    if (params.size() > 0 && !is_hash(params[0]))
                        throw XRouterError("Incorrect hash: " + params[0], xrouter::INVALID_PARAMETERS);
                    break;
                case xrGetAllTransactions:
                case xrGetBalanceUpdate:
                    if (params.size() > 0 && !is_address(params[0]))
                        throw XRouterError("Incorrect address: " + params[0], xrouter::INVALID_PARAMETERS);
                    break;
                default:
                    break;
            }

            // Check param2
            switch (command) {
                case xrGetAllTransactions:
                case xrGetBalanceUpdate:
                case xrGetTxBloomFilter:
                    if (params.size() > 1 && !is_number(params[1]))
                        throw XRouterError("Incorrect block number: " + params[1], xrouter::INVALID_PARAMETERS);
                    break;
                default:
                    break;
            }
        }

        // Confirmations
        auto confs = std::max(xrsettings->confirmations(command, service, XROUTER_DEFAULT_CONFIRMATIONS), confirmations);
        const auto fqService = buildCommandKey(service, XRouterCommand_ToString(command));

        // Open connections (at least number equal to how many confirmations we want)
        if (!openConnections(fqService, static_cast<uint32_t>(confs))) {
            std::string err("Not enough connections, require " + std::to_string(confs) + " for " + fqService);
            ERR() << err;
            throw XRouterError(err, xrouter::NOT_ENOUGH_NODES);
        }

        // Select available nodes
        std::vector<CNode*> selectedNodes = getAvailableNodes(command, service, confs);
        auto selected = static_cast<int>(selectedNodes.size());
        
        if (selected < confs)
            throw XRouterError("Failed to find " + std::to_string(confs) + " service node(s) supporting " +
                               fqService + " found " + std::to_string(selected), xrouter::NOT_ENOUGH_NODES);
        
        std::map<std::string, std::string> paytx_map;
        int snodeCount = 0;
        std::string fundErr{"Could not create payments to service nodes. Please check that your wallet "
                            "is fully unlocked and you have at least " + std::to_string(confs) +
                            " available unspent transaction outputs."};

        // Obtain all the snodes that meet our criteria
        for (CNode* pnode : selectedNodes) {
            const auto & addr = pnode->addr.ToString();
            if (!hasConfig(addr))
                continue; // skip nodes that do not have configs

            // Create the fee payment
            std::string feePayment;
            CAmount fee = to_amount(getConfig(addr)->getCommandFee(command, service));
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
            if (snodeCount == confs)
                break;
        }

        // Do we have enough snodes? If not unlock utxos
        if (snodeCount < confs) {
            for (const auto & item : paytx_map) {
                const std::string & tx = item.second;
                unlockOutputs(tx);
            }
            throw XRouterError("Unable to find " + std::to_string(confs) +
                               " service node(s) to process request", xrouter::NOT_ENOUGH_NODES);
        }

        int timeout = xrsettings->commandTimeout(command, service);

        // Send xrouter request to each selected node
        for (CNode* pnode : selectedNodes) {
            const auto & addr = pnode->addr.ToString();
            if (!paytx_map.count(addr))
                continue;

            // Record the node sending request to
            addQuery(uuid, addr);
            queryMgr.addQuery(uuid, addr);

            json_spirit::Object jbody;
            jbody.emplace_back("feetx", paytx_map[addr]);

            // Send packet to xrouter node
            XRouterPacketPtr packet(new XRouterPacket(command, uuid));
            packet->append(service);
            packet->append(json_spirit::write_string(Value(jbody)));
            for (const std::string & p : params)
                packet->append(p);
            packet->sign(cpubkey, cprivkey);
            pnode->PushMessage("xrouter", packet->body());
            
            std::chrono::time_point<std::chrono::system_clock> time = std::chrono::system_clock::now();
            if (!lastPacketsSent.count(addr))
                lastPacketsSent[addr] = std::map<std::string, std::chrono::time_point<std::chrono::system_clock> >();
            lastPacketsSent[addr][fqService] = time;

            LOG() << "Sent message to node " << pnode->addrName;
        }

        // At this point we need to wait for responses
        int confirmation_count = 0;
        auto queryCheckStart = GetAdjustedTime();
        auto queries = queryMgr.allLocks(uuid);
        std::vector<NodeAddr> review;
        for (auto & query : queries)
            review.push_back(query.first);

        // Check that all replies have arrived, only run as long as timeout
        while (!ShutdownRequested() && confirmation_count < confs
            && GetAdjustedTime() - queryCheckStart < timeout)
        {
            boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
            for (int i = review.size() - 1; i >= 0; --i)
                if (queryMgr.hasReply(uuid, review[i])) {
                    ++confirmation_count;
                    review.erase(review.begin()+i);
                }
        }

        // Clean up
        queryMgr.purge(uuid);

        if (confirmation_count < confs)
            throw XRouterError("Failed to get response in time. Try xrGetReply command later.", xrouter::SERVER_TIMEOUT);

        // Handle the results
        std::string result;
        int c = queryMgr.mostCommonReply(uuid, result); // TODO Send all replies and handle confirmations
//        if (c <= confs || result.empty())
//            throw XRouterError("No consensus between responses", xrouter::INTERNAL_SERVER_ERROR);

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
    return this->xrouterCall(xrGetBlockCount, uuidRet, currency, confirmations, {});
}

std::string App::getBlockHash(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & blockId)
{
    return this->xrouterCall(xrGetBlockHash, uuidRet, currency, confirmations, { blockId });
}

std::string App::getBlock(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & blockHash)
{
    return this->xrouterCall(xrGetBlock, uuidRet, currency, confirmations, { blockHash });
}

std::string App::getTransaction(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & hash)
{
    return this->xrouterCall(xrGetTransaction, uuidRet, currency, confirmations, { hash });
}

std::string App::getAllBlocks(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & number)
{
    return this->xrouterCall(xrGetAllBlocks, uuidRet, currency, confirmations, { number });
}

std::string App::getAllTransactions(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & account, const std::string & number)
{
    return this->xrouterCall(xrGetAllTransactions, uuidRet, currency, confirmations, { account, number });
}

std::string App::getBalance(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & account)
{
    return this->xrouterCall(xrGetBalance, uuidRet, currency, confirmations, { account });
}

std::string App::getBalanceUpdate(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & account, const std::string & number)
{
    return this->xrouterCall(xrGetBalanceUpdate, uuidRet, currency, confirmations, { account, number });
}

std::string App::getTransactionsBloomFilter(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & filter, const std::string & number)
{
    return this->xrouterCall(xrGetTxBloomFilter, uuidRet, currency, confirmations, { filter, number });
}

std::string App::convertTimeToBlockCount(std::string & uuidRet, const std::string& currency, const int & confirmations, std::string time) {

    return this->xrouterCall(xrGetBlockForTime, uuidRet, currency, confirmations, { time });
}

std::string App::sendTransaction(std::string & uuidRet, const std::string & currency, const std::string & transaction)
{
    int confirmations{1}; // default 1 on this call
    return this->xrouterCall(xrSendTransaction, uuidRet, currency, confirmations, { transaction });
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
    const auto & nodeAddr = node->addr.ToString();
    addQuery(uuid, nodeAddr);
    queryMgr.addQuery(uuid, nodeAddr);

    XRouterPacketPtr packet(new XRouterPacket(xrGetConfig, uuid));
    packet->append(addr);
    packet->sign(cpubkey, cprivkey);
    node->PushMessage("xrouter", packet->body());

    return uuid;
}

std::string App::sendXRouterConfigRequestSync(CNode* node) {
    const auto & uuid = generateUUID();
    const auto & nodeAddr = node->addr.ToString();
    addQuery(uuid, nodeAddr);
    queryMgr.addQuery(uuid, nodeAddr);

    XRouterPacketPtr packet(new XRouterPacket(xrGetConfig, uuid));
    packet->sign(cpubkey, cprivkey);
    node->PushMessage("xrouter", packet->body());

    // Wait for response
    auto qcond = queryMgr.queryCond(uuid, nodeAddr);
    if (qcond) {
        int timeout = xrsettings->configSyncTimeout();
        auto l = queryMgr.queryLock(uuid, nodeAddr);
        boost::mutex::scoped_lock lock(*l);
        if (!qcond->timed_wait(lock, boost::posix_time::seconds(timeout),
                [this,&uuid,&nodeAddr]() { return ShutdownRequested() || queryMgr.hasReply(uuid, nodeAddr); }))
        {
            queryMgr.purge(uuid); // clean up
            return "Could not get XRouter config";
        }
    }

    std::string reply;
    queryMgr.reply(uuid, nodeAddr, reply);

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
