//*****************************************************************************
//*****************************************************************************

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

#include "xbridgebtcwalletconnector.h"
#include "base58.h"

#include "util/logger.h"
#include "util/txlog.h"

#include <boost/asio.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/asio/ssl.hpp>
#include <stdio.h>

//*****************************************************************************
//*****************************************************************************
namespace rpc
{

using namespace json_spirit;
using namespace std;
using namespace boost;
using namespace boost::asio;

Object CallRPC(const std::string & rpcuser, const std::string & rpcpasswd,
               const std::string & rpcip, const std::string & rpcport,
               const std::string & strMethod, const Array & params);

//*****************************************************************************
//*****************************************************************************
bool listaccounts(const std::string & rpcuser, const std::string & rpcpasswd,
                  const std::string & rpcip, const std::string & rpcport,
                  std::vector<std::string> & accounts)
{
    try
    {
        // LOG() << "rpc call <listaccounts>";

        Array params;
        Object reply = CallRPC(rpcuser, rpcpasswd, rpcip, rpcport,
                               "listaccounts", params);

        // Parse reply
        const Value & result = find_value(reply, "result");
        const Value & error  = find_value(reply, "error");

        if (error.type() != null_type)
        {
            // Error
            LOG() << "error: " << write_string(error, false);
            // int code = find_value(error.get_obj(), "code").get_int();
            return false;
        }
        else if (result.type() != obj_type)
        {
            // Result
            LOG() << "result not an object " <<
                     (result.type() == null_type ? "" :
                      result.type() == str_type  ? result.get_str() :
                                                   write_string(result, true));
            return false;
        }

        Object acclist = result.get_obj();
        for (auto nameval : acclist)
        {
            if (!nameval.name_.empty())
            {
                accounts.push_back(nameval.name_);
            }
        }
    }
    catch (std::exception & e)
    {
        LOG() << "listaccounts exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool getaddressesbyaccount(const std::string & rpcuser, const std::string & rpcpasswd,
                           const std::string & rpcip, const std::string & rpcport,
                           const std::string & account,
                           std::vector<std::string> & addresses)
{
    try
    {
        // LOG() << "rpc call <getaddressesbyaccount>";

        Array params;
        params.push_back(account);
        Object reply = CallRPC(rpcuser, rpcpasswd, rpcip, rpcport,
                               "getaddressesbyaccount", params);

        // Parse reply
        const Value & result = find_value(reply, "result");
        const Value & error  = find_value(reply, "error");

        if (error.type() != null_type)
        {
            // Error
            LOG() << "error: " << write_string(error, false);
            // int code = find_value(error.get_obj(), "code").get_int();
            return false;
        }
        else if (result.type() != array_type)
        {
            // Result
            LOG() << "result not an array " <<
                     (result.type() == null_type ? "" :
                      result.type() == str_type  ? result.get_str() :
                                                   write_string(result, true));
            return false;
        }

        Array arr = result.get_array();
        for (const Value & v : arr)
        {
            if (v.type() == str_type)
            {
                addresses.push_back(v.get_str());
            }
        }
    }
    catch (std::exception & e)
    {
        LOG() << "getaddressesbyaccount exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool listUnspent(const std::string & rpcuser,
                 const std::string & rpcpasswd,
                 const std::string & rpcip,
                 const std::string & rpcport,
                 std::vector<wallet::UtxoEntry> & entries)
{
    const static std::string txid("txid");
    const static std::string vout("vout");
    const static std::string amount("amount");

    try
    {
        LOG() << "rpc call <listunspent>";

        Array params;
        Object reply = CallRPC(rpcuser, rpcpasswd, rpcip, rpcport,
                               "listunspent", params);

        // Parse reply
        const Value & result = find_value(reply, "result");
        const Value & error  = find_value(reply, "error");

        if (error.type() != null_type)
        {
            // Error
            LOG() << "error: " << write_string(error, false);
            // int code = find_value(error.get_obj(), "code").get_int();
            return false;
        }
        else if (result.type() != array_type)
        {
            // Result
            LOG() << "result not an array " <<
                     (result.type() == null_type ? "" :
                      result.type() == str_type  ? result.get_str() :
                                                   write_string(result, true));
            return false;
        }

        Array arr = result.get_array();
        for (const Value & v : arr)
        {
            if (v.type() == obj_type)
            {

                wallet::UtxoEntry u;

                Object o = v.get_obj();
                for (const auto & v : o)
                {
                    if (v.name_ == txid)
                    {
                        u.txId = v.value_.get_str();
                    }
                    else if (v.name_ == vout)
                    {
                        u.vout = v.value_.get_int();
                    }
                    else if (v.name_ == amount)
                    {
                        u.amount = v.value_.get_real();
                    }
                }

                if (!u.txId.empty() && u.amount > 0)
                {
                    entries.push_back(u);
                }
            }
        }
    }
    catch (std::exception & e)
    {
        LOG() << "listunspent exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool lockUnspent(const std::string & rpcuser,
                 const std::string & rpcpasswd,
                 const std::string & rpcip,
                 const std::string & rpcport,
                 const std::vector<wallet::UtxoEntry> & entries,
                 const bool lock)
{
    const static std::string txid("txid");
    const static std::string vout("vout");

    try
    {
        LOG() << "rpc call <lockunspent>";

        Array params;

        // 1. unlock
        params.push_back(lock ? false : true);

        // 2. txoutputs
        Array outputs;
        for (const wallet::UtxoEntry & entry : entries)
        {
            Object o;
            o.push_back(Pair(txid, entry.txId));
            o.push_back(Pair(vout, (int)entry.vout));

            outputs.push_back(o);
        }

        params.push_back(outputs);

        Object reply = CallRPC(rpcuser, rpcpasswd, rpcip, rpcport,
                               "lockunspent", params);

        // Parse reply
        // const Value & result = find_value(reply, "result");
        const Value & error  = find_value(reply, "error");

        if (error.type() != null_type)
        {
            // Error
            LOG() << "error: " << write_string(error, false);
            // int code = find_value(error.get_obj(), "code").get_int();
            return false;
        }
//        else if (result.type() != array_type)
//        {
//            // Result
//            LOG() << "result not an array " <<
//                     (result.type() == null_type ? "" :
//                      result.type() == str_type  ? result.get_str() :
//                                                   write_string(result, true));
//            return false;
//        }

    }
    catch (std::exception & e)
    {
        LOG() << "listunspent exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool gettxout(const std::string & rpcuser,
              const std::string & rpcpasswd,
              const std::string & rpcip,
              const std::string & rpcport,
              wallet::UtxoEntry & txout)
{
    try
    {
        LOG() << "rpc call <gettxout>";

        txout.amount = 0;

        Array params;
        params.push_back(txout.txId);
        params.push_back(static_cast<int>(txout.vout));
        Object reply = CallRPC(rpcuser, rpcpasswd, rpcip, rpcport,
                               "gettxout", params);

        // Parse reply
        const Value & result = find_value(reply, "result");
        const Value & error  = find_value(reply, "error");

        if (error.type() != null_type)
        {
            // Error
            LOG() << "error: " << write_string(error, false);
            // int code = find_value(error.get_obj(), "code").get_int();
            return false;
        }
        else if (result.type() != obj_type)
        {
            // Result
            LOG() << "result not an object " <<
                     (result.type() == null_type ? "" :
                      result.type() == str_type  ? result.get_str() :
                                                   write_string(result, true));
            return false;
        }

        Object o = result.get_obj();
        txout.amount = find_value(o, "value").get_real();
    }
    catch (std::exception & e)
    {
        LOG() << "listunspent exception " << e.what();
        return false;
    }

    return true;
}

} // namespace rpc

//*****************************************************************************
//*****************************************************************************
XBridgeBtcWalletConnector::XBridgeBtcWalletConnector()
{

}

//*****************************************************************************
//*****************************************************************************
std::vector<unsigned char> XBridgeBtcWalletConnector::toXAddr(const std::string & addr) const
{
    std::vector<unsigned char> vaddr;
    if (DecodeBase58Check(addr.c_str(), vaddr))
    {
        vaddr.erase(vaddr.begin());
    }
    return vaddr;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeBtcWalletConnector::requestAddressBook(std::vector<wallet::AddressBookEntry> & entries)
{
    std::vector<std::string> accounts;
    if (!rpc::listaccounts(m_user, m_passwd, m_ip, m_port, accounts))
    {
        return false;
    }
    // LOG() << "received " << accounts.size() << " accounts";
    for (std::string & account : accounts)
    {
        std::vector<std::string> addrs;
        if (rpc::getaddressesbyaccount(m_user, m_passwd, m_ip, m_port, account, addrs))
        {
            entries.push_back(std::make_pair(account, addrs));
            // LOG() << acc << " - " << boost::algorithm::join(addrs, ",");
        }
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeBtcWalletConnector::getUnspent(std::vector<wallet::UtxoEntry> & inputs) const
{
    if (!rpc::listUnspent(m_user, m_passwd, m_ip, m_port, inputs))
    {
        LOG() << "rpc::listUnspent failed" << __FUNCTION__;
        return false;
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeBtcWalletConnector::lockUnspent(std::vector<wallet::UtxoEntry> & inputs,
                                            const bool lock) const
{
    if (!rpc::lockUnspent(m_user, m_passwd, m_ip, m_port, inputs, lock))
    {
        LOG() << "rpc::lockUnspent failed" << __FUNCTION__;
        return false;
    }

    return true;
}
