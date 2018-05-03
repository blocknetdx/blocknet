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
#include <boost/range/algorithm_ext/push_back.hpp>
#include <boost/range/adaptor/map.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/algorithm/copy.hpp>

#include <stdio.h>
#include <atomic>
#include <numeric>
#include <math.h>
#include "util/settings.h"
#include "util/logger.h"
#include "util/xbridgeerror.h"
#include "util/xutil.h"
#include "xbridgeapp.h"
#include "xbridgeexchange.h"
#include "xbridgetransaction.h"
#include "xbridgetransactiondescr.h"
#include "xuiconnector.h"
#include "rpcserver.h"
#include "init.h"
#include "wallet.h"
using namespace json_spirit;
using namespace std;
using namespace boost;
using namespace boost::asio;

using TransactionMap    = std::map<uint256, xbridge::TransactionDescrPtr>;

using TransactionPair   = std::pair<uint256, xbridge::TransactionDescrPtr>;

using RealVector        = std::vector<double>;

using TransactionVector = std::vector<xbridge::TransactionDescrPtr>;
namespace bpt           = boost::posix_time;



//******************************************************************************
//******************************************************************************
Value dxGetLocalTokens(const Array & params, bool fHelp)
{
    if (fHelp) {

        throw runtime_error("dxGetLocalTokens\nList coins supported by your node. You can only trade with these supported coins.");

    }
    if (params.size() > 0) {

        return util::makeError(xbridge::INVALID_PARAMETERS, __FUNCTION__,
                               "This function does not accept any parameter");

    }

    Array r;

    std::vector<std::string> currencies = xbridge::App::instance().availableCurrencies();
    for (std::string currency : currencies) {

        r.emplace_back(currency);

    }
    return r;
}

//******************************************************************************
//******************************************************************************
Value dxGetNetworkTokens(const Array & params, bool fHelp)
{
    if (fHelp) {

        throw runtime_error("dxGetNetworkTokens\nList coins supported by the network.");

    }
    if (params.size() > 0) {

        return util::makeError(xbridge::INVALID_PARAMETERS, __FUNCTION__,
                               "This function does not accept any parameters");

    }

    return dxGetLocalTokens(params, fHelp); // TODO Remove when xwallet broadcast is ready

    Array r;

    std::vector<std::string> currencies = xbridge::App::instance().networkCurrencies();
    for (std::string currency : currencies) {

        r.emplace_back(currency);

    }
    return r;
}

//******************************************************************************
//******************************************************************************

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

        throw runtime_error("dxGetOrders\nList of all orders.");

    }
    if (!params.empty()) {

        return util::makeError(xbridge::INVALID_PARAMETERS, __FUNCTION__,
                               "This function does not accept any parameters");

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
        jtr.emplace_back(Pair("maker_size",     util::xBridgeStringValueFromAmount(tr->fromAmount)));
        jtr.emplace_back(Pair("taker",          tr->toCurrency));
        jtr.emplace_back(Pair("taker_size",     util::xBridgeStringValueFromAmount(tr->toAmount)));
        jtr.emplace_back(Pair("updated_at",     util::iso8601(tr->txtime)));
        jtr.emplace_back(Pair("created_at",     util::iso8601(tr->created)));
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

        throw runtime_error("dxGetOrderFills (maker) (taker) (combined, default=true)[optional]\n"
                            "Returns all the recent trades by trade pair that have been filled \n"
                            "(i.e. completed). Maker symbol is always listed first. The [combined] \n"
                            "flag defaults to true. When set to false [combined] will return only \n"
                            "maker trades, switch maker and taker to get the reverse."
                            );

    }

    bool invalidParams = ((params.size() != 2) &&
                          (params.size() != 3));
    if (invalidParams) {

        return util::makeError(xbridge::INVALID_PARAMETERS, __FUNCTION__,
                               "(maker) (taker) (combined, default=true)[optional]");

    }

    bool combined = params.size() == 3 ? params[2].get_bool() : true;

    const auto maker = params[0].get_str();
    const auto taker = params[1].get_str();



    TransactionMap history = xbridge::App::instance().history();



    TransactionVector result;

    for (auto &item : history) {
        const xbridge::TransactionDescrPtr &ptr = item.second;
        if ((ptr->state == xbridge::TransactionDescr::trFinished) &&
            (combined ? ((ptr->fromCurrency == maker && ptr->toCurrency == taker) || (ptr->toCurrency == maker && ptr->fromCurrency == taker)) : (ptr->fromCurrency == maker && ptr->toCurrency == taker))) {
            result.push_back(ptr);
        }
    }

    std::sort(result.begin(), result.end(),
              [](const xbridge::TransactionDescrPtr &a,  const xbridge::TransactionDescrPtr &b)
    {
         return (a->txtime) > (b->txtime);
    });

    Array arr;
    for(const auto &transaction : result) {

        Object tmp;
        tmp.emplace_back(Pair("id",         transaction->id.GetHex()));
        tmp.emplace_back(Pair("time",       util::iso8601(transaction->txtime)));
        tmp.emplace_back(Pair("maker",      transaction->fromCurrency));
        tmp.emplace_back(Pair("maker_size", util::xBridgeStringValueFromAmount(transaction->fromAmount)));
        tmp.emplace_back(Pair("taker",      transaction->toCurrency));
        tmp.emplace_back(Pair("taker_size", util::xBridgeStringValueFromAmount(transaction->toAmount)));
        arr.emplace_back(tmp);

    }
    return arr;
}

