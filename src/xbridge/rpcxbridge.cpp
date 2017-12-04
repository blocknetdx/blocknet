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
#include <stdio.h>
#include <atomic>

#include "util/settings.h"
#include "util/logger.h"
#include "xbridgeapp.h"
#include "xbridgeexchange.h"
#include "xbridgetransaction.h"
#include "rpcserver.h"

using namespace json_spirit;
using namespace std;
using namespace boost;
using namespace boost::asio;

//*****************************************************************************
//*****************************************************************************

Value dxGetTransactionList(const Array & params, bool fHelp)
{
    if (fHelp || params.size() > 1)
    {
        throw runtime_error("dxGetTransactionList\nList transactions.");
    }

    Array arr;

    xbridge::App & xapp = xbridge::App::instance();

    boost::mutex::scoped_lock l(xapp.m_txLocker);


    // pending tx
    {
        std::map<uint256, xbridge::TransactionDescrPtr> trlist = xbridge::App::m_pendingTransactions;
        for (const auto & trEntry : trlist)
        {
            const auto & tr = trEntry.second;

            xbridge::WalletConnectorPtr connFrom = xapp.connectorByCurrency(tr->fromCurrency);
            xbridge::WalletConnectorPtr connTo   = xapp.connectorByCurrency(tr->toCurrency);
            if (!connFrom || !connTo)
            {
                continue;
            }

            Object jtr;
            jtr.push_back(Pair("id", tr->id.GetHex()));
            jtr.push_back(Pair("from", tr->fromCurrency));
            jtr.push_back(Pair("from address", connFrom->fromXAddr(tr->from)));
            double fromAmount = static_cast<double>(tr->fromAmount) / xbridge::TransactionDescr::COIN;
            jtr.push_back(Pair("fromAmount", boost::lexical_cast<std::string>(fromAmount)));
            jtr.push_back(Pair("to", tr->toCurrency));
            jtr.push_back(Pair("to address", connTo->fromXAddr(tr->to)));
            double toAmount = static_cast<double>(tr->toAmount) / xbridge::TransactionDescr::COIN;
            jtr.push_back(Pair("toAmount", boost::lexical_cast<std::string>(toAmount)));
            jtr.push_back(Pair("state", tr->strState()));

            arr.push_back(jtr);
        }
    }

    // active tx
    {
        std::map<uint256, xbridge::TransactionDescrPtr> trlist = xbridge::App::m_transactions;
        for (const auto & trEntry : trlist)
        {
            const auto & tr = trEntry.second;

            xbridge::WalletConnectorPtr connFrom = xapp.connectorByCurrency(tr->fromCurrency);
            xbridge::WalletConnectorPtr connTo   = xapp.connectorByCurrency(tr->toCurrency);
            if (!connFrom || !connTo)
            {
                continue;
            }

            Object jtr;
            jtr.push_back(Pair("id", tr->id.GetHex()));
            jtr.push_back(Pair("from", tr->fromCurrency));
            jtr.push_back(Pair("from address", connFrom->fromXAddr(tr->from)));
            double fromAmount = static_cast<double>(tr->fromAmount) / xbridge::TransactionDescr::COIN;
            jtr.push_back(Pair("fromAmount", boost::lexical_cast<std::string>(fromAmount)));
            jtr.push_back(Pair("to", tr->toCurrency));
            jtr.push_back(Pair("to address", connTo->fromXAddr(tr->to)));
            double toAmount = static_cast<double>(tr->toAmount) / xbridge::TransactionDescr::COIN;
            jtr.push_back(Pair("toAmount", boost::lexical_cast<std::string>(toAmount)));
            jtr.push_back(Pair("state", tr->strState()));

            arr.push_back(jtr);
        }
    }

    return arr;
}

//*****************************************************************************
//*****************************************************************************

