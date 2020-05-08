// Copyright (c) 2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <xrouter/xrouterclient.h>

#include <chainparams.h>
#include <compat/sanity.h>
#include <crypto/sha256.h>
#include <key.h>
#include <net.h>
#include <shutdown.h>
#include <ui_interface.h>
#include <util/system.h>
#include <util/time.h>

#include <csignal>
#include <utility>
#include <sys/stat.h>

#ifdef ENABLE_EVENTSSL
#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#endif // ENABLE_EVENTSSL

#if defined(USE_XROUTERCLIENT)
const std::function<std::string(const char*)> G_TRANSLATION_FUN = nullptr;
#endif

namespace xrouter {

static bool InitSanityCheck() {
    if(!ECC_InitSanityCheck()) {
        InitError("Elliptic curve cryptography sanity check failure. Aborting.");
        return false;
    }

    if (!glibc_sanity_test() || !glibcxx_sanity_test())
        return false;

    if (!Random_SanityCheck()) {
        InitError("OS cryptographic RNG sanity check failure. Aborting.");
        return false;
    }

    return true;
}

static bool AppInitSanityChecks(std::unique_ptr<ECCVerifyHandle> & globalVerifyHandle) {
    // Initialize elliptic curve code
    std::string sha256_algo = SHA256AutoDetect();
    LogPrintf("Using the '%s' SHA256 implementation\n", sha256_algo);
    RandomInit();
    ECC_Start();
    globalVerifyHandle.reset(new ECCVerifyHandle());

    // Sanity check
    if (!InitSanityCheck())
        return InitError(strprintf(_("Initialization sanity check failed. %s is shutting down."), _(PACKAGE_NAME)));

    return true;
}

#ifndef WIN32
static void HandleSIGTERM(int)
{
    StartShutdown();
}

static void HandleSIGHUP(int)
{
}
#else
static BOOL WINAPI consoleCtrlHandler(DWORD dwCtrlType)
{
    StartShutdown();
    Sleep(INFINITE);
    return true;
}
#endif

#ifndef WIN32
static void registerSignalHandler(int signal, void(*handler)(int))
{
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(signal, &sa, nullptr);
}
#endif

[[noreturn]] static void new_handler_terminate() {
    fprintf(stderr, "Error: %s\n", "Out of memory. Terminating.");
    std::terminate();
}

static bool AppInitBasicSetup() {
#ifdef _MSC_VER
    // Turn off Microsoft heap dump noise
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, CreateFileA("NUL", GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, 0));
    // Disable confusing "helpful" text message on abort, Ctrl-C
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
#ifdef WIN32
    // Enable Data Execution Prevention (DEP)
    SetProcessDEPPolicy(PROCESS_DEP_ENABLE);
#endif

    if (!SetupNetworking())
        return InitError("Initializing networking failed");

#ifndef WIN32
    if (!gArgs.GetBoolArg("-sysperms", false)) {
        umask(077);
    }

    // Clean shutdown on SIGTERM
    registerSignalHandler(SIGTERM, HandleSIGTERM);
    registerSignalHandler(SIGINT, HandleSIGTERM);

    // Reopen debug.log on SIGHUP
    registerSignalHandler(SIGHUP, HandleSIGHUP);

    // Ignore SIGPIPE, otherwise it will bring the daemon down if the client closes unexpectedly
    signal(SIGPIPE, SIG_IGN);
#else
    SetConsoleCtrlHandler(consoleCtrlHandler, true);
#endif

    std::set_new_handler(new_handler_terminate);

    return true;
}

static void Interrupt() {
    if (g_connman)
        g_connman->Interrupt();
}

static void WaitForShutdown() {
    while (!ShutdownRequested())
        MilliSleep(200);
    Interrupt();
}