Value dxGetOrderHistory(const json_spirit::Array& params, bool fHelp)
{

    if (fHelp) {

        throw runtime_error("dxGetOrderHistory (maker) (taker) (start time) (end time) (granularity) (order_ids, default=false)[optional]\n"
                            "Returns the order history over a specified time interval. [start_time] and [end_time] are \n"
                            "in unix time seconds [granularity] in seconds of supported time interval lengths include: \n"
                            "60,300,900,3600,21600,86400. [order_ids] is a boolean, defaults to false (not showing ids).");

    }
    if (params.size() < 5) {

        return util::makeError(xbridge::INVALID_PARAMETERS, __FUNCTION__,
                               "(maker) (taker) (start time) (end time) (granularity) "
                               "(order_ids, default=false)[optional]");
    }

    Array arr;
    TransactionMap history = xbridge::App::instance().history();

    if(history.empty()) {

        LOG() << "empty history transactions list";
        return arr;

    }

    const auto fromCurrency     = params[0].get_str();
    const auto toCurrency       = params[1].get_str();
    const auto startTimeFrame   = params[2].get_int();
    auto endTimeFrame           = params[3].get_int();
    const auto granularity      = params[4].get_int();

    // Validate start time (no start date less than 2/25/2018)
    if (startTimeFrame < 1519540000) {

        return util::makeError(xbridge::INVALID_PARAMETERS, __FUNCTION__,
                               "Start time too early.");

    }

    // Validate start and end times (no times too far in the future)
    std::time_t currentTime = std::time(nullptr);
    std::time_t oneDayFromNow = currentTime + 86400;
    if (startTimeFrame > oneDayFromNow || endTimeFrame > oneDayFromNow) {
        Object error;
        error.emplace_back(Pair("error", "Start/end times are too large."));
        error.emplace_back(Pair("code", xbridge::INVALID_PARAMETERS));
        error.emplace_back(Pair("name",  __FUNCTION__));
        return error;
    }
    if (endTimeFrame > currentTime)
        endTimeFrame = (int)currentTime + 1;

    // Validate granularity
    switch (granularity) {
        case 60:
        case 300:
        case 900:
        case 3600:
        case 21600:
        case 86400:
            break;
        default:
            return util::makeError(xbridge::INVALID_PARAMETERS, __FUNCTION__,
                                   "granularity must be one of: 60,300,900,3600,21600,86400");
    }

    bool isShowTxids = params.size() == 6 ? params[5].get_bool() : false;

    TransactionMap trList;
    std::vector<xbridge::TransactionDescrPtr> trVector;

    //copy all transactions between startTimeFrame and endTimeFrame
    std::copy_if(history.begin(), history.end(), std::inserter(trList, trList.end()),
                 [&startTimeFrame, &endTimeFrame, &toCurrency, &fromCurrency](const TransactionPair &transaction){
        return  ((transaction.second->created)   <=  bpt::from_time_t(endTimeFrame)) &&
                ((transaction.second->created)   >=  bpt::from_time_t(startTimeFrame)) &&
                ((transaction.second->toCurrency == toCurrency && transaction.second->fromCurrency == fromCurrency) || // return requested and inverse trading pairs
                (transaction.second->toCurrency  == fromCurrency && transaction.second->fromCurrency == toCurrency)) &&
                (transaction.second->state       == xbridge::TransactionDescr::trFinished);
    });

    if(trList.empty()) {

        LOG() << "No orders for the specified period " << __FUNCTION__;
        return  arr;

    }

    //copy values into vector
    for (const auto &trEntry : trList) {
        const auto &tr = trEntry.second;
        trVector.push_back(tr);
    }

    // Sort ascending by updated time
    std::sort(trVector.begin(), trVector.end(),
              [](const xbridge::TransactionDescrPtr &a, const xbridge::TransactionDescrPtr &b)
    {
         return (a->txtime) < (b->txtime);
    });

    // Setup intervals. Each time period (interval) has a high,low,open,close,volume. This code is
    // responsible for calculating the results for each interval.
    int jstart = 0;
    for (int timeInterval = startTimeFrame; timeInterval < endTimeFrame; timeInterval += granularity) {
        Array interval;

        double volume = 0; // store total volume for interval
        xbridge::TransactionDescrPtr open = nullptr; // open order
        xbridge::TransactionDescrPtr close = nullptr; // close order
        xbridge::TransactionDescrPtr high = nullptr; // high order
        xbridge::TransactionDescrPtr low = nullptr; // low order
        bool orderFound = false;
        Array orderIds;

        // start searching from point of last checked order (since orders are only processed once)
        for (int j = jstart; j < (int)trVector.size(); j++) {
            const auto &tr = trVector[j];
            auto t = (int)(util::timeToInt(tr->txtime)/1000/1000); // need seconds, timeToInt is in microseconds
            // only check orders within boundaries (time interval)
            if (t >= timeInterval && t < timeInterval + granularity) {
                // Record if order found
                orderFound = true;
                // open is always first order
                if (open == nullptr)
                    open = tr;
                // defaults
                if (high == nullptr)
                    high = tr;
                if (low == nullptr)
                    low = tr;
                // close is always last order
                close = tr;

                // volume is based in "to amount" (track volume of what we're priced in, in this case orders are priced in "to amount")
                volume += fromCurrency == tr->fromCurrency ? util::xBridgeValueFromAmount(tr->fromAmount) :
                          util::xBridgeValueFromAmount(tr->toAmount);

                // calc prices, algo: to/from = price (in terms of to). e.g. LTC-SYS price = SYS-size / LTC-size = SYS per unit priced in LTC
                double high_price    = fromCurrency == high->fromCurrency ? util::price(high) : util::priceBid(high);
                double low_price     = fromCurrency == low->fromCurrency  ? util::price(low)  : util::priceBid(low);
                double current_price = fromCurrency == tr->fromCurrency   ? util::price(tr)   : util::priceBid(tr);

                // record highest if current price larger than highest price
                if (current_price > high_price)
                    high = tr;
                // record lowest if current price is lower than lowest price
                if (current_price < low_price)
                    low = tr;

                // store order id if necessary
                if (isShowTxids)
                    orderIds.emplace_back(tr->id.GetHex());

                jstart = j; // want to skip already searched orders
            }
        }

        // Process if at least 1 order is found
        if (orderFound) {
            // latest prices
            double open_price  = fromCurrency == open->fromCurrency  ? util::price(open)  : util::priceBid(open);
            double close_price = fromCurrency == close->fromCurrency ? util::price(close) : util::priceBid(close);
            double high_price  = fromCurrency == high->fromCurrency  ? util::price(high)  : util::priceBid(high);
            double low_price   = fromCurrency == low->fromCurrency   ? util::price(low)   : util::priceBid(low);

            // format: [ time, low, high, open, close, volume ]
            interval.emplace_back(util::iso8601(boost::posix_time::from_time_t(timeInterval + granularity)));
            interval.emplace_back(low_price);
            interval.emplace_back(high_price);
            interval.emplace_back(open_price);
            interval.emplace_back(close_price);
            interval.emplace_back(volume);

            if (isShowTxids)
                interval.emplace_back(orderIds);

        } else { // if no orders for time interval, return empty data
            // format: [ time, low, high, open, close, volume ]
            interval.emplace_back(util::iso8601(boost::posix_time::from_time_t(timeInterval + granularity)));
            interval.emplace_back(0);
            interval.emplace_back(0);
            interval.emplace_back(0);
            interval.emplace_back(0);
            interval.emplace_back(0);
            if (isShowTxids)
                interval.emplace_back(Array());
        }

        arr.emplace_back(interval);
    }

    return arr;
}

