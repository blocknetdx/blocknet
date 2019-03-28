#include "xrouterapp.h"
#include "xroutererror.h"
#include "xrouterutils.h"

#include "uint256.h"
#include "bloom.h"
#include "core_io.h"

#include <exception>
#include <iostream>

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

using namespace json_spirit;

//******************************************************************************
//******************************************************************************
Value xrGetBlockCount(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetBlockCount currency [servicenode_consensus_number]\nLookup total number of blocks in a specified blockchain.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Currency not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }
    
    int confirmations{0};
    if (params.size() >= 2)
        confirmations = params[1].get_int();

    std::string currency = params[0].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().getBlockCount(uuid, currency, confirmations);
    return xrouter::form_reply(uuid, reply);
}

Value xrGetBlockHash(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetBlockHash currency number [servicenode_consensus_number]\nLookup block hash by block number in a specified blockchain.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Currency not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back("error", "Block hash not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    int confirmations{0};
    if (params.size() >= 3)
        confirmations = params[2].get_int();

    std::string currency = params[0].get_str();
    int block = params[1].get_int();
    std::string uuid;
    std::string reply = xrouter::App::instance().getBlockHash(uuid, currency, confirmations, block);
    return xrouter::form_reply(uuid, reply);
}

Value xrGetBlock(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetBlock currency hash [servicenode_consensus_number]\nLookup block data by block hash in a specified blockchain.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Currency not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back("error", "Block hash not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    int confirmations{0};
    if (params.size() >= 3)
        confirmations = params[2].get_int();

    std::string currency = params[0].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().getBlock(uuid, currency, confirmations, params[1].get_str());
    return xrouter::form_reply(uuid, reply);
}

Value xrGetTransaction(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetTransaction currency txid [servicenode_consensus_number]\nLookup transaction data by transaction id in a specified blockchain.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Currency not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back("error", "Tx hash not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    int confirmations{0};
    if (params.size() >= 3)
        confirmations = params[2].get_int();

    std::string currency = params[0].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().getTransaction(uuid, currency, confirmations, params[1].get_str());
    return xrouter::form_reply(uuid, reply);
}

Value xrGetBlocks(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetBlocks currency blockhash1,blockhash2,blockhash3 [servicenode_consensus_number]\nReturns blocks associated with the specified hashes.");
    }

    if (params.size() < 1) {
        Object error;
        error.emplace_back("error", "Currency not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    if (params.size() < 2) {
        Object error;
        error.emplace_back("error", "Block hashes not specified (comma delimited list)");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    std::set<std::string> blockHashes;
    const auto & hashes = params[1].get_str();
    boost::split(blockHashes, hashes, boost::is_any_of(","));
    for (const auto & hash : blockHashes) {
        if (hash.empty() || hash.find(',') != std::string::npos) {
            Object error;
            error.emplace_back("error", "Block hashes must be specified in a comma delimited list with no spaces.\n"
                                        "Example: xrGetBlocks BLOCK 302a309d6b6c4a65e4b9ff06c7ea81bb17e985d00abdb01978ace62cc5e18421,"
                                        "175d2a428b5649c2a4732113e7f348ba22a0e69cc0a87631449d1d77cd6e1b04,"
                                        "34989eca8ed66ff53631294519e147a12f4860123b4bdba36feac6da8db492ab");
            error.emplace_back("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    }

    int confirmations{0};
    if (params.size() >= 3)
        confirmations = params[2].get_int();

    std::string currency = params[0].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().getBlocks(uuid, currency, confirmations, blockHashes);
    return xrouter::form_reply(uuid, reply);
}

Value xrGetTransactions(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetTransactions currency txhash1,txhash2,txhash3 [servicenode_consensus_number]\nReturns all transactions to/from account starting from block [number] for selected currency.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Currency not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }
    
    if (params.size() < 2)
    {
        Object error;
        error.emplace_back("error", "Transaction hashes not specified (comma delimited list)");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    std::set<std::string> txHashes;
    const auto & hashes = params[1].get_str();
    boost::split(txHashes, hashes, boost::is_any_of(","));
    for (const auto & hash : txHashes) {
        if (hash.empty() || hash.find(',') != std::string::npos) {
            Object error;
            error.emplace_back("error", "Transaction hashes must be specified in a comma delimited list with no spaces.\n"
                                        "Example: xrGetTransactions BLOCK 24ff5506a30772acfb65012f1b3309d62786bc386be3b6ea853a798a71c010c8,"
                                        "24b6bcb44f045d7a4cf8cd47c94a14cc609352851ea973f8a47b20578391629f,"
                                        "66a5809c7090456965fe30280b88f69943e620894e1c4538a724ed9a89c769be");
            error.emplace_back("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    }
    
    int confirmations{0};
    if (params.size() >= 3)
        confirmations = params[2].get_int();

    std::string currency = params[0].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().getTransactions(uuid, currency, confirmations, txHashes);
    return xrouter::form_reply(uuid, reply);
}

Value xrGetBalance(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetBalance currency account [servicenode_consensus_number]\nReturns balance for selected account for selected currency.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Currency not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back("error", "Account not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    int confirmations{0};
    if (params.size() >= 3)
        confirmations = params[2].get_int();

    std::string currency = params[0].get_str();
    std::string account = params[1].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().getBalance(uuid, currency, confirmations, account);
    return xrouter::form_reply(uuid, reply);
}

Value xrGetTxBloomFilter(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetTxBloomFilter currency filter [number] [servicenode_consensus_number]\nReturns transactions fitting bloom filter starting with block number (default: 0) for selected currency.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Currency not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back("error", "Filter not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    int number{0};
    if (params.size() >= 3)
        number = params[2].get_int();

    int confirmations{0};
    if (params.size() >= 4)
        confirmations = params[3].get_int();
    
    const auto & currency = params[0].get_str();
    const auto & filter = params[1].get_str();
    std::string uuid;
    const auto reply = xrouter::App::instance().getTransactionsBloomFilter(uuid, currency, confirmations, filter, number);
    return xrouter::form_reply(uuid, reply);
}

Value xrGenerateBloomFilter(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGenerateBloomFilter address1 address2 ...\nReturns bloom filter for given base58 addresses or public key hashes.");
    }
    
    Object result;

    if (params.size() == 0) {
        result.emplace_back("error", "No valid addresses");
        result.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return result;
    }
    
    CBloomFilter f(10 * static_cast<unsigned int>(params.size()), 0.1, 5, 0);

    Array invalid;

    for (const auto & param : params) {
        vector<unsigned char> data;
        const std::string & addr = param.get_str();
        xrouter::UnknownChainAddress address(addr);
        if (!address.IsValid()) { // Try parsing pubkey
            data = ParseHex(addr);
            CPubKey pubkey(data);
            if (!pubkey.IsValid()) {
                invalid.push_back(Value(addr));
                continue;
            }
            f.insert(data);
        } else {
            // This is a bitcoin address
            CKeyID keyid; address.GetKeyID(keyid);
            data = vector<unsigned char>(keyid.begin(), keyid.end());
            f.insert(data);
        }
    }
    
    if (!invalid.empty()) {
        result.emplace_back("skipped-invalid", invalid);
    }
    
    if (invalid.size() == params.size()) {
        result.emplace_back("error", "No valid addresses");
        result.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return result;
    }

    result.emplace_back("bloomfilter", f.to_hex());

    Object reply;
    reply.emplace_back("result", result);

    const std::string & uuid = xrouter::generateUUID();
    return xrouter::form_reply(uuid, write_string(Value(reply), false));
}

Value xrService(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrService service_name param1 param2 param3 ... paramN\nSends the custom call with [service_name] name.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Service name not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }
    
    const std::string & service = params[0].get_str();
    std::vector<std::string> call_params;
    for (unsigned int i = 1; i < params.size(); i++)
        call_params.push_back(params[i].get_str());
    std::string uuid;
    std::string reply = xrouter::App::instance().xrouterCall(xrouter::xrService, uuid, service, 0, call_params);
    return xrouter::form_reply(uuid, reply);
}

Value xrSendTransaction(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrSendTransaction txdata\nSends signed transaction for selected currency.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Currency not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back("error", "Transaction not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }
    
    std::string currency = params[0].get_str();
    std::string transaction = params[1].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().sendTransaction(uuid, currency, transaction);
    return xrouter::form_reply(uuid, reply);
}

Value xrGetReply(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetReply uuid\nRetrieves reply to request with uuid.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "UUID not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }
    
    std::string uuid = params[0].get_str();
    Object result;
    std::string reply = xrouter::App::instance().getReply(uuid);
    return xrouter::form_reply(uuid, reply);
}

Value xrUpdateConfigs(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrUpdateConfigs\nRequests latest configuration files for all connected service nodes.");
    }

    bool forceCheck = false;
    if (params.size() == 1)
        forceCheck = params[0].get_bool();

    Object result;
    std::string reply = xrouter::App::instance().updateConfigs(forceCheck);
    Object obj;
    obj.emplace_back("reply", reply);
    return obj;
}

Value xrShowConfigs(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrShowConfigs\nPrints all service node configs.");
    }
    
    Object result;
    std::string reply = xrouter::App::instance().printConfigs();
    return reply;
}

Value xrReloadConfigs(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrReloadConfigs\nReloads xrouter.conf and plugin configs from disk in case they were changed externally.");
    }
    
    Object result;
    xrouter::App::instance().reloadConfigs();
    return true;
}

Value xrStatus(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrStatus\nShow current XRouter status and info.");
    }
    
    Object result;
    std::string reply = xrouter::App::instance().getStatus();
    return reply;
}

Value xrConnectedNodes(const Array& params, bool fHelp)
{
    if (fHelp)
        throw std::runtime_error("xrConnectedNodes\nLists all the connected nodes and associated configuration "
                                 "information and fee schedule.");

    if (!params.empty()) {
        Object error;
        error.emplace_back("error", "This call does not support parameters");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    const std::string & uuid = xrouter::generateUUID();
    auto & app = xrouter::App::instance();
    const auto configs = app.getNodeConfigs();

    Array data;
    app.snodeConfigJSON(configs, data);

    return xrouter::form_reply(uuid, data);
}

Value xrConnect(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrConnect fully_qualified_service_name [node_count, optional, default=1]\n"
                                 "Connects to Service Nodes with the specified service and downloads their configs.\n"
                                 "\"fully_qualified_service_name\": Service name including the namespace, "
                                 "xr:: for SPV commands or xrs:: for plugin commands (e.g. xr::BLOCK)\n"
                                 "\"node_count\": Optionally specify the number of Service Nodes to connect to");
    }

    if (params.size() < 1) {
        Object error;
        error.emplace_back("error", "Service not specified. Example: xrConnect xr::BLOCK");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    const std::string & service = params[0].get_str();
    int nodeCount{1};

    if (params.size() > 1) {
        nodeCount = params[1].get_int();
    }

    const std::string & uuid = xrouter::generateUUID();
    auto & app = xrouter::App::instance();
    const auto configs = app.xrConnect(service, nodeCount);

    Array data;
    app.snodeConfigJSON(configs, data);

    return xrouter::form_reply(uuid, data);
}

Value xrGetBlockAtTime(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetBlockAtTime currency timestamp [servicenode_consensus_number]\nGet the block count at specified time.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Currency not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }
    
    if (params.size() < 2)
    {
        Object error;
        error.emplace_back("error", "Timestamp not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    int64_t time = params[1].get_int64();

    int confirmations{0};
    if (params.size() >= 3)
        confirmations = params[2].get_int();
    
    std::string currency = params[0].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().convertTimeToBlockCount(uuid, currency, confirmations, time);
    return xrouter::form_reply(uuid, reply);
}

Value xrRegisterDomain(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrRegisterDomain domain [true/false] [addr]\nCreate the transactions described above needed to register the domain name. If the second parameter is true, your xrouter.conf is updated automatically. The third parameter is the destination address of hte transaction, leave this parameter blank if you want to use your service node collateral address");
    }
    
    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Domain name not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    bool update = false;
    if (params.size() > 3)
    {
        Object error;
        error.emplace_back("error", "Too many parameters");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }
    
    if (params.size() >= 2) {
        if (params[1].get_str() == "false") {
            update = false;
        } else if (params[1].get_str() == "true") {
            update = true;
        } else {
            Object error;
            error.emplace_back("error", "Invalid parameter: must be true or false");
            error.emplace_back("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    }
    
    std::string domain = params[0].get_str();
    std::string addr;
    if (params.size() <= 2)
    {
        addr = xrouter::App::instance().getMyPaymentAddress();
    }
    else
    {
        addr = params[2].get_str();
    }

    if (addr.empty()) {
        Object error;
        error.emplace_back("error", "Bad payment address");
        error.emplace_back("code", xrouter::BAD_ADDRESS);
        return error;
    }

    std::string uuid;
    auto reply = xrouter::App::instance().registerDomain(uuid, domain, addr, update);
    return xrouter::form_reply(uuid, reply);
}

Value xrQueryDomain(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrQueryDomain domain\nCheck if the domain name is registered and return true if it is found");
    }
    
    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Domain name not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    std::string domain = params[0].get_str();
    std::string uuid;
    bool hasDomain = xrouter::App::instance().checkDomain(uuid, domain);
    std::string reply{hasDomain ? "true" : "false"};
    return xrouter::form_reply(uuid, reply);
}

Value xrTest(const Array& params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrTest\nAuxiliary call");
    }
    
    xrouter::App::instance().runTests();
    return "true";
}
