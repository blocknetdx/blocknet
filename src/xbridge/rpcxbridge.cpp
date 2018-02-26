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
#include "rpcserver.h"

using namespace json_spirit;
using namespace std;
using namespace boost;
using namespace boost::asio;

using TransactionMap    = std::map<uint256, xbridge::TransactionDescrPtr>;

using TransactionPair   = std::pair<uint256, xbridge::TransactionDescrPtr>;

using RealVector        = std::vector<double>;

using TransactionVector = std::vector<xbridge::TransactionDescrPtr>;
namespace bpt           = boost::posix_time;


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
        jtr.emplace_back(Pair("maker_size",     util::xBridgeValueFromAmount(tr->fromAmount)));
        jtr.emplace_back(Pair("taker",          tr->toCurrency));
        jtr.emplace_back(Pair("taker_size",     util::xBridgeValueFromAmount(tr->toAmount)));
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

        throw runtime_error("dxGetOrderFills Returns all the recent trades by trade pair that have been filled \n"
                            "(i.e. completed). Maker symbol is always listed first. The [combined] flag set to false\n "
                            "will return only maker trades, switch maker and taker to get the reverse.\n"
                            "(maker) (taker) (combined - optional)"
                            );

    }

    bool invalidParams = ((params.size() != 2) &&
                          (params.size() != 3));
    if (invalidParams) {

        Object error;
        const auto statusCode = xbridge::INVALID_PARAMETERS;
        error.emplace_back(Pair("error",
                                xbridge::xbridgeErrorText(statusCode)));
        error.emplace_back(Pair("code", statusCode));
        error.emplace_back(Pair("name", __FUNCTION__ ));
        return  error;

    }

    bool combined = params.size() == 3 && params[2].get_bool();

    const auto maker = params[0].get_str();
    const auto taker = params[1].get_str();



    TransactionMap history = xbridge::App::instance().history();



    TransactionVector result;

    boost::push_back(result,
                     history | boost::adaptors::map_values
                     | boost::adaptors::filtered(
                         [&maker, &taker, combined](const xbridge::TransactionDescrPtr &ptr) -> bool {

        return (ptr->state == xbridge::TransactionDescr::trFinished) &&

                (combined ? (ptr->fromCurrency == maker && ptr->toCurrency == taker) :
                            (ptr->fromCurrency == maker));

    }));

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
        tmp.emplace_back(Pair("maker_size", util::xBridgeValueFromAmount(transaction->fromAmount)));
        tmp.emplace_back(Pair("taker",      transaction->toCurrency));
        tmp.emplace_back(Pair("taker_size", util::xBridgeValueFromAmount(transaction->toAmount)));
        arr.emplace_back(tmp);

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
    for (const auto &trEntry : trList) {

        const auto &tr          = trEntry.second;
        const auto fromAmount   = util::xBridgeValueFromAmount(tr->fromAmount);
        const auto toAmount     = util::xBridgeValueFromAmount(tr->toAmount);
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
    opens.emplace_back(util::xBridgeValueFromAmount(trVector[0]->fromAmount));
    opens.emplace_back(util::xBridgeValueFromAmount(trVector[0]->toAmount));
    res.emplace_back(Pair("o", opens));

    Array close;
    //write end price
    close.emplace_back(util::xBridgeValueFromAmount(trVector[trVector.size() - 1]->fromAmount));
    close.emplace_back(util::xBridgeValueFromAmount(trVector[trVector.size() - 1]->toAmount));
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
        res.emplace_back(Pair("txids", tmp));
    }

    //write status
    res.emplace_back(Pair("s", "ok"));
    arr.emplace_back(res);

    return arr;
}

//*****************************************************************************
//*****************************************************************************