//*****************************************************************************
//*****************************************************************************

Value dxGetOrder(const Array & params, bool fHelp)
{
    if (fHelp) {

         throw runtime_error("dxGetOrder (id)\n"
                             "Get order info by id.");

    }
    if (params.size() != 1) {

        return util::makeError(xbridge::INVALID_PARAMETERS, __FUNCTION__,
                               "(id)");

    }

    uint256 id(params[0].get_str());

    auto &xapp = xbridge::App::instance();

    const xbridge::TransactionDescrPtr order = xapp.transaction(uint256(id));

    if(order == nullptr) {

        return util::makeError(xbridge::TRANSACTION_NOT_FOUND, __FUNCTION__,
                               id.ToString());

    }

    xbridge::WalletConnectorPtr connFrom = xapp.connectorByCurrency(order->fromCurrency);
    xbridge::WalletConnectorPtr connTo   = xapp.connectorByCurrency(order->toCurrency);
    if(!connFrom) {

        return util::makeError(xbridge::NO_SESSION, __FUNCTION__,
                               order->fromCurrency);

    }
    if (!connTo) {

        return util::makeError(xbridge::NO_SESSION, __FUNCTION__,
                               order->toCurrency);

    }

    Object result;
    result.emplace_back(Pair("id",          order->id.GetHex()));
    result.emplace_back(Pair("maker",       order->fromCurrency));
    result.emplace_back(Pair("maker_size",  util::xBridgeStringValueFromAmount(order->fromAmount)));
    result.emplace_back(Pair("taker",       order->toCurrency));
    result.emplace_back(Pair("taker_size",  util::xBridgeStringValueFromAmount(order->toAmount)));
    result.emplace_back(Pair("updated_at",  util::iso8601(order->txtime)));
    result.emplace_back(Pair("created_at",  util::iso8601(order->created)));
    result.emplace_back(Pair("status",      order->strState()));
    return result;
}

