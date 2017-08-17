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

    boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

    // pending tx
    {
        std::map<uint256, XBridgeTransactionDescrPtr> trlist = XBridgeApp::m_pendingTransactions;
        for (const auto & trEntry : trlist)
        {
            Object jtr;
            const auto tr = trEntry.second;
            jtr.push_back(Pair("id", tr->id.GetHex()));
            jtr.push_back(Pair("from", tr->fromCurrency));
            jtr.push_back(Pair("from address", tr->from));
            double fromAmount = static_cast<double>(tr->fromAmount) / XBridgeTransactionDescr::COIN;
            jtr.push_back(Pair("fromAmount", boost::lexical_cast<std::string>(fromAmount)));
            jtr.push_back(Pair("to", tr->toCurrency));
            jtr.push_back(Pair("to address", tr->to));
            double toAmount = static_cast<double>(tr->toAmount) / XBridgeTransactionDescr::COIN;
            jtr.push_back(Pair("toAmount", boost::lexical_cast<std::string>(toAmount)));
            jtr.push_back(Pair("state", tr->strState()));

            arr.push_back(jtr);
        }
    }

    // active tx
    {
        std::map<uint256, XBridgeTransactionDescrPtr> trlist = XBridgeApp::m_transactions;
        for (const auto & trEntry : trlist)
        {
            Object jtr;
            const auto tr = trEntry.second;
            jtr.push_back(Pair("id", tr->id.GetHex()));
            jtr.push_back(Pair("from", tr->fromCurrency));
            jtr.push_back(Pair("from address", tr->from));
            double fromAmount = static_cast<double>(tr->fromAmount) / XBridgeTransactionDescr::COIN;
            jtr.push_back(Pair("fromAmount", boost::lexical_cast<std::string>(fromAmount)));
            jtr.push_back(Pair("to", tr->toCurrency));
            jtr.push_back(Pair("to address", tr->to));
            double toAmount = static_cast<double>(tr->toAmount) / XBridgeTransactionDescr::COIN;
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

    boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

    {
        std::map<uint256, XBridgeTransactionDescrPtr> trlist = XBridgeApp::m_historicTransactions;
        for (const auto & trEntry : trlist)
        {
            Object jtr;
            const auto tr = trEntry.second;
            jtr.push_back(Pair("id", tr->id.GetHex()));
            jtr.push_back(Pair("from", tr->fromCurrency));
            jtr.push_back(Pair("from address", tr->from));
            double fromAmount = static_cast<double>(tr->fromAmount) / XBridgeTransactionDescr::COIN;
            jtr.push_back(Pair("fromAmount", boost::lexical_cast<std::string>(fromAmount)));
            jtr.push_back(Pair("to", tr->toCurrency));
            jtr.push_back(Pair("to address", tr->to));
            double toAmount = static_cast<double>(tr->toAmount) / XBridgeTransactionDescr::COIN;
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
        throw runtime_error("dxGetTransactionInfo\nTransaction info.");
    }

    std::string id = params[0].get_str();

    Array arr;

    boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

    // pending tx
    {
        std::map<uint256, XBridgeTransactionDescrPtr> trlist = XBridgeApp::m_pendingTransactions;
        for (const auto & trEntry : trlist)
        {
            Object jtr;
            const auto tr = trEntry.second;
            jtr.push_back(Pair("id", tr->id.GetHex()));
            jtr.push_back(Pair("from", tr->fromCurrency));
            jtr.push_back(Pair("from address", tr->from));
            double fromAmount = static_cast<double>(tr->fromAmount) / XBridgeTransactionDescr::COIN;
            jtr.push_back(Pair("fromAmount", boost::lexical_cast<std::string>(fromAmount)));
            jtr.push_back(Pair("to", tr->toCurrency));
            jtr.push_back(Pair("to address", tr->to));
            double toAmount = static_cast<double>(tr->toAmount) / XBridgeTransactionDescr::COIN;
            jtr.push_back(Pair("toAmount", boost::lexical_cast<std::string>(toAmount)));
            jtr.push_back(Pair("state", tr->strState()));

            arr.push_back(jtr);
        }
    }

    // active tx
    {
        std::map<uint256, XBridgeTransactionDescrPtr> trlist = XBridgeApp::m_transactions;
        for (const auto & trEntry : trlist)
        {
            Object jtr;
            const auto tr = trEntry.second;
            jtr.push_back(Pair("id", tr->id.GetHex()));
            jtr.push_back(Pair("from", tr->fromCurrency));
            jtr.push_back(Pair("from address", tr->from));
            double fromAmount = static_cast<double>(tr->fromAmount) / XBridgeTransactionDescr::COIN;
            jtr.push_back(Pair("fromAmount", boost::lexical_cast<std::string>(fromAmount)));
            jtr.push_back(Pair("to", tr->toCurrency));
            jtr.push_back(Pair("to address", tr->to));
            double toAmount = static_cast<double>(tr->toAmount) / XBridgeTransactionDescr::COIN;
            jtr.push_back(Pair("toAmount", boost::lexical_cast<std::string>(toAmount)));
            jtr.push_back(Pair("state", tr->strState()));

            arr.push_back(jtr);
        }
    }

    // historic tx
    {
        std::map<uint256, XBridgeTransactionDescrPtr> trlist = XBridgeApp::m_historicTransactions;
        for (const auto & trEntry : trlist)
        {
            Object jtr;
            const auto tr = trEntry.second;
            jtr.push_back(Pair("id", tr->id.GetHex()));
            jtr.push_back(Pair("from", tr->fromCurrency));
            jtr.push_back(Pair("from address", tr->from));
            double fromAmount = static_cast<double>(tr->fromAmount) / XBridgeTransactionDescr::COIN;
            jtr.push_back(Pair("fromAmount", boost::lexical_cast<std::string>(fromAmount)));
            jtr.push_back(Pair("to", tr->toCurrency));
            jtr.push_back(Pair("to address", tr->to));
            double toAmount = static_cast<double>(tr->toAmount) / XBridgeTransactionDescr::COIN;
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

    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isEnabled())
        throw runtime_error("Not an exchange node.");

    Object obj;

    std::vector<StringPair> wallets = e.listOfWallets();
    for (StringPair & w : wallets)
    {
        obj.push_back(Pair(w.first, w.second));
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

    uint256 id = XBridgeApp::instance().sendXBridgeTransaction
            (from, fromCurrency, (boost::uint64_t)(fromAmount * XBridgeTransactionDescr::COIN),
             to,   toCurrency,   (boost::uint64_t)(toAmount * XBridgeTransactionDescr::COIN));


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

    uint256 idresult = XBridgeApp::instance().acceptXBridgeTransaction(id, from, to);

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

    XBridgeApp::instance().cancelXBridgeTransaction(id, crRpcRequest);

    Object obj;
    obj.push_back(Pair("id", id.GetHex()));
    return obj;
}