Value dxGetOrder(const Array & params, bool fHelp)
{
    if (fHelp) {

         throw runtime_error("dxGetOrder (id) Get order info by id.	.");

    }
    if (params.size() != 1) {

        Object error;
        const auto statusCode = xbridge::INVALID_PARAMETERS;
        error.emplace_back(Pair("error", xbridge::xbridgeErrorText(statusCode)));
        error.emplace_back(Pair("code",  statusCode));
        error.emplace_back(Pair("name",  __FUNCTION__));
        return  error;

    }

    uint256 id(params[0].get_str());    

    auto &xapp = xbridge::App::instance();

    const xbridge::TransactionDescrPtr order = xapp.transaction(uint256(id));

    if(order == nullptr) {

        Object error;
        const auto statusCode = xbridge::Error::TRANSACTION_NOT_FOUND;
        error.emplace_back(Pair("error", xbridge::xbridgeErrorText(statusCode)));
        error.emplace_back(Pair("code",  statusCode));
        error.emplace_back(Pair("name",  __FUNCTION__));
        return  error;

    }

    xbridge::WalletConnectorPtr connFrom = xapp.connectorByCurrency(order->fromCurrency);
    xbridge::WalletConnectorPtr connTo   = xapp.connectorByCurrency(order->toCurrency);
    if(!connFrom) {

        Object error;
        auto statusCode = xbridge::Error::NO_SESSION;
        error.emplace_back(Pair("error", xbridge::xbridgeErrorText(statusCode, order->fromCurrency)));
        error.emplace_back(Pair("code",  statusCode));
        error.emplace_back(Pair("name",  __FUNCTION__));
        return  error;

    }
    if (!connTo) {

        Object error;
        auto statusCode = xbridge::Error::NO_SESSION;
        error.emplace_back(Pair("error", xbridge::xbridgeErrorText(statusCode, order->toCurrency)));
        error.emplace_back(Pair("code",  statusCode));
        error.emplace_back(Pair("name",  __FUNCTION__));
        return  error;

    }

    Object result;
    result.emplace_back(Pair("id",          order->id.GetHex()));
    result.emplace_back(Pair("maker",       order->fromCurrency));
    result.emplace_back(Pair("maker_size",  util::xBridgeValueFromAmount(order->fromAmount)));
    result.emplace_back(Pair("taker",       order->toCurrency));
    result.emplace_back(Pair("taker_size",  util::xBridgeValueFromAmount(order->toAmount)));
    result.emplace_back(Pair("updated_at",  util::iso8601(order->txtime)));
    result.emplace_back(Pair("created_at",  util::iso8601(order->created)));
    result.emplace_back(Pair("status",      order->strState()));
    return result;
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
                            "(maker) (maker size) (maker address) "
                            "(taker) (taker size) (taker address) (type) (dryrun)[optional]\n"
                            "Create a new order. dryrun will validate the order without submitting the order to the network.");

    }
    if (params.size() < 7) {

        Object error;
        error.emplace_back(Pair("error", xbridge::xbridgeErrorText(xbridge::INVALID_PARAMETERS)));
        error.emplace_back(Pair("code", xbridge::INVALID_PARAMETERS));
        error.emplace_back(Pair("name", "dxMakeOrder"));

        return error;
    }


    std::string fromAddress     = params[2].get_str();
    std::string fromCurrency    = params[0].get_str();
    double      fromAmount      = params[1].get_real();
    std::string toAddress       = params[5].get_str();
    std::string toCurrency      = params[3].get_str();
    double      toAmount        = params[4].get_real();
    std::string type            = params[6].get_str();

    // Validate the order type
    if (type != "exact") {
        Object error;
        error.emplace_back(Pair("error", xbridge::xbridgeErrorText(xbridge::INVALID_PARAMETERS, "Only the exact type is supported at this time.")));
        error.emplace_back(Pair("code", xbridge::INVALID_PARAMETERS));
        error.emplace_back(Pair("name", "dxMakeOrder"));
        return error;
    }

    auto statusCode = xbridge::SUCCESS;

    xbridge::App &app = xbridge::App::instance();
    if (!app.isValidAddress(fromAddress)) {
        Object error;
        error.emplace_back(Pair("error", xbridge::xbridgeErrorText(xbridge::INVALID_ADDRESS)));
        error.emplace_back(Pair("code", xbridge::INVALID_ADDRESS));
        error.emplace_back(Pair("name", "dxMakeOrder"));

        return error;
    }
    if (!app.isValidAddress(toAddress)) {
        Object error;
        error.emplace_back(Pair("error", xbridge::xbridgeErrorText(xbridge::INVALID_ADDRESS)));
        error.emplace_back(Pair("code", xbridge::INVALID_ADDRESS));
        error.emplace_back(Pair("name", "dxMakeOrder"));

        return error;
    }
    // Perform explicit check on dryrun to avoid executing order on bad spelling
    bool dryrun = false;
    if (params.size() == 8) {
        std::string dryrunParam = params[7].get_str();
        if (dryrunParam != "dryrun") {
            Object error;
            error.emplace_back(Pair("error", xbridge::xbridgeErrorText(xbridge::INVALID_PARAMETERS, dryrunParam)));
            error.emplace_back(Pair("code", xbridge::INVALID_PARAMETERS));
            error.emplace_back(Pair("name", "dxMakeOrder"));
            return error;
        }
        dryrun = true;
    }


    Object result;
    statusCode = app.checkCreateParams(fromCurrency, toCurrency,
                                       util::xBridgeAmountFromReal(fromAmount));
    switch (statusCode) {
    case xbridge::SUCCESS:{
        // If dryrun
        if (dryrun) {
            result.emplace_back(Pair("id", uint256().GetHex()));
            result.emplace_back(Pair("maker", fromCurrency));
            result.emplace_back(Pair("maker_size", util::xBridgeValueFromAmount(util::xBridgeAmountFromReal(fromAmount))));
            result.emplace_back(Pair("maker_address", fromAddress));
            result.emplace_back(Pair("taker", toCurrency));
            result.emplace_back(Pair("taker_size", util::xBridgeValueFromAmount(util::xBridgeAmountFromReal(toAmount))));
            result.emplace_back(Pair("taker_address", toAddress));
            result.emplace_back(Pair("status", "created"));
            return result;
        }
        break;
    }

    case xbridge::INVALID_CURRENCY: {

        result.emplace_back(Pair("error", xbridge::xbridgeErrorText(statusCode, fromCurrency)));
        result.emplace_back(Pair("code", statusCode));
        result.emplace_back(Pair("name", "dxMakeOrder"));

        return result;

    }
    case xbridge::NO_SESSION:{

        result.emplace_back(Pair("error", xbridge::xbridgeErrorText(statusCode, fromCurrency)));
        result.emplace_back(Pair("code", statusCode));
        result.emplace_back(Pair("name", "dxMakeOrder"));

        return result;

    }
    case xbridge::INSIFFICIENT_FUNDS:{

        result.emplace_back(Pair("error", xbridge::xbridgeErrorText(statusCode, toAddress)));
        result.emplace_back(Pair("code", statusCode));
        result.emplace_back(Pair("name", "dxMakeOrder"));

        return result;
    }

    default:
        result.emplace_back(Pair("error", xbridge::xbridgeErrorText(statusCode)));
        result.emplace_back(Pair("code", statusCode));
        result.emplace_back(Pair("name", "dxMakeOrder"));

        return result;
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
        obj.emplace_back(Pair("maker_size",     util::xBridgeValueFromAmount(util::xBridgeAmountFromReal(fromAmount))));
        obj.emplace_back(Pair("taker_address",  toAddress));
        obj.emplace_back(Pair("taker",          toCurrency));
        obj.emplace_back(Pair("taker_size",     util::xBridgeValueFromAmount(util::xBridgeAmountFromReal(toAmount))));
        const auto &createdTime = xbridge::App::instance().transaction(id)->created;
        obj.emplace_back(Pair("created_at",     util::iso8601(createdTime)));
        obj.emplace_back(Pair("updated_at",     util::iso8601(bpt::second_clock::universal_time())));
        obj.emplace_back(Pair("block_id",       blockHash.GetHex()));
        obj.emplace_back(Pair("status",         "created"));
        return obj;

    } else {

        Object error;
        error.emplace_back(Pair("error", xbridge::xbridgeErrorText(statusCode)));
        error.emplace_back(Pair("code", statusCode));
        error.emplace_back(Pair("name", "dxMakeOrder"));
        
        return error;

    }
}