//******************************************************************************
//******************************************************************************
Value dxMakeOrder(const Array &params, bool fHelp)
{

    if (fHelp) {

        throw runtime_error("dxMakeOrder (maker) (maker size) (maker address) "
                            "(taker) (taker size) (taker address) (type) (dryrun)[optional]\n"
                            "Create a new order. dryrun will validate the order without submitting the order to the network.");

    }
    if (params.size() < 7) {

        return util::makeError(xbridge::INVALID_PARAMETERS, __FUNCTION__,
                               "(taker) (taker size) (taker address) (type) (dryrun)[optional]\n"
                               "Create a new order. dryrun will validate the order without submitting the order to the network.");

    }

    if (!util::xBridgeValidCoin(params[1].get_str())) {
        Object error;
        error.emplace_back(Pair("error",    xbridge::xbridgeErrorText(xbridge::INVALID_PARAMETERS,
                      "maker size is too precise, maximum precision supported is " +
                              std::to_string(util::xBridgeSignificantDigits(xbridge::TransactionDescr::COIN)) + " digits")));
        error.emplace_back(Pair("code",     xbridge::INVALID_PARAMETERS));
        error.emplace_back(Pair("name",     __FUNCTION__));
        return error;
    }

    if (!util::xBridgeValidCoin(params[4].get_str())) {
        Object error;
        error.emplace_back(Pair("error",    xbridge::xbridgeErrorText(xbridge::INVALID_PARAMETERS,
                      "taker size is too precise, maximum precision supported is " +
                              std::to_string(util::xBridgeSignificantDigits(xbridge::TransactionDescr::COIN)) + " digits")));
        error.emplace_back(Pair("code",     xbridge::INVALID_PARAMETERS));
        error.emplace_back(Pair("name",     __FUNCTION__));
        return error;
    }

    std::string fromCurrency    = params[0].get_str();
    double      fromAmount      = boost::lexical_cast<double>(params[1].get_str());
    std::string fromAddress     = params[2].get_str();

    std::string toCurrency      = params[3].get_str();
    double      toAmount        = boost::lexical_cast<double>(params[4].get_str());
    std::string toAddress       = params[5].get_str();

    std::string type            = params[6].get_str();

    // Validate the order type
    if (type != "exact") {

        return util::makeError(xbridge::INVALID_PARAMETERS, __FUNCTION__,
                               "Only the exact type is supported at this time.");

    }

    // Check that addresses are not the same
    if (fromAddress == toAddress) {
        return util::makeError(xbridge::INVALID_PARAMETERS, __FUNCTION__,
                               "maker address and taker address cannot be the same: " + fromAddress);
    }

    // Check upper limits
    if (fromAmount > (double)xbridge::TransactionDescr::MAX_COIN ||
            toAmount > (double)xbridge::TransactionDescr::MAX_COIN) {
        return util::makeError(xbridge::INVALID_PARAMETERS, __FUNCTION__,
                               "Maximum supported size is " + std::to_string(xbridge::TransactionDescr::MAX_COIN));
    }
    // Check lower limits
    if (fromAmount <= 0 || toAmount <= 0) {
        return util::makeError(xbridge::INVALID_PARAMETERS, __FUNCTION__,
                               "Minimum supported size is " + util::xBridgeStringValueFromPrice(1.0/xbridge::TransactionDescr::COIN));
    }

    // Validate address prefix
    xbridge::WalletConnectorPtr connFrom = xbridge::App::instance().connectorByCurrency(fromCurrency);
    xbridge::WalletConnectorPtr connTo   = xbridge::App::instance().connectorByCurrency(toCurrency);
    if (!connFrom) return util::makeError(xbridge::NO_SESSION, __FUNCTION__, "unable to connect to wallet: " + fromCurrency);
    if (!connTo) return util::makeError(xbridge::NO_SESSION, __FUNCTION__, "unable to connect to wallet: " + toCurrency);
    if (!connFrom->hasValidAddressPrefix(fromAddress))
        return util::makeError(xbridge::INVALID_ADDRESS, __FUNCTION__,
                               ": " + fromCurrency + " address is bad, are you using the correct address?");
    if (!connTo->hasValidAddressPrefix(toAddress))
        return util::makeError(xbridge::INVALID_ADDRESS, __FUNCTION__,
                               ": " + toCurrency + " address is bad, are you using the correct address?");

    auto statusCode = xbridge::SUCCESS;

    xbridge::App &app = xbridge::App::instance();


    if (!app.isValidAddress(fromAddress)) {

        return util::makeError(xbridge::INVALID_ADDRESS, __FUNCTION__, fromAddress);

    }
    if (!app.isValidAddress(toAddress)) {

        return util::makeError(xbridge::INVALID_ADDRESS, __FUNCTION__, toAddress);

    }
    if(fromAmount <= .0) {
        return util::makeError(xbridge::INVALID_PARAMETERS, __FUNCTION__,
                               "maker size must be greater than 0");
    }
    if(toAmount <= .0) {

        return util::makeError(xbridge::INVALID_PARAMETERS, __FUNCTION__,
                               "taker size must be greater than 0");

    }
    // Perform explicit check on dryrun to avoid executing order on bad spelling
    bool dryrun = false;
    if (params.size() == 8) {
        std::string dryrunParam = params[7].get_str();
        if (dryrunParam != "dryrun") {

            return util::makeError(xbridge::INVALID_PARAMETERS, __FUNCTION__, dryrunParam);

        }
        dryrun = true;
    }


    Object result;
    statusCode = app.checkCreateParams(fromCurrency, toCurrency,
                                       util::xBridgeAmountFromReal(fromAmount), fromAddress);
    switch (statusCode) {
    case xbridge::SUCCESS:{
        // If dryrun
        if (dryrun) {
            result.emplace_back(Pair("id", uint256().GetHex()));
            result.emplace_back(Pair("maker", fromCurrency));
            result.emplace_back(Pair("maker_size",
                                     util::xBridgeStringValueFromAmount(util::xBridgeAmountFromReal(fromAmount))));
            result.emplace_back(Pair("maker_address", fromAddress));
            result.emplace_back(Pair("taker", toCurrency));
            result.emplace_back(Pair("taker_size",
                                     util::xBridgeStringValueFromAmount(util::xBridgeAmountFromReal(toAmount))));
            result.emplace_back(Pair("taker_address", toAddress));
            result.emplace_back(Pair("status", "created"));
            return result;
        }
        break;
    }

    case xbridge::INVALID_CURRENCY: {
        return util::makeError(statusCode, __FUNCTION__, fromCurrency);
    }
    case xbridge::NO_SESSION:{
        return util::makeError(statusCode, __FUNCTION__, fromCurrency);
    }
    case xbridge::INSIFFICIENT_FUNDS:{
        return util::makeError(statusCode, __FUNCTION__, fromAddress);
    }

    default:
        return util::makeError(statusCode, __FUNCTION__);
    }


    uint256 id = uint256();
    uint256 blockHash = uint256();
    statusCode = xbridge::App::instance().sendXBridgeTransaction
          (fromAddress, fromCurrency, util::xBridgeAmountFromReal(fromAmount),
           toAddress, toCurrency, util::xBridgeAmountFromReal(toAmount), id, blockHash);

    if (statusCode == xbridge::SUCCESS) {

        Object obj;
        obj.emplace_back(Pair("id",             id.GetHex()));
        obj.emplace_back(Pair("maker_address",  fromAddress));
        obj.emplace_back(Pair("maker",          fromCurrency));
        obj.emplace_back(Pair("maker_size",     util::xBridgeStringValueFromAmount(util::xBridgeAmountFromReal(fromAmount))));
        obj.emplace_back(Pair("taker_address",  toAddress));
        obj.emplace_back(Pair("taker",          toCurrency));
        obj.emplace_back(Pair("taker_size",     util::xBridgeStringValueFromAmount(util::xBridgeAmountFromReal(toAmount))));
        const auto &createdTime = xbridge::App::instance().transaction(id)->created;
        obj.emplace_back(Pair("created_at",     util::iso8601(createdTime)));
        obj.emplace_back(Pair("updated_at",     util::iso8601(bpt::microsec_clock::universal_time()))); // TODO Need actual updated time, this is just estimate
        obj.emplace_back(Pair("block_id",       blockHash.GetHex()));
        obj.emplace_back(Pair("status",         "created"));
        return obj;

    } else {
        return util::makeError(statusCode, __FUNCTION__);
    }
}

