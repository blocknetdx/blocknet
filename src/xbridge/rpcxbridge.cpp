#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/signals2.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <stdio.h>
#include <atomic>
#include <numeric>
#include <math.h>
#include "util/settings.h"
#include "util/logger.h"
#include "util/xbridgeerror.h"
#include "xbridgeapp.h"
#include "xbridgeexchange.h"
#include "xbridgetransaction.h"
#include "rpcserver.h"

#include "posixtimeconversion.h"

using namespace json_spirit;
using namespace std;
using namespace boost;
using namespace boost::asio;

using TransactionMap    = std::map<uint256, xbridge::TransactionDescrPtr>;
using TransactionPair   = std::pair<uint256, xbridge::TransactionDescrPtr>;

using RealVector        = std::vector<double>;

namespace bpt           = boost::posix_time;

double xBridgeValueFromAmount(uint64_t amount)
{
    return static_cast<double>(amount) / xbridge::TransactionDescr::COIN;
}

uint64_t xBridgeAmountFromReal(double val)
{
    // TODO: should we check amount ranges and throw JSONRPCError like they do in rpcserver.cpp ?
    return static_cast<uint64_t>(val * xbridge::TransactionDescr::COIN + 0.5);
}

/** \brief Returns the list of open and pending transactions
  * \param params A list of input params.
  * \param fHelp For debug purposes, throw the exception describing parameters.
  * \return A list of open(they go first) and pending transactions.
  *
  * Returns the list of open and pending transactions as JSON structures.
  * The open transactions go first.
  */

Value dxGetOrders(const Array & params, bool fHelp)
{
    if (fHelp) {

        throw runtime_error("dxGetOrders returns the list of open orders.");

    }
    if (!params.empty()) {

        Object error;
        const auto statusCode = xbridge::INVALID_PARAMETERS;
        error.emplace_back(Pair("error",
                                xbridge::xbridgeErrorText(statusCode, "This function does not accept parameters")));
        error.emplace_back(Pair("code", statusCode));
        error.emplace_back(Pair("name", __FUNCTION__));
        return  error;

    }    

    auto &xapp = xbridge::App::instance();
    TransactionMap trlist = xapp.transactions();

    Array result;
    for (const auto& trEntry : trlist) {

        const auto &tr = trEntry.second;

        xbridge::WalletConnectorPtr connFrom = xapp.connectorByCurrency(tr->fromCurrency);
        xbridge::WalletConnectorPtr connTo   = xapp.connectorByCurrency(tr->toCurrency);
        if (!connFrom || !connTo) {

            continue;

        }

        Object jtr;
        jtr.emplace_back(Pair("id",             tr->id.GetHex()));
        jtr.emplace_back(Pair("maker",          tr->fromCurrency));
        jtr.emplace_back(Pair("maker_size",     xBridgeValueFromAmount(tr->fromAmount)));
        jtr.emplace_back(Pair("taker",          tr->toCurrency));
        jtr.emplace_back(Pair("taker_size",     xBridgeValueFromAmount(tr->toAmount)));
        jtr.emplace_back(Pair("updated_at",     bpt::to_iso_extended_string(tr->txtime)));
        jtr.emplace_back(Pair("created_at",     bpt::to_iso_extended_string(tr->created)));
        jtr.emplace_back(Pair("status",         tr->strState()));
        result.emplace_back(jtr);

    }


    return result;
}

//*****************************************************************************
//*****************************************************************************

Value dxGetOrderFills(const Array & params, bool fHelp)
{
    if (fHelp) {

        throw runtime_error("dxGetOrderFills "
                            "(ALL - optional parameter, if specified then all transactions are shown, "
                            "not only successfully completed ");

    }

    bool invalidParams = ((params.size() != 0) &&
                          (params.size() != 1));
    if (invalidParams) {

        Object error;
        error.emplace_back(Pair("error",
                                "Invalid number of parameters"));
        error.emplace_back(Pair("code", xbridge::INVALID_PARAMETERS));
        return  error;

    }

    bool isShowAll = params.size() == 1 && params[0].get_str() == "ALL";

    Array arr;

    TransactionMap trlist = xbridge::App::instance().history();

    for (const auto &trEntry : trlist) {
        Object buy;
        const auto &tr = trEntry.second;
        if (!isShowAll && tr->state != xbridge::TransactionDescr::trFinished) {
            continue;
        }

        double fromAmount   = static_cast<double>(tr->fromAmount);
        double toAmount     = static_cast<double>(tr->toAmount);
        double price        = fromAmount / toAmount;
        std::string buyTime = to_iso_extended_string(tr->created);
        buy.emplace_back(Pair("time",           buyTime));
        buy.emplace_back(Pair("traidId",        tr->id.GetHex()));
        buy.emplace_back(Pair("price",          price));
        buy.emplace_back(Pair("size",           tr->toAmount));
        buy.emplace_back(Pair("side",           "buy"));
        buy.emplace_back(Pair("blockHash",      tr->blockHash.GetHex()));
        arr.emplace_back(buy);
    }

    return arr;
}

