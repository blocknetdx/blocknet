#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

#include <exception>
#include <iostream>
#include "bloom.h"
#include "core_io.h"

#include "xrouterapp.h"
#include "xroutererror.h"
#include "xrouterutils.h"
#include "uint256.h"
using namespace json_spirit;

static Object form_reply(std::string reply)
{
    Object ret;
    Value reply_val;
    read_string(reply, reply_val);
    
    if (reply_val.type() == array_type) {
        ret.emplace_back(Pair("reply", reply_val));
        ret.emplace_back(Pair("code", xrouter::SUCCESS));
        return ret;
    }
    
    if (reply_val.type() != obj_type) {
        ret.emplace_back(Pair("reply", reply));
        ret.emplace_back(Pair("code", xrouter::SUCCESS));
        return ret;
    }
    
    Object reply_obj = reply_val.get_obj();
    const Value & result = find_value(reply_obj, "result");
    const Value & error  = find_value(reply_obj, "error");
    const Value & code  = find_value(reply_obj, "code");
    const Value & uuid  = find_value(reply_obj, "uuid");
    
    if (error.type() != null_type) {
        ret.emplace_back(Pair("error", error));
        if (code.type() != null_type)
            ret.emplace_back(Pair("code", code));
        else
            ret.emplace_back(Pair("code", xrouter::INTERNAL_SERVER_ERROR));
    }
    else if (result.type() != null_type) {
        ret.emplace_back(Pair("reply", result));
        ret.emplace_back(Pair("code", xrouter::SUCCESS));
    }
    else {
        return reply_obj;
    }
    
    if (uuid.type() != null_type) {
        ret.emplace_back(Pair("uuid", uuid));
    } else {
        ret.emplace_back(Pair("uuid", ""));
    }
    
    return ret;
}

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
        error.emplace_back(Pair("error", "Currency not specified"));
        error.emplace_back(Pair("code", xrouter::INVALID_PARAMETERS));
        error.emplace_back(Pair("uuid", ""));
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
        throw std::runtime_error("xrGetBlockHash currency number [servicenode_consensus_number]\nLookup block hash by block number in a specified blockchain.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back(Pair("error", "Currency not specified"));
        error.emplace_back(Pair("code", xrouter::INVALID_PARAMETERS));
        error.emplace_back(Pair("uuid", ""));
        return error;
    }

    // TODO: check that it is integer
    if (params.size() < 2)
    {
        Object error;
        error.emplace_back(Pair("error", "Block hash not specified"));
        error.emplace_back(Pair("code", xrouter::INVALID_PARAMETERS));
        error.emplace_back(Pair("uuid", ""));
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
        throw std::runtime_error("xrGetBlock currency hash [servicenode_consensus_number]\nLookup block data by block hash in a specified blockchain.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back(Pair("error", "Currency not specified"));
        error.emplace_back(Pair("code", xrouter::INVALID_PARAMETERS));
        error.emplace_back(Pair("uuid", ""));
        return error;
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back(Pair("error", "Block hash not specified"));
        error.emplace_back(Pair("code", xrouter::INVALID_PARAMETERS));
        error.emplace_back(Pair("uuid", ""));
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
        throw std::runtime_error("xrGetTransaction currency txid [servicenode_consensus_number]\nLookup transaction data by transaction id in a specified blockchain.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back(Pair("error", "Currency not specified"));
        error.emplace_back(Pair("code", xrouter::INVALID_PARAMETERS));
        error.emplace_back(Pair("uuid", ""));
        return error;
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back(Pair("error", "Tx hash not specified"));
        error.emplace_back(Pair("code", xrouter::INVALID_PARAMETERS));
        error.emplace_back(Pair("uuid", ""));
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

Value xrGetBlocks(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetBlocks currency number [servicenode_consensus_number]\nReturns a list of all blocks starting with n for selected currency.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back(Pair("error", "Currency not specified"));
        error.emplace_back(Pair("code", xrouter::INVALID_PARAMETERS));
        error.emplace_back(Pair("uuid", ""));
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

Value xrGetTransactions(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetTransactions currency account [number] [servicenode_consensus_number]\nReturns all transactions to/from account starting from block [number] for selected currency.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back(Pair("error", "Currency not specified"));
        error.emplace_back(Pair("code", xrouter::INVALID_PARAMETERS));
        error.emplace_back(Pair("uuid", ""));
        return error;
    }
    
    if (params.size() < 2)
    {
        Object error;
        error.emplace_back(Pair("error", "Address not specified"));
        error.emplace_back(Pair("code", xrouter::INVALID_PARAMETERS));
        error.emplace_back(Pair("uuid", ""));
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
        throw std::runtime_error("xrGetBalance currency account [servicenode_consensus_number]\nReturns balance for selected account for selected currency.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back(Pair("error", "Currency not specified"));
        error.emplace_back(Pair("code", xrouter::INVALID_PARAMETERS));
        error.emplace_back(Pair("uuid", ""));
        return error;
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back(Pair("error", "Account not specified"));
        error.emplace_back(Pair("code", xrouter::INVALID_PARAMETERS));
        error.emplace_back(Pair("uuid", ""));
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
        throw std::runtime_error("xrGetBalanceUpdate currency account [number] [servicenode_consensus_number]\nReturns balance update for account starting with block number (default: 0) for selected currency.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back(Pair("error", "Currency not specified"));
        error.emplace_back(Pair("code", xrouter::INVALID_PARAMETERS));
        error.emplace_back(Pair("uuid", ""));
        return error;
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back(Pair("error", "Account not specified"));
        error.emplace_back(Pair("code", xrouter::INVALID_PARAMETERS));
        error.emplace_back(Pair("uuid", ""));
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
        throw std::runtime_error("xrGetTransactionsBloomFilter currency filter [number] [servicenode_consensus_number]\nReturns transactions fitting bloom filter starting with block number (default: 0) for selected currency.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back(Pair("error", "Currency not specified"));
        error.emplace_back(Pair("code", xrouter::INVALID_PARAMETERS));
        error.emplace_back(Pair("uuid", ""));
        return error;
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back(Pair("error", "Filter not specified"));
        error.emplace_back(Pair("code", xrouter::INVALID_PARAMETERS));
        error.emplace_back(Pair("uuid", ""));
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

    std::string reply = xrouter::App::instance().getTransactionsBloomFilter(currency, params[1].get_str(), number, confirmations);
    return form_reply(reply);
}

Value xrGenerateBloomFilter(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGenerateBloomFilter address1 address2 ...\nReturns bloom filter for given base58 addresses or public key hashes.");
    }
    
    Object result;
    if (params.size() == 0) {
        result.emplace_back(Pair("error", "No valid addresses"));
        result.emplace_back(Pair("code", xrouter::INVALID_PARAMETERS));
        result.emplace_back(Pair("uuid", ""));
        return result;
    }
    
    CBloomFilter f(10 * params.size(), 0.1, 5, 0);
    
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
        result.emplace_back(Pair("skipped-invalid", invalid));
    }
    
    if (invalid.size() == params.size()) {
        result.emplace_back(Pair("error", "No valid addresses"));
        result.emplace_back(Pair("code", xrouter::INVALID_PARAMETERS));
        result.emplace_back(Pair("uuid", ""));
        return result;
    }
    
    result.emplace_back(Pair("reply", f.to_hex()));
    result.emplace_back(Pair("code", xrouter::SUCCESS));
    
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
        error.emplace_back(Pair("error", "Service name not specified"));
        error.emplace_back(Pair("code", xrouter::INVALID_PARAMETERS));
        error.emplace_back(Pair("uuid", ""));
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
        error.emplace_back(Pair("code", xrouter::INVALID_PARAMETERS));
        error.emplace_back(Pair("uuid", ""));
        return error;
    }

    if (params.size() < 2)
    {
        Object error;
        error.emplace_back(Pair("error", "Transaction not specified"));
        error.emplace_back(Pair("code", xrouter::INVALID_PARAMETERS));
        error.emplace_back(Pair("uuid", ""));
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
        error.emplace_back(Pair("code", xrouter::INVALID_PARAMETERS));
        error.emplace_back(Pair("uuid", ""));
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

Value xrTimeToBlockNumber(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrTimeToBlockNumber currency timestamp [servicenode_consensus_number]\nGet the block count at specified time.");
    }

    return "This function is not implemented yet";
    
    if (params.size() < 1)
    {
        Object error;
        error.emplace_back(Pair("error", "Currency not specified"));
        error.emplace_back(Pair("code", xrouter::INVALID_PARAMETERS));
        error.emplace_back(Pair("uuid", ""));
        return error;
    }
    
    if (params.size() < 2)
    {
        Object error;
        error.emplace_back(Pair("error", "Timestamp not specified"));
        error.emplace_back(Pair("code", xrouter::INVALID_PARAMETERS));
        error.emplace_back(Pair("uuid", ""));
        return error;
    }
    
    std::string confirmations = "";
    if (params.size() >= 3)
    {
        confirmations = params[2].get_str();
    }
    
    std::string currency = params[0].get_str();
    std::string reply = xrouter::App::instance().convertTimeToBlockCount(currency, params[1].get_str(), confirmations);
    return form_reply(reply);
}

Value xrRegisterDomain(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrRegisterDomain domain [true/false] [addr]\nCreate the transactions described above needed to register the domain name. If the second parameter is true, your xrouter.conf is updated automatically. The third parameter is the destination address of hte transaction, leave this parameter blank if you want to use your service node collateral address");
    }
    
    if (params.size() < 1)
    {
        Object error;
        error.emplace_back(Pair("error", "Domain name not specified"));
        error.emplace_back(Pair("code", xrouter::INVALID_PARAMETERS));
        error.emplace_back(Pair("uuid", ""));
        return error;
    }

    bool update = false;
    if (params.size() > 3)
    {
        Object error;
        error.emplace_back(Pair("error", "Too many parameters"));
        error.emplace_back(Pair("code", xrouter::INVALID_PARAMETERS));
        error.emplace_back(Pair("uuid", ""));
        return error;
    }
    
    if (params.size() >= 2) {
        if (params[1].get_str() == "false") {
            update = false;
        } else if (params[1].get_str() == "true") {
            update = true;
        } else {
            Object error;
            error.emplace_back(Pair("error", "Invalid parameter: must be true or false"));
            error.emplace_back(Pair("code", xrouter::INVALID_PARAMETERS));
            error.emplace_back(Pair("uuid", ""));
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
    
    return form_reply(xrouter::App::instance().registerDomain(domain, addr, update));
}

Value xrQueryDomain(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrQueryDomain domain\nCheck if the domain name is registered and return true if it is found");
    }
    
    if (params.size() < 1)
    {
        Object error;
        error.emplace_back(Pair("error", "Domain name not specified"));
        error.emplace_back(Pair("code", xrouter::INVALID_PARAMETERS));
        error.emplace_back(Pair("uuid", ""));
        return error;
    }

    std::string domain = params[0].get_str();
    bool res = xrouter::App::instance().queryDomain(domain);
    return form_reply(res ? "true" : "false");
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
        error.emplace_back(Pair("error", "Too many parameters"));
        error.emplace_back(Pair("code", xrouter::INVALID_PARAMETERS));
        error.emplace_back(Pair("uuid", ""));
        return error;
    }
    
    if (params.size() == 1) {
        if (params[0].get_str() == "false") {
            update = false;
        } else if (params[0].get_str() == "true") {
            update = true;
        } else {
            Object error;
            error.emplace_back(Pair("error", "Invalid parameter: must be true or false"));
            error.emplace_back(Pair("code", xrouter::INVALID_PARAMETERS));
            error.emplace_back(Pair("uuid", ""));
            return error;
        }
    }

    std::string res = xrouter::App::instance().createDepositAddress(update);
    return form_reply(res);
}

Value xrPaymentChannels(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrPaymentChannels\nPrint info about currently open payment channels");
    }
    
    return xrouter::App::instance().printPaymentChannels();
}

Value xrClosePaymentChannel(const Array& params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrClosePaymentChannel id\nClose payment channel with the given id");
    }
    
    if (params.size() < 1)
    {
        Object error;
        error.emplace_back(Pair("error", "Node id not specified"));
        error.emplace_back(Pair("code", xrouter::INVALID_PARAMETERS));
        error.emplace_back(Pair("uuid", ""));
        return error;
    }
    
    std::string id = params[0].get_str();
    xrouter::App::instance().closePaymentChannel(id);
    return "";
}

Value xrCloseAllPaymentChannels(const Array& params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrClosePaymentChannel\nClose all payment channels");
    }
    
    xrouter::App::instance().closeAllPaymentChannels();
    return "";
}

Value xrTest(const Array& params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrTest\nAuxiliary call");
    }
    
    xrouter::App::instance().runTests();
    return "";
}