//******************************************************************************
//******************************************************************************
Value dxTakeOrder(const Array & params, bool fHelp)
{

    if (fHelp)
    {
        throw runtime_error("dxTakeOrder (id) (address from) (address to) [optional](dryrun)\n"
                            "Accepts the order. dryrun will evaluate input without accepting the order.");
    }

    auto statusCode = xbridge::SUCCESS;

    if ((params.size() != 3) && (params.size() != 4))
    {
        return util::makeError(xbridge::INVALID_PARAMETERS, __FUNCTION__,
                               "(id) (address from) (address to) [optional](dryrun)");
    }

    uint256 id(params[0].get_str());
    std::string fromAddress    = params[1].get_str();
    std::string toAddress      = params[2].get_str();

    xbridge::App &app = xbridge::App::instance();

    if (!app.isValidAddress(fromAddress))
    {
        return util::makeError(xbridge::INVALID_ADDRESS, __FUNCTION__, fromAddress);
    }

    if (!app.isValidAddress(toAddress))
    {
        return util::makeError(xbridge::INVALID_PARAMETERS, __FUNCTION__, toAddress);
    }

    // Check that addresses are not the same
    if (fromAddress == toAddress) {
        return util::makeError(xbridge::INVALID_PARAMETERS, __FUNCTION__,
                               "address from and address to cannot be the same: " + fromAddress);
    }

    // Perform explicit check on dryrun to avoid executing order on bad spelling
    bool dryrun = false;
    if (params.size() == 4) {
        std::string dryrunParam = params[3].get_str();
        if (dryrunParam != "dryrun") {
            return util::makeError(xbridge::INVALID_PARAMETERS, __FUNCTION__, dryrunParam);
        }
        dryrun = true;
    }

    Object result;
    xbridge::TransactionDescrPtr txDescr;
    statusCode = app.checkAcceptParams(id, txDescr, fromAddress);

    switch (statusCode)
    {
    case xbridge::SUCCESS: {
        if (txDescr->isLocal()) // no self trades
            return util::makeError(xbridge::INVALID_PARAMETERS, __FUNCTION__, "unable to accept your own order");

        // Validate address prefix
        xbridge::WalletConnectorPtr connFrom = xbridge::App::instance().connectorByCurrency(txDescr->fromCurrency);
        xbridge::WalletConnectorPtr connTo   = xbridge::App::instance().connectorByCurrency(txDescr->toCurrency);
        if (!connFrom) return util::makeError(xbridge::NO_SESSION, __FUNCTION__, "unable to connect to wallet: " + txDescr->fromCurrency);
        if (!connTo) return util::makeError(xbridge::NO_SESSION, __FUNCTION__, "unable to connect to wallet: " + txDescr->toCurrency);
        // taker [to] will match order [from] currency
        if (!connFrom->hasValidAddressPrefix(toAddress))
            return util::makeError(xbridge::INVALID_ADDRESS, __FUNCTION__,
                                   ": " + txDescr->fromCurrency + " address is bad, are you using the correct address?");
        // taker [from] will match order [to] currency
        if (!connTo->hasValidAddressPrefix(fromAddress))
            return util::makeError(xbridge::INVALID_ADDRESS, __FUNCTION__,
                                   ": " + txDescr->toCurrency + " address is bad, are you using the correct address?");

        if(dryrun)
        {
            result.emplace_back(Pair("id", uint256().GetHex()));

            result.emplace_back(Pair("maker", txDescr->fromCurrency));
            result.emplace_back(Pair("maker_size", util::xBridgeStringValueFromAmount(txDescr->fromAmount)));

            result.emplace_back(Pair("taker", txDescr->toCurrency));
            result.emplace_back(Pair("taker_size", util::xBridgeStringValueFromAmount(txDescr->toAmount)));

            result.emplace_back(Pair("updated_at", util::iso8601(bpt::microsec_clock::universal_time())));
            result.emplace_back(Pair("created_at", util::iso8601(txDescr->created)));

            result.emplace_back(Pair("status", "filled"));
            return result;
        }

        break;
    }
    case xbridge::TRANSACTION_NOT_FOUND:
    {
        return util::makeError(xbridge::TRANSACTION_NOT_FOUND, __FUNCTION__, id.ToString());
    }

    case xbridge::NO_SESSION:
    {
        return util::makeError(xbridge::NO_SESSION, __FUNCTION__, txDescr->toCurrency);
    }

    case xbridge::INSIFFICIENT_FUNDS:
    {
        return util::makeError(xbridge::INSIFFICIENT_FUNDS, __FUNCTION__,
                               std::string(txDescr->to.begin(),txDescr->to.end()));
    }

    default:
        return util::makeError(statusCode, __FUNCTION__);

    }

    // TODO swap is destructive on state (also complicates historical data)
    std::swap(txDescr->fromCurrency, txDescr->toCurrency);
    std::swap(txDescr->fromAmount, txDescr->toAmount);

    statusCode = app.acceptXBridgeTransaction(id, fromAddress, toAddress);
    if (statusCode == xbridge::SUCCESS) {

        result.emplace_back(Pair("id", id.GetHex()));

        result.emplace_back(Pair("maker", txDescr->fromCurrency));
        result.emplace_back(Pair("maker_size", util::xBridgeStringValueFromAmount(txDescr->fromAmount)));

        result.emplace_back(Pair("taker", txDescr->toCurrency));
        result.emplace_back(Pair("taker_size", util::xBridgeStringValueFromAmount(txDescr->toAmount)));

        result.emplace_back(Pair("updated_at", util::iso8601(bpt::microsec_clock::universal_time())));
        result.emplace_back(Pair("created_at", util::iso8601(txDescr->created)));

        result.emplace_back(Pair("status", txDescr->strState()));
        return result;

    } else {
        // restore state on error
        std::swap(txDescr->fromCurrency, txDescr->toCurrency);
        std::swap(txDescr->fromAmount, txDescr->toAmount);
        return util::makeError(statusCode, __FUNCTION__);
    }
}

//******************************************************************************
//******************************************************************************
Value dxCancelOrder(const Array &params, bool fHelp)
{
    if(fHelp)
    {
        throw runtime_error("dxCancelOrder (id)\n"
                            "Cancel xbridge order.");
    }

    if (params.size() != 1)
    {
        return util::makeError(xbridge::INVALID_PARAMETERS, __FUNCTION__,
                               "(id)");
    }

    LOG() << "rpc cancel order " << __FUNCTION__;
    uint256 id(params[0].get_str());

    xbridge::TransactionDescrPtr tx = xbridge::App::instance().transaction(id);
    if (!tx)
    {
        return util::makeError(xbridge::TRANSACTION_NOT_FOUND, __FUNCTION__, id.ToString());
    }

    if (tx->state >= xbridge::TransactionDescr::trCreated)
    {
        return util::makeError(xbridge::INVALID_STATE, __FUNCTION__, "order is already " + tx->strState());
    }

    const auto res = xbridge::App::instance().cancelXBridgeTransaction(id, crRpcRequest);
    if (res != xbridge::SUCCESS)
    {
        return util::makeError(res, __FUNCTION__);
    }

    xbridge::WalletConnectorPtr connFrom = xbridge::App::instance().connectorByCurrency(tx->fromCurrency);
    xbridge::WalletConnectorPtr connTo   = xbridge::App::instance().connectorByCurrency(tx->toCurrency);
    if (!connFrom) {
        return util::makeError(xbridge::NO_SESSION, __FUNCTION__, tx->fromCurrency);
    }

    if (!connTo) {
        return util::makeError(xbridge::NO_SESSION, __FUNCTION__, tx->toCurrency);
    }
    Object obj;
    obj.emplace_back(Pair("id", id.GetHex()));

    obj.emplace_back(Pair("maker", tx->fromCurrency));
    obj.emplace_back(Pair("maker_size", util::xBridgeStringValueFromAmount(tx->fromAmount)));
    obj.emplace_back(Pair("maker_address", connFrom->fromXAddr(tx->from)));

    obj.emplace_back(Pair("taker", tx->toCurrency));
    obj.emplace_back(Pair("taker_size", util::xBridgeStringValueFromAmount(tx->toAmount)));
    obj.emplace_back(Pair("taker_address", connTo->fromXAddr(tx->to)));
    obj.emplace_back(Pair("refund_tx", tx->refTx));

    obj.emplace_back(Pair("updated_at", util::iso8601(tx->txtime)));
    obj.emplace_back(Pair("created_at", util::iso8601(tx->created)));

    obj.emplace_back(Pair("status", tx->strState()));
    return obj;
}