Value dxGetOrderHistory(const json_spirit::Array& params, bool fHelp)
{

    if (fHelp) {

        throw runtime_error("dxGetOrderHistory "
                            "(maker) (taker) (start time) (end time) (order_ids - optional) ");

    }
    if ((params.size() != 4 && params.size() != 5)) {

        Object error;
        error.emplace_back(Pair("error",
                                "Invalid number of parameters"));
        error.emplace_back(Pair("code", xbridge::INVALID_PARAMETERS));
        return  error;

    }

    Array arr;
    TransactionMap trlist = xbridge::App::instance().history();

    if(trlist.empty()) {

        LOG() << "empty history transactions list";
        return arr;

    }

    const auto fromCurrency     = params[0].get_str();
    const auto toCurrency       = params[1].get_str();
    const auto startTimeFrame   = params[2].get_int();
    const auto endTimeFrame     = params[3].get_int();
    bool isShowTxids = false;
    if(params.size() == 5) {

        isShowTxids = (params[4].get_str() == "order_ids");

    }

    TransactionMap trList;
    std::vector<xbridge::TransactionDescrPtr> trVector;

    //copy all transactions between startTimeFrame and endTimeFrame
    std::copy_if(trlist.begin(), trlist.end(), std::inserter(trList, trList.end()),
                 [&startTimeFrame, &endTimeFrame, &toCurrency, &fromCurrency](const TransactionPair &transaction){
        return  ((transaction.second->created)      <   bpt::from_time_t(endTimeFrame)) &&
                ((transaction.second->created)      >   bpt::from_time_t(startTimeFrame)) &&
                (transaction.second->toCurrency     ==  toCurrency) &&
                (transaction.second->fromCurrency   ==  fromCurrency) &&
                (transaction.second->state          ==  xbridge::TransactionDescr::trFinished);
    });

    if(trList.empty()) {

        LOG() << "No transactions for the specified period " << __FUNCTION__;
        return  arr;

    }

    RealVector toAmounts;
    RealVector fromAmounts;

    Object res;

    Array times;
    times.emplace_back(startTimeFrame);
    times.emplace_back(endTimeFrame);
    res.emplace_back(Pair("t", times));

    //copy values into vector
    for (const auto &trEntry : trList) {

        const auto &tr          = trEntry.second;
        const auto fromAmount   = xBridgeValueFromAmount(tr->fromAmount);
        const auto toAmount     = xBridgeValueFromAmount(tr->toAmount);
        toAmounts.emplace_back(toAmount);
        fromAmounts.emplace_back(fromAmount);
        trVector.push_back(tr);

    }


    std::sort(trVector.begin(), trVector.end(),
              [](const xbridge::TransactionDescrPtr &a,  const xbridge::TransactionDescrPtr &b)
    {
         return (a->created) < (b->created);
    });

    Array opens;
    //write start price
    opens.emplace_back(xBridgeValueFromAmount(trVector[0]->fromAmount));
    opens.emplace_back(xBridgeValueFromAmount(trVector[0]->toAmount));
    res.emplace_back(Pair("o", opens));

    Array close;
    //write end price
    close.emplace_back(xBridgeValueFromAmount(trVector[trVector.size() - 1]->fromAmount));
    close.emplace_back(xBridgeValueFromAmount(trVector[trVector.size() - 1]->toAmount));
    res.emplace_back(Pair("c", close));

    Array volumes;
    //write sum of bids and asks
    volumes.emplace_back(accumulate(toAmounts.begin(), toAmounts.end(), .0));
    volumes.emplace_back(accumulate(fromAmounts.begin(), fromAmounts.end(), .0));
    res.emplace_back(Pair("v", volumes));

    Array highs;
    //write higs values of the bids and asks  in timeframe
    highs.emplace_back(*std::max_element(toAmounts.begin(), toAmounts.end()));
    highs.emplace_back(*std::max_element(fromAmounts.begin(), fromAmounts.end()));
    res.emplace_back(Pair("h", highs));

    Array lows;
    //write lows values of the bids and ask in the timeframe
    lows.emplace_back(*std::min_element(toAmounts.begin(), toAmounts.end()));
    lows.emplace_back(*std::min_element(fromAmounts.begin(), fromAmounts.end()));
    res.emplace_back(Pair("l", lows));

    if (isShowTxids) {

        Array tmp;
        for(auto tr : trVector) {

            tmp.emplace_back(tr->id.GetHex());

        }
        res.emplace_back(Pair("order_ids", tmp));
    }

    //write status
    res.emplace_back(Pair("s", "ok"));
    arr.emplace_back(res);

    return arr;
}

//*****************************************************************************
//*****************************************************************************