XRouterClient::XRouterClient(int argc, char* argv[], CConnman::Options connOpts) : smgr(sn::ServiceNodeMgr::instance()),
                                                                                   connOptions(std::move(connOpts))
{
    fListen = false; // the xrouter client does not accept inbound connections
    SetupEnvironment();

    if (!AppInitBasicSetup()) {
        const std::string error("Error: failed app init");
        fprintf(stderr, "%s\n", error.c_str());
        throw std::runtime_error(error);
    }

    if (!AppInitSanityChecks(globalVerifyHandle)) {
        const std::string error("Error: failed sanity checks");
        fprintf(stderr, "%s\n", error.c_str());
        throw std::runtime_error(error);
    }

    SetupChainParamsBaseOptions();
    SetupHelpOptions(gArgs);
    gArgs.AddArg("-xrouterconf", "Specify the location to the xrouter.conf", true, OptionsCategory::XROUTER);

    std::string error;
    if (!gArgs.ParseParameters(argc, argv, error)) {
        error = strprintf("Error parsing command line arguments: %s", error.c_str());
        fprintf(stderr, "%s\n", error.c_str());
        throw std::runtime_error(error);
    }

    // Check for -testnet or -regtest parameter (Params() calls are only valid after this clause)
    try {
        SelectParams(gArgs.GetChainName());
    } catch (const std::exception& e) {
        error = strprintf("Error: failed to select chain %s", e.what());
        fprintf(stderr, "%s\n", error.c_str());
        throw std::runtime_error(error);
    }

    uint64_t a1{GetRand(std::numeric_limits<uint64_t>::max())};
    uint64_t a2{GetRand(std::numeric_limits<uint64_t>::max())};
    if (connOptions.unit_test_mode)
        a1 = 0x1337, a2 = 0x1337;
    g_connman = MakeUnique<CConnman>(a1, a2);

    peerMgr = MakeUnique<XRouterPeerMgr>(g_connman.get(), scheduler, smgr);
    peerMgr->onSnodePing([this](const sn::ServiceNode &snode) {
        if (snode.isNull() || !snode.running())
            return;
        settings[snode.getHostPort()] = MakeUnique<XRouterSnodeConfig>(snode);
    });

    interfaces.chain = interfaces::MakeChain();

    connOptions.uiInterface = &uiInterface;
    connOptions.m_banman = g_banman.get();
    connOptions.m_msgproc = peerMgr.get();
    if (connOptions.vSeedNodes.empty())
        connOptions.vSeedNodes = Params().DNSSeeds();

    // Our key used for xrouter calls
    clientKey.MakeNewKey(true);
    cpubkey = ToByteVector(clientKey.GetPubKey());
    cprivkey = ToByteVector(clientKey);
}

XRouterClient::XRouterClient(CConnman::Options connOpts) : XRouterClient(0, nullptr, std::move(connOpts)) { }

bool XRouterClient::start(std::string & error) {
    if (started) {
        error = "XRouter client is already running";
        fprintf(stderr, "Error: %s\n", error.c_str());
        return false;
    }
    started = true;

#ifdef ENABLE_EVENTSSL
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
#endif // ENABLE_EVENTSSL

    // Start xrouter
    const auto conf = gArgs.GetArg("-xrouterconf", "");
    const auto xrouterConfPath = conf.empty() ? boost::filesystem::current_path() : boost::filesystem::path(conf);
    xrouter::createConf(xrouterConfPath, true); // Create config if it doesn't exist

    const auto xrouterConfFile = xrouterConfPath / "xrouter.conf";
    xrsettings = MakeUnique<XRouterSettings>(CPubKey{});

    // Load the xrouter configuration
    try {
        if (!xrsettings->init(xrouterConfFile, false)) {
            error = strprintf("Failed to read xrouter config %s", xrouterConfFile.string());
            return false;
        }
    } catch (...) {
        return false;
    }

    CScheduler::Function serviceLoop = std::bind(&CScheduler::serviceQueue, &scheduler); // Start the lightweight task scheduler thread
    threadGroup.create_thread(std::bind(&TraceThread<CScheduler::Function>, "scheduler", serviceLoop));

    bool success{true};
    if (!g_connman->Start(scheduler, connOptions)) {
        error = "XRouter failed to start the connection manager";
        fprintf(stderr, "Error: %s\n", error.c_str());
        success = false;
    }

    return success;
}

bool XRouterClient::start() {
    std::string error;
    return start(error);
}

bool XRouterClient::stop(std::string & error) {
    if (!started)
        return false; // ignore if already stopped
    started = false;

    try {
        Interrupt();
        if (g_connman)
            g_connman->Stop();
        threadGroup.interrupt_all();
        threadGroup.join_all();
    } catch (std::exception & e) {
        error = strprintf("XRouter failed to gracefully stop the xrouter client: %s", e.what());
        fprintf(stderr, "Error: %s\n", error.c_str());
        return false;
    } catch (...) {
        return false;
    }

//    fprintf(stdout, "XRouter client stopped\n");
    return true;
}

bool XRouterClient::stop() {
    std::string error;
    return stop(error);
}

