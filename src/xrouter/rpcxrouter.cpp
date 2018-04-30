#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

#include <exception>
#include <iostream>

#include "xrouterapp.h"
#include "uint256.h"
using namespace json_spirit;

//******************************************************************************
//******************************************************************************
Value xrGetBlockCount(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetBlock\nLookup blocks in a specified blockchain.");
    }

    if (params.size() < 1)
    {
        Object error;
        error.emplace_back(Pair("error", "Currency not specified"));
        error.emplace_back(Pair("name",     __FUNCTION__));
        return error;
    }

    std::string currency    = params[0].get_str();
    Object result;

    std::string reply = xrouter::App::instance().getBlockCount(currency);
    Object obj;
    obj.emplace_back(Pair("reply", reply));
    return obj;
}

Value xrGetBlockHash(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetBlock\nLookup blocks in a specified blockchain.");
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
    
    std::string currency    = params[0].get_str();
    Object result;

    std::string reply = xrouter::App::instance().getBlockHash(currency, params[1].get_str());
    Object obj;
    obj.emplace_back(Pair("reply", reply));
    return obj;
}

Value xrGetBlock(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetBlock\nLookup blocks in a specified blockchain.");
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

    std::string currency    = params[0].get_str();
    Object result;

    std::string reply = xrouter::App::instance().getBlock(currency, params[1].get_str());
    Object obj;
    obj.emplace_back(Pair("reply", reply));
    return obj;
}

Value xrGetTransaction(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetTransaction\nLookup blocks in a specified blockchain.");
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

    std::string currency    = params[0].get_str();
    Object result;

    std::string reply = xrouter::App::instance().getTransaction(currency, params[1].get_str());
    Object obj;
    obj.emplace_back(Pair("reply", reply));
    return obj;
}

Value xrGetAllBlocks(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetBalance\nReturns balances for selected currency.");
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

    std::string currency = params[0].get_str();
    
    Object result;

    std::string reply = xrouter::App::instance().getAllBlocks(currency, number);
    Object obj;
    obj.emplace_back(Pair("reply", reply));
    return obj;
}

Value xrGetAllTransactions(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetBalance\nReturns balances for selected currency.");
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

    std::string currency = params[0].get_str();
    
    Object result;

    std::string reply = xrouter::App::instance().getAllTransactions(currency, params[1].get_str(), number);
    Object obj;
    obj.emplace_back(Pair("reply", reply));
    return obj;
}

Value xrGetBalance(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetBalance\nReturns balances for selected currency.");
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

    std::string currency = params[0].get_str();
    std::string account = params[1].get_str();
    Object result;

    std::string reply = xrouter::App::instance().getBalance(currency, account);
    Object obj;
    obj.emplace_back(Pair("reply", reply));
    return obj;
}

Value xrGetBalanceUpdate(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetBalance\nReturns balances for selected currency.");
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
    
    std::string currency = params[0].get_str();
    std::string account = params[1].get_str();
    Object result;

    std::string reply = xrouter::App::instance().getBalanceUpdate(currency, account, number);
    Object obj;
    obj.emplace_back(Pair("reply", reply));
    return obj;
}