Value dxGetOrdersInfo(const Array & params, bool fHelp)
{
    if (fHelp) {

         throw runtime_error("dxGetOrdersInfo (id) Transaction info.");

    }
    if (params.size() != 1) {

        Object error;
        error.emplace_back(Pair("error",
                                "Invalid number of parameters"));
        error.emplace_back(Pair("code", xbridge::INVALID_PARAMETERS));
        return  error;

    }

    uint256 id(params[0].get_str());
    Array arr;

    xbridge::App & xapp = xbridge::App::instance();

    const xbridge::TransactionDescrPtr tr = xapp.transaction(uint256(id));
    if (tr != nullptr) {
        xbridge::WalletConnectorPtr connFrom = xapp.connectorByCurrency(tr->fromCurrency);
        xbridge::WalletConnectorPtr connTo   = xapp.connectorByCurrency(tr->toCurrency);
        if (!connFrom || !connTo) {

            throw runtime_error("connector not found");

        }

        Object jtr;
        jtr.emplace_back(Pair("id",             tr->id.GetHex()));
        jtr.emplace_back(Pair("created",        bpt::to_iso_extended_string(tr->created)));
        jtr.emplace_back(Pair("from",           tr->fromCurrency));
        jtr.emplace_back(Pair("fromAddress",    tr->isLocal() ? connFrom->fromXAddr(tr->from) : ""));
        jtr.emplace_back(Pair("fromAmount",     xBridgeValueFromAmount(tr->fromAmount)));
        jtr.emplace_back(Pair("to",             tr->toCurrency));
        jtr.emplace_back(Pair("toAddress",      tr->isLocal() ? connTo->fromXAddr(tr->to) : ""));
        jtr.emplace_back(Pair("toAmount",       xBridgeValueFromAmount(tr->toAmount)));
        jtr.emplace_back(Pair("state",          tr->strState()));
        jtr.emplace_back(Pair("blockHash",      tr->blockHash.GetHex()));

        arr.emplace_back(jtr);
    }

    return arr;
}


//******************************************************************************
//******************************************************************************
Value dxGetLocalTokens(const Array & params, bool fHelp)
{
    if (fHelp) {

        throw runtime_error("dxGetLocalTokens\nList currencies supported by the wallet.");

    }
    if (params.size() > 0) {

        Object error;
        error.emplace_back(Pair("error",
                                "This function does not accept any parameter"));
        error.emplace_back(Pair("code", xbridge::INVALID_PARAMETERS));
        return  error;

    }

    Object obj;

    std::vector<std::string> currencies = xbridge::App::instance().availableCurrencies();
    for (std::string currency : currencies) {

        obj.emplace_back(Pair(currency, ""));

    }
    return obj;
}

//******************************************************************************
//******************************************************************************
Value dxGetNetworkTokens(const Array & params, bool fHelp)
{
    if (fHelp) {

        throw runtime_error("dxGetNetworkTokens\nList currencies supported by the network.");

    }
    if (params.size() > 0) {

        Object error;
        error.emplace_back(Pair("error",
                                "This function does not accept any parameter"));
        error.emplace_back(Pair("code", xbridge::INVALID_PARAMETERS));
        return  error;

    }

    Object obj;

    std::vector<std::string> currencies = xbridge::App::instance().networkCurrencies();
    for (std::string currency : currencies) {

        obj.emplace_back(Pair(currency, ""));

    }
    return obj;
}

//******************************************************************************
//******************************************************************************
Value dxMakeOrder(const Array &params, bool fHelp)
{

    if (fHelp) {

        throw runtime_error("dxMakeOrder "
                            "(address from) (currency from) (amount from) "
                            "(address to) (currency to) (amount to)\n"
                            "Create xbridge transaction.");

    }
    if (params.size() != 6) {

        Object error;
        error.emplace_back(Pair("error",
                                "Invalid number of parameters"));
        error.emplace_back(Pair("code", xbridge::INVALID_PARAMETERS));
        return  error;
    }


    std::string fromAddress     = params[0].get_str();
    std::string fromCurrency    = params[1].get_str();
    double      fromAmount      = params[2].get_real();
    std::string toAddress       = params[3].get_str();
    std::string toCurrency      = params[4].get_str();
    double      toAmount        = params[5].get_real();

    auto statusCode = xbridge::SUCCESS;

    xbridge::App &app = xbridge::App::instance();
    if (!app.isValidAddress(fromAddress)) {
        statusCode = xbridge::INVALID_ADDRESS;
        Object error;
        error.emplace_back(Pair("error",
                                xbridge::xbridgeErrorText(statusCode, fromAddress)));
        error.emplace_back(Pair("code", statusCode));
        return  error;
    }
    if (!app.isValidAddress(toAddress)) {
        statusCode = xbridge::INVALID_ADDRESS;
        Object error;
        error.emplace_back(Pair("error",
                                xbridge::xbridgeErrorText(statusCode, toAddress)));
        error.emplace_back(Pair("code", statusCode));
        return  error;
    }
    bool validateParams = ((params.size() == 7) && (params[6].get_str() == "validate"));

    //if validate mode enabled

    Object result;
    statusCode = app.checkCreateParams(fromCurrency, toCurrency,
                                       xBridgeAmountFromReal(fromAmount));
    switch (statusCode) {
    case xbridge::SUCCESS:{

        if (validateParams) {

            result.emplace_back(Pair("status",         "created"));
            result.emplace_back(Pair("id",             uint256().GetHex()));
            result.emplace_back(Pair("from",           fromAddress));
            result.emplace_back(Pair("fromCurrency",   fromCurrency));
            result.emplace_back(Pair("fromAmount",     fromAmount));
            result.emplace_back(Pair("to",             toAddress));
            result.emplace_back(Pair("toCurrency",     toCurrency));
            result.emplace_back(Pair("toAmount",       toAmount));
            return result;

        }
        break;
    }

    case xbridge::INVALID_CURRENCY: {

        result.emplace_back(Pair("error",
                                 xbridge::xbridgeErrorText(statusCode, fromCurrency)));
        result.emplace_back(Pair("code", statusCode));
        return  result;

    }
    case xbridge::NO_SESSION:{

        result.emplace_back(Pair("error",
                                 xbridge::xbridgeErrorText(statusCode, fromCurrency)));
        result.emplace_back(Pair("code", statusCode));
        return  result;

    }
    case xbridge::INSIFFICIENT_FUNDS:{

        result.emplace_back(Pair("error",
                                 xbridge::xbridgeErrorText(statusCode, toAddress)));
        result.emplace_back(Pair("code", statusCode));
        return  result;
    }

    default:
        result.emplace_back(Pair("error", xbridge::xbridgeErrorText(statusCode)));
        result.emplace_back(Pair("code", statusCode));
        return  result;
    }


    uint256 id = uint256();
    uint256 blockHash = uint256();
    statusCode = xbridge::App::instance().sendXBridgeTransaction
          (fromAddress, fromCurrency, xBridgeAmountFromReal(fromAmount),
           toAddress, toCurrency, xBridgeAmountFromReal(toAmount), id, blockHash);

    if (statusCode == xbridge::SUCCESS) {

        Object obj;
        obj.emplace_back(Pair("id",             id.GetHex()));
        const auto &createdTime = xbridge::App::instance().transaction(id)->created;
        obj.emplace_back(Pair("time",           bpt::to_iso_extended_string(createdTime)));
        obj.emplace_back(Pair("from",           fromAddress));
        obj.emplace_back(Pair("fromCurrency",   fromCurrency));
        obj.emplace_back(Pair("fromAmount",     fromAmount));
        obj.emplace_back(Pair("to",             toAddress));
        obj.emplace_back(Pair("toCurrency",     toCurrency));
        obj.emplace_back(Pair("toAmount",       toAmount));
        obj.emplace_back(Pair("blockHash",      blockHash.GetHex()));
        return obj;

    } else {

        Object error;
        error.emplace_back(Pair("error", xbridge::xbridgeErrorText(statusCode)));
        error.emplace_back(Pair("code", statusCode));
        return error;

    }
}

