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
        return false;

    xrouterpath = GetDataDir(false) / "xrouter.conf";
    LOG() << "Loading xrouter config from file " << xrouterpath.string();
    xrsettings = std::make_shared<XRouterSettings>();
    if (!xrsettings->init(xrouterpath))
        return false;

    server = std::make_shared<XRouterServer>();

    return true;
}

bool App::start()
{
    if (!isEnabled())
        return false;

    // Only start server mode if we're a servicenode
    if (fServiceNode) {
        bool res = server->start();
        if (res) { // set outgoing xrouter requests to use snode key
            WaitableLock l(mu);
            cpubkey = server->pubKey();
            cprivkey = server->privKey();
        }
        // Update the node's default payment address
        if (activeServicenode.status == ACTIVE_SERVICENODE_STARTED) {
            auto pmn = mnodeman.Find(activeServicenode.vin);
            if (pmn)
                updatePaymentAddress(pmn->pubKeyCollateralAddress);
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

    {
        WaitableLock l(mu);
        xrouterIsReady = true;
    }

    return true;
}

/**
 * Returns true if XRouter is ready to receive packets.
 * @return
 */
bool App::isReady() {
    WaitableLock l(mu);
    return xrouterIsReady;
}

/**
 * Returns true if xrouter is ready to accept packets.
 * @return
 */
bool App::canListen() {
    return isReady() && activeServicenode.status == ACTIVE_SERVICENODE_STARTED;
}

// We append "xr" if XRouter is activated at all, "xr::spv_command" for wallet commands,
// and "xrs::service_name" for custom services (plugins).
std::vector<std::string> App::getServicesList() 
{
    std::vector<std::string> result;
    if (!isEnabled() || !isReady())
        return result;

    // If shutting down send empty list
    if (ShutdownRequested())
        return result;

    result.push_back(xr); // signal xrouter support
    std::string services = xr;

    // Add wallet services
    for (const std::string & s : xrsettings->getWallets()) {
        const auto w = walletCommandKey(xr, s);
        result.push_back(w);
        services += "," + w;
    }

    // Add plugin services
    for (const std::string & s : xrsettings->getPlugins()) {
        if (!xrsettings->isAvailableCommand(xrService, s)) // exclude any disabled plugins
            continue;
        const auto p = pluginCommandKey(s);
        result.push_back(p);
        services += "," + p;
    }

    return result;
}

static std::vector<CServicenode> getServiceNodes()
{
    return mnodeman.GetCurrentList();
}

bool App::createConnectors() {
    if (fServiceNode && isEnabled())
        return server->createConnectors();
    ERR() << "Failed to load wallets: Must be a servicenode with xrouter=1 specified in config";
    return false;
}

bool App::openConnections(enum XRouterCommand command, const std::string & service, const uint32_t & count,
                          const std::vector<CNode*> & skipNodes)
{
    const auto fqService = (command == xrService) ? pluginCommandKey(service) // plugin
                                                  : walletCommandKey(service, XRouterCommand_ToString(command)); // spv wallet
    // use top-level wallet key (e.g. xr::BLOCK)
    const auto fqServiceAdjusted = (command == xrService) ? fqService
                                                          : walletCommandKey(service);
    if (fqServiceAdjusted.empty())
        return false;
    if (count < 1 || count > 50) {
        WARN() << "Cannot open less than 1 connection or more than 50, attempted: " << std::to_string(count) << " "
               << fqService;
        return false;
    }

    std::vector<CServicenode> snodes;
    std::vector<CNode*> nodes;
    std::map<NodeAddr, CServicenode> snodec;
    std::map<NodeAddr, CNode*> nodec;
    getLatestNodeContainers(snodes, nodes, snodec, nodec);
    CWaitableCriticalSection lu;

    uint32_t connected{0};
    std::set<NodeAddr> connectedSnodes;
    for (auto & pnode : skipNodes) // add skipped nodes to prevent connection attempts
        connectedSnodes.insert(pnode->NodeAddress());

    // Retain node and add to containers
    auto addNode = [&lu,&nodes,&nodec](CNode *pnode) {
        if (!pnode)
            return;
        WaitableLock l(lu);
        pnode->AddRef();
        nodes.push_back(pnode);
        nodec[pnode->NodeAddress()] = pnode;
    };

    auto connectedCount = [&connected,&lu]() {
        WaitableLock l(lu);
        return connected;
    };

    // Only select nodes with a fee smaller than the max fee we're willing to pay
    const auto maxfee = xrsettings->maxFee(command, service);
    auto failedChecks = [this,maxfee,command,service,fqService](const NodeAddr & nodeAddr,
                                                                XRouterSettingsPtr settings) -> bool
    {
        if (maxfee > 0) {
            auto fee = settings->commandFee(command, service);
            if (fee > maxfee)
                return true;
        }
        auto rateLimit = settings->clientRequestLimit(command, service);
        return rateLimitExceeded(nodeAddr, fqService, getLastRequest(nodeAddr, fqService), rateLimit);
    };

    auto addSelected = [this,&connected,&connectedSnodes,&lu,&failedChecks](const std::string & snodeAddr) -> bool {
        WaitableLock l(lu);
        if (!hasConfig(snodeAddr) || connectedSnodes.count(snodeAddr))
            return false;
        auto settings = getConfig(snodeAddr);
        if (settings && !failedChecks(snodeAddr, settings)) {
            ++connected;
            connectedSnodes.insert(snodeAddr);
            return true;
        }
        return false;
    };

    // Check if existing snode connections have what we need
    std::map<NodeAddr, CServicenode> snodesNeedConfig;
    for (auto & s : snodes) {
        if (connected >= count)
            break; // done, found enough nodes

        const auto & snodeAddr = s.addr.ToString();
        if (!nodec.count(snodeAddr)) // skip non-connected nodes
            continue;

        if (connectedSnodes.count(snodeAddr)) // skip already selected nodes
            continue;

        if (!s.HasService(xr)) // has xrouter
            continue;

        if (!s.HasService(fqServiceAdjusted)) // has the service
            continue;

        if (hasConfig(snodeAddr)) // has config, count it
            addSelected(snodeAddr);
        else
            snodesNeedConfig[snodeAddr] = s;
    }

    if (connected >= count) {
        releaseNodes(nodes);
        return true; // done if we have enough existing connections
    }

    // Manages fetching the config from specified nodes
    auto fetchConfig = [this,&addSelected](CNode *node, const CServicenode & snode)
    {
        const std::string & nodeAddr = node->NodeAddress();

        // fetch config
        updateSentRequest(nodeAddr, XRouterCommand_ToString(xrGetConfig));
        std::string uuid = sendXRouterConfigRequest(node);
        LOG() << "Requesting config from snode " << CBitcoinAddress(snode.pubKeyCollateralAddress.GetID()).ToString()
              << " query " << uuid;

        auto qcond = queryMgr.queryCond(uuid, nodeAddr);
        if (qcond) {
            auto l = queryMgr.queryLock(uuid, nodeAddr);
            boost::mutex::scoped_lock lock(*l);
            if (qcond->timed_wait(lock, boost::posix_time::milliseconds(3500),
                                  [this,&uuid,&nodeAddr]() { return ShutdownRequested() || queryMgr.hasReply(uuid, nodeAddr); }))
            {
                if (queryMgr.hasReply(uuid, nodeAddr))
                    addSelected(nodeAddr);
            }
            queryMgr.purge(uuid); // clean up
        }
    };

    auto connect = [this,&lu,&connectedSnodes,&fetchConfig,&addSelected,&addNode](const std::string & snodeAddr,
                                                                                  const CServicenode & snode)
    {
        // If we have a pending connection proceed in this block and wait for it to complete, otherwise add a pending
        // connection if none found and then skip waiting here to avoid race conditions and proceed to open a connection
        PendingConnectionMgr::PendingConnection conn;
        if (!pendingConnMgr.addPendingConnection(snodeAddr, conn)) {
            auto l = pendingConnMgr.connectionLock(snodeAddr);
            auto cond = pendingConnMgr.connectionCond(snodeAddr);
            if (l && cond) {
                boost::mutex::scoped_lock lock(*l);
                if (cond->timed_wait(lock, boost::posix_time::milliseconds(4500),
                    [this,&snodeAddr]() { return ShutdownRequested() || !pendingConnMgr.hasPendingConnection(snodeAddr); }))
                {
                    bool alreadyConnected{false};
                    {
                        WaitableLock l(lu);
                        alreadyConnected = connectedSnodes.count(snodeAddr) > 0;
                    }
                    if (ShutdownRequested() || alreadyConnected)
                        return; // no need to connect

                    // Make sure the connection has been found
                    bool found{false};
                    {
                        LOCK(cs_vNodes);
                        for (auto & pnode : vNodes) {
                            if (snodeAddr == pnode->NodeAddress()) {
                                found = true;
                                break;
                            }
                        }
                    }

                    if (found) { // if we found a valid connection
                        addSelected(snodeAddr);
                        return; // done
                    }
                }
            }
        }

        bool alreadyConnected{false};
        {
            WaitableLock l(lu);
            alreadyConnected = connectedSnodes.count(snodeAddr) > 0;
        }
        if (alreadyConnected) {
            pendingConnMgr.notify(snodeAddr);
            return; // already connected
        }

        // Connect to snode
        CAddress addr;
        CNode *node = OpenXRouterConnection(addr, snodeAddr.c_str()); // Filters out bad nodes (banned, etc)
        if (node) {
            LOG() << "Connected to servicenode " << CBitcoinAddress(snode.pubKeyCollateralAddress.GetID()).ToString();
            addNode(node); // store the node connection
            if (!hasConfig(snodeAddr) || needConfigUpdate(snodeAddr))
                fetchConfig(node, snode);
            else
                addSelected(snodeAddr);
        } else
            LOG() << "Failed to connect to servicenode " << CBitcoinAddress(snode.pubKeyCollateralAddress.GetID()).ToString();

        pendingConnMgr.notify(snodeAddr);
    };


    // Check our snode configs and connect to any nodes with required services
    // that we're not already connected to
    std::map<NodeAddr, CServicenode> needConnectionsHaveConfigs;
    auto configs = getConfigs();
    for (const auto & item : configs) {
        if (connected >= count)
            break; // done, found enough nodes

        const auto & snodeAddr = item.first;
        auto config = item.second;

        if (nodec.count(snodeAddr) || connectedSnodes.count(snodeAddr) || snodesNeedConfig.count(snodeAddr))
            continue; // already processed, skip

        if (config->hasWallet(service) || config->hasPlugin(service)) {
            if (snodec.count(snodeAddr)) // only connect if snode is in the list
                needConnectionsHaveConfigs[snodeAddr] = snodec[snodeAddr];
        }
    }

    // At this point all remaining snodes are ones we don't have configs for.
    std::map<NodeAddr, CServicenode> needConnectionsNoConfigs;
    for (auto & s : snodes) {
        if (connected >= count)
            break; // done, found enough nodes

        const auto & snodeAddr = s.addr.ToString();
        if (snodeAddr.empty()) // Sanity check
            continue;
        if (nodec.count(snodeAddr) || connectedSnodes.count(snodeAddr) || snodesNeedConfig.count(snodeAddr)
            || needConnectionsHaveConfigs.count(snodeAddr))
            continue; // already processed, skip

        if (!s.HasService(xr)) // has xrouter
            continue;

        if (!s.HasService(fqServiceAdjusted)) // has the service
            continue;

        needConnectionsNoConfigs[snodeAddr] = s;
    }

    // Connect to already connected snodes that need configs
    if (!snodesNeedConfig.empty() || !needConnectionsHaveConfigs.empty() || !needConnectionsNoConfigs.empty()) {
        std::vector<NodeAddr> all;
        for (auto & it : snodesNeedConfig)
            all.push_back(it.first);
        for (auto & it : needConnectionsHaveConfigs)
            all.push_back(it.first);
        for (auto & it : needConnectionsNoConfigs)
            all.push_back(it.first);

        if (all.size() < count - connectedCount()) { // not enough nodes
            releaseNodes(nodes);
            return connectedCount() >= count;
        }

        boost::thread_group tg;

        // Make connections via threads (max 2 per cpu core)
        for (auto & snodeAddr : all) {
            if (count - connectedCount() <= 0)
                break; // done, all connected!

            bool needConfig = snodesNeedConfig.count(snodeAddr) > 0;
            bool needConnectionHaveConfig = needConnectionsHaveConfigs.count(snodeAddr) > 0;
            bool needConnectionAndConfig = needConnectionsNoConfigs.count(snodeAddr) > 0;

            CServicenode s;
            if (needConfig)                    s = snodesNeedConfig[snodeAddr];
            else if (needConnectionHaveConfig) s = needConnectionsHaveConfigs[snodeAddr];
            else if (needConnectionAndConfig)  s = needConnectionsNoConfigs[snodeAddr];

            CNode *node = nullptr;
            if (needConfig) {
                node = nodec[snodeAddr];
                if (!node || node->Disconnecting())
                    continue;
            }

            tg.create_thread([node,s,snodeAddr,needConfig,needConnectionHaveConfig,
                              needConnectionAndConfig,&fetchConfig,&connect]()
            {
                RenameThread("blocknetdx-xrouter-connections");
                boost::this_thread::interruption_point();

                if (needConfig) {
                    if (!node || node->Disconnecting()) // ignore disconnecting nodes
                        return;
                    fetchConfig(node, s);
                } else if (needConnectionHaveConfig) {
                    connect(snodeAddr, s);
                } else if (needConnectionAndConfig) {
                    connect(snodeAddr, s);
                }
            });

            // Wait until we have all required connections (count - connected = 0)
            // Wait while thread group has more running threads than we need connections for -or-
            //            thread group has too many threads (2 per cpu core)
            while (tg.size() > (count - connectedCount()) || tg.size() >= boost::thread::hardware_concurrency() * 2) {
                if (ShutdownRequested()) {
                    releaseNodes(nodes);
                    tg.interrupt_all();
                    tg.join_all();
                    return false;
                }
                boost::this_thread::interruption_point();
                boost::this_thread::sleep_for(boost::chrono::milliseconds(50));
            }

            if (ShutdownRequested())
                break;
        }

        tg.join_all();
    }

    releaseNodes(nodes);
    return connected >= count;
}

std::string App::updateConfigs(bool force)
{
    if (!isEnabled() || !isReady())
        return "XRouter is turned off. Please set 'xrouter=1' in blocknetdx.conf to run XRouter.";
    
    std::vector<CServicenode> vServicenodes = getServiceNodes();
    std::vector<CNode*> nodes = CNode::CopyNodes();

    // Build snode cache
    std::map<std::string, CServicenode> snodes;
    for (CServicenode & s : vServicenodes) {
        if (!s.addr.ToString().empty())
            snodes[s.addr.ToString()] = s;
    }

    // Query servicenodes that haven't had configs updated recently
    for (CNode* pnode : nodes) {
        const auto nodeAddr = pnode->NodeAddress();
        // skip non-xrouter nodes, disconnecting nodes, and self
        if (!snodes.count(nodeAddr) || !snodes[nodeAddr].HasService(xr) || !pnode->SuccessfullyConnected()
            || pnode->Disconnecting() || snodes[nodeAddr].pubKeyServicenode == activeServicenode.pubKeyServicenode)
            continue;

        // If not force check, rate limit config requests
        if (!force) {
            const auto & service = XRouterCommand_ToString(xrGetConfig);
            if (!needConfigUpdate(nodeAddr, service))
                continue;
        }

        // Request the config
        auto & snode = snodes[nodeAddr]; // safe here due to check above
        updateSentRequest(nodeAddr, XRouterCommand_ToString(xrGetConfig));
        std::string uuid = sendXRouterConfigRequest(pnode);
        LOG() << "Requesting config from snode " << CBitcoinAddress(snode.pubKeyCollateralAddress.GetID()).ToString()
              << " query " << uuid;
    }

    releaseNodes(nodes);
    return "Config requests have been sent";
}

std::string App::printConfigs()
{
    WaitableLock l(mu);
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

    // shutdown threads
    requestHandlers.interrupt_all();
    requestHandlers.join_all();

    if (!server->stop())
        return false;

    return true;
}
 
//*****************************************************************************
//*****************************************************************************
std::vector<CNode*> App::availableNodesRetained(enum XRouterCommand command, const std::string & service, const int & count)
{
    std::vector<CNode*> selectedNodes;

    auto sconfigs = getConfigs();
    if (sconfigs.empty())
        return selectedNodes; // don't have any configs, return

    std::vector<CServicenode> snodes;
    std::vector<CNode*> nodes;
    std::map<NodeAddr, CServicenode> snodec;
    std::map<NodeAddr, CNode*> nodec;
    getLatestNodeContainers(snodes, nodes, snodec, nodec);

    // Max fee we're willing to pay to snodes
    auto maxfee = xrsettings->maxFee(command, service);

    // Look through known xrouter node configs and select those that support command/service/fees
    for (const auto & item : sconfigs) {
        const auto & nodeAddr = item.first;
        auto settings = item.second;

        // fully qualified command e.g. xr::ServiceName
        const auto & commandStr = XRouterCommand_ToString(command);
        const auto & fqCmd = (command == xrService) ? pluginCommandKey(service) // plugin
                                                    : walletCommandKey(service, commandStr); // spv wallet

        if (command != xrService && !settings->hasWallet(service))
            continue;
        if (!settings->isAvailableCommand(command, service))
            continue;
        if (!snodec.count(nodeAddr)) // Ignore if not a snode
            continue;
        if (!snodec[nodeAddr].HasService(command == xrService ? fqCmd : walletCommandKey(service))) // use top-level wallet key (e.g. xr::BLOCK)
            continue; // Ignore snodes that don't have the service

        // This is the node whose config we are looking at now
        CNode *node = nullptr;
        if (nodec.count(nodeAddr))
            node = nodec[nodeAddr];

        // If the service node is not among peers skip
        if (!node)
            continue;

        // Only select nodes with a fee smaller than the max fee we're willing to pay
        if (maxfee > 0) {
            auto fee = settings->commandFee(command, service);
            if (fee > maxfee) {
                const auto & snodeAddr = CBitcoinAddress(snodec[nodeAddr].pubKeyCollateralAddress.GetID()).ToString();
                LOG() << "Skipping node " << snodeAddr << " because its fee=" << fee << " is higher than maxfee=" << maxfee;
                continue;
            }
        }

        auto rateLimit = settings->clientRequestLimit(command, service);
        if (rateLimitExceeded(nodeAddr, fqCmd, getLastRequest(nodeAddr, fqCmd), rateLimit)) {
            const auto & snodeAddr = CBitcoinAddress(snodec[nodeAddr].pubKeyCollateralAddress.GetID()).ToString();
            LOG() << "Skipping node " << snodeAddr << " because not enough time passed since the last call";
            continue;
        }
        
        selectedNodes.push_back(node);
    }

    // Sort selected nodes descending by score
    std::sort(selectedNodes.begin(), selectedNodes.end(), [this](const CNode *a, const CNode *b) {
        return getScore(a->NodeAddress()) > getScore(b->NodeAddress());
    });

    // Retain selected nodes
    for (auto & pnode : selectedNodes)
        pnode->AddRef();

    // Release other nodes
    releaseNodes(nodes);

    return selectedNodes;
}

CNode* App::getNodeForService(std::string name)
{
    std::vector<CServicenode> vServicenodes = getServiceNodes();
    std::vector<CNode*> nodes = CNode::CopyNodes();

    auto releaseNodes = [](CCriticalSection & cs_, std::vector<CNode*> & ns_) {
        LOCK(cs_);
        for (auto & pnode : ns_)
            pnode->Release();
    };

    // Build snode cache
    std::map<std::string, CServicenode> snodes;
    for (CServicenode & s : vServicenodes) {
        if (!s.addr.ToString().empty())
            snodes[s.addr.ToString()] = s;
    }

    // Build node cache
    std::map<std::string, CNode*> nodec;
    for (auto & pnode : nodes) {
        const auto addr = pnode->NodeAddress();
        if (snodes.count(addr))
            nodec[addr] = pnode;
    }

    double maxfee = xrsettings->maxFee(xrService, name);

    // Domains
    if (name.find("/") != string::npos) {
        std::vector<std::string> parts;
        boost::split(parts, name, boost::is_any_of("/"));
        std::string domain = parts[0];
        std::string name_part = parts[1];
        if (!hasDomain(domain)) {
            releaseNodes(cs_vNodes, nodes);
            return nullptr; // Domain not found
        }

        std::vector<CNode*> candidates; // TODO Multiple candidates?
        const auto nodeAddr = snodeDomains[domain];

        if (!nodec.count(nodeAddr)) {
            releaseNodes(cs_vNodes, nodes);
            return nullptr; // unknown node
        }

        CNode *pnode = nodec[nodeAddr];
        candidates.push_back(pnode);

        // Check if node has settings
        auto settings = getConfig(nodeAddr);
        if (!settings->hasPlugin(name_part)) {
            releaseNodes(cs_vNodes, nodes);
            return nullptr;
        }

            // Check if fee is within expected range
        if (maxfee >= 0) {
            CAmount fee = to_amount(settings->getPluginSettings(name)->fee());
            if (fee > maxfee) {
                releaseNodes(cs_vNodes, nodes);
                return nullptr; // fee too expensive, ignore node
            }
        }

        CNode *res = nullptr;

        if (candidates.empty()) {
            releaseNodes(cs_vNodes, nodes);
            return nullptr;
        } else if (candidates.size() == 1)
            res = candidates[0];
        else {
            // Perform verification check of domain names
            int best_block = -1;
            for (CNode *node : candidates) {
                const auto nodeAddr = node->NodeAddress();
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
            
            if (!res) {
                releaseNodes(cs_vNodes, nodes);
                return nullptr;
            }
        }

        auto rateLimit = settings->getPluginSettings(name)->clientRequestLimit();
        if (rateLimitExceeded(nodeAddr, name, getLastRequest(nodeAddr, name), rateLimit)) {
            releaseNodes(cs_vNodes, nodes);
            return nullptr; // exceeded rate limit on this plugin
        }

        releaseNodes(cs_vNodes, nodes);
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
            CAmount fee = to_amount(settings->getPluginSettings(name)->fee());
            if (fee > maxfee)
                continue; // unwilling to pay fee, too much
        }

        releaseNodes(cs_vNodes, nodes);
        return res;
    }

    releaseNodes(cs_vNodes, nodes);
    return nullptr;
}

std::string App::parseConfig(XRouterSettingsPtr cfg)
{
    Object result;
    result.emplace_back("config", cfg->rawText());
    Object plugins;
    for (const std::string & s : cfg->getPlugins())
        plugins.emplace_back(s, cfg->getPluginSettings(s)->rawText());
    result.emplace_back("plugins", plugins);
    return json_spirit::write_string(Value(result), true);
}

//*****************************************************************************
//*****************************************************************************

bool App::processInvalid(CNode *node, XRouterPacketPtr packet, CValidationState & state)
{
    const auto & uuid = packet->suuid();
    const auto & nodeAddr = node->NodeAddress();

    // Do not process if we aren't expecting a result
    if (!queryMgr.hasNodeQuery(nodeAddr))
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

    // Log the invalid reply
    LOG() << "Received error reply to query " << uuid << "\n" << reply;

    return true;
}

bool App::processReply(CNode *node, XRouterPacketPtr packet, CValidationState & state)
{
    const auto & uuid = packet->suuid();
    const auto & nodeAddr = node->NodeAddress();

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

    LOG() << "Received reply to query " << uuid << "\n" << reply;

    return true;
}

bool App::processConfigReply(CNode *node, XRouterPacketPtr packet, CValidationState & state)
{
    const auto & uuid = packet->suuid();
    const auto & nodeAddr = node->NodeAddress();

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

    try {
        Value reply_val;
        read_string(reply, reply_val);
        Object reply_obj = reply_val.get_obj();
        std::string config = find_value(reply_obj, "config").get_str();
        Object plugins = find_value(reply_obj, "plugins").get_obj();

        auto settings = std::make_shared<XRouterSettings>(false); // not our config
        auto configInit = settings->init(config);
        if (!configInit) {
            ERR() << "Failed to read config on query " << uuid << " from node " << nodeAddr;
            updateScore(nodeAddr, -10);
            reply = "Failed to parse config from XRouter node " + nodeAddr + "\n" + reply;
            queryMgr.addReply(uuid, nodeAddr, reply);
            queryMgr.purge(uuid, nodeAddr);
            return false;
        }

        settings->assignNode(nodeAddr);

        for (const auto & plugin : plugins) {
            try {
                auto psettings = std::make_shared<XRouterPluginSettings>(false); // not our config
                psettings->read(plugin.value_.get_str());
                settings->addPlugin(plugin.name_, psettings);
            } catch (...) {
                ERR() << "Failed to read plugin " << plugin.name_ << " on query " << uuid << " from node " << nodeAddr;
                updateScore(nodeAddr, -2);
            }
        }

        // Update settings for node
        updateConfig(nodeAddr, settings);
        queryMgr.addReply(uuid, nodeAddr, reply);
        queryMgr.purge(uuid, nodeAddr);

    } catch (...) {
        ERR() << "Failed to read config on query " << uuid << " from node " << nodeAddr;
        updateScore(nodeAddr, -10);
        reply = "Failed to parse config from XRouter node " + nodeAddr + "\n" + reply;
        queryMgr.addReply(uuid, nodeAddr, reply);
        queryMgr.purge(uuid, nodeAddr);
        return false;
    }

    LOG() << "Received reply to query " << uuid << " from node " << nodeAddr << "\n" << reply;

    return true;
}

//*****************************************************************************
//*****************************************************************************
void App::onMessageReceived(CNode* node, const std::vector<unsigned char> & message)
{
    // If xrouter == 0, xrouter is turned off on this node
    if (!isEnabled() || !isReady())
        return;

    auto checkDoS = [](CValidationState & state, CNode *pnode) {
        int dos = 0;
        if (state.IsInvalid(dos)) {
            LogPrint("xrouter", "invalid xrouter packet from peer=%d %s : %s\n",
                     pnode->id, pnode->cleanSubVer,
                     state.GetRejectReason());
            if (dos > 0) {
                LOCK(cs_main);
                Misbehaving(pnode->GetId(), dos);
            }
        } else if (state.IsError()) {
            LogPrint("xrouter", "xrouter packet from peer=%d %s processed with error: %s\n",
                     pnode->id, pnode->cleanSubVer,
                     state.GetRejectReason());
        }
    };
    auto retainNode = [](CNode *pnode) {
        pnode->AddRef();
    };

    retainNode(node); // retain for thread below

    // Handle the xrouter request
    requestHandlers.create_thread([this, node, message, &checkDoS]() {
        RenameThread("blocknetdx-xrouter");
        boost::this_thread::interruption_point();
        CValidationState state;

        bool released{false};
        auto releaseNode = [&released](CNode *pnode) {
            if (!released) {
                released = true;
                pnode->Release();
            }
        };

        try {
            XRouterPacketPtr packet(new XRouterPacket);
            if (!packet->copyFrom(message)) {
                if (server->isStarted()) { // Send error back to client
                    try {
                        Object error;
                        error.emplace_back("error", "XRouter Node reported a protocol error on a received packet. "
                                                    "Unable to deserialize packet, possible bad packet header");
                        error.emplace_back("code", xrouter::BAD_REQUEST);
                        const std::string reply = json_spirit::write_string(Value(error), true);
                        XRouterPacket packet(xrInvalid, "protocol_error");
                        packet.append(reply);
                        packet.sign(server->pubKey(), server->privKey());
                        node->PushMessage("xrouter", packet.body());
                    } catch (std::exception & e) { // catch json errors
                        ERR() << "Failed to send error reply to client " << node->NodeAddress() << " error: "
                              << e.what();
                    }
                }

                updateScore(node->NodeAddress(), -10);
                state.DoS(10, error("XRouter: invalid packet received"), REJECT_INVALID, "xrouter-error");
                checkDoS(state, node);
                releaseNode(node);

                return;
            }

            const auto & command = packet->command();
            const auto & uuid = packet->suuid();
            const auto & nodeAddr = node->NodeAddress();
            const auto & commandStr = XRouterCommand_ToString(command);

            if (command == xrService) {
                auto service = packet->service();
                if (service.size() > 100) // truncate service name
                    service = service.substr(0, 100);
                LOG() << "XRouter command: " << commandStr << xrdelimiter + service << " query: " << uuid << " node: " << nodeAddr;
            }
            else
                LOG() << "XRouter command: " << commandStr << " query: " << uuid << " node: " << nodeAddr;

            if (command == xrInvalid) { // Process invalid packets (protocol error packets)
                processInvalid(node, packet, state);
                checkDoS(state, node);
                releaseNode(node);
            } else if (command == xrReply) { // Process replies
                processReply(node, packet, state);
                checkDoS(state, node);
                releaseNode(node);
            } else if (command == xrConfigReply) { // Process config replies
                processConfigReply(node, packet, state);
                checkDoS(state, node);
                releaseNode(node);
            } else if (canListen() && server->isStarted()) { // Process server requests
                server->addInFlightQuery(nodeAddr, uuid);
                try {
                    server->onMessageReceived(node, packet, state);
                    server->removeInFlightQuery(nodeAddr, uuid);
                    releaseNode(node);
                } catch (...) { // clean up on error
                    server->removeInFlightQuery(nodeAddr, uuid);
                    releaseNode(node);
                }
                checkDoS(state, node);
            } else {
                releaseNode(node);
            }

        } catch (...) {
            ERR() << strprintf("xrouter query from %s processed with error: ", node->NodeAddress());
            checkDoS(state, node);
            releaseNode(node);
        }
    });
}

//*****************************************************************************
//*****************************************************************************
std::string App::xrouterCall(enum XRouterCommand command, std::string & uuidRet, const std::string & fqService,
                             const int & confirmations, const std::vector<std::string> & params)
{
    const std::string & uuid = generateUUID();
    uuidRet = uuid; // set uuid
    std::map<std::string, std::string> feePaymentTxs;
    std::vector<CNode*> selectedNodes;

    try {
        if (!isEnabled() || !isReady())
            throw XRouterError("XRouter is turned off. Please set 'xrouter=1' in blocknetdx.conf", xrouter::UNAUTHORIZED);

        std::string cleaned;
        if (!removeNamespace(fqService, cleaned))
            throw XRouterError("Bad service name: " + fqService, xrouter::INVALID_PARAMETERS);

        const auto & service = cleaned;

        if (command != xrService) {
            // Check param1
            switch (command) {
                case xrGetBlockHash:
                case xrGetBlocks:
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
                case xrGetTransactions:
                    if (params.size() > 0 && !is_address(params[0]))
                        throw XRouterError("Incorrect address: " + params[0], xrouter::INVALID_PARAMETERS);
                    break;
                default:
                    break;
            }

            // Check param2
            switch (command) {
                case xrGetTransactions:
                case xrGetTxBloomFilter:
                    if (params.size() > 1 && !is_number(params[1]))
                        throw XRouterError("Incorrect block number: " + params[1], xrouter::INVALID_PARAMETERS);
                    break;
                default:
                    break;
            }
        }

        auto confs = xrsettings->confirmations(command, service, confirmations); // Confirmations
        const auto & commandStr = XRouterCommand_ToString(command);
        const auto & fqService = (command == xrService) ? pluginCommandKey(service) // plugin
                                                        : walletCommandKey(service, commandStr); // spv wallet

        // Select available nodes
        selectedNodes = availableNodesRetained(command, service, confs);
        auto selected = static_cast<int>(selectedNodes.size());

        // If we don't have enough open connections...
        if (selected < confs) {
            // Open connections (at least number equal to how many confirmations we want)
            if (!openConnections(command, service, static_cast<uint32_t>(confs - selected), selectedNodes)) {
                std::string err("Failed to find " + std::to_string(confs) + " service node(s) supporting " + fqService + " found " + std::to_string(selected));
                throw XRouterError(err, xrouter::NOT_ENOUGH_NODES);
            }

            // Reselect available nodes
            releaseNodes(selectedNodes);
            selectedNodes = availableNodesRetained(command, service, confs);
            selected = static_cast<int>(selectedNodes.size());
        }

        // Check if we have enough nodes
        if (selected < confs)
            throw XRouterError("Failed to find " + std::to_string(confs) + " service node(s) supporting " +
                               fqService + " found " + std::to_string(selected), xrouter::NOT_ENOUGH_NODES);

        int snodeCount = 0;
        std::string fundErr{"Could not create payments to service nodes. Please check that your wallet "
                            "is fully unlocked and you have at least " + std::to_string(confs) +
                            " available unspent transaction outputs."};

        // Calculate fees for selected nodes
        for (CNode* pnode : selectedNodes) {
            const auto & addr = pnode->NodeAddress();
            if (!hasConfig(addr))
                continue; // skip nodes that do not have configs

            auto config = getConfig(addr);

            // Create the fee payment
            std::string feePayment;
            CAmount fee = to_amount(config->commandFee(command, service));
            if (fee > 0) {
                try {
                    const auto paymentAddress = config->paymentAddress(command, service);
                    if (!generatePayment(addr, paymentAddress, fee, feePayment))
                        throw XRouterError(fundErr, xrouter::INSUFFICIENT_FUNDS);
                } catch (XRouterError & e) {
                    ERR() << "Failed to create payment to node " << addr << " " << e.msg;
                    continue;
                }
            }
            
            feePaymentTxs[addr] = feePayment;
            snodeCount++;
            if (snodeCount == confs)
                break;
        }

        // Do we have enough snodes? If not unlock utxos
        if (snodeCount < confs) {
            throw XRouterError("Unable to find " + std::to_string(confs) +
                               " service node(s) to process request", xrouter::NOT_ENOUGH_NODES);
        }

        int timeout = xrsettings->commandTimeout(command, service);

        // Send xrouter request to each selected node
        for (CNode* pnode : selectedNodes) {
            const auto & addr = pnode->NodeAddress();
            if (!feePaymentTxs.count(addr))
                continue;

            // Record the node sending request to
            addQuery(uuid, addr);
            queryMgr.addQuery(uuid, addr);

            // Send packet to xrouter node
            XRouterPacket packet(command, uuid);
            packet.append(service);
            packet.append(feePaymentTxs[addr]); // fee
            packet.append(static_cast<uint32_t>(params.size()));
            for (const std::string & p : params)
                packet.append(p);
            packet.sign(cpubkey, cprivkey);
            pnode->PushMessage("xrouter", packet.body());

            updateSentRequest(addr, fqService);
            LOG() << "Sent command " << fqService << " query " << uuid << " to node " << pnode->addrName;
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

        bool incompleteRequest{false};
        std::set<NodeAddr> failed;

        if (confirmation_count < confs) {
            incompleteRequest = true;
            failed.insert(review.begin(), review.end());

            auto snodes = getServiceNodes();
            std::map<NodeAddr, CServicenode*> snodec;
            for (auto & s : snodes) {
                if (!s.addr.ToString().empty())
                    snodec[s.addr.ToString()] = &s;
            }

            // Penalize the snodes that didn't respond
            for (const auto & addr : review) {
                if (!snodec.count(addr))
                    continue;
                updateScore(addr, -25);
            }

            std::set<std::string> snodeAddresses;
            for (const auto & addr : review) {
                if (!snodec.count(addr))
                    continue;
                auto s = snodec[addr];
                snodeAddresses.insert(CBitcoinAddress(s->pubKeyCollateralAddress.GetID()).ToString());
            }

            const auto & nodes = boost::algorithm::join(snodeAddresses, ",");
            ERR() << "Failed to get response in time for query " << uuid << " Nodes failed to respond, penalizing: " << nodes;
        }

        // Handle the results
        std::set<NodeAddr> diff;
        std::set<NodeAddr> agree;
        std::string result; // TODO need field to communicate non-fatal consensus errors to client
        int c = queryMgr.mostCommonReply(uuid, result, agree, diff);
        for (const auto & addr : diff) // penalize nodes that didn't match consensus
            updateScore(addr, -5);
        if (c > 1) { // only update score if there's consensus
            for (const auto & addr : agree) {
                if (!failed.count(addr))
                    updateScore(addr, c > 1 ? c * 2 : 0); // boost majority consensus nodes
            }
        }

//        if (c <= confs || result.empty())
//            throw XRouterError("No consensus between responses", xrouter::INTERNAL_SERVER_ERROR);

        releaseNodes(selectedNodes);
        return result;

    } catch (XRouterError & e) {
        LOG() << e.msg;

        for (const auto & item : feePaymentTxs) { // unlock any fee txs
            const std::string & tx = item.second;
            unlockOutputs(tx);
        }

        Object error;
        error.emplace_back(Pair("error", e.msg));
        error.emplace_back(Pair("code", e.code));
        error.emplace_back(Pair("uuid", uuid));

        releaseNodes(selectedNodes);
        return json_spirit::write_string(Value(error), true);

    } catch (std::exception & e) {
        LOG() << e.what();

        for (const auto & item : feePaymentTxs) { // unlock any fee txs
            const std::string & tx = item.second;
            unlockOutputs(tx);
        }

        Object error;
        error.emplace_back(Pair("error", "Internal Server Error"));
        error.emplace_back(Pair("code", INTERNAL_SERVER_ERROR));
        error.emplace_back(Pair("uuid", uuid));

        releaseNodes(selectedNodes);
        return json_spirit::write_string(Value(error), true);
    }
}

std::string App::getBlockCount(std::string & uuidRet, const std::string & currency, const int & confirmations)
{
    return this->xrouterCall(xrGetBlockCount, uuidRet, currency, confirmations, {});
}

std::string App::getBlockHash(std::string & uuidRet, const std::string & currency, const int & confirmations, const int & block)
{
    return this->xrouterCall(xrGetBlockHash, uuidRet, currency, confirmations, { std::to_string(block) });
}

std::string App::getBlock(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & blockHash)
{
    return this->xrouterCall(xrGetBlock, uuidRet, currency, confirmations, { blockHash });
}

std::string App::getBlocks(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::set<std::string> & blockHashes)
{
    return this->xrouterCall(xrGetBlocks, uuidRet, currency, confirmations, { blockHashes.begin(), blockHashes.end() });
}

std::string App::getTransaction(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & hash)
{
    return this->xrouterCall(xrGetTransaction, uuidRet, currency, confirmations, { hash });
}

std::string App::getTransactions(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::set<std::string> & txs)
{
    return this->xrouterCall(xrGetTransactions, uuidRet, currency, confirmations, std::vector<std::string>(txs.begin(), txs.end()));
}

std::string App::decodeRawTransaction(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & rawtx)
{
    return this->xrouterCall(xrDecodeRawTransaction, uuidRet, currency, confirmations, { rawtx });
}

std::string App::getTransactionsBloomFilter(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & filter, const int & number)
{
    return this->xrouterCall(xrGetTxBloomFilter, uuidRet, currency, confirmations, { filter, std::to_string(number) });
}

std::string App::sendTransaction(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & transaction)
{
    return this->xrouterCall(xrSendTransaction, uuidRet, currency, confirmations, { transaction });
}

std::string App::convertTimeToBlockCount(std::string & uuidRet, const std::string & currency, const int & confirmations, const int64_t & time)
{
    return this->xrouterCall(xrGetBlockAtTime, uuidRet, currency, confirmations, { std::to_string(time) });
}

std::string App::getBalance(std::string & uuidRet, const std::string & currency, const int & confirmations, const std::string & address)
{
    return this->xrouterCall(xrGetBalance, uuidRet, currency, confirmations, { address });
}

std::string App::getReply(const std::string & id)
{
    auto replies = queryMgr.allReplies(id);
    std::vector<CServicenode> snodes = getServiceNodes();
    std::map<NodeAddr, std::tuple<std::string, int64_t, std::string> > snodec;
    for (auto & s : snodes) {
        if (!s.addr.ToString().empty()) {
            const auto & snodeAddr = s.addr.ToString();
            std::vector<unsigned char> spubkey; // pubkey
            servicenodePubKey(snodeAddr, spubkey);
            snodec[snodeAddr] = { HexStr(spubkey), getScore(snodeAddr), CBitcoinAddress(s.pubKeyCollateralAddress.GetID()).ToString() };
        }
    }


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
            const auto & reply = it.second;
            const auto & item = snodec[it.first];
            try {
                Value reply_val;
                static const std::string nouuid;
                read_string(write_string(Value(form_reply(nouuid, reply))), reply_val);
                Object o = reply_val.get_obj();
                o.emplace_back("nodepubkey", std::get<0>(item));
                o.emplace_back("score", std::get<1>(item));
                o.emplace_back("address", std::get<2>(item));
                arr.emplace_back(o);
            } catch (...) { }
        }
        result = arr;
    }

    return json_spirit::write_string(Value(result), true);
}

bool App::generatePayment(const NodeAddr & nodeAddr, const std::string & paymentAddress,
        const CAmount & fee, std::string & payment)
{
    if (!hasConfig(nodeAddr))
        throw std::runtime_error("No config found for servicenode: " + nodeAddr);

    if (fee <= 0)
        return true;

    // Get payment address from snode list if the default is empty
    std::string snodeAddress = paymentAddress;
    if (snodeAddress.empty()) {
        if (!getPaymentAddress(nodeAddr, snodeAddress))
            return false;
    }

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
        if (s.addr.ToString() == node->NodeAddress()) {
            return s.pubKeyCollateralAddress;
        }
    }
    
    if (debug_on_client()) {
        std::string test = "03872bfe748a5a3868c74c8f820ed1387a58d48c67a7c415c7b3fad1ca61803365";
        return CPubKey(ParseHex(test));
    }
    
    return CPubKey();
}

