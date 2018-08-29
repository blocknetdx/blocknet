#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

#include <exception>
#include <iostream>
#include "bloom.h"
#include "core_io.h"

#include "xrouterapp.h"
#include "uint256.h"
using namespace json_spirit;

static Object form_reply(std::string reply)
{
    Object ret;
    Value reply_val;
    read_string(reply, reply_val);
    
    if (reply_val.type() == array_type) {
        ret.emplace_back(Pair("reply", reply_val));
        return ret;
    }
    
    if (reply_val.type() != obj_type) {
        ret.emplace_back(Pair("reply", reply));
        return ret;
    }
    
    Object reply_obj = reply_val.get_obj();
    const Value & result = find_value(reply_obj, "result");
    const Value & error  = find_value(reply_obj, "error");

    
    if (error.type() != null_type)
    {
        return reply_obj;
    }
    else if (result.type() != null_type)
    {
        ret.emplace_back(Pair("reply", result));
    }
    else
    {
        return reply_obj;
    }
    
    return ret;
}

//******************************************************************************
//******************************************************************************
Value xrGetBlockCount(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetBlockCount currency\nLookup total number of blocks in a specified blockchain.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back(Pair("error", "Currency not specified"));
        error.emplace_back(Pair("name",     __FUNCTION__));
        return error;
    }
    
    std::string confirmations = "";
    if (params.size() >= 2)
    {
        confirmations = params[1].get_str();
    }

    std::string currency    = params[0].get_str();
    std::string reply = xrouter::App::instance().getBlockCount(currency, confirmations);
    return form_reply(reply);
}

Value xrGetBlockHash(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetBlockHash currency number\nLookup block hash by block number in a specified blockchain.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back(Pair("error", "Currency not specified"));
        error.emplace_back(Pair("name",     __FUNCTION__));
        return error;
    }

    // TODO: check that it is integer
    if (params.size() < 2)
    {
        Object error;
        error.emplace_back(Pair("error", "Block hash not specified"));
        error.emplace_back(Pair("name",     __FUNCTION__));
        return error;
    }
    
    std::string confirmations = "";
    if (params.size() >= 3)
    {
        confirmations = params[2].get_str();
    }
    
    std::string currency    = params[0].get_str();
    std::string reply = xrouter::App::instance().getBlockHash(currency, params[1].get_str(), confirmations);
    return form_reply(reply);
}

Value xrGetBlock(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetBlock currency hash\nLookup block data by block hash in a specified blockchain.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back(Pair("error", "Currency not specified"));
        error.emplace_back(Pair("name",     __FUNCTION__));
        return error;
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back(Pair("error", "Block hash not specified"));
        error.emplace_back(Pair("name",     __FUNCTION__));
        return error;
    }
    
    std::string confirmations = "";
    if (params.size() >= 3)
    {
        confirmations = params[2].get_str();
    }

    std::string currency    = params[0].get_str();
    std::string reply = xrouter::App::instance().getBlock(currency, params[1].get_str(), confirmations);
    return form_reply(reply);
}

Value xrGetTransaction(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetTransaction currency txid\nLookup transaction data by transaction id in a specified blockchain.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back(Pair("error", "Currency not specified"));
        error.emplace_back(Pair("name",     __FUNCTION__));
        return error;
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back(Pair("error", "Tx hash not specified"));
        error.emplace_back(Pair("name",     __FUNCTION__));
        return error;
    }
    
    std::string confirmations = "";
    if (params.size() >= 3)
    {
        confirmations = params[2].get_str();
    }

    std::string currency    = params[0].get_str();
    std::string reply = xrouter::App::instance().getTransaction(currency, params[1].get_str(), confirmations);
    return form_reply(reply);
}

Value xrGetAllBlocks(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetAllBlocks currency [number]\nReturns a list of all blocks starting with n for selected currency.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back(Pair("error", "Currency not specified"));
        error.emplace_back(Pair("name",     __FUNCTION__));
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
    
    std::string confirmations = "";
    if (params.size() >= 3)
    {
        confirmations = params[2].get_str();
    }

    std::string currency = params[0].get_str();
    std::string reply = xrouter::App::instance().getAllBlocks(currency, number, confirmations);
    return form_reply(reply);
}

Value xrGetAllTransactions(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetAllTransactions currency account [number]\nReturns all transactions to/from account starting from block [number] for selected currency.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back(Pair("error", "Currency not specified"));
        error.emplace_back(Pair("name",     __FUNCTION__));
        return error;
    }
    
    if (params.size() < 2)
    {
        Object error;
        error.emplace_back(Pair("error", "Address not specified"));
        error.emplace_back(Pair("name",     __FUNCTION__));
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
    
    std::string confirmations = "";
    if (params.size() >= 4)
    {
        confirmations = params[3].get_str();
    }

    std::string currency = params[0].get_str();
    std::string reply = xrouter::App::instance().getAllTransactions(currency, params[1].get_str(), number, confirmations);
    return form_reply(reply);
}

