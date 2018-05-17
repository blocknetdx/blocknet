#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

#include <exception>
#include <iostream>
#include "bloom.h"

#include "xrouterapp.h"
#include "uint256.h"
using namespace json_spirit;

static Object form_reply(std::string reply)
{
    Object ret;
    Value reply_val;
    read_string(reply, reply_val);
    if (reply_val.type() != obj_type) {
        ret.emplace_back(Pair("reply", reply));
        return ret;
    }
    
    Object reply_obj = reply_val.get_obj();
    const Value & result = find_value(reply_obj, "result");
    const Value & error  = find_value(reply_obj, "error");

    
    if (error.type() != null_type)
    {
        ret.emplace_back(Pair("error", error));
    }
    else if (result.type() != null_type)
    {
        ret.emplace_back(Pair("reply", result));
    }
    
    return ret;
}

//******************************************************************************
//******************************************************************************
Value xrGetBlockCount(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetBlock currency\nLookup total number of blocks in a specified blockchain.");
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
        throw std::runtime_error("xrGetBlock currency number\nLookup block hash by block number in a specified blockchain.");
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
        throw std::runtime_error("xrGetBalance\nReturns a list of all blocks starting with n for selected currency.");
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
        throw std::runtime_error("xrGetBalance currency account [number]\nReturns all transactions to/from account starting from block [number] for selected currency.");
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
        throw std::runtime_error("xrGetBalance currency account [number]\nReturns balance update for account starting with block number (default: 0) for selected currency.");
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
        throw std::runtime_error("xrGetTransactionsBloomFilter currency account [number]\nReturns transactions for account starting with block number (default: 0) for selected currency.");
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
    
    CBloomFilter f(10, 0.1, 5, 0);
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << f;
    std::cout << stream.str() << " " << stream.size() << std::endl << std::flush;
    std::vector<int> addr(stream.begin(), stream.end());
    for (unsigned int i = 0; i < addr.size(); i++)
            std::cout << (int)addr[i];
        std::cout << std::endl;

    std::string reply = xrouter::App::instance().getTransactionsBloomFilter(currency, number, stream.str(), confirmations);
    return form_reply(reply);
}

Value xrSendTransaction(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetBalance txdata\nSends signed transaction for selected currency.");
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
    Object result;

    std::string reply = xrouter::App::instance().sendTransaction(currency, transaction);
    Object obj;
    obj.emplace_back(Pair("reply", reply));
    return obj;
}