std::map<NodeAddr, XRouterSettingsPtr> App::xrConnect(const std::string & fqService, const int & count) {
    std::map<NodeAddr, XRouterSettingsPtr> selectedConfigs;
    std::vector<std::string> nparts;
    if (!xrsplit(fqService, xrdelimiter, nparts))
        throw XRouterError("Bad service name, acceptable characters [a-z A-Z 0-9 $], sample format: xrs::ExampleServiceName123", BAD_REQUEST);

    const auto msg = "Missing top-level namespace (xr:: or xrs::) Example xr::BLOCK or xrs::CustomServiceName";
    if (nparts.size() <= 1)
        throw XRouterError(msg, BAD_REQUEST);

    std::string ns = nparts[0];
    if (ns != xr && ns != xrs)
        throw XRouterError(msg, BAD_REQUEST);

    XRouterCommand command = ns == xrs ? xrService
                                       : (nparts.size() == 2 ? xrGetBlockCount : XRouterCommand_FromString(nparts.back()));
    std::string commandStr = XRouterCommand_ToString(command);

    if (command == xrInvalid)
        throw XRouterError("Unknown xr:: command", BAD_REQUEST);

    const std::string wallet = nparts[1];
    const std::string plugin = boost::algorithm::join(std::vector<std::string>{nparts.begin()+1, nparts.end()}, xrdelimiter);
    const std::string service = command != xrService ? wallet : plugin;

    openConnections(command, service, count, { }); // open connections to snodes that have our service

    const auto configs = getNodeConfigs(); // get configs and store matching ones
    for (const auto & item : configs) {
        if (command != xrService && item.second->hasWallet(service)) {
            selectedConfigs.insert(item);
            continue;
        }
        if (item.second->hasPlugin(service))
            selectedConfigs.insert(item);
    }

    return selectedConfigs;
}