bool XRouterClient::waitForService(const unsigned int timeoutMsec, const std::vector<std::pair<std::string, int>> & services, const unsigned int sleepTimeMsec) {
    auto start = std::chrono::system_clock::now();
    auto current = start;
    while (std::chrono::duration_cast<std::chrono::milliseconds>(current.time_since_epoch() - start.time_since_epoch()).count() < timeoutMsec) {
        auto copyservices = services;
        // If no service is specified wait for first running snode
        if (copyservices.empty()) {
            if (runningCount() > 0)
                return true;
        } else {
            // If services are specified make sure we wait until the expected number is found on running snodes
            for (int i = (int)copyservices.size() - 1; i >= 0; --i) {
                const auto & service = copyservices[i];
                if (runningCount(service.first) >= service.second)
                    copyservices.erase(copyservices.begin()+i);
            }
            if (copyservices.empty())
                return true;
        }
        MilliSleep(sleepTimeMsec);
        current = std::chrono::system_clock::now();
    }
    return false; // timeout occurred
}

bool XRouterClient::waitForService(const unsigned int timeoutMsec, const std::string & service, const int serviceCount, const unsigned int sleepTimeMsec) {
    return waitForService(timeoutMsec, {{service, serviceCount}}, sleepTimeMsec);
}

CConnman::Options XRouterClient::defaultOptions() {
    CConnman::Options connOptions;
    connOptions.nLocalServices = NODE_NONE;
    connOptions.nMaxConnections = DEFAULT_MAX_PEER_CONNECTIONS;
    connOptions.nMaxOutbound = 1; // TODO Blocknet XRouter max outbound too low?
    connOptions.nMaxAddnode = 1;
    connOptions.nMaxFeeler = 0;
    connOptions.nBestHeight = 0;
    connOptions.m_banman = nullptr;
    connOptions.m_msgproc = nullptr;
    connOptions.nSendBufferMaxSize = 1000*DEFAULT_MAXSENDBUFFER;
    connOptions.nReceiveFloodSize = 1000*DEFAULT_MAXRECEIVEBUFFER;
    connOptions.nMaxOutboundTimeframe = MAX_UPLOAD_TIMEFRAME;
    connOptions.nMaxOutboundLimit = 0;
    connOptions.m_peer_connect_timeout = DEFAULT_PEER_CONNECT_TIMEOUT;
    return connOptions;
}

std::vector<sn::ServiceNode> XRouterClient::getServiceNodes() {
    return std::move(smgr.list());
}

///
/// getBlockCount
///

std::string XRouterClient::getBlockCountRaw(const std::string & currency, std::string & uuid, const int querynodes) {
    return xrouterCall(xrGetBlockCount, uuid, currency, querynodes, UniValue(UniValue::VARR));
}
UniValue XRouterClient::getBlockCount(const std::string & currency, std::string & uuid, const int querynodes) {
    const auto res = getBlockCountRaw(currency, uuid, querynodes);
    return uret(res, uuid);
}
UniValue XRouterClient::getBlockCount(const std::string & currency, const int querynodes) {
    std::string uuid;
    return getBlockCount(currency, uuid, querynodes);
}

///
/// getBlockHash
///

std::string XRouterClient::getBlockHashRaw(const std::string & currency, const unsigned int block, std::string & uuid, const int querynodes) {
    auto uv = UniValue(UniValue::VARR);
    uv.push_back(std::to_string(block));
    return xrouterCall(xrGetBlockHash, uuid, currency, querynodes, uv);
}
UniValue XRouterClient::getBlockHash(const std::string & currency, const unsigned int block, std::string & uuid, const int querynodes) {
    const auto res = getBlockHashRaw(currency, block, uuid, querynodes);
    return uret(res, uuid);
}
UniValue XRouterClient::getBlockHash(const std::string & currency, const unsigned int block, const int querynodes) {
    std::string uuid;
    return getBlockHash(currency, block, uuid, querynodes);
}
std::string XRouterClient::getBlockHashRaw(const std::string & currency, const std::string & block, std::string & uuid, const int querynodes) {
    auto uv = UniValue(UniValue::VARR);
    uv.push_back(block);
    return xrouterCall(xrGetBlockHash, uuid, currency, querynodes, uv);
}
UniValue XRouterClient::getBlockHash(const std::string & currency, const std::string & block, std::string & uuid, const int querynodes) {
    const auto res = getBlockHashRaw(currency, block, uuid, querynodes);
    return uret(res, uuid);
}
UniValue XRouterClient::getBlockHash(const std::string & currency, const std::string & block, const int querynodes) {
    std::string uuid;
    return getBlockHash(currency, block, uuid, querynodes);
}

///
/// getBlock
///