//******************************************************************************
//******************************************************************************
Value dxTakeOrder(const Array & params, bool fHelp)
{

    if (fHelp) {

        throw runtime_error("dxTakeOrder (id) "
                            "(address from) (address to)\n"
                            "Accept xbridge transaction.");

    }
    auto statusCode = xbridge::SUCCESS;

    if ((params.size() != 3) && (params.size() != 4)) {

        statusCode = xbridge::INVALID_PARAMETERS;
        Object error;
        error.emplace_back(Pair("error",
                                xbridge::xbridgeErrorText(statusCode)));
        error.emplace_back(Pair("code", statusCode));
        return  error;

    }

    uint256 id(params[0].get_str());
    std::string fromAddress    = params[1].get_str();
    std::string toAddress      = params[2].get_str();

    xbridge::App &app = xbridge::App::instance();

    if (!app.isValidAddress(fromAddress)) {

        statusCode = xbridge::INVALID_ADDRESS;
        Object error;
        error.emplace_back(Pair("error",
                                xbridge::xbridgeErrorText(statusCode, fromAddress)));
        error.emplace_back(Pair("code", statusCode));
        return  error;

    }
    if (!app.isValidAddress(toAddress)) {

        statusCode = xbridge::INVALID_ADDRESS;
        Object error;
        error.emplace_back(Pair("error",
                                xbridge::xbridgeErrorText(statusCode, toAddress)));
        error.emplace_back(Pair("code", statusCode));
        return  error;

    }
    bool validateParams = ((params.size() == 4) && (params[3].get_str() == "validate"));
    //if validate mode enabled
    Object result;
    xbridge::TransactionDescrPtr ptr;
    statusCode = app.checkAcceptParams(id, ptr);

    switch (statusCode) {
    case xbridge::SUCCESS: {

        if(validateParams) {

            result.emplace_back(Pair("status", "Accepted"));
            result.emplace_back(Pair("id", uint256().GetHex()));
            result.emplace_back(Pair("from", fromAddress));
            result.emplace_back(Pair("to", toAddress));
            return result;

        }
        break;

    }
    case xbridge::TRANSACTION_NOT_FOUND: {

        result.emplace_back(Pair("error",
                                 xbridge::xbridgeErrorText(statusCode, id.GetHex())));
        result.emplace_back(Pair("code", statusCode));
        return  result;

    }

    case xbridge::NO_SESSION: {

        result.emplace_back(Pair("error",
                                 xbridge::xbridgeErrorText(statusCode, ptr->toCurrency)));
        result.emplace_back(Pair("code", statusCode));
        return  result;

    }

    case xbridge::INSIFFICIENT_FUNDS:{

        result.emplace_back(Pair("error",
                                 xbridge::xbridgeErrorText(statusCode, ptr->to)));
        result.emplace_back(Pair("code", statusCode));
        return  result;

    }

    default:
        result.emplace_back(Pair("error", xbridge::xbridgeErrorText(statusCode)));
        result.emplace_back(Pair("code", statusCode));
        return  result;
    }

    std::swap(ptr->fromCurrency, ptr->toCurrency);
    std::swap(ptr->fromAmount, ptr->toAmount);

    statusCode = app.acceptXBridgeTransaction(id, fromAddress, toAddress);
    if (statusCode == xbridge::SUCCESS) {

        Object obj;
        obj.emplace_back(Pair("status", "Accepted"));
        obj.emplace_back(Pair("time",   bpt::to_iso_extended_string(bpt::second_clock::universal_time())));
        obj.emplace_back(Pair("id",     id.GetHex()));
        obj.emplace_back(Pair("from",   fromAddress));
        obj.emplace_back(Pair("to",     toAddress));
        return obj;

    } else {

        Object obj;
        obj.emplace_back(Pair("error",
                              xbridge::xbridgeErrorText(statusCode)));
        obj.emplace_back(Pair("code", statusCode));
        return obj;

    }
}