//******************************************************************************
//******************************************************************************
Value dxGetOrderBook(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp)
    {
        throw runtime_error("dxGetOrderBook (detail level, 1-4) (maker) (taker) (max orders, default=50)[optional]\n"
                            "Returns the order book. There are 4 detail levels that can be specified to obtain \n"
                            "different outputs for the orderbook. 1 lists the best bid and ask. 2 lists the \n"
                            "aggregated bids and asks. 3 lists the non-aggregated bids and asks. 4 is level 1 \n"
                            "with order ids. Optionally specify the maximum orders you wish to return.");
    }

    if ((params.size() < 3 || params.size() > 4))
    {
        return util::makeError(xbridge::INVALID_PARAMETERS, __FUNCTION__,
                               "(detail level, 1-4) (maker) (taker) (max orders, default=50)[optional]");
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
            return util::makeError(xbridge::INVALID_DETAIL_LEVEL, __FUNCTION__);
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
            res.emplace_back(Pair("asks", asks));
            res.emplace_back(Pair("bids", bids));
            return res;
        }

        TransactionMap asksList;
        TransactionMap bidsList;

        //copy all transactions in currencies specified in the parameters

        // ask orders are based in the first token in the trading pair
        std::copy_if(trList.begin(), trList.end(), std::inserter(asksList, asksList.end()),
                     [&toCurrency, &fromCurrency](const TransactionPair &transaction)
        {
            if(transaction.second == nullptr)
                return false;
            if (transaction.second->fromAmount <= 0 || transaction.second->toAmount <= 0)
                return false;
            if (transaction.second->state != xbridge::TransactionDescr::trPending)
                return false;

            return  ((transaction.second->toCurrency == toCurrency) &&
                    (transaction.second->fromCurrency == fromCurrency));
        });

        // bid orders are based in the second token in the trading pair (inverse of asks)
        std::copy_if(trList.begin(), trList.end(), std::inserter(bidsList, bidsList.end()),
                     [&toCurrency, &fromCurrency](const TransactionPair &transaction)
        {
            if(transaction.second == nullptr)
                return false;
            if (transaction.second->fromAmount <= 0 || transaction.second->toAmount <= 0)
                return false;
            if (transaction.second->state != xbridge::TransactionDescr::trPending)
                return false;

            return  ((transaction.second->toCurrency == fromCurrency) &&
                    (transaction.second->fromCurrency == toCurrency));
        });

        std::vector<xbridge::TransactionDescrPtr> asksVector;
        std::vector<xbridge::TransactionDescrPtr> bidsVector;

        for (const auto &trEntry : asksList)
            asksVector.emplace_back(trEntry.second);

        for (const auto &trEntry : bidsList)
            bidsVector.emplace_back(trEntry.second);

        // sort asks descending
        std::sort(asksVector.begin(), asksVector.end(),
                  [](const xbridge::TransactionDescrPtr &a, const xbridge::TransactionDescrPtr &b)
        {
            const auto priceA = util::price(a);
            const auto priceB = util::price(b);
            return priceA > priceB;
        });

        //sort bids descending
        std::sort(bidsVector.begin(), bidsVector.end(),
                  [](const xbridge::TransactionDescrPtr &a, const xbridge::TransactionDescrPtr &b)
        {
            const auto priceA = util::priceBid(a);
            const auto priceB = util::priceBid(b);
            return priceA > priceB;
        });

        // floating point comparisons
        // see Knuth 4.2.2 Eq 36
        auto floatCompare = [](const double a, const double b) -> bool
        {
            const auto epsilon = std::numeric_limits<double>::epsilon();
            return (fabs(a - b) / fabs(a) <= epsilon) && (fabs(a - b) / fabs(b) <= epsilon);
        };

        switch (detailLevel)
        {
        case 1:
        {
            //return only the best bid and ask
            if (!bidsList.empty()) {
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

                    const auto priceA = util::priceBid(tr1);
                    const auto priceB = util::priceBid(tr2);

                    return priceA < priceB;
                });

                const auto bidsCount = std::count_if(bidsList.begin(), bidsList.end(),
                                                     [bidsItem, floatCompare](const TransactionPair &a)
                {
                    const auto &tr = a.second;

                    if(tr == nullptr)
                        return false;

                    const auto price = util::priceBid(tr);

                    const auto &bestTr = bidsItem->second;
                    if (bestTr != nullptr)
                    {
                        const auto bestBidPrice = util::priceBid(bestTr);
                        return floatCompare(price, bestBidPrice);
                    }

                    return false;
                });

                const auto &tr = bidsItem->second;
                if (tr != nullptr)
                {
                    const auto bidPrice = util::priceBid(tr);
                    bids.emplace_back(Array{util::xBridgeStringValueFromPrice(bidPrice),
                                            util::xBridgeStringValueFromAmount(tr->toAmount),
                                            static_cast<int64_t>(bidsCount)});
                }
            }

            if (!asksList.empty()) {
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

                    const auto priceA = util::price(tr1);
                    const auto priceB = util::price(tr2);
                    return priceA < priceB;
                });

                const auto asksCount = std::count_if(asksList.begin(), asksList.end(),
                                                     [asksItem, floatCompare](const TransactionPair &a)
                {
                    const auto &tr = a.second;

                    if(tr == nullptr)
                        return false;

                    const auto price = util::price(tr);

                    const auto &bestTr = asksItem->second;
                    if (bestTr != nullptr)
                    {
                        const auto bestAskPrice = util::price(bestTr);
                        return floatCompare(price, bestAskPrice);
                    }

                    return false;
                });

                const auto &tr = asksItem->second;
                if (tr != nullptr)
                {
                    const auto askPrice = util::price(tr);
                    asks.emplace_back(Array{util::xBridgeStringValueFromPrice(askPrice),
                                            util::xBridgeStringValueFromAmount(tr->fromAmount),
                                            static_cast<int64_t>(asksCount)});
                }
            }

            res.emplace_back(Pair("asks", asks));
            res.emplace_back(Pair("bids", bids));
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
                const auto bidAmount    = bidsVector[i]->toAmount;
                const auto bidPrice     = util::priceBid(bidsVector[i]);
                auto bidSize            = bidAmount;
                const auto bidsCount    = std::count_if(bidsList.begin(), bidsList.end(),
                                                     [bidPrice, floatCompare](const TransactionPair &a)
                {
                    const auto &tr = a.second;

                    if(tr == nullptr)
                        return false;

                    const auto price = util::priceBid(tr);

                    return floatCompare(price, bidPrice);
                });
                //array sorted by bid price, we can to skip the transactions with equals bid price
                while((++i < bound) && floatCompare(util::priceBid(bidsVector[i]), bidPrice)) {
                    bidSize += bidsVector[i]->toAmount;
                }
                bid.emplace_back(util::xBridgeStringValueFromPrice(bidPrice));
                bid.emplace_back(util::xBridgeStringValueFromPrice(bidSize));
                bid.emplace_back(static_cast<int64_t>(bidsCount));
                bids.emplace_back(bid);
            }

            bound = std::min(maxOrders, asksVector.size());

            for (size_t i = 0; i < bound; i++)
            {
                if(asksVector[i] == nullptr)
                    continue;

                Array ask;
                //calculate asks and push to array
                const auto askAmount    = asksVector[i]->fromAmount;
                const auto askPrice     = util::price(asksVector[i]);
                auto askSize            = askAmount;
                const auto asksCount    = std::count_if(asksList.begin(), asksList.end(),
                                                     [askPrice, floatCompare](const TransactionPair &a)
                {
                    const auto &tr = a.second;

                    if(tr == nullptr)
                        return false;

                    const auto price = util::price(tr);

                    return floatCompare(price, askPrice);
                });

                //array sorted by price, we can to skip the transactions with equals price
                while((++i < bound) && floatCompare(util::price(asksVector[i]), askPrice)){
                    askSize += asksVector[i]->fromAmount;
                }
                ask.emplace_back(util::xBridgeStringValueFromPrice(askPrice));
                ask.emplace_back(util::xBridgeStringValueFromPrice(askSize));
                ask.emplace_back(static_cast<int64_t>(asksCount));
                asks.emplace_back(ask);
            }

            res.emplace_back(Pair("asks", asks));
            res.emplace_back(Pair("bids", bids));
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
                const auto bidAmount   = bidsVector[i]->toAmount;
                const auto bidPrice    = util::priceBid(bidsVector[i]);
                bid.emplace_back(util::xBridgeStringValueFromPrice(bidPrice));
                bid.emplace_back(util::xBridgeStringValueFromAmount(bidAmount));
                bid.emplace_back(bidsVector[i]->id.GetHex());

                bids.emplace_back(bid);
            }

            bound = std::min(maxOrders, asksVector.size());

            for (size_t i = 0; i < bound; i++)
            {
                if(asksVector[i] == nullptr)
                    continue;

                Array ask;
                const auto bidAmount    = asksVector[i]->fromAmount;
                const auto askPrice     = util::price(asksVector[i]);
                ask.emplace_back(util::xBridgeStringValueFromPrice(askPrice));
                ask.emplace_back(util::xBridgeStringValueFromAmount(bidAmount));
                ask.emplace_back(asksVector[i]->id.GetHex());

                asks.emplace_back(ask);
            }

            res.emplace_back(Pair("asks", asks));
            res.emplace_back(Pair("bids", bids));
            return  res;
        }
        case 4:
        {
            //return Only the best bid and ask
            if (!bidsList.empty()) {
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

                    const auto priceA = util::priceBid(tr1);
                    const auto priceB = util::priceBid(tr2);

                    return priceA < priceB;
                });

                const auto &tr = bidsItem->second;
                if (tr != nullptr)
                {
                    const auto bidPrice = util::priceBid(tr);
                    bids.emplace_back(util::xBridgeStringValueFromPrice(bidPrice));
                    bids.emplace_back(util::xBridgeStringValueFromAmount(tr->toAmount));

                    Array bidsIds;
                    bidsIds.emplace_back(tr->id.GetHex());

                    for(const TransactionPair &tp : bidsList)
                    {
                        const auto &otherTr = tp.second;

                        if(otherTr == nullptr)
                            continue;

                        if(tr->id == otherTr->id)
                            continue;

                        const auto otherTrBidPrice = util::priceBid(otherTr);

                        if(!floatCompare(bidPrice, otherTrBidPrice))
                            continue;

                        bidsIds.emplace_back(otherTr->id.GetHex());
                    }

                    bids.emplace_back(bidsIds);
                }
            }

            if (!asksList.empty()) {
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

                    const auto priceA = util::price(tr1);
                    const auto priceB = util::price(tr2);
                    return priceA < priceB;
                });

                const auto &tr = asksItem->second;
                if (tr != nullptr)
                {
                    const auto askPrice = util::price(tr);
                    asks.emplace_back(util::xBridgeStringValueFromPrice(askPrice));
                    asks.emplace_back(util::xBridgeStringValueFromAmount(tr->fromAmount));

                    Array asksIds;
                    asksIds.emplace_back(tr->id.GetHex());

                    for(const TransactionPair &tp : asksList)
                    {
                        const auto &otherTr = tp.second;

                        if(otherTr == nullptr)
                            continue;

                        if(tr->id == otherTr->id)
                            continue;

                        const auto otherTrAskPrice = util::price(otherTr);

                        if(!floatCompare(askPrice, otherTrAskPrice))
                            continue;

                        asksIds.emplace_back(otherTr->id.GetHex());
                    }

                    asks.emplace_back(asksIds);
                }
            }

            res.emplace_back(Pair("asks", asks));
            res.emplace_back(Pair("bids", bids));
            return res;
        }

        default:
            return util::makeError(xbridge::INVALID_DETAIL_LEVEL, __FUNCTION__);
        }
    }
}