std::string XRouterClient::getBlockRaw(const std::string & currency, const unsigned int block, std::string & uuid, const int querynodes) {
    auto uv = UniValue(UniValue::VARR);
    uv.push_back(std::to_string(block));
    return xrouterCall(xrGetBlock, uuid, currency, querynodes, uv);
}
UniValue XRouterClient::getBlock(const std::string & currency, const unsigned int block, std::string & uuid, const int querynodes) {
    const auto res = getBlockRaw(currency, block, uuid, querynodes);
    return uret(res, uuid);
}
UniValue XRouterClient::getBlock(const std::string & currency, const unsigned int block, const int querynodes) {
    std::string uuid;
    return getBlock(currency, block, uuid, querynodes);
}
std::string XRouterClient::getBlockRaw(const std::string & currency, const std::string & block, std::string & uuid, const int querynodes) {
    auto uv = UniValue(UniValue::VARR);
    uv.push_back(block);
    return xrouterCall(xrGetBlock, uuid, currency, querynodes, uv);
}
UniValue XRouterClient::getBlock(const std::string & currency, const std::string & block, std::string & uuid, const int querynodes) {
    const auto res = getBlockRaw(currency, block, uuid, querynodes);
    return uret(res, uuid);
}
UniValue XRouterClient::getBlock(const std::string & currency, const std::string & block, const int querynodes) {
    std::string uuid;
    return getBlock(currency, block, uuid, querynodes);
}

///
/// getBlocks
///

std::string XRouterClient::getBlocksRaw(const std::string & currency, const std::vector<unsigned int> & blocks, std::string & uuid, const int querynodes) {
    auto uv = UniValue(UniValue::VARR);
    for (const auto & block : blocks) // basic int to string conversion
        uv.push_back(std::to_string(block));
    return xrouterCall(xrGetBlocks, uuid, currency, querynodes, uv);
}
UniValue XRouterClient::getBlocks(const std::string & currency, const std::vector<unsigned int> & blocks, std::string & uuid, const int querynodes) {
    const auto res = getBlocksRaw(currency, blocks, uuid, querynodes);
    return uret(res, uuid);
}
UniValue XRouterClient::getBlocks(const std::string & currency, const std::vector<unsigned int> & blocks, const int querynodes) {
    std::string uuid;
    return getBlocks(currency, blocks, uuid, querynodes);
}
std::string XRouterClient::getBlocksRaw(const std::string & currency, const std::vector<std::string> & blocks, std::string & uuid, const int querynodes) {
    auto uv = UniValue(UniValue::VARR);
    for (const auto & block : blocks)
        uv.push_back(block);
    return xrouterCall(xrGetBlocks, uuid, currency, querynodes, uv);
}
UniValue XRouterClient::getBlocks(const std::string & currency, const std::vector<std::string> & blocks, std::string & uuid, const int querynodes) {
    const auto res = getBlocksRaw(currency, blocks, uuid, querynodes);
    return uret(res, uuid);
}
UniValue XRouterClient::getBlocks(const std::string & currency, const std::vector<std::string> & blocks, const int querynodes) {
    std::string uuid;
    return getBlocks(currency, blocks, uuid, querynodes);
}

///
/// getTransaction
///

std::string XRouterClient::getTransactionRaw(const std::string & currency, const std::string & transaction, std::string & uuid, const int querynodes) {
    auto uv = UniValue(UniValue::VARR);
    uv.push_back(transaction);
    return xrouterCall(xrGetTransaction, uuid, currency, querynodes, uv);
}
UniValue XRouterClient::getTransaction(const std::string & currency, const std::string & transaction, std::string & uuid, const int querynodes) {
    const auto res = getTransactionRaw(currency, transaction, uuid, querynodes);
    return uret(res, uuid);
}
UniValue XRouterClient::getTransaction(const std::string & currency, const std::string & transaction, const int querynodes) {
    std::string uuid;
    return getTransaction(currency, transaction, uuid, querynodes);
}

///
/// getTransactions
///

std::string XRouterClient::getTransactionsRaw(const std::string & currency, const std::vector<std::string> & txns, std::string & uuid, const int querynodes) {
    auto uv = UniValue(UniValue::VARR);
    for (const auto & tx : txns)
        uv.push_back(tx);
    return xrouterCall(xrGetTransactions, uuid, currency, querynodes, uv);
}
UniValue XRouterClient::getTransactions(const std::string & currency, const std::vector<std::string> & txns, std::string & uuid, const int querynodes) {
    const auto res = getTransactionsRaw(currency, txns, uuid, querynodes);
    return uret(res, uuid);
}
UniValue XRouterClient::getTransactions(const std::string & currency, const std::vector<std::string> & txns, const int querynodes) {
    std::string uuid;
    return getTransactions(currency, txns, uuid, querynodes);
}

///
/// decodeTransaction
///