Value dxGetTransactionsHistoryList(const Array & params, bool fHelp)
{
    if (fHelp || params.size() > 1)
    {
        throw runtime_error("dxGetTransactionsHistoryList\nHistoric list transactions.");
    }

    Array arr;

    xbridge::App & xapp = xbridge::App::instance();

    boost::mutex::scoped_lock l(xbridge::App::m_txLocker);

    {
        std::map<uint256, xbridge::TransactionDescrPtr> trlist = xbridge::App::m_historicTransactions;
        for (const auto & trEntry : trlist)
        {
            const auto & tr = trEntry.second;

            xbridge::WalletConnectorPtr connFrom = xapp.connectorByCurrency(tr->fromCurrency);
            xbridge::WalletConnectorPtr connTo   = xapp.connectorByCurrency(tr->toCurrency);
            if (!connFrom || !connTo)
            {
                continue;
            }

            Object jtr;
            jtr.push_back(Pair("id", tr->id.GetHex()));
            jtr.push_back(Pair("from", tr->fromCurrency));
            jtr.push_back(Pair("from address", connFrom->fromXAddr(tr->from)));
            double fromAmount = static_cast<double>(tr->fromAmount) / xbridge::TransactionDescr::COIN;
            jtr.push_back(Pair("fromAmount", boost::lexical_cast<std::string>(fromAmount)));
            jtr.push_back(Pair("to", tr->toCurrency));
            jtr.push_back(Pair("to address", connTo->fromXAddr(tr->to)));
            double toAmount = static_cast<double>(tr->toAmount) / xbridge::TransactionDescr::COIN;
            jtr.push_back(Pair("toAmount", boost::lexical_cast<std::string>(toAmount)));
            jtr.push_back(Pair("state", tr->strState()));

            arr.push_back(jtr);
        }
    }

    return arr;
}

//*****************************************************************************
//*****************************************************************************

Value dxGetTransactionInfo(const Array & params, bool fHelp)
{
    if (fHelp || params.size() > 1)
    {
        throw runtime_error("dxGetTransactionInfo (id)\nTransaction info.");
    }

    std::string id = params[0].get_str();

    Array arr;

    xbridge::App & xapp = xbridge::App::instance();

    boost::mutex::scoped_lock l(xbridge::App::m_txLocker);

    // pending tx
    {
        std::map<uint256, xbridge::TransactionDescrPtr> trlist = xbridge::App::m_pendingTransactions;
        for (const auto & trEntry : trlist)
        {
            const auto & tr = trEntry.second;

            if(id != tr->id.GetHex())
            {
                continue;
            }

            xbridge::WalletConnectorPtr connFrom = xapp.connectorByCurrency(tr->fromCurrency);
            xbridge::WalletConnectorPtr connTo   = xapp.connectorByCurrency(tr->toCurrency);
            if (!connFrom || !connTo)
            {
                continue;
            }

            Object jtr;
            jtr.push_back(Pair("id", tr->id.GetHex()));
            jtr.push_back(Pair("from", tr->fromCurrency));
            jtr.push_back(Pair("from address", connFrom->fromXAddr(tr->from)));
            double fromAmount = static_cast<double>(tr->fromAmount) / xbridge::TransactionDescr::COIN;
            jtr.push_back(Pair("fromAmount", boost::lexical_cast<std::string>(fromAmount)));
            jtr.push_back(Pair("to", tr->toCurrency));
            jtr.push_back(Pair("to address", connTo->fromXAddr(tr->to)));
            double toAmount = static_cast<double>(tr->toAmount) / xbridge::TransactionDescr::COIN;
            jtr.push_back(Pair("toAmount", boost::lexical_cast<std::string>(toAmount)));
            jtr.push_back(Pair("state", tr->strState()));

            arr.push_back(jtr);
        }
    }

    // active tx
    {
        std::map<uint256, xbridge::TransactionDescrPtr> trlist = xbridge::App::m_transactions;
        for (const auto & trEntry : trlist)
        {
            const auto & tr = trEntry.second;

            if(id != tr->id.GetHex())
            {
                continue;
            }

            xbridge::WalletConnectorPtr connFrom = xapp.connectorByCurrency(tr->fromCurrency);
            xbridge::WalletConnectorPtr connTo   = xapp.connectorByCurrency(tr->toCurrency);
            if (!connFrom || !connTo)
            {
                continue;
            }

            Object jtr;
            jtr.push_back(Pair("id", tr->id.GetHex()));
            jtr.push_back(Pair("from", tr->fromCurrency));
            jtr.push_back(Pair("from address", connFrom->fromXAddr(tr->from)));
            double fromAmount = static_cast<double>(tr->fromAmount) / xbridge::TransactionDescr::COIN;
            jtr.push_back(Pair("fromAmount", boost::lexical_cast<std::string>(fromAmount)));
            jtr.push_back(Pair("to", tr->toCurrency));
            jtr.push_back(Pair("to address", connTo->fromXAddr(tr->to)));
            double toAmount = static_cast<double>(tr->toAmount) / xbridge::TransactionDescr::COIN;
            jtr.push_back(Pair("toAmount", boost::lexical_cast<std::string>(toAmount)));
            jtr.push_back(Pair("state", tr->strState()));

            arr.push_back(jtr);
        }
    }

    // historic tx
    {
        std::map<uint256, xbridge::TransactionDescrPtr> trlist = xbridge::App::m_historicTransactions;
        for (const auto & trEntry : trlist)
        {
            const auto & tr = trEntry.second;

            if(id != tr->id.GetHex())
            {
                continue;
            }

            xbridge::WalletConnectorPtr connFrom = xapp.connectorByCurrency(tr->fromCurrency);
            xbridge::WalletConnectorPtr connTo   = xapp.connectorByCurrency(tr->toCurrency);
            if (!connFrom || !connTo)
            {
                continue;
            }


            Object jtr;
            jtr.push_back(Pair("id", tr->id.GetHex()));
            jtr.push_back(Pair("from", tr->fromCurrency));
            jtr.push_back(Pair("from address", connFrom->fromXAddr(tr->from)));
            double fromAmount = static_cast<double>(tr->fromAmount) / xbridge::TransactionDescr::COIN;
            jtr.push_back(Pair("fromAmount", boost::lexical_cast<std::string>(fromAmount)));
            jtr.push_back(Pair("to", tr->toCurrency));
            jtr.push_back(Pair("to address", connTo->fromXAddr(tr->to)));
            double toAmount = static_cast<double>(tr->toAmount) / xbridge::TransactionDescr::COIN;
            jtr.push_back(Pair("toAmount", boost::lexical_cast<std::string>(toAmount)));
            jtr.push_back(Pair("state", tr->strState()));

            arr.push_back(jtr);
        }
    }

    return arr;
}