//******************************************************************************
//******************************************************************************
Value dxCancelOrder(const Array &params, bool fHelp)
{
    if(fHelp) {

        throw runtime_error("dxCancelOrder (id)\n"
                            "Cancel xbridge transaction.");

    }
    if (params.size() != 1) {

        Object error;
        error.emplace_back(Pair("error",
                                "Invalid number of parameters"));
        error.emplace_back(Pair("code", xbridge::INVALID_PARAMETERS));
        return  error;

    }
    LOG() << "rpc cancel transaction " << __FUNCTION__;
    uint256 id(params[0].get_str());
    bpt::ptime txtime;
    if (xbridge::App::instance().transactions().count(id) ) {

        txtime = xbridge::App::instance().transaction(id)->txtime;
        const auto res = xbridge::App::instance().cancelXBridgeTransaction(id, crRpcRequest);
        if (res == xbridge::SUCCESS) {

            Object obj;
            obj.emplace_back(Pair("id", id.GetHex()));
            //I purposely did not grab a mutex
            txtime = xbridge::App::instance().transaction(id)->txtime;
            obj.emplace_back(Pair("time", bpt::to_iso_extended_string(txtime)));
            return  obj;

        } else {

            Object obj;
            obj.emplace_back(Pair("error", xbridge::xbridgeErrorText(res)));
            obj.emplace_back(Pair("code", res));
            return obj;

        }
    } else {

        Object obj;
        obj.emplace_back(Pair("error", xbridge::xbridgeErrorText(xbridge::Error::TRANSACTION_NOT_FOUND, id.GetHex())));
        obj.emplace_back(Pair("code", xbridge::Error::TRANSACTION_NOT_FOUND));
        return obj;

    }
}

//******************************************************************************
//******************************************************************************
json_spirit::Value dxrollbackTransaction(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp) {

        throw runtime_error("dxRollbackTransaction (id)\n"
                            "Rollback xbridge transaction.");

    }
    if (params.size() != 1) {

        Object error;
        error.emplace_back(Pair("error",
                                "Invalid number of parameters"));
        error.emplace_back(Pair("code", xbridge::INVALID_PARAMETERS));
        return  error;

    }
    LOG() << "rpc rollback transaction " << __FUNCTION__;
    uint256 id(params[0].get_str());
    const auto res = xbridge::App::instance().rollbackXBridgeTransaction(id);
    if (res == xbridge::SUCCESS) {

        Object obj;
        obj.emplace_back(Pair("id", id.GetHex()));
        return  obj;

    } else {

        Object obj;
        obj.emplace_back(Pair("error", xbridge::xbridgeErrorText(res)));
        obj.emplace_back(Pair("code", res));
        return obj;

    }
}