std::string XRouterClient::decodeTransactionRaw(const std::string & currency, const std::string & rawtransaction, std::string & uuid, const int querynodes) {
    auto uv = UniValue(UniValue::VARR);
    uv.push_back(rawtransaction);
    return xrouterCall(xrDecodeRawTransaction, uuid, currency, querynodes, uv);
}
UniValue XRouterClient::decodeTransaction(const std::string & currency, const std::string & rawtransaction, std::string & uuid, const int querynodes) {
    const auto res = decodeTransactionRaw(currency, rawtransaction, uuid, querynodes);
    return uret(res, uuid);
}
UniValue XRouterClient::decodeTransaction(const std::string & currency, const std::string & rawtransaction, const int querynodes) {
    std::string uuid;
    return decodeTransaction(currency, rawtransaction, uuid, querynodes);
}

///
/// sendTransaction
///

std::string XRouterClient::sendTransactionRaw(const std::string & currency, const std::string & rawtransaction, std::string & uuid, const int querynodes) {
    auto uv = UniValue(UniValue::VARR);
    uv.push_back(rawtransaction);
    return xrouterCall(xrSendTransaction, uuid, currency, querynodes, uv);
}
UniValue XRouterClient::sendTransaction(const std::string & currency, const std::string & rawtransaction, std::string & uuid, const int querynodes) {
    const auto res = sendTransactionRaw(currency, rawtransaction, uuid, querynodes);
    return uret(res, uuid);
}
UniValue XRouterClient::sendTransaction(const std::string & currency, const std::string & rawtransaction, const int querynodes) {
    std::string uuid;
    return sendTransaction(currency, rawtransaction, uuid, querynodes);
}

///
/// callService
///

std::string XRouterClient::callServiceRaw(const std::string & service, std::string & uuid, const UniValue & call_params, const int querynodes) {
    return xrouterCall(xrouter::xrService, uuid, service, querynodes, call_params);
}
std::string XRouterClient::callServiceRaw(const std::string & service, std::string & uuid, const std::vector<std::string> & call_params, const int querynodes) {
    auto uv = UniValue(UniValue::VARR);
    for (const auto & param : call_params)
        uv.push_back(param);
    return callServiceRaw(service, uuid, uv, querynodes);
}
UniValue XRouterClient::callService(const std::string & service, std::string & uuid, const UniValue & call_params, const int querynodes) {
    const auto res = callServiceRaw(service, uuid, call_params, querynodes);
    return uret(res, uuid);
}
UniValue XRouterClient::callService(const std::string & service, std::string & uuid, const std::vector<std::string> & call_params, const int querynodes) {
    const auto res = callServiceRaw(service, uuid, call_params, querynodes);
    return uret(res, uuid);
}
UniValue XRouterClient::callService(const std::string & service, const UniValue & call_params, const int querynodes) {
    std::string uuid;
    return callService(service, uuid, call_params, querynodes);
}
UniValue XRouterClient::callService(const std::string & service, const std::vector<std::string> & call_params, const int querynodes) {
    std::string uuid;
    return callService(service, uuid, call_params, querynodes);
}
UniValue XRouterClient::callService(const std::string & service, const std::string & param1, const int querynodes) {
    std::string uuid;
    return callService(service, uuid, std::vector<std::string>{param1}, querynodes);
}
UniValue XRouterClient::callService(const std::string & service, const std::string & param1, const std::string & param2, const int querynodes) {
    std::string uuid;
    return callService(service, uuid, {param1, param2}, querynodes);
}
UniValue XRouterClient::callService(const std::string & service, const std::string & param1, const std::string & param2, const std::string & param3, const int querynodes) {
    std::string uuid;
    return callService(service, uuid, {param1, param2, param3}, querynodes);
}
UniValue XRouterClient::callService(const std::string & service, const std::string & param1, const std::string & param2, const std::string & param3, const std::string & param4, const int querynodes) {
    std::string uuid;
    return callService(service, uuid, {param1, param2, param3, param4}, querynodes);
}
UniValue XRouterClient::callService(const std::string & service, const int querynodes) {
    std::string uuid;
    return callService(service, uuid, UniValue(UniValue::VARR), querynodes);
}

UniValue XRouterClient::uret(const std::string & res, const std::string & uuid) {
    UniValue reply;
    if (!reply.read(res)) // if json parse fails, set as string
        reply.setStr(res);
    return xrouter::form_reply(uuid, reply);
}

int XRouterClient::runningCount(const std::string & service) {
    const auto & list = smgr.list();
    int count{0};
    for (const auto & snode : list) {
        if (snode.running() && snode.hasService(service) && snode.isEXRCompatible())
            ++count;
    }
    return count;
}

