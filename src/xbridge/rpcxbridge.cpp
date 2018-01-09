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

Value dxGetTransactions(const Array & params, bool fHelp)
{
    if(fHelp) {
        throw runtime_error("dxGetTransactions\nList transactions.");
    }
    if (params.size() > 0) {
        Object error;
        error.emplace_back(Pair("error",
                                "This function does not accept any parameter"));
        error.emplace_back(Pair("code", xbridge::INVALID_PARAMETERS));
        return  error;
    }

    Array arr;

    xbridge::App & xapp = xbridge::App::instance();
    TransactionMap trlist = xapp.transactions();
    for (const auto & trEntry : trlist)
    {
        const xbridge::TransactionDescrPtr & tr = trEntry.second;

        xbridge::WalletConnectorPtr connFrom = xapp.connectorByCurrency(tr->fromCurrency);
        xbridge::WalletConnectorPtr connTo   = xapp.connectorByCurrency(tr->toCurrency);
        if (!connFrom || !connTo)
        {
            continue;
        }

        Object jtr;
        jtr.emplace_back(Pair("id",           tr->id.GetHex()));
        jtr.emplace_back(Pair("from",         tr->fromCurrency));
        jtr.emplace_back(Pair("from address", connFrom->fromXAddr(tr->from)));
        jtr.emplace_back(Pair("fromAmount",   xBridgeValueFromAmount(tr->fromAmount)));
        jtr.emplace_back(Pair("to",           tr->toCurrency));
        jtr.emplace_back(Pair("to address",   connTo->fromXAddr(tr->to)));
        jtr.emplace_back(Pair("toAmount",     xBridgeValueFromAmount(tr->toAmount)));
        jtr.emplace_back(Pair("state",        tr->strState()));

        arr.emplace_back(jtr);
    }

    return arr;
}

//*****************************************************************************
//*****************************************************************************