//******************************************************************************
//******************************************************************************
Value dxGetOrderBook(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp)
    {
        throw runtime_error("dxGetOrderBook "
                            "(detail level) (maker) (taker) "
                            "(max orders)[optional, default=50) ");
    }

    if ((params.size() < 3 || params.size() > 4))
    {
        Object error;
        error.emplace_back(Pair("error", xbridge::xbridgeErrorText(xbridge::INVALID_PARAMETERS)));
        error.emplace_back(Pair("code", xbridge::INVALID_PARAMETERS));
        error.emplace_back(Pair("name", "dxGetOrderBook"));
        return  error;
    }

    Object res;
    TransactionMap trList = xbridge::App::instance().transactions();
    {
        /**
         * @brief detaiLevel - Get a list of open orders for a product.
         * The amount of detail shown can be customized with the level parameter.
         */
        const auto detailLevel  = params[0].get_int();
        const auto fromCurrency = params[1].get_str();
        const auto toCurrency   = params[2].get_str();

        std::size_t maxOrders = 50;

        if (detailLevel == 2 && params.size() == 4)
            maxOrders = params[3].get_int();

        if (maxOrders < 1)
            maxOrders = 1;

        if (detailLevel < 1 || detailLevel > 4)
        {
            Object error;
            error.emplace_back(Pair("error", xbridge::xbridgeErrorText(xbridge::INVALID_DETAIL_LEVEL)));
            error.emplace_back(Pair("code", xbridge::INVALID_DETAIL_LEVEL));
            error.emplace_back(Pair("name", "dxGetOrderBook"));
            return error;
        }

        res.emplace_back(Pair("detail", detailLevel));
        res.emplace_back(Pair("maker", fromCurrency));
        res.emplace_back(Pair("taker", toCurrency));

        /**
         * @brief bids - array with bids
         */
        Array bids;
        /**
         * @brief asks - array with asks
         */
        Array asks;

        if(trList.empty())
        {
            LOG() << "empty transactions list";
            res.emplace_back(Pair("bids", bids));
            res.emplace_back(Pair("asks", asks));
            return res;
        }

        TransactionMap asksList;
        TransactionMap bidsList;

        //copy all transactions in currencies specified in the parameters
        std::copy_if(trList.begin(), trList.end(), std::inserter(asksList, asksList.end()),
                     [&toCurrency, &fromCurrency](const TransactionPair &transaction)
        {
            if(transaction.second == nullptr)
                return false;

            return  ((transaction.second->toCurrency == fromCurrency) &&
                    (transaction.second->fromCurrency == toCurrency));
        });

        std::copy_if(trList.begin(), trList.end(), std::inserter(bidsList, bidsList.end()),
                     [&toCurrency, &fromCurrency](const TransactionPair &transaction)
        {
            if(transaction.second == nullptr)
                return false;

            return  ((transaction.second->toCurrency == toCurrency) &&
                    (transaction.second->fromCurrency == fromCurrency));
        });

        std::vector<xbridge::TransactionDescrPtr> asksVector;
        std::vector<xbridge::TransactionDescrPtr> bidsVector;

        for (const auto &trEntry : asksList)
            asksVector.emplace_back(trEntry.second);

        for (const auto &trEntry : bidsList)
            bidsVector.emplace_back(trEntry.second);

        //sort descending
        std::sort(bidsVector.begin(), bidsVector.end(),
                  [](const xbridge::TransactionDescrPtr &a, const xbridge::TransactionDescrPtr &b)
        {
            const auto priceA = xBridgeValueFromAmount(a->fromAmount) / xBridgeValueFromAmount(a->toAmount);
            const auto priceB = xBridgeValueFromAmount(b->fromAmount) / xBridgeValueFromAmount(b->toAmount);
            return priceA > priceB;
        });

        std::sort(asksVector.begin(), asksVector.end(),
                  [](const xbridge::TransactionDescrPtr &a, const xbridge::TransactionDescrPtr &b)
        {
            const auto priceA = xBridgeValueFromAmount(a->fromAmount) / xBridgeValueFromAmount(a->toAmount);
            const auto priceB = xBridgeValueFromAmount(b->fromAmount) / xBridgeValueFromAmount(b->toAmount);
            return priceA < priceB;
        });

        // floating point comparisons
        // see Knuth 4.2.2 Eq 36
        auto floatCompare = [](const double a, const double b) -> bool
        {
            auto epsilon = std::numeric_limits<double>::epsilon();
            return (fabs(a - b) / fabs(a) <= epsilon) && (fabs(a - b) / fabs(b) <= epsilon);
        };

        switch (detailLevel)
        {
        case 1:
        {
            //return only the best bid and ask
            const auto bidsItem = std::max_element(bidsList.begin(), bidsList.end(),
                                       [](const TransactionPair &a, const TransactionPair &b)
            {
                //find transaction with best bids
                const auto &tr1 = a.second;
                const auto &tr2 = b.second;

                if(tr1 == nullptr)
                    return true;

                if(tr2 == nullptr)
                    return false;

                const auto priceA = xBridgeValueFromAmount(tr1->fromAmount) / xBridgeValueFromAmount(tr1->toAmount);
                const auto priceB = xBridgeValueFromAmount(tr2->fromAmount) / xBridgeValueFromAmount(tr2->toAmount);

                return priceA < priceB;
            });

            const auto bidsCount = std::count_if(bidsList.begin(), bidsList.end(), [bidsItem, floatCompare](const TransactionPair &a)
            {
                const auto &tr = a.second;

                if(tr == nullptr)
                    return false;

                const auto price = xBridgeValueFromAmount(tr->fromAmount) / xBridgeValueFromAmount(tr->toAmount);

                const auto &bestTr = bidsItem->second;
                if (bestTr != nullptr)
                {
                    const auto bestBidPrice = xBridgeValueFromAmount(bestTr->fromAmount) / xBridgeValueFromAmount(bestTr->toAmount);
                    return floatCompare(price, bestBidPrice);
                }

                return false;
            });

            {
                const auto &tr = bidsItem->second;
                if (tr != nullptr)
                {
                    const auto bidPrice = xBridgeValueFromAmount(tr->fromAmount) / xBridgeValueFromAmount(tr->toAmount);
                    bids.emplace_back(Array{bidPrice, xBridgeValueFromAmount(tr->fromAmount), (int64_t)bidsCount});
                }
            }

            const auto asksItem = std::min_element(asksList.begin(), asksList.end(),
                                                   [](const TransactionPair &a, const TransactionPair &b)
            {
                //find transactions with best asks
                const auto &tr1 = a.second;
                const auto &tr2 = b.second;

                if(tr1 == nullptr)
                    return true;

                if(tr2 == nullptr)
                    return false;

                const auto priceA = xBridgeValueFromAmount(tr1->fromAmount) / xBridgeValueFromAmount(tr1->toAmount);
                const auto priceB = xBridgeValueFromAmount(tr2->fromAmount) / xBridgeValueFromAmount(tr2->toAmount);
                return priceA < priceB;
            });

            const auto asksCount = std::count_if(asksList.begin(), asksList.end(), [asksItem, floatCompare](const TransactionPair &a)
            {
                const auto &tr = a.second;

                if(tr == nullptr)
                    return false;

                const auto price = xBridgeValueFromAmount(tr->fromAmount) / xBridgeValueFromAmount(tr->toAmount);

                const auto &bestTr = asksItem->second;
                if (bestTr != nullptr)
                {
                    const auto bestAskPrice = xBridgeValueFromAmount(bestTr->fromAmount) / xBridgeValueFromAmount(bestTr->toAmount);
                    return floatCompare(price, bestAskPrice);
                }

                return false;
            });

            {
                const auto &tr = asksItem->second;
                if (tr != nullptr)
                {
                    const auto askPrice = xBridgeValueFromAmount(tr->fromAmount) / xBridgeValueFromAmount(tr->toAmount);
                    asks.emplace_back(Array{askPrice, xBridgeValueFromAmount(tr->fromAmount), (int64_t)asksCount});
                }
            }

            res.emplace_back(Pair("bids", bids));
            res.emplace_back(Pair("asks", asks));
            return res;
        }
        case 2:
        {
            //Top X bids and asks (aggregated)

            /**
             * @brief bound - calculate upper bound
             */
            auto bound = std::min(maxOrders, bidsVector.size());

            for (size_t i = 0; i < bound; i++)
            {
                if(bidsVector[i] == nullptr)
                    continue;

                Array bid;
                //calculate bids and push to array
                const auto fromAmount   = bidsVector[i]->fromAmount;
                const auto toAmount     = bidsVector[i]->toAmount;
                const auto bidPrice     = xBridgeValueFromAmount(fromAmount) / xBridgeValueFromAmount(toAmount);
                const auto bidSize      = xBridgeValueFromAmount(fromAmount);
                const auto bidsCount    = std::count_if(bidsList.begin(), bidsList.end(),
                                                     [bidPrice, floatCompare](const TransactionPair &a)
                {
                    const auto &tr = a.second;

                    if(tr == nullptr)
                        return false;

                    const auto price = xBridgeValueFromAmount(tr->fromAmount) / xBridgeValueFromAmount(tr->toAmount);

                    return floatCompare(price, bidPrice);
                });

                bid.emplace_back(bidPrice);
                bid.emplace_back(bidSize);
                bid.emplace_back((int64_t)bidsCount);

                bids.emplace_back(bid);
            }

            bound = std::min(maxOrders, asksVector.size());

            for (size_t i = 0; i < bound; i++)
            {
                if(asksVector[i] == nullptr)
                    continue;

                Array ask;
                //calculate asks and push to array
                const auto fromAmount   = asksVector[i]->fromAmount;
                const auto toAmount     = asksVector[i]->toAmount;
                const auto askPrice     = xBridgeValueFromAmount(fromAmount) / xBridgeValueFromAmount(toAmount);
                const auto askSize      = xBridgeValueFromAmount(fromAmount);
                const auto asksCount    = std::count_if(asksList.begin(), asksList.end(),
                                                     [askPrice, floatCompare](const TransactionPair &a)
                {
                    const auto &tr = a.second;

                    if(tr == nullptr)
                        return false;

                    const auto price = xBridgeValueFromAmount(tr->fromAmount) / xBridgeValueFromAmount(tr->toAmount);

                    return floatCompare(price, askPrice);
                });

                ask.emplace_back(askPrice);
                ask.emplace_back(askSize);
                ask.emplace_back((int64_t)asksCount);

                asks.emplace_back(ask);
            }

            res.emplace_back(Pair("bids", bids));
            res.emplace_back(Pair("asks", asks));
            return  res;
        }
        case 3:
        {
            //Full order book (non aggregated)
            auto bound = std::min(maxOrders, bidsVector.size());

            for (size_t i = 0; i < bound; i++)
            {
                if(bidsVector[i] == nullptr)
                    continue;

                Array bid;
                const auto fromAmount   = bidsVector[i]->fromAmount;
                const auto toAmount     = bidsVector[i]->toAmount;
                const auto bidPrice = xBridgeValueFromAmount(fromAmount) / xBridgeValueFromAmount(toAmount);
                bid.emplace_back(bidPrice);
                bid.emplace_back(xBridgeValueFromAmount(fromAmount));
                bid.emplace_back(bidsVector[i]->id.GetHex());

                bids.emplace_back(bid);
            }

            bound = std::min(maxOrders, asksVector.size());

            for (size_t i = 0; i < bound; i++)
            {
                if(asksVector[i] == nullptr)
                    continue;

                Array ask;
                const auto fromAmount   = asksVector[i]->fromAmount;
                const auto toAmount     = asksVector[i]->toAmount;
                const auto askPrice = xBridgeValueFromAmount(fromAmount) / xBridgeValueFromAmount(toAmount);
                ask.emplace_back(askPrice);
                ask.emplace_back(xBridgeValueFromAmount(fromAmount));
                ask.emplace_back(asksVector[i]->id.GetHex());

                asks.emplace_back(ask);
            }

            res.emplace_back(Pair("bids", bids));
            res.emplace_back(Pair("asks", asks));
            return  res;
        }
        case 4:
        {
            //return Only the best bid and ask
            const auto bidsItem = std::max_element(bidsList.begin(), bidsList.end(),
                                       [](const TransactionPair &a, const TransactionPair &b)
            {
                //find transaction with best bids
                const auto &tr1 = a.second;
                const auto &tr2 = b.second;

                if(tr1 == nullptr)
                    return true;

                if(tr2 == nullptr)
                    return false;

                const auto priceA = xBridgeValueFromAmount(tr1->fromAmount) / xBridgeValueFromAmount(tr1->toAmount);
                const auto priceB = xBridgeValueFromAmount(tr2->fromAmount) / xBridgeValueFromAmount(tr2->toAmount);

                return priceA < priceB;
            });

            {
                const auto &tr = bidsItem->second;
                if (tr != nullptr)
                {
                    const auto bidPrice = xBridgeValueFromAmount(tr->fromAmount) / xBridgeValueFromAmount(tr->toAmount);
                    bids.emplace_back(bidPrice);
                    bids.emplace_back(xBridgeValueFromAmount(tr->fromAmount));

                    Array bidsIds;
                    bidsIds.emplace_back(tr->id.GetHex());

                    for(const TransactionPair &tp : bidsList)
                    {
                        const auto &otherTr = tp.second;

                        if(otherTr == nullptr)
                            continue;

                        if(tr->id == otherTr->id)
                            continue;

                        const auto otherTrBidPrice = xBridgeValueFromAmount(otherTr->fromAmount) / xBridgeValueFromAmount(otherTr->toAmount);

                        if(!floatCompare(bidPrice, otherTrBidPrice))
                            continue;

                        bidsIds.emplace_back(otherTr->id.GetHex());
                    }

                    bids.emplace_back(bidsIds);
                }
            }

            const auto asksItem = std::min_element(asksList.begin(), asksList.end(),
                                                   [](const TransactionPair &a, const TransactionPair &b)
            {
                //find transactions with best asks
                const auto &tr1 = a.second;
                const auto &tr2 = b.second;

                if(tr1 == nullptr)
                    return true;

                if(tr2 == nullptr)
                    return false;

                const auto priceA = xBridgeValueFromAmount(tr1->fromAmount) / xBridgeValueFromAmount(tr1->toAmount);
                const auto priceB = xBridgeValueFromAmount(tr2->fromAmount) / xBridgeValueFromAmount(tr2->toAmount);
                return priceA < priceB;
            });

            {
                const auto &tr = asksItem->second;
                if (tr != nullptr)
                {
                    const auto askPrice = xBridgeValueFromAmount(tr->fromAmount) / xBridgeValueFromAmount(tr->toAmount);
                    asks.emplace_back(askPrice);
                    asks.emplace_back(xBridgeValueFromAmount(tr->fromAmount));

                    Array asksIds;
                    asksIds.emplace_back(tr->id.GetHex());

                    for(const TransactionPair &tp : asksList)
                    {
                        const auto &otherTr = tp.second;

                        if(otherTr == nullptr)
                            continue;

                        if(tr->id == otherTr->id)
                            continue;

                        const auto otherTrAskPrice = xBridgeValueFromAmount(otherTr->fromAmount) / xBridgeValueFromAmount(otherTr->toAmount);

                        if(!floatCompare(askPrice, otherTrAskPrice))
                            continue;

                        asksIds.emplace_back(otherTr->id.GetHex());
                    }

                    asks.emplace_back(asksIds);
                }
            }

            res.emplace_back(Pair("bids", bids));
            res.emplace_back(Pair("asks", asks));
            return res;
        }

        default:
            Object error;
            error.emplace_back(Pair("error", xbridge::xbridgeErrorText(xbridge::INVALID_PARAMETERS)));
            error.emplace_back(Pair("code", xbridge::INVALID_PARAMETERS));
            error.emplace_back(Pair("name", "dxGetOrderBook"));
            return error;
        }
    }
}