void App::snodeConfigJSON(const std::map<NodeAddr, XRouterSettingsPtr> & configs, json_spirit::Array & data) {
    if (configs.empty()) // no configs
        return;

    for (const auto & item : configs) { // Iterate over all node configs
        if (item.second == nullptr)
            continue;

        Object o;

        // pubkey
        std::vector<unsigned char> spubkey;
        servicenodePubKey(item.second->getNode(), spubkey);
        o.emplace_back("nodepubkey", HexStr(spubkey));

        // score
        o.emplace_back("score", getScore(item.first));

        // payment address
        std::string address;
        getPaymentAddress(item.second->getNode(), address);
        o.emplace_back("paymentaddress", address);

        // wallets
        const auto & wallets = item.second->getWallets();
        o.emplace_back("xwallets", Array(wallets.begin(), wallets.end()));

        // fees
        o.emplace_back("feedefault", item.second->defaultFee());
        Object ofs;
        const auto & schedule = item.second->feeSchedule();
        for (const auto & s : schedule)
            ofs.emplace_back(s.first,  s.second);
        o.emplace_back("fees", ofs);

        // plugins
        Object plugins;
        for (const auto & plugin : item.second->getPlugins()) {
            auto pls = item.second->getPluginSettings(plugin);
            if (pls) {
                Object plg;
                plg.emplace_back("parameters", boost::algorithm::join(pls->parameters(), ","));
                plg.emplace_back("fee", pls->fee());
                plg.emplace_back("requestlimit", pls->clientRequestLimit());
                plg.emplace_back("paymentaddress", item.second->paymentAddress(xrService, plugin));
                plugins.emplace_back(plugin, plg);
            }
        }
        o.emplace_back("plugins", plugins);

        data.emplace_back(o);
    }
}