Value dxGetTransactionsHistory(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw runtime_error("dxGetTransactionsHistory "
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

    for (const auto &trEntry : trlist)
    {
        Object buy;
        const auto &tr = trEntry.second;
        if (!isShowAll && tr->state != xbridge::TransactionDescr::trFinished)
        {
            continue;
        }

        double fromAmount = static_cast<double>(tr->fromAmount);
        double toAmount = static_cast<double>(tr->toAmount);
        double price = fromAmount / toAmount;
        std::string buyTime = to_iso_extended_string(tr->created);
        buy.emplace_back(Pair("time",      buyTime));
        buy.emplace_back(Pair("traid_id",  tr->id.GetHex()));
        buy.emplace_back(Pair("price",     price));
        buy.emplace_back(Pair("size",      tr->toAmount));
        buy.emplace_back(Pair("side",      "buy"));
        arr.emplace_back(buy);
    }

    return arr;
}

Value dxGetTradeHistory(const json_spirit::Array& params, bool fHelp)
{

    if (fHelp) {
        throw runtime_error("dxGetTradeHistory "
                            "(from currency) (to currency) (start time) (end time) (txids - optional) ");
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

    if(trlist.empty())
    {
        LOG() << "empty history transactions list";
        return arr;
    }

    const auto fromCurrency     = params[0].get_str();
    const auto toCurrency       = params[1].get_str();
    const auto startTimeFrame   = params[2].get_int();
    const auto endTimeFrame     = params[3].get_int();
    bool isShowTxids = false;
    if(params.size() == 5)
    {
        isShowTxids = (params[4].get_str() == "txids");
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
    for (const auto &trEntry : trList)
    {
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

    if(isShowTxids)
    {
        Array tmp;
        for(auto tr : trVector)
        {
            tmp.emplace_back(tr->id.GetHex());
        }
        res.emplace_back(Pair("txids", tmp));
    }

    //write status
    res.emplace_back(Pair("s", "ok"));
    arr.emplace_back(res);

    return arr;
}

//*****************************************************************************
//*****************************************************************************

Value dxGetTransactionInfo(const Array & params, bool fHelp)
{
    if (fHelp) {
         throw runtime_error("dxGetTransactionInfo (id) Transaction info.");
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
    if (tr)
    {
        xbridge::WalletConnectorPtr connFrom = xapp.connectorByCurrency(tr->fromCurrency);
        xbridge::WalletConnectorPtr connTo   = xapp.connectorByCurrency(tr->toCurrency);
        if (!connFrom || !connTo)
        {
            throw runtime_error("connector not found");
        }

        Object jtr;
        jtr.emplace_back(Pair("id",           tr->id.GetHex()));
        jtr.emplace_back(Pair("created",      bpt::to_iso_extended_string(tr->created)));
        jtr.emplace_back(Pair("from",         tr->fromCurrency));
        jtr.emplace_back(Pair("from address", connFrom->fromXAddr(tr->from)));
        jtr.emplace_back(Pair("fromAmount",   xBridgeValueFromAmount(tr->fromAmount)));
        jtr.emplace_back(Pair("to",           tr->toCurrency));
        jtr.emplace_back(Pair("to address",   connTo->fromXAddr(tr->to)));
        jtr.emplace_back(Pair("toAmount",     xBridgeValueFromAmount(tr->toAmount)));
        jtr.emplace_back(Pair("state",        tr->strState()));

        arr.emplace_back(jtr);
    }

    return arr;
}


//******************************************************************************
//******************************************************************************
Value dxGetCurrencies(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw runtime_error("dxGetCurrencies\nList currencies.");
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
    for (std::string currency : currencies)
    {
        obj.emplace_back(Pair(currency, ""));
    }
    return obj;
}

//******************************************************************************
//******************************************************************************
Value dxCreateTransaction(const Array &params, bool fHelp)
{
    if (fHelp) {
        throw runtime_error("dxCreateTransaction "
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
    if(validateParams) {
        Object result;
        statusCode = app.checkCreateParams(fromCurrency, toCurrency,
                                           xBridgeAmountFromReal(fromAmount));
        switch (statusCode) {
        case xbridge::SUCCESS:
            result.emplace_back(Pair("status",         "created"));
            result.emplace_back(Pair("id",             uint256().GetHex()));
            result.emplace_back(Pair("from",           fromAddress));
            result.emplace_back(Pair("fromCurrency",   fromCurrency));
            result.emplace_back(Pair("fromAmount",     fromAmount));
            result.emplace_back(Pair("to",             toAddress));
            result.emplace_back(Pair("toCurrency",     toCurrency));
            result.emplace_back(Pair("toAmount",       toAmount));
            return result;
        case xbridge::INVALID_CURRENCY:
            result.emplace_back(Pair("error",
                                    xbridge::xbridgeErrorText(statusCode, fromCurrency)));
            break;
        case xbridge::NO_SESSION:
            result.emplace_back(Pair("error",
                                    xbridge::xbridgeErrorText(statusCode, fromCurrency)));
            break;
        case xbridge::INSIFFICIENT_FUNDS:
            result.emplace_back(Pair("error",
                                     xbridge::xbridgeErrorText(statusCode, toAddress)));
            break;
        default:
            result.emplace_back(Pair("error", xbridge::xbridgeErrorText(statusCode)));
        }
        result.emplace_back(Pair("code", statusCode));
        return  result;
    }

    uint256 id = uint256();
    statusCode = xbridge::App::instance().sendXBridgeTransaction
          (fromAddress, fromCurrency, xBridgeAmountFromReal(fromAmount),
           toAddress, toCurrency, xBridgeAmountFromReal(toAmount), id);

    if(statusCode== xbridge::SUCCESS) {
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
Value dxAcceptTransaction(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw runtime_error("dxAcceptTransaction (id) "
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
    if(validateParams) {
        Object result;
        xbridge::TransactionDescrPtr ptr;
        statusCode = app.checkAcceptParams(id, ptr);
        switch (statusCode) {
        case xbridge::SUCCESS:
            result.emplace_back(Pair("status", "Accepted"));
            result.emplace_back(Pair("id", uint256().GetHex()));
            result.emplace_back(Pair("from", fromAddress));
            result.emplace_back(Pair("to", toAddress));
            return result;
        case xbridge::TRANSACTION_NOT_FOUND:
            result.emplace_back(Pair("error",
                                    xbridge::xbridgeErrorText(statusCode, id.GetHex())));
            break;
        case xbridge::NO_SESSION:
            result.emplace_back(Pair("error",
                                    xbridge::xbridgeErrorText(statusCode, ptr->toCurrency)));
            break;
        case xbridge::INSIFFICIENT_FUNDS:
            result.emplace_back(Pair("error",
                                     xbridge::xbridgeErrorText(statusCode, ptr->to)));
            break;
        default:
            result.emplace_back(Pair("error", xbridge::xbridgeErrorText(statusCode)));
        }
        result.emplace_back(Pair("code", statusCode));
        return  result;
    }

    statusCode = app.acceptXBridgeTransaction(id, fromAddress, toAddress);
    if(statusCode == xbridge::SUCCESS) {
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
Value dxCancelTransaction(const Array &params, bool fHelp)
{
    if(fHelp) {
        throw runtime_error("dxCancelTransaction (id)\n"
                            "Cancel xbridge transaction.");
    }
    if (params.size() != 1) {
        Object error;
        error.emplace_back(Pair("error",
                                "Invalid number of parameters"));
        error.emplace_back(Pair("code", xbridge::INVALID_ADDRESS));
        return  error;
    }
    LOG() << "rpc cancel transaction " << __FUNCTION__;
    uint256 id(params[0].get_str());
    const auto res = xbridge::App::instance().cancelXBridgeTransaction(id, crRpcRequest);
    if (res == xbridge::SUCCESS)
    {
        Object obj;
        obj.emplace_back(Pair("id", id.GetHex()));
        //I purposely did not grab a mutex
        const auto &txtime = xbridge::App::instance().transaction(id)->txtime;
        obj.emplace_back(Pair("time", bpt::to_iso_extended_string(txtime)));
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
    if(res == xbridge::SUCCESS) {
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
json_spirit::Value dxGetOrderBook(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp) {
        throw runtime_error("dxGetOrderBook "
                            "(the level of detail) (from currency) (to currency) "
                            "(max orders - optional, default = 50) ");
    }
    if ((params.size() < 3 || params.size() > 5)) {
        Object error;
        error.emplace_back(Pair("error",
                                "Invalid number of parameters"));
        error.emplace_back(Pair("code",
                                xbridge::INVALID_PARAMETERS));
        return  error;
    }

    Object res;
    TransactionMap trList = xbridge::App::instance().transactions();
    {
        if(trList.empty()) {
            LOG() << "empty  transactions list ";
            return res;
        }

        TransactionMap asksList;

        TransactionMap bidsList;

        /**
         * @brief detaiLevel - Get a list of open orders for a product.
         * The amount of detail shown can be customized with the level parameter.
         */
        const auto detailLevel  = params[0].get_int();
        const auto fromCurrency = params[1].get_str();
        const auto toCurrency   = params[2].get_str();
        bool isShowTxids = (params.size() == 5) && (params[4].get_str() == "txids");

        if (detailLevel < 1 || detailLevel > 3) {
            Object error;
            error.emplace_back(Pair("error",
                                    "invalid detail level value:"));
            error.emplace_back(Pair("code", xbridge::INVALID_PARAMETERS));
            return  error;
        }

        std::size_t maxOrders = 50;
        if(detailLevel == 2 && params.size() == 4) {
            maxOrders = params[3].get_int();
        }
        if(maxOrders < 1) {
            Object error;
            error.emplace_back(Pair("error",
                                    "Negative value of maximum orders parameter"));
            error.emplace_back(Pair("code", xbridge::INVALID_PARAMETERS));
            return  error;
        }

        //copy all transactions in currencies specified in the parameters
        std::copy_if(trList.begin(), trList.end(), std::inserter(asksList, asksList.end()),
                     [&toCurrency, &fromCurrency](const TransactionPair &transaction) {
            return  ((transaction.second->toCurrency == fromCurrency) &&
                    (transaction.second->fromCurrency == toCurrency));
        });

        std::copy_if(trList.begin(), trList.end(), std::inserter(bidsList, bidsList.end()),
                     [&toCurrency, &fromCurrency](const TransactionPair &transaction) {
            return  ((transaction.second->toCurrency == toCurrency) &&
                    (transaction.second->fromCurrency == fromCurrency));
        });


        /**
         * @brief bids - array with bids
         */
        Array bids;
        /**
         * @brief asks - array with asks
         */
        Array asks;

        std::vector<xbridge::TransactionDescrPtr> asksVector;
        std::vector<xbridge::TransactionDescrPtr> bidsVector;

        for (const auto &trEntry : asksList) {
            asksVector.emplace_back(trEntry.second);
        }

        for (const auto &trEntry : bidsList) {
            bidsVector.emplace_back(trEntry.second);
        }

        //sort descending
        std::sort(bidsVector.begin(), bidsVector.end(),
                  [](const xbridge::TransactionDescrPtr &a,  const xbridge::TransactionDescrPtr &b) {
            const auto priceA = xBridgeValueFromAmount(a->fromAmount) / xBridgeValueFromAmount(a->toAmount);
            const auto priceB = xBridgeValueFromAmount(b->fromAmount) / xBridgeValueFromAmount(b->toAmount);
            return priceA > priceB;
        });

        std::sort(asksVector.begin(), asksVector.end(), [](const xbridge::TransactionDescrPtr &a,  const xbridge::TransactionDescrPtr &b){
            const auto priceA = xBridgeValueFromAmount(a->fromAmount) / xBridgeValueFromAmount(a->toAmount);
            const auto priceB = xBridgeValueFromAmount(b->fromAmount) / xBridgeValueFromAmount(b->toAmount);
            return priceA < priceB;
        });

        switch (detailLevel)
        {
        case 1:
        {
            //return Only the best bid and ask
            const auto bidsItem = std::max_element(bidsList.begin(), bidsList.end(),
                                       [](const TransactionPair &a, const TransactionPair &b) {
                //find transaction with best bids
                const auto &tr1 = a.second;
                const auto &tr2 = b.second;
                const auto priceA = xBridgeValueFromAmount(tr1->fromAmount) / xBridgeValueFromAmount(tr1->toAmount);
                const auto priceB = xBridgeValueFromAmount(tr2->fromAmount) / xBridgeValueFromAmount(tr2->toAmount);
                return priceA < priceB;
            });

            {
                const auto &tr = bidsItem->second;
                if(tr != nullptr) {
                    const auto bidPrice = xBridgeValueFromAmount(tr->fromAmount) / xBridgeValueFromAmount(tr->toAmount);
                    bids.emplace_back(bidPrice);
                    bids.emplace_back(xBridgeValueFromAmount(tr->fromAmount));
                    if(isShowTxids) {
                        bids.emplace_back(tr->id.GetHex());
                    }
                }
            }
            ///////////////////////////////////////////////////////////////////////////////////////////////

            const auto asksItem = std::min_element(asksList.begin(), asksList.end(),
                                                   [](const TransactionPair &a, const TransactionPair &b) {
                //find transactions with best asks
                const auto &tr1 = a.second;
                const auto &tr2 = b.second;
                const auto priceA = xBridgeValueFromAmount(tr1->fromAmount) / xBridgeValueFromAmount(tr1->toAmount);
                const auto priceB = xBridgeValueFromAmount(tr2->fromAmount) / xBridgeValueFromAmount(tr2->toAmount);
                return priceA < priceB;
            });

            {
                const auto &tr = asksItem->second;
                if(tr != nullptr) {
                    const auto askPrice = xBridgeValueFromAmount(tr->fromAmount) / xBridgeValueFromAmount(tr->toAmount);
                    asks.emplace_back(askPrice);
                    asks.emplace_back(xBridgeValueFromAmount(tr->fromAmount));
                    if(isShowTxids) {
                        asks.emplace_back(tr->id.GetHex());
                    }
                }
            }
            res.emplace_back(Pair("bids", bids));
            res.emplace_back(Pair("asks", asks));
            return  res;
        }
        case 2:
        {
            //Top X bids and asks (aggregated)

            /**
             * @brief bound - calculate upper bound
             */
            auto bound = std::min(maxOrders, bidsVector.size());

            // floating point comparisons
            // see Knuth 4.2.2 Eq 36
            auto floatCompare = [](const double a, const double b) -> bool {
                auto epsilon = std::numeric_limits<double>::epsilon();
                return (fabs(a - b) / fabs(a) <= epsilon  ) && (fabs(a - b) / fabs(b) <= epsilon);
            };
            for(size_t i = 0; i < bound; i++) {
                Array tmp;
                //calculate bids and push to array
                const auto fromAmount   = bidsVector[i]->fromAmount;
                const auto toAmount     = bidsVector[i]->toAmount;
                const auto bidPrice     = xBridgeValueFromAmount(fromAmount) / xBridgeValueFromAmount(toAmount);
                auto volume             = xBridgeValueFromAmount(bidsVector[i]->fromAmount);
                size_t j = i + 1;
                tmp.emplace_back(bidPrice);
                Array txids;
                if(isShowTxids) {
                    txids.emplace_back(bidsVector[i]->id.GetHex());
                }
                while(j < bound) {
                    const auto fromAmount2  = bidsVector[j]->fromAmount;
                    const auto toAmount2    = bidsVector[j]->toAmount;
                    const auto bidPrice2    = xBridgeValueFromAmount(fromAmount2) / xBridgeValueFromAmount(toAmount2);
                    if(!floatCompare(bidPrice, bidPrice2)) {
                        i = j - 1;
                        break;
                    }
                    volume += xBridgeValueFromAmount(bidsVector[j]->fromAmount);
                    if(isShowTxids) {
                        txids.emplace_back(bidsVector[j]->id.GetHex());
                    }
                    j++;
                }
                tmp.emplace_back(volume);
                if(isShowTxids) {
                    tmp.emplace_back(txids);
                }
                bids.emplace_back(tmp);
            }
            bound = std::min(maxOrders, asksVector.size());
            for(size_t  i = 0; i < bound; i++ ) {
                Array tmp;
                //calculate asks and push to array
                const auto fromAmount   = asksVector[i]->fromAmount;
                const auto toAmount     = asksVector[i]->toAmount;
                const auto askPrice     = xBridgeValueFromAmount(fromAmount) / xBridgeValueFromAmount(toAmount);
                auto volume             = xBridgeValueFromAmount(asksVector[i]->fromAmount);
                size_t j = i + 1;
                tmp.emplace_back(askPrice);
                Array txids;
                if(isShowTxids) {
                    txids.emplace_back(asksVector[i]->id.GetHex());
                }
                while(j < bound) {
                    const auto fromAmount2  = asksVector[j]->fromAmount;
                    const auto toAmount2    = asksVector[j]->toAmount;
                    const auto askPrice2    = xBridgeValueFromAmount(fromAmount2) / xBridgeValueFromAmount(toAmount2);
                    if(!floatCompare(askPrice, askPrice2)) {
                        i = j - 1;
                        break;
                    }
                    volume += xBridgeValueFromAmount(asksVector[j]->fromAmount);
                    if(isShowTxids) {
                        txids.emplace_back(asksVector[j]->id.GetHex());
                    }
                    j++;
                }
                tmp.emplace_back(volume);
                if(isShowTxids) {
                    tmp.emplace_back(txids);
                }
                asks.emplace_back(tmp);
            }
            res.emplace_back(Pair("bids", bids));
            res.emplace_back(Pair("asks", asks));
            return  res;
        }
        case 3:
        {
            //Full order book (non aggregated)

            for (const auto &tr : bidsVector) {
                Array tmp;
                const auto bidPrice = xBridgeValueFromAmount(tr->fromAmount) / xBridgeValueFromAmount(tr->toAmount);
                tmp.emplace_back(bidPrice);
                tmp.emplace_back(xBridgeValueFromAmount(tr->fromAmount));
                tmp.emplace_back(tr->id.GetHex());
                bids.emplace_back(tmp);

            }
            for (const auto &tr : asksVector) {
                Array tmp;
                const auto askPrice = xBridgeValueFromAmount(tr->fromAmount) / xBridgeValueFromAmount(tr->toAmount);
                tmp.emplace_back(askPrice);
                tmp.emplace_back(xBridgeValueFromAmount(tr->fromAmount));
                tmp.emplace_back(tr->id.GetHex());
                asks.emplace_back(tmp);
            }
            res.emplace_back(Pair("bids", bids));
            res.emplace_back(Pair("asks", asks));
            return  res;

        }
        default:
            Object error;
            error.emplace_back(Pair("error",
                                    "invalid detail level value:"));
            error.emplace_back(Pair("code", xbridge::INVALID_PARAMETERS));
            return  error;
        }
    }
}