//******************************************************************************
//******************************************************************************
Value dxTakeOrder(const Array & params, bool fHelp)
{

    if (fHelp)
    {
        throw runtime_error("dxTakeOrder (id) "
                            "(address from) (address to) [optional](dryrun)\n"
                            "Accepts the order. dryrun will evaluate input without accepting the order.");
    }

    auto statusCode = xbridge::SUCCESS;

    if ((params.size() != 3) && (params.size() != 4))
    {
        Object error;
        error.emplace_back(Pair("error", xbridge::xbridgeErrorText(xbridge::INVALID_PARAMETERS)));
        error.emplace_back(Pair("code", xbridge::INVALID_PARAMETERS));
        error.emplace_back(Pair("name", "dxTakeOrder"));
        return  error;
    }

    uint256 id(params[0].get_str());
    std::string fromAddress    = params[1].get_str();
    std::string toAddress      = params[2].get_str();

    xbridge::App &app = xbridge::App::instance();

    if (!app.isValidAddress(fromAddress))
    {
        Object error;
        error.emplace_back(Pair("error", xbridge::xbridgeErrorText(xbridge::INVALID_ADDRESS, fromAddress)));
        error.emplace_back(Pair("code", xbridge::INVALID_ADDRESS));
        error.emplace_back(Pair("name", "dxTakeOrder"));
        return  error;
    }

    if (!app.isValidAddress(toAddress))
    {
        Object error;
        error.emplace_back(Pair("error", xbridge::xbridgeErrorText(xbridge::INVALID_ADDRESS, toAddress)));
        error.emplace_back(Pair("code", xbridge::INVALID_ADDRESS));
        error.emplace_back(Pair("name", "dxTakeOrder"));
        return  error;
    }

    // Perform explicit check on dryrun to avoid executing order on bad spelling
    bool dryrun = false;
    if (params.size() == 4) {
        std::string dryrunParam = params[3].get_str();
        if (dryrunParam != "dryrun") {
            Object error;
            error.emplace_back(Pair("error", xbridge::xbridgeErrorText(xbridge::INVALID_PARAMETERS, dryrunParam)));
            error.emplace_back(Pair("code", xbridge::INVALID_PARAMETERS));
            error.emplace_back(Pair("name", "dxTakeOrder"));
            return error;
        }
        dryrun = true;
    }

    Object result;
    xbridge::TransactionDescrPtr txDescr;
    statusCode = app.checkAcceptParams(id, txDescr);

    switch (statusCode)
    {
    case xbridge::SUCCESS: {
        if(dryrun)
        {
            result.emplace_back(Pair("id", uint256().GetHex()));

            result.emplace_back(Pair("maker", txDescr->fromCurrency));
            result.emplace_back(Pair("maker_size", util::xBridgeValueFromAmount(txDescr->fromAmount)));

            result.emplace_back(Pair("taker", txDescr->toCurrency));
            result.emplace_back(Pair("taker_size", util::xBridgeValueFromAmount(txDescr->toAmount)));

            result.emplace_back(Pair("updated_at", util::iso8601(bpt::second_clock::universal_time())));
            result.emplace_back(Pair("created_at", util::iso8601(txDescr->created)));

            result.emplace_back(Pair("status", "filled"));
            return result;
        }

        break;
    }
    case xbridge::TRANSACTION_NOT_FOUND:
    {
        result.emplace_back(Pair("error", xbridge::xbridgeErrorText(statusCode, id.GetHex())));
        result.emplace_back(Pair("code", statusCode));
        result.emplace_back(Pair("name", "dxTakeOrder"));
        return result;
    }

    case xbridge::NO_SESSION:
    {
        result.emplace_back(Pair("error", xbridge::xbridgeErrorText(statusCode, txDescr->toCurrency)));
        result.emplace_back(Pair("code", statusCode));
        result.emplace_back(Pair("name", "dxTakeOrder"));
        return result;
    }

    case xbridge::INSIFFICIENT_FUNDS:
    {
        result.emplace_back(Pair("error", xbridge::xbridgeErrorText(statusCode, txDescr->to)));
        result.emplace_back(Pair("code", statusCode));
        result.emplace_back(Pair("name", "dxTakeOrder"));
        return result;
    }

    default:
    {
        result.emplace_back(Pair("error", xbridge::xbridgeErrorText(statusCode)));
        result.emplace_back(Pair("code", statusCode));
        result.emplace_back(Pair("name", "dxTakeOrder"));
        return result;
    }
    }

    std::swap(txDescr->fromCurrency, txDescr->toCurrency);
    std::swap(txDescr->fromAmount, txDescr->toAmount);

    statusCode = app.acceptXBridgeTransaction(id, fromAddress, toAddress);
    if (statusCode == xbridge::SUCCESS)
    {
        result.emplace_back(Pair("id", id.GetHex()));

        result.emplace_back(Pair("maker", txDescr->fromCurrency));
        result.emplace_back(Pair("maker_size", util::xBridgeValueFromAmount(txDescr->fromAmount)));

        result.emplace_back(Pair("taker", txDescr->toCurrency));
        result.emplace_back(Pair("taker_size", util::xBridgeValueFromAmount(txDescr->toAmount)));

        result.emplace_back(Pair("updated_at", util::iso8601(bpt::second_clock::universal_time())));
        result.emplace_back(Pair("created_at", util::iso8601(txDescr->created)));

        result.emplace_back(Pair("status", txDescr->strState()));
        return result;
    }
    else
    {
        result.emplace_back(Pair("error", xbridge::xbridgeErrorText(statusCode)));
        result.emplace_back(Pair("code", statusCode));
        result.emplace_back(Pair("name", "dxTakeOrder"));
        return result;
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
        Object error;
        error.emplace_back(Pair("error", xbridge::xbridgeErrorText(xbridge::INVALID_PARAMETERS)));
        error.emplace_back(Pair("code", xbridge::INVALID_PARAMETERS));
        error.emplace_back(Pair("name", "dxCancelOrder"));
        return error;
    }

    LOG() << "rpc cancel order " << __FUNCTION__;
    uint256 id(params[0].get_str());

    xbridge::TransactionDescrPtr tx = xbridge::App::instance().transaction(id);
    if (!tx)
    {
        Object obj;
        obj.emplace_back(Pair("error", xbridge::xbridgeErrorText(xbridge::Error::TRANSACTION_NOT_FOUND, id.GetHex())));
        obj.emplace_back(Pair("code", xbridge::Error::TRANSACTION_NOT_FOUND));
        obj.emplace_back(Pair("name", "dxCancelOrder"));
        return obj;
    }

    if (tx->state >= xbridge::TransactionDescr::trCreated)
    {
        Object error;
        error.emplace_back(Pair("error", xbridge::xbridgeErrorText(xbridge::INVALID_STATE)));
        error.emplace_back(Pair("code", xbridge::INVALID_STATE));
        error.emplace_back(Pair("name", "dxCancelOrder"));
        return error;
    }

    const auto res = xbridge::App::instance().cancelXBridgeTransaction(id, crRpcRequest);
    if (res != xbridge::SUCCESS)
    {
        Object obj;
        obj.emplace_back(Pair("error", xbridge::xbridgeErrorText(res)));
        obj.emplace_back(Pair("code", res));
        obj.emplace_back(Pair("name", "dxCancelOrder"));
        return obj;
    }

    xbridge::WalletConnectorPtr connFrom = xbridge::App::instance().connectorByCurrency(tx->fromCurrency);
    xbridge::WalletConnectorPtr connTo   = xbridge::App::instance().connectorByCurrency(tx->toCurrency);
    if (!connFrom || !connTo)
    {
        Object error;
        error.emplace_back(Pair("error", xbridge::xbridgeErrorText(xbridge::NO_SESSION)));
        error.emplace_back(Pair("code", xbridge::NO_SESSION));
        error.emplace_back(Pair("name", "dxCancelOrder"));
        return error;
    }

    Object obj;
    obj.emplace_back(Pair("id", id.GetHex()));

    obj.emplace_back(Pair("maker", tx->fromCurrency));
    obj.emplace_back(Pair("maker_size", util::xBridgeValueFromAmount(tx->fromAmount)));
    obj.emplace_back(Pair("maker_address", connFrom->fromXAddr(tx->from)));

    obj.emplace_back(Pair("taker", tx->toCurrency));
    obj.emplace_back(Pair("taker_size", util::xBridgeValueFromAmount(tx->toAmount)));
    obj.emplace_back(Pair("taker_address", connTo->fromXAddr(tx->to)));

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
            const auto priceA = util::xBridgeValueFromAmount(a->fromAmount) / util::xBridgeValueFromAmount(a->toAmount);
            const auto priceB = util::xBridgeValueFromAmount(b->fromAmount) / util::xBridgeValueFromAmount(b->toAmount);
            return priceA > priceB;
        });

        std::sort(asksVector.begin(), asksVector.end(),
                  [](const xbridge::TransactionDescrPtr &a, const xbridge::TransactionDescrPtr &b)
        {
            const auto priceA = util::xBridgeValueFromAmount(a->fromAmount) / util::xBridgeValueFromAmount(a->toAmount);
            const auto priceB = util::xBridgeValueFromAmount(b->fromAmount) / util::xBridgeValueFromAmount(b->toAmount);
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

                const auto priceA = util::xBridgeValueFromAmount(tr1->fromAmount) / util::xBridgeValueFromAmount(tr1->toAmount);
                const auto priceB = util::xBridgeValueFromAmount(tr2->fromAmount) / util::xBridgeValueFromAmount(tr2->toAmount);

                return priceA < priceB;
            });

            const auto bidsCount = std::count_if(bidsList.begin(), bidsList.end(), [bidsItem, floatCompare](const TransactionPair &a)
            {
                const auto &tr = a.second;

                if(tr == nullptr)
                    return false;

                const auto price = util::xBridgeValueFromAmount(tr->fromAmount) / util::xBridgeValueFromAmount(tr->toAmount);

                const auto &bestTr = bidsItem->second;
                if (bestTr != nullptr)
                {
                    const auto bestBidPrice = util::xBridgeValueFromAmount(bestTr->fromAmount) / util::xBridgeValueFromAmount(bestTr->toAmount);
                    return floatCompare(price, bestBidPrice);
                }

                return false;
            });

            {
                const auto &tr = bidsItem->second;
                if (tr != nullptr)
                {
                    const auto bidPrice = util::xBridgeValueFromAmount(tr->fromAmount) / util::xBridgeValueFromAmount(tr->toAmount);
                    bids.emplace_back(Array{bidPrice, util::xBridgeValueFromAmount(tr->fromAmount), (int64_t)bidsCount});
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

                const auto priceA = util::xBridgeValueFromAmount(tr1->fromAmount) / util::xBridgeValueFromAmount(tr1->toAmount);
                const auto priceB = util::xBridgeValueFromAmount(tr2->fromAmount) / util::xBridgeValueFromAmount(tr2->toAmount);
                return priceA < priceB;
            });

            const auto asksCount = std::count_if(asksList.begin(), asksList.end(), [asksItem, floatCompare](const TransactionPair &a)
            {
                const auto &tr = a.second;

                if(tr == nullptr)
                    return false;

                const auto price = util::xBridgeValueFromAmount(tr->fromAmount) / util::xBridgeValueFromAmount(tr->toAmount);

                const auto &bestTr = asksItem->second;
                if (bestTr != nullptr)
                {
                    const auto bestAskPrice = util::xBridgeValueFromAmount(bestTr->fromAmount) / util::xBridgeValueFromAmount(bestTr->toAmount);
                    return floatCompare(price, bestAskPrice);
                }

                return false;
            });

            {
                const auto &tr = asksItem->second;
                if (tr != nullptr)
                {
                    const auto askPrice = util::xBridgeValueFromAmount(tr->fromAmount) / util::xBridgeValueFromAmount(tr->toAmount);
                    asks.emplace_back(Array{askPrice, util::xBridgeValueFromAmount(tr->fromAmount), static_cast<int64_t>(asksCount)});
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
                const auto bidPrice     = util::xBridgeValueFromAmount(fromAmount) / util::xBridgeValueFromAmount(toAmount);
                const auto bidSize      = util::xBridgeValueFromAmount(fromAmount);
                const auto bidsCount    = std::count_if(bidsList.begin(), bidsList.end(),
                                                     [bidPrice, floatCompare](const TransactionPair &a)
                {
                    const auto &tr = a.second;

                    if(tr == nullptr)
                        return false;

                    const auto price = util::xBridgeValueFromAmount(tr->fromAmount) / util::xBridgeValueFromAmount(tr->toAmount);

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
                const auto askPrice     = util::xBridgeValueFromAmount(fromAmount) / util::xBridgeValueFromAmount(toAmount);
                const auto askSize      = util::xBridgeValueFromAmount(fromAmount);
                const auto asksCount    = std::count_if(asksList.begin(), asksList.end(),
                                                     [askPrice, floatCompare](const TransactionPair &a)
                {
                    const auto &tr = a.second;

                    if(tr == nullptr)
                        return false;

                    const auto price = util::xBridgeValueFromAmount(tr->fromAmount) / util::xBridgeValueFromAmount(tr->toAmount);

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
                const auto bidPrice = util::xBridgeValueFromAmount(fromAmount) / util::xBridgeValueFromAmount(toAmount);
                bid.emplace_back(bidPrice);
                bid.emplace_back(util::xBridgeValueFromAmount(fromAmount));
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
                const auto askPrice = util::xBridgeValueFromAmount(fromAmount) / util::xBridgeValueFromAmount(toAmount);
                ask.emplace_back(askPrice);
                ask.emplace_back(util::xBridgeValueFromAmount(fromAmount));
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

                const auto priceA = util::xBridgeValueFromAmount(tr1->fromAmount) / util::xBridgeValueFromAmount(tr1->toAmount);
                const auto priceB = util::xBridgeValueFromAmount(tr2->fromAmount) / util::xBridgeValueFromAmount(tr2->toAmount);

                return priceA < priceB;
            });

            {
                const auto &tr = bidsItem->second;
                if (tr != nullptr)
                {
                    const auto bidPrice = util::xBridgeValueFromAmount(tr->fromAmount) / util::xBridgeValueFromAmount(tr->toAmount);
                    bids.emplace_back(bidPrice);
                    bids.emplace_back(util::xBridgeValueFromAmount(tr->fromAmount));

                    Array bidsIds;
                    bidsIds.emplace_back(tr->id.GetHex());

                    for(const TransactionPair &tp : bidsList)
                    {
                        const auto &otherTr = tp.second;

                        if(otherTr == nullptr)
                            continue;

                        if(tr->id == otherTr->id)
                            continue;

                        const auto otherTrBidPrice = util::xBridgeValueFromAmount(otherTr->fromAmount) / util::xBridgeValueFromAmount(otherTr->toAmount);

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

                const auto priceA = util::xBridgeValueFromAmount(tr1->fromAmount) / util::xBridgeValueFromAmount(tr1->toAmount);
                const auto priceB = util::xBridgeValueFromAmount(tr2->fromAmount) / util::xBridgeValueFromAmount(tr2->toAmount);
                return priceA < priceB;
            });

            {
                const auto &tr = asksItem->second;
                if (tr != nullptr)
                {
                    const auto askPrice = util::xBridgeValueFromAmount(tr->fromAmount) / util::xBridgeValueFromAmount(tr->toAmount);
                    asks.emplace_back(askPrice);
                    asks.emplace_back(util::xBridgeValueFromAmount(tr->fromAmount));

                    Array asksIds;
                    asksIds.emplace_back(tr->id.GetHex());

                    for(const TransactionPair &tp : asksList)
                    {
                        const auto &otherTr = tp.second;

                        if(otherTr == nullptr)
                            continue;

                        if(tr->id == otherTr->id)
                            continue;

                        const auto otherTrAskPrice = util::xBridgeValueFromAmount(otherTr->fromAmount) / util::xBridgeValueFromAmount(otherTr->toAmount);

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