std::string XRouterClient::xrouterCall(enum XRouterCommand command, std::string & uuidRet, const std::string & fqServiceName,
        const int & confirmations, const UniValue & params)
{
    const std::string & uuid = generateUUID();
    uuidRet = uuid; // set uuid

    std::string cleaned;
    if (!removeNamespace(fqServiceName, cleaned))
        throw XRouterError("Bad service name: " + fqServiceName, xrouter::INVALID_PARAMETERS);
    const auto & service = cleaned;

    std::map<std::string, std::string> feePaymentTxs;
    std::vector<std::pair<std::string, int> > nodeErrors;

    try {
        switch (command) {
            case xrGetBlockHash:
                if (!params.empty() && !is_number(params.getValues()[0].get_str()) && !is_hex(params.getValues()[0].get_str()))
                    throw XRouterError("Incorrect block number: " + params.getValues()[0].get_str(), xrouter::INVALID_PARAMETERS);
                break;
            case xrGetBlock:
                if (!params.empty() && !is_number(params.getValues()[0].get_str()) && !is_hash(params.getValues()[0].get_str()) && !is_hex(params.getValues()[0].get_str()))
                    throw XRouterError("Incorrect hash: " + params.getValues()[0].get_str(), xrouter::INVALID_PARAMETERS);
                break;
            case xrGetTransaction:
                if (!params.empty() && !is_hash(params.getValues()[0].get_str()))
                    throw XRouterError("Incorrect hash: " + params.getValues()[0].get_str(), xrouter::INVALID_PARAMETERS);
                break;
            case xrGetBlocks:
            case xrGetTransactions: {
                if (params.empty())
                    throw XRouterError("Missing parameters for " + fqServiceName, xrouter::INVALID_PARAMETERS);
                for (const auto & p : params.getValues()) {
                    if (!is_hash(p.get_str()))
                        throw XRouterError("Incorrect hash " + p.get_str() + " for " + fqServiceName, xrouter::INVALID_PARAMETERS);
                }
                break;
            }
            default:
                break;
        }

        const auto confs = xrsettings->confirmations(command, service, confirmations); // Confirmations
        const auto & commandStr = XRouterCommand_ToString(command);
        const auto & fqService = (command == xrService) ? pluginCommandKey(service) // plugin
                                                        : walletCommandKey(service, commandStr); // spv wallet

        std::vector<sn::ServiceNode> queryNodes;
        int snodeCount = 0;
        std::string fundErr{"Could not create payments. Please check that your wallet "
                            "is fully unlocked and you have at least " + std::to_string(confs) +
                            " available unspent transaction."};

        const auto fqServiceAdjusted = (command == xrService) ? fqService
                                                              : walletCommandKey(service);

        std::vector<sn::ServiceNode> listSelectedSnodes;
        auto list = smgr.list();
        for (const auto & s : list) {
            if (!s.hasService(xr)) // has xrouter
                continue;
            if (!s.hasService(fqServiceAdjusted)) // has the service
                continue;
            if (!s.running() || !s.isEXRCompatible())
                continue; // only running snodes and exr compatible snodes
            listSelectedSnodes.push_back(s);
        }
        std::sort(listSelectedSnodes.begin(), listSelectedSnodes.end(), [this](const sn::ServiceNode & a, const sn::ServiceNode & b) {
            return queryMgr.getScore(a.getHostPort()) > queryMgr.getScore(b.getHostPort());
        });

        // Compose a final list of snodes to request. selectedNodes here should be sorted
        // ascending best to worst
        for (auto & snode : listSelectedSnodes) {
            const auto & addr = snode.getHostPort();
            if (!settings.count(addr) || !settings[addr]->isValid())
                continue; // skip invalid snodes and ones that do not have configs

            // TODO Blocknet Create the fee payment
            const auto & config = settings[addr]->settings();
            CAmount fee = to_amount(config->commandFee(command, service));
            if (fee > 0) {
//                try {
//                    const auto paymentAddress = config->paymentAddress(command, service);
//                    std::string feePayment;
//                    if (!xrapp.generatePayment(addr, paymentAddress, fee, feePayment))
//                        throw XRouterError(fundErr, xrouter::INSUFFICIENT_FUNDS);
//                    if (!feePayment.empty()) // record fee if it's not empty
//                        feePaymentTxs[addr] = feePayment;
//                } catch (XRouterError & e) {
//                    nodeErrors.emplace_back(strprintf("Failed to create payment to service node %s , %s", addr, e.msg), e.code);
//                    continue;
//                }
                continue; // TODO Blocknet xrclient support paid calls
            }

            queryNodes.push_back(snode);
            ++snodeCount;
            if (snodeCount == confs)
                break;
        }

        // Do we have enough snodes to meet confirmation requirements?
        if (snodeCount < confs) {
            const auto msg = strprintf("Found %u service node(s), however, %u meet your requirements. %u service "
                                       "node(s) are required to process the request", snodeCount, snodeCount, confs);
            throw XRouterError(msg, xrouter::NOT_ENOUGH_NODES);
        }

        const int timeout = xrsettings->commandTimeout(command, service);
        boost::thread_group tg;

        // Send xrouter request to each selected node
        for (auto & snode : queryNodes) {
            const std::string & addr = snode.getHostPort();
            std::string feetx;
            if (feePaymentTxs.count(addr))
                feetx = feePaymentTxs[addr];

            // Record the node sending request to
            queryMgr.addQuery(uuid, addr);

            // Query Enterprise XRouter snodes
            const auto tls = settings[addr]->settings()->tls(command, service);
            // Set the fully qualified service url to the form /xr/BLOCK/xrGetBlockCount
            const auto & fqUrl = fqServiceToUrl((command == xrService) ? pluginCommandKey(service) // plugin
                                                                       : walletCommandKey(service, commandStr, true)); // spv wallet
            try {
                tg.create_thread([uuid,addr,snode,tls,fqUrl,params,feetx,timeout,this]() {
                    RenameThread("blocknet-xrclientrequest");
                    if (ShutdownRequested())
                        return;

                    XRouterReply xrresponse;
                    try {
                        std::string data;
                        if (!params.empty())
                            data = params.write();
                        if (tls)
                            xrresponse = xrouter::CallXRouterUrlSSL(snode.getHost(), snode.getHostAddr().GetPort(), fqUrl, data,
                                    timeout, clientKey, snode.getSnodePubKey(), feetx);
                        else
                            xrresponse = xrouter::CallXRouterUrl(snode.getHost(), snode.getHostAddr().GetPort(), fqUrl, data,
                                    timeout, clientKey, snode.getSnodePubKey(), feetx);
                    } catch (std::exception & e) {
                        UniValue error(UniValue::VOBJ);
                        error.pushKV("error", e.what());
                        error.pushKV("code", xrouter::Error::BAD_REQUEST);
                        error.pushKV("reply", UniValue::VNULL);
                        queryMgr.addReply(uuid, addr, error.write());
                        queryMgr.purge(uuid, addr);
                        return; // failed to connect
                    }

                    // Do not process if we aren't expecting a result. Also prevent reply malleability (only first reply is accepted)
                    if (!queryMgr.hasQuery(uuid, addr) || queryMgr.hasReply(uuid, addr))
                        return; // done, nothing found

                    // Verify servicenode response
                    CHashWriter hw(SER_GETHASH, 0);
                    hw << std::vector<unsigned char>(xrresponse.result.begin(), xrresponse.result.end());
                    CPubKey sigPubKey;
                    if (snode.getSnodePubKey() != xrresponse.hdrpubkey
                        || !sigPubKey.RecoverCompact(hw.GetHash(), xrresponse.hdrsignature)
                        || snode.getSnodePubKey() != sigPubKey)
                    {
                        UniValue error(UniValue::VOBJ);
                        error.pushKV("error", "Unable to verify if the service node is valid. Received bad signature on this request.");
                        error.pushKV("code", xrouter::Error::BAD_SIGNATURE);
                        error.pushKV("reply", xrresponse.result);
                        queryMgr.addReply(uuid, addr, error.write());
                        queryMgr.purge(uuid, addr);
                        return;
                    }

                    // Store the reply
                    queryMgr.addReply(uuid, addr, xrresponse.result);
                    queryMgr.purge(uuid, addr);
                });
            } catch (...) { }

            queryMgr.updateSentRequest(addr, fqService);
        }

        // At this point we need to wait for responses
        int confirmation_count = 0;
        auto queryCheckStart = GetTime();
        auto queries = queryMgr.allLocks(uuid);
        std::vector<NodeAddr> review;
        for (auto & query : queries)
            review.push_back(query.first);

        // Check that all replies have arrived, only run as long as timeout
        while (!ShutdownRequested() && confirmation_count < confs
            && GetTime() - queryCheckStart < timeout)
        {
            boost::this_thread::sleep_for(boost::chrono::milliseconds(5));
            for (int i = (int)review.size() - 1; i >= 0; --i)
                if (queryMgr.hasReply(uuid, review[i])) {
                    ++confirmation_count;
                    review.erase(review.begin()+i);
                }
        }

        // Clean up
        queryMgr.purge(uuid);

        std::set<NodeAddr> failed;

        if (confirmation_count < confs) {
            failed.insert(review.begin(), review.end());

            auto sns = getServiceNodes();
            std::map<NodeAddr, sn::ServiceNode*> snodec;
            for (auto & s : sns) {
                if (!s.getHostPort().empty())
                    snodec[s.getHostPort()] = &s;
            }

            // Penalize the snodes that didn't respond
            for (const auto & addr : review) {
                if (!snodec.count(addr))
                    continue;
                queryMgr.updateScore(addr, -25);
            }

            std::set<std::string> snodeAddresses;
            for (const auto & addr : review) {
                if (!snodec.count(addr))
                    continue;
                auto s = snodec[addr];
                snodeAddresses.insert(EncodeDestination(CTxDestination(s->getPaymentAddress())));
            }

            // TODO Blocknet xrclient unlock failed txs
//            for (const auto & addr : failed) { // unlock any fee txs
//                const auto & tx = feePaymentTxs[addr];
//                unlockOutputs(tx);
//            }
        }

        // Handle the results
        std::map<NodeAddr, std::string> replies;
        std::set<NodeAddr> diff;
        std::set<NodeAddr> agree;
        std::string rawResult;
        int c = queryMgr.mostCommonReply(uuid, rawResult, replies, agree, diff);
        for (const auto & addr : diff) // penalize nodes that didn't match consensus
            queryMgr.updateScore(addr, -5);
        if (c > 1) { // only update score if there's consensus
            for (const auto & addr : agree) {
                if (!failed.count(addr))
                    queryMgr.updateScore(addr, c > 1 ? c * 2 : 0); // boost majority consensus nodes
            }
        }

        // Check for errors
        for (const auto & rp : replies) {
            try {
                UniValue resultVal;
                if (resultVal.read(rp.second) && resultVal.isObject()) {
                    const auto & err_code = find_value(resultVal, "code");
                    const auto & rv = find_value(resultVal, "result");
                    if (err_code.isNum() && err_code.get_int() == INTERNAL_SERVER_ERROR)
                        queryMgr.updateScore(rp.first, -2); // penalize server errors
                    else if (rv.isObject()) {
                        const auto & err_code2 = find_value(rv, "code");
                        if (err_code2.isNum() && err_code2.get_int() == INTERNAL_SERVER_ERROR)
                            queryMgr.updateScore(rp.first, -2); // penalize server errors
                    }
                }
            } catch (...) { } // do not report on non-error objs
        }

        // TODO Blocknet xrclient unlock any utxos associated with replies that returned an error
//        if (!feePaymentTxs.empty()) {
//            for (const auto & item : replies) {
//                const auto & nodeAddr = item.first;
//                const auto & reply = item.second;
//                UniValue uv;
//                if (uv.read(reply) && uv.isObject()) {
//                    const auto & err = find_value(uv.get_obj(), "error");
//                    if (!err.isNull()) {
//                        const auto & tx = feePaymentTxs[nodeAddr];
//                        unlockOutputs(tx);
//                    }
//                }
//            }
//        }

        return rawResult;

    } catch (XRouterError & e) {
        // TODO Blocknet xrclient unlock any fee txs
//        for (const auto & item : feePaymentTxs) {
//            const std::string & tx = item.second;
//            unlockOutputs(tx);
//        }

        std::string errmsg = e.msg;
        if (!nodeErrors.empty()) {
            for (const auto & em : nodeErrors)
                errmsg += strprintf(" | %s code %u", em.first, em.second);
        }

        UniValue error(UniValue::VOBJ);
        error.pushKV("error", errmsg);
        error.pushKV("code", e.code);
        error.pushKV("uuid", uuid);
        return error.write();

    } catch (std::exception & e) {
        // TODO Blocknet xrclient unlock any fee txs
//        for (const auto & item : feePaymentTxs) {
//            const std::string & tx = item.second;
//            unlockOutputs(tx);
//        }

        UniValue error(UniValue::VOBJ);
        error.pushKV("error", "Internal Server Error");
        error.pushKV("code", INTERNAL_SERVER_ERROR);
        error.pushKV("uuid", uuid);
        return error.write();
    }
}

XRouterClient::~XRouterClient() {
    for (const auto & client : interfaces.chain_clients)
        client->stop();
    peerMgr.reset();
    g_connman.reset();
    interfaces.chain_clients.clear();
    UnregisterAllValidationInterfaces();
    GetMainSignals().UnregisterBackgroundSignalScheduler();
    globalVerifyHandle.reset();
    ECC_Stop();
    settings.clear();
    smgr.reset();
    gArgs.ClearArgs();
#ifdef ENABLE_EVENTSSL
    ENGINE_cleanup();
    ERR_free_strings();
    EVP_cleanup();
#endif // ENABLE_EVENTSSL
}

}