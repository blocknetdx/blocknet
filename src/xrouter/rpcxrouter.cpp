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

    // TODO: check that it is integer
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
    std::string reply = xrouter::App::instance().getBlockHash(uuid, currency, confirmations, params[1].get_str());
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
        throw std::runtime_error("xrGetBlocks currency number [servicenode_consensus_number]\nReturns a list of all blocks starting with n for selected currency.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Currency not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    std::string number;
    if (params.size() < 2)
    {
        number = "0";
    }
    else
    {
        number = params[1].get_str();
    }

    int confirmations{0};
    if (params.size() >= 3)
        confirmations = params[2].get_int();

    std::string currency = params[0].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().getAllBlocks(uuid, currency, confirmations, number);
    return xrouter::form_reply(uuid, reply);
}

Value xrGetTransactions(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetTransactions currency account [number] [servicenode_consensus_number]\nReturns all transactions to/from account starting from block [number] for selected currency.");
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
        error.emplace_back("error", "Address not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }

    std::string number;
    if (params.size() < 3)
    {
        number = "0";
    }
    else
    {
        number = params[2].get_str();
    }
    
    int confirmations{0};
    if (params.size() >= 4)
        confirmations = params[3].get_int();

    std::string currency = params[0].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().getAllTransactions(uuid, currency, confirmations, params[1].get_str(), number);
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

Value xrGetBalanceUpdate(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetBalanceUpdate currency account [number] [servicenode_consensus_number]\nReturns balance update for account starting with block number (default: 0) for selected currency.");
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

    std::string number;
    if (params.size() < 3)
    {
        number = "0";
    }
    else
    {
        number = params[2].get_str();
    }

    int confirmations{0};
    if (params.size() >= 4)
        confirmations = params[3].get_int();
    
    std::string currency = params[0].get_str();
    std::string account = params[1].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().getBalanceUpdate(uuid, currency, confirmations, account, number);
    return xrouter::form_reply(uuid, reply);
}

Value xrGetTransactionsBloomFilter(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetTransactionsBloomFilter currency filter [number] [servicenode_consensus_number]\nReturns transactions fitting bloom filter starting with block number (default: 0) for selected currency.");
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

    std::string number;
    if (params.size() < 3)
    {
        number = "0";
    }
    else
    {
        number = params[2].get_str();
    }

    int confirmations{0};
    if (params.size() >= 4)
        confirmations = params[3].get_int();
    
    std::string currency = params[0].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().getTransactionsBloomFilter(uuid, currency, confirmations, params[1].get_str(), number);
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
    
    CBloomFilter f(10 * static_cast<int>(params.size()), 0.1, 5, 0);
    
    vector<unsigned char> data;
    Array invalid;
    for (unsigned int i = 0; i < params.size(); i++) {
        std::string addr_string = params[i].get_str();
        xrouter::UnknownChainAddress address(addr_string);
        if (!address.IsValid()) {
            // This is a hash
            data = ParseHex(addr_string);
            CPubKey pubkey(data);
            if (!pubkey.IsValid()) {
                //LOG() << "Ignoring invalid address " << addr_string << std::endl;
                invalid.push_back(Value(addr_string));
                continue;
            }
            
            f.insert(data);
            continue;
        } else {
            // This is a bitcoin address
            CKeyID keyid;
            address.GetKeyID(keyid);
            data = vector<unsigned char>(keyid.begin(), keyid.end());
            f.insert(data);
        }
    }
    
    if (invalid.size() > 0) {
        result.emplace_back("skipped-invalid", invalid);
    }
    
    if (invalid.size() == params.size()) {
        result.emplace_back("error", "No valid addresses");
        result.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return result;
    }
    
    result.emplace_back("reply", f.to_hex());
    result.emplace_back("code", xrouter::SUCCESS);
    
    return result;
}

Value xrCustomCall(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrCustomCall service_name param1 param2 param3 ... paramN\nSends the custom call with [service_name] name.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back("error", "Service name not specified");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }
    
    std::string name = params[0].get_str();
    std::vector<std::string> call_params;
    for (unsigned int i = 1; i < params.size(); i++)
        call_params.push_back(params[i].get_str());
    std::string uuid;
    std::string reply = xrouter::App::instance().sendCustomCall(uuid, name, call_params);
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
        throw std::runtime_error("xrUpdateConfigs\nSends requests for all service node configs.");
    }
    
    Object result;
    std::string reply = xrouter::App::instance().updateConfigs();
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

Value xrOpenConnections(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrOpenConnections\nOpens connections to all available service nodes.");
    }
    
    Object result;
    xrouter::App::instance().openConnections();
    return true;
}

Value xrTimeToBlockNumber(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrTimeToBlockNumber currency timestamp [servicenode_consensus_number]\nGet the block count at specified time.");
    }

    return "This function is not implemented yet";
    
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

    int confirmations{0};
    if (params.size() >= 3)
        confirmations = params[2].get_int();
    
    std::string currency = params[0].get_str();
    std::string uuid;
    std::string reply = xrouter::App::instance().convertTimeToBlockCount(uuid, currency, confirmations, params[1].get_str());
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
    bool reply = xrouter::App::instance().checkDomain(uuid, domain);
    return xrouter::form_reply(uuid, reply ? "true" : "false");
}

Value xrCreateDepositAddress(const Array& params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrCreateDepositAddress [<true/false>]\nSetup deposit pubkey and address on service node. If the first parameters is 'true', xrouter.conf will be updated automatically. ");
    }
    
    bool update = false;
    if (params.size() > 1)
    {
        Object error;
        error.emplace_back("error", "Too many parameters");
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return error;
    }
    
    if (params.size() == 1) {
        if (params[0].get_str() == "false") {
            update = false;
        } else if (params[0].get_str() == "true") {
            update = true;
        } else {
            Object error;
            error.emplace_back("error", "Invalid parameter: must be true or false");
            error.emplace_back("code", xrouter::INVALID_PARAMETERS);
            return error;
        }
    }

    std::string uuid;
    std::string res = xrouter::App::instance().createDepositAddress(uuid, update);
    return xrouter::form_reply(uuid, res);
}

Value xrTest(const Array& params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrTest\nAuxiliary call");
    }
    
    xrouter::App::instance().runTests();
    return "true";
}