//******************************************************************************
//******************************************************************************
json_spirit::Value dxGetMyOrders(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp) {

        throw runtime_error("dxGetMyOrders");

    }

    if (params.size() > 0) {

        Object error;
        error.emplace_back(Pair("error",
                                "This function does not accept any parameter"));
        error.emplace_back(Pair("code", xbridge::INVALID_PARAMETERS));
        return  error;

    }

    xbridge::App & xapp = xbridge::App::instance();

    // todo: should we lock?
    Array r;

    TransactionMap trList = xbridge::App::instance().transactions();
    {
        if(trList.empty()) {

            LOG() << "empty  transactions list ";
            return r;

        }

        // using TransactionMap    = std::map<uint256, xbridge::TransactionDescrPtr>;

        for(auto i : trList)
        {

            const auto& t = *i.second;

            if(!t.isLocal())
                continue;

            xbridge::WalletConnectorPtr connFrom = xapp.connectorByCurrency(t.fromCurrency);
            xbridge::WalletConnectorPtr connTo   = xapp.connectorByCurrency(t.toCurrency);

            Object o;

            o.emplace_back(Pair("id", t.id.GetHex()));

            // maker data
            o.emplace_back(Pair("maker", t.fromCurrency));
            o.emplace_back(Pair("maker_size", xBridgeValueFromAmount(t.fromAmount)));
            o.emplace_back(Pair("maker_address", connFrom->fromXAddr(t.from)));
            // taker data
            o.emplace_back(Pair("taker", t.toCurrency));
            o.emplace_back(Pair("taker_size", xBridgeValueFromAmount(t.toAmount)));
            o.emplace_back(Pair("taker_address", connFrom->fromXAddr(t.to)));

            // todo: check if it's ISO 8601
            o.emplace_back(Pair("updated_at", bpt::to_iso_extended_string(t.txtime)));
            o.emplace_back(Pair("created_at", bpt::to_iso_extended_string(t.created)));

            // should we make returning value correspond to the description or vice versa?
            // Order status: created|open|pending|filled|canceled
            o.emplace_back(Pair("status", t.strState()));

            r.emplace_back(o);
        }


    }

    return r;
}