//******************************************************************************
//******************************************************************************
json_spirit::Value dxGetMyOrders(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp) {

        throw runtime_error("dxGetMyOrders\nLists all orders owned by you.");

    }

    if (!params.empty()) {

        Object error;
        error.emplace_back(Pair("error",
                                            xbridge::xbridgeErrorText(xbridge::INVALID_PARAMETERS,
                                                                      "This function does not accept any parameters")));
        error.emplace_back(Pair("code",     xbridge::INVALID_PARAMETERS));
        error.emplace_back(Pair("name",     __FUNCTION__));
        return  error;

    }

    xbridge::App & xapp = xbridge::App::instance();

    Array r;
    TransactionVector orders;

    TransactionMap trList = xbridge::App::instance().transactions();

    // Filter local orders
    for (auto i : trList) {
        const xbridge::TransactionDescrPtr &t = i.second;
        if(!t->isLocal())
            continue;
        orders.push_back(t);
    }

    // Add historical orders
    TransactionMap history = xbridge::App::instance().history();

    // Filter local orders only
    for (auto &item : history) {
        const xbridge::TransactionDescrPtr &ptr = item.second;
        if (ptr->isLocal() &&
                (ptr->state == xbridge::TransactionDescr::trFinished ||
                 ptr->state == xbridge::TransactionDescr::trCancelled)) {
            orders.push_back(ptr);
        }
    }

    // Return if no records
    if (orders.empty())
        return r;

    // sort ascending by updated time
    std::sort(orders.begin(), orders.end(),
        [](const xbridge::TransactionDescrPtr &a,  const xbridge::TransactionDescrPtr &b) {
            return (a->txtime) < (b->txtime);
        });

    std::map<std::string, bool> seen;
    for (const auto &t : orders) {
        // do not process already seen orders
        if (seen.count(t->id.GetHex()))
            continue;
        seen[t->id.GetHex()] = true;

        xbridge::WalletConnectorPtr connFrom = xapp.connectorByCurrency(t->fromCurrency);
        xbridge::WalletConnectorPtr connTo   = xapp.connectorByCurrency(t->toCurrency);

        Object o;
        o.emplace_back(Pair("id", t->id.GetHex()));

        // maker data
        o.emplace_back(Pair("maker", t->fromCurrency));
        o.emplace_back(Pair("maker_size", util::xBridgeStringValueFromAmount(t->fromAmount)));
        o.emplace_back(Pair("maker_address", connFrom->fromXAddr(t->from)));
        // taker data
        o.emplace_back(Pair("taker", t->toCurrency));
        o.emplace_back(Pair("taker_size", util::xBridgeStringValueFromAmount(t->toAmount)));
        o.emplace_back(Pair("taker_address", connTo->fromXAddr(t->to)));
        // dates
        o.emplace_back(Pair("updated_at", util::iso8601(t->txtime)));
        o.emplace_back(Pair("created_at", util::iso8601(t->created)));
        o.emplace_back(Pair("status", t->strState()));

        r.emplace_back(o);
    }

    return r;
}