Value xrGetBalance(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetBalance currency account\nReturns balance for selected account for selected currency.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back(Pair("error", "Currency not specified"));
        error.emplace_back(Pair("name",     __FUNCTION__));
        return error;
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back(Pair("error", "Account not specified"));
        error.emplace_back(Pair("name",     __FUNCTION__));
        return error;
    }
    
    std::string confirmations = "";
    if (params.size() >= 3)
    {
        confirmations = params[2].get_str();
    }

    std::string currency = params[0].get_str();
    std::string account = params[1].get_str();
    std::string reply = xrouter::App::instance().getBalance(currency, account, confirmations);
    return form_reply(reply);
}

Value xrGetBalanceUpdate(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetBalanceUpdate currency account [number]\nReturns balance update for account starting with block number (default: 0) for selected currency.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back(Pair("error", "Currency not specified"));
        error.emplace_back(Pair("name",     __FUNCTION__));
        return error;
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back(Pair("error", "Account not specified"));
        error.emplace_back(Pair("name",     __FUNCTION__));
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
    
    std::string confirmations = "";
    if (params.size() >= 4)
    {
        confirmations = params[3].get_str();
    }
    
    std::string currency = params[0].get_str();
    std::string account = params[1].get_str();
    std::string reply = xrouter::App::instance().getBalanceUpdate(currency, account, number, confirmations);
    return form_reply(reply);
}

Value xrGetTransactionsBloomFilter(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetTransactionsBloomFilter currency filter [number]\nReturns transactions fitting bloom filter starting with block number (default: 0) for selected currency.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back(Pair("error", "Currency not specified"));
        error.emplace_back(Pair("name",     __FUNCTION__));
        return error;
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back(Pair("error", "Filter not specified"));
        error.emplace_back(Pair("name",     __FUNCTION__));
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
    
    std::string confirmations = "";
    if (params.size() >= 4)
    {
        confirmations = params[3].get_str();
    }
    
    std::string currency = params[0].get_str();
    std::string account = params[1].get_str();
    
    CBloomFilter f(params[1].get_str().size(), 0.1, 5, 0);
    f.from_hex(params[1].get_str());
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << f;

    std::string reply = xrouter::App::instance().getTransactionsBloomFilter(currency, number, stream.str(), confirmations);
    return form_reply(reply);
}

Value xrGenerateBloomFilter(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGenerateBloomFilter address1 address2 ...\nReturns bloom filter for given base58 addresses or public key hashes.");
    }

    if (params.size() == 0)
        return "";
    
    CBloomFilter f(10 * params.size(), 0.1, 5, 0);
    
    vector<unsigned char> data;
    for (unsigned int i = 0; i < params.size(); i++) {
        std::string addr_string = params[i].get_str();
        CBitcoinAddress address(addr_string);
        if (!address.IsValid()) {
            // This is a hash
            data = ParseHex(addr_string);
            CPubKey pubkey(data);
            if (!pubkey.IsValid()) {
                std::cout << "Ignoring invalid address " << addr_string << std::endl;
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
    
    return f.to_hex();
}

Value xrCustomCall(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrCustomCall service_name param1 param2 param3 ... paramN\nSends the custom call with [service_name] name.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back(Pair("error", "Service name not specified"));
        error.emplace_back(Pair("name",     __FUNCTION__));
        return error;
    }
    
    std::string name = params[0].get_str();
    std::vector<std::string> call_params;
    for (unsigned int i = 1; i < params.size(); i++)
        call_params.push_back(params[i].get_str());
    std::string reply = xrouter::App::instance().sendCustomCall(name, call_params);
    return form_reply(reply);
}

Value xrSendTransaction(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrSendTransaction txdata\nSends signed transaction for selected currency.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back(Pair("error", "Currency not specified"));
        error.emplace_back(Pair("name",     __FUNCTION__));
        return error;
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back(Pair("error", "Transaction not specified"));
        error.emplace_back(Pair("name",     __FUNCTION__));
        return error;
    }
    
    std::string currency = params[0].get_str();
    std::string transaction = params[1].get_str();
    std::string reply = xrouter::App::instance().sendTransaction(currency, transaction);
    return form_reply(reply);
}

Value xrGetReply(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetReply uuid\nRetrieves reply to request with uuid.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back(Pair("error", "UUID not specified"));
        error.emplace_back(Pair("name",     __FUNCTION__));
        return error;
    }
    
    std::string id = params[0].get_str();
    Object result;
    std::string reply = xrouter::App::instance().getReply(id);
    return form_reply(reply);
}

Value xrUpdateConfigs(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrUpdateConfigs\nSends requests for all service node configs.");
    }
    
    Object result;
    std::string reply = xrouter::App::instance().updateConfigs();
    Object obj;
    obj.emplace_back(Pair("reply", reply));
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
    return "XRouter Configs reloaded";
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
    return "";
}

Value xrRegisterDomain(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrRegisterDomain\nNot implemented yet");
    }
    
    //Object result;
    //xrouter::App::instance().openConnections();
    return "";
}

Value xrQueryDomain(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrQueryDomain\nNot implemented yet");
    }
    
    //Object result;
    //xrouter::App::instance().openConnections();
    return "";
}