//******************************************************************************
//******************************************************************************
Value dxGetCurrencyList(const Array & params, bool fHelp)
{
    if (fHelp || params.size() > 1)
    {
        throw runtime_error("dxGetCurrencyList\nList currencies.");
    }

    Object obj;

    std::vector<std::string> currencies = xbridge::App::instance().availableCurrencies();
    for (std::string currency : currencies)
    {
        obj.push_back(Pair(currency, ""));
    }

    return obj;
}

//******************************************************************************
//******************************************************************************
Value dxCreateTransaction(const Array & params, bool fHelp)
{
    if (fHelp || params.size() != 6)
    {
        throw runtime_error("dxCreateTransaction "
                            "(address from) (currency from) (amount from) "
                            "(address to) (currency to) (amount to)\n"
                            "Create xbridge transaction.");
    }

    std::string from            = params[0].get_str();
    std::string fromCurrency    = params[1].get_str();
    double      fromAmount      = params[2].get_real();
    std::string to              = params[3].get_str();
    std::string toCurrency      = params[4].get_str();
    double      toAmount        = params[5].get_real();

    if ((from.size() != 33 && from.size() != 34) ||
            (to.size() != 33 && to.size() != 34))
    {
        throw runtime_error("incorrect address");
    }

    uint256 id = xbridge::App::instance().sendXBridgeTransaction
            (from, fromCurrency, (boost::uint64_t)(fromAmount * xbridge::TransactionDescr::COIN),
             to,   toCurrency,   (boost::uint64_t)(toAmount * xbridge::TransactionDescr::COIN));


    Object obj;
    obj.push_back(Pair("id", id.GetHex()));
    return obj;
}

//******************************************************************************
//******************************************************************************
Value dxAcceptTransaction(const Array & params, bool fHelp)
{
    if (fHelp || params.size() != 3)
    {
        throw runtime_error("dxAcceptTransaction (id) "
                            "(address from) (address to)\n"
                            "Accept xbridge transaction.");
    }

    uint256 id(params[0].get_str());
    std::string from    = params[1].get_str();
    std::string to      = params[2].get_str();

    if ((from.size() != 33 && from.size() != 34) ||
            (to.size() != 33 && to.size() != 34))
    {
        throw runtime_error("incorrect address");
    }

    uint256 idresult = xbridge::App::instance().acceptXBridgeTransaction(id, from, to);

    Object obj;
    obj.push_back(Pair("id", idresult.GetHex()));
    return obj;
}

//******************************************************************************
//******************************************************************************
Value dxCancelTransaction(const Array & params, bool fHelp)
{
    if (fHelp || params.size() != 1)
    {
        throw runtime_error("dxCancelTransaction (id)\n"
                            "Cancel xbridge transaction.");
    }

    uint256 id(params[0].get_str());

    xbridge::App::instance().cancelXBridgeTransaction(id, crRpcRequest);

    Object obj;
    obj.push_back(Pair("id", id.GetHex()));
    return obj;
}