//******************************************************************************
//******************************************************************************
json_spirit::Value  dxGetTokenBalances(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp)
    {
        throw runtime_error("dxGetTokenBalances\n"
                            "List of connected wallet balances. These balances do not include orders that are using \n"
                            "locked utxos to support a pending or open order. The DX works best with presliced utxos \n"
                            "so that your entire wallet balance is capable of multiple simultaneous trades.");
    }

    if (params.size() != 0)
    {
        Object error;
        error.emplace_back(Pair("error",
                                            xbridge::xbridgeErrorText(xbridge::INVALID_PARAMETERS,
                                                                      "This function does not accept any parameters")));
        error.emplace_back(Pair("code",     xbridge::INVALID_PARAMETERS));
        error.emplace_back(Pair("name",     __FUNCTION__));
        return  error;
    }

    Object res;

    // Wallet balance
    double walletBalance = boost::numeric_cast<double>(pwalletMain->GetBalance()) / boost::numeric_cast<double>(COIN);
    res.emplace_back("Wallet", util::xBridgeStringValueFromPrice(walletBalance));

    // Add connected wallet balances
    const auto &connectors = xbridge::App::instance().connectors();
    for(const auto &connector : connectors)
    {
        const auto balance = connector->getWalletBalance();

        //ignore not connected wallets
        if(balance >= 0)
            res.emplace_back(connector->currency, util::xBridgeStringValueFromPrice(balance));
    }

    return res;
}

//******************************************************************************
//******************************************************************************
Value dxGetLockedUtxos(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp)
    {
        throw runtime_error("dxGetLockedUtxos (id)\n"
                            "Return list of locked utxo of an order.");
    }

    if (params.size() != 0 || params.size() != 1)
    {
        Object error;
        error.emplace_back(Pair("error",    xbridge::xbridgeErrorText(xbridge::INVALID_PARAMETERS, "requered transaction id or empty param")));
        error.emplace_back(Pair("code",     xbridge::INVALID_PARAMETERS));
        error.emplace_back(Pair("name",     __FUNCTION__));
        return error;
    }

    xbridge::Exchange & e = xbridge::Exchange::instance();
    if (!e.isStarted())
    {
        Object error;
        error.emplace_back(Pair("error",    xbridge::xbridgeErrorText(xbridge::Error::NOT_EXCHANGE_NODE)));
        error.emplace_back(Pair("code",     xbridge::Error::NOT_EXCHANGE_NODE));
        error.emplace_back(Pair("name",     __FUNCTION__));
        return error;
    }

    uint256 id;

    if(params.size() == 1)
        id = uint256(params[0].get_str());

    std::vector<xbridge::wallet::UtxoEntry> items;
    if(!e.getUtxoItems(id, items))
    {

        Object error;
        error.emplace_back(Pair("error",    xbridge::xbridgeErrorText(xbridge::Error::TRANSACTION_NOT_FOUND, id.GetHex())));
        error.emplace_back(Pair("code",     xbridge::Error::TRANSACTION_NOT_FOUND));
        error.emplace_back(Pair("name",     __FUNCTION__));
        return error;
    }

    Array utxo;

    for(const xbridge::wallet::UtxoEntry & entry : items)
        utxo.emplace_back(entry.toString());

    Object obj;
    if(id.IsNull())
    {
        obj.emplace_back(Pair("all_locked_utxo", utxo));

        return obj;
    }

    xbridge::TransactionPtr pendingTx = e.pendingTransaction(id);
    xbridge::TransactionPtr acceptedTx = e.transaction(id);

    if (!pendingTx->isValid() && !acceptedTx->isValid())
    {
        Object error;
        error.emplace_back(Pair("error",    xbridge::xbridgeErrorText(xbridge::Error::TRANSACTION_NOT_FOUND, id.GetHex())));
        error.emplace_back(Pair("code",     xbridge::Error::TRANSACTION_NOT_FOUND));
        error.emplace_back(Pair("name",     __FUNCTION__));
        return error;
    }

    obj.emplace_back(Pair("id", id.GetHex()));

    if(pendingTx->isValid())
        obj.emplace_back(Pair(pendingTx->a_currency(), utxo));
    else if(acceptedTx->isValid())
        obj.emplace_back(Pair(acceptedTx->a_currency() + "_and_" + acceptedTx->b_currency(), utxo));

    return obj;
}