std::string App::sendXRouterConfigRequest(CNode* node, std::string addr) {
    const auto & uuid = generateUUID();
    const auto & nodeAddr = node->NodeAddress();
    addQuery(uuid, nodeAddr);
    queryMgr.addQuery(uuid, nodeAddr);

    XRouterPacket packet(xrGetConfig, uuid);
    packet.append(addr);
    packet.sign(cpubkey, cprivkey);
    node->PushMessage("xrouter", packet.body());

    return uuid;
}

std::string App::sendXRouterConfigRequestSync(CNode* node) {
    const auto & uuid = generateUUID();
    const auto & nodeAddr = node->NodeAddress();
    addQuery(uuid, nodeAddr);
    queryMgr.addQuery(uuid, nodeAddr);

    XRouterPacket packet(xrGetConfig, uuid);
    packet.sign(cpubkey, cprivkey);
    node->PushMessage("xrouter", packet.body());

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
    LOG() << "Reloading xrouter config from file " << xrouterpath.string();
    xrsettings->init(xrouterpath);
    createConnectors();
}

std::string App::getStatus() {
    Object result;

    auto snode = mnodeman.Find(activeServicenode.vin);
    if (snode != nullptr) {
        std::map<NodeAddr, XRouterSettingsPtr> my;
        my[snode->addr.ToString()] = xrsettings;

        Array data;
        snodeConfigJSON(my, data);

        if (data.empty() || data.size() > 1)
            return "";

        result = data[0].get_obj();
    }

    result.emplace_back("xrouter", isEnabled());
    result.emplace_back("servicenode", snode != nullptr);
    result.emplace_back("config", xrsettings->rawText());

    return json_spirit::write_string(Value(result), json_spirit::pretty_print, 8);
}

void App::getLatestNodeContainers(std::vector<CServicenode> & snodes, std::vector<CNode*> & nodes,
                                  std::map<NodeAddr, CServicenode> & snodec, std::map<NodeAddr, CNode*> & nodec)
{
    snodes.clear(); nodes.clear(); snodec.clear(); nodec.clear();

    snodes = getServiceNodes();
    nodes = CNode::CopyNodes();

    // Build snode cache
    for (CServicenode & s : snodes) {
        if (!s.addr.ToString().empty())
            snodec[s.addr.ToString()] = s;
    }

    // Build node cache
    for (auto & pnode : nodes) {
        const auto & addr = pnode->NodeAddress();
        if (!addr.empty())
            nodec[addr] = pnode;
    }
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
