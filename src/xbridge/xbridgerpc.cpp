// Copyright (c) 2017-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <xbridge/xbridgerpc.h>

#include <xbridge/util/logger.h>
#include <xbridge/util/xutil.h>
#include <xbridge/xbridgewalletconnectorbtc.h>

#include <base58.h>
#include <logging.h>
#include <primitives/transaction.h>
#include <rpc/protocol.h>
#include <util/system.h>

#include <stdio.h>

#include <json/json_spirit_reader_template.h>
#include <json/json_spirit_writer_template.h>
#include <json/json_spirit_utils.h>

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/lexical_cast.hpp>

#define HTTP_DEBUG

//*****************************************************************************
//*****************************************************************************
namespace rpc
{

using namespace json_spirit;
using namespace std;
using namespace boost;
using namespace boost::asio;


#define PAIRTYPE(t1, t2)    std::pair<t1, t2>

const unsigned int MAX_SIZE = 0x02000000;

//******************************************************************************
//******************************************************************************
std::vector<unsigned char> toXAddr(const std::string & addr)
{
    std::vector<unsigned char> vaddr;
    if (DecodeBase58Check(addr.c_str(), vaddr))
    {
        vaddr.erase(vaddr.begin());
    }
    return vaddr;
}

//******************************************************************************
//******************************************************************************
string JSONRPCRequest(const string& strMethod, const Array& params, const Value& id)
{
    Object request;
    request.push_back(Pair("method", strMethod));
    request.push_back(Pair("params", params));
    request.push_back(Pair("id", id));
    return write_string(Value(request), json_spirit::none, 6) + "\n";
}

//******************************************************************************
//******************************************************************************
string HTTPPost(const string& strMsg, const map<string,string>& mapRequestHeaders)
{
    ostringstream s;
    s << "POST / HTTP/1.1\r\n"
      << "User-Agent: xbridgep2p-json-rpc\r\n"
      << "Host: 127.0.0.1\r\n"
      << "Content-Type: application/json\r\n"
      << "Content-Length: " << strMsg.size() << "\r\n"
      << "Connection: close\r\n"
      << "Accept: application/json\r\n";
    for (const std::pair<string, string> & item : mapRequestHeaders)
        s << item.first << ": " << item.second << "\r\n";
    s << "\r\n" << strMsg;

    return s.str();
}

//******************************************************************************
//******************************************************************************
int ReadHTTPStatus(std::basic_istream<char>& stream, int &proto)
{
    string str;
    getline(stream, str);
    vector<string> vWords;
    boost::split(vWords, str, boost::is_any_of(" "));
    if (vWords.size() < 2)
        return HTTP_INTERNAL_SERVER_ERROR;
    proto = 0;
    const char *ver = strstr(str.c_str(), "HTTP/1.");
    if (ver != NULL)
        proto = boost::lexical_cast<int>(ver+7);
    return boost::lexical_cast<int>(vWords[1].c_str());
}

//******************************************************************************
//******************************************************************************
int ReadHTTPHeader(std::basic_istream<char>& stream, map<string, string>& mapHeadersRet)
{
    int nLen = 0;
    while (true)
    {
        string str;
        std::getline(stream, str);
        if (str.empty() || str == "\r")
            break;
        string::size_type nColon = str.find(":");
        if (nColon != string::npos)
        {
            string strHeader = str.substr(0, nColon);
            boost::trim(strHeader);
            boost::to_lower(strHeader, std::locale::classic());
            string strValue = str.substr(nColon+1);
            boost::trim(strValue);
            mapHeadersRet[strHeader] = strValue;
            if (strHeader == "content-length")
                nLen = boost::lexical_cast<int>(strValue.c_str());
        }
    }
    return nLen;
}

//******************************************************************************
//******************************************************************************
int readHTTP(std::basic_istream<char>& stream, map<string, string>& mapHeadersRet, string& strMessageRet)
{
    mapHeadersRet.clear();
    strMessageRet = "";

    // Read status
    int nProto = 0;
    int nStatus = ReadHTTPStatus(stream, nProto);

    // Read header
    int nLen = ReadHTTPHeader(stream, mapHeadersRet);
    if (nLen < 0 || nLen > (int)MAX_SIZE)
        return HTTP_INTERNAL_SERVER_ERROR;

    // Read message
    if (nLen > 0)
    {
        vector<char> vch(nLen);
        stream.read(&vch[0], nLen);
        strMessageRet = string(vch.begin(), vch.end());
    }

    string sConHdr = mapHeadersRet["connection"];

    if ((sConHdr != "close") && (sConHdr != "keep-alive"))
    {
        if (nProto >= 1)
            mapHeadersRet["connection"] = "keep-alive";
        else
            mapHeadersRet["connection"] = "close";
    }

    return nStatus;
}

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
        Object reply = xbridge::CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "listaccounts", params);

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
        Object reply = xbridge::CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "getaddressesbyaccount", params);

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
bool requestAddressBook(const std::string & rpcuser, const std::string & rpcpasswd,
                        const std::string & rpcip, const std::string & rpcport,
                        std::vector<AddressBookEntry> & entries)
{
    std::vector<std::string> accounts;
    if (!listaccounts(rpcuser, rpcpasswd, rpcip, rpcport, accounts))
    {
        return false;
    }
    // LOG() << "received " << accounts.size() << " accounts";
    for (std::string & acc : accounts)
    {
        std::vector<std::string> addrs;
        if (getaddressesbyaccount(rpcuser, rpcpasswd, rpcip, rpcport, acc, addrs))
        {
            entries.push_back(std::make_pair(acc, addrs));
            // LOG() << acc << " - " << boost::algorithm::join(addrs, ",");
        }
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool getInfo(const std::string & rpcuser,
             const std::string & rpcpasswd,
             const std::string & rpcip,
             const std::string & rpcport,
             Info & info)
{
    try
    {
        LOG() << "rpc call <getinfo>";

        Array params;
        Object reply = xbridge::CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "getinfo", params);

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
            LOG() << "getinfo result not an object " <<
                     (result.type() == null_type ? "" :
                      result.type() == str_type  ? result.get_str() :
                                                   write_string(result, true));
            return false;
        }

        Object o = result.get_obj();

        info.blocks = find_value(o, "blocks").get_int();
    }
    catch (std::exception & e)
    {
        LOG() << "getinfo exception " << e.what();
        return false;
    }

    return true;
}


//*****************************************************************************
//*****************************************************************************
bool createRawTransaction(const std::string & rpcuser,
                          const std::string & rpcpasswd,
                          const std::string & rpcip,
                          const std::string & rpcport,
                          const std::vector<std::pair<string, int> > & inputs,
                          const std::vector<std::pair<std::string, double> > & outputs,
                          const uint32_t lockTime,
                          std::string & tx,
                          const bool cltv=false)
{
    try
    {
        LOG() << "rpc call <createrawtransaction>";

        // inputs
        Array i;
        for (const std::pair<string, int> & input : inputs)
        {
            Object tmp;
            tmp.push_back(Pair("txid", input.first));
            tmp.push_back(Pair("vout", input.second));
            if (cltv)
                tmp.push_back(Pair("sequence", static_cast<int64_t>(xbridge::SEQUENCE_FINAL)));
            i.push_back(tmp);
        }

        // outputs
        Object o;
        for (const std::pair<std::string, double> & dest : outputs)
        {
            o.push_back(Pair(dest.first, dest.second));
        }

        Array params;
        params.push_back(i);
        params.push_back(o);

        // locktime
        if (lockTime > 0)
        {
            params.push_back(uint64_t(lockTime));
        }

        Object reply = xbridge::CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "createrawtransaction", params);

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
        else if (result.type() != str_type)
        {
            // Result
            LOG() << "result not an string " <<
                     (result.type() == null_type ? "" :
                                                   write_string(result, true));
            return false;
        }

        tx = write_string(result, false);
        if (tx[0] == '\"')
        {
            tx.erase(0, 1);
        }
        if (tx[tx.size()-1] == '\"')
        {
            tx.erase(tx.size()-1, 1);
        }
    }
    catch (std::exception & e)
    {
        LOG() << "createrawtransaction exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool decodeRawTransaction(const std::string & rpcuser,
                          const std::string & rpcpasswd,
                          const std::string & rpcip,
                          const std::string & rpcport,
                          const std::string & rawtx,
                          std::string & txid,
                          std::string & tx)
{
    try
    {
        LOG() << "rpc call <decoderawtransaction>";

        Array params;
        params.push_back(rawtx);
        Object reply = xbridge::CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "decoderawtransaction", params);

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

        tx   = write_string(result, false);

        const Value & vtxid = find_value(result.get_obj(), "txid");
        if (vtxid.type() == str_type)
        {
            txid = vtxid.get_str();
        }
    }
    catch (std::exception & e)
    {
        LOG() << "decoderawtransaction exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
std::string prevtxsJson(const std::vector<std::tuple<std::string, int, std::string, std::string> > & prevtxs)
{
    // prevtxs
    if (!prevtxs.size())
    {
        return std::string();
    }

    Array arrtx;
    for (const std::tuple<std::string, int, std::string, std::string> & prev : prevtxs)
    {
        Object o;
        o.push_back(Pair("txid",         std::get<0>(prev)));
        o.push_back(Pair("vout",         std::get<1>(prev)));
        o.push_back(Pair("scriptPubKey", std::get<2>(prev)));
        std::string redeem = std::get<3>(prev);
        if (redeem.size())
        {
            o.push_back(Pair("redeemScript", redeem));
        }
        arrtx.push_back(o);
    }

    return write_string(Value(arrtx));
}

//*****************************************************************************
//*****************************************************************************
bool signRawTransaction(const std::string & rpcuser,
                        const std::string & rpcpasswd,
                        const std::string & rpcip,
                        const std::string & rpcport,
                        std::string & rawtx,
                        bool & complete)
{
    std::vector<std::tuple<std::string, int, std::string, std::string> > arr;
    std::string prevtxs = prevtxsJson(arr);
    std::vector<string> keys;
    return signRawTransaction(rpcuser, rpcpasswd, rpcip, rpcport,
                              rawtx, prevtxs, keys, complete);
}

//*****************************************************************************
//*****************************************************************************
bool signRawTransaction(const std::string & rpcuser,
                        const std::string & rpcpasswd,
                        const std::string & rpcip,
                        const std::string & rpcport,
                        std::string & rawtx,
                        const string & prevtxs,
                        const std::vector<std::string> & keys,
                        bool & complete)
{
    try
    {
        LOG() << "rpc call <signrawtransaction>";

        Array params;
        params.push_back(rawtx);

        // prevtxs
        if (!prevtxs.size())
        {
            params.push_back(Value::null);
        }
        else
        {
            Value v;
            if (!read_string(prevtxs, v))
            {
                ERR() << "error read json " << __FUNCTION__;
                ERR() << prevtxs;
                params.push_back(Value::null);
            }
            else
            {
                params.push_back(v.get_array());
            }
        }

        // priv keys
        if (!keys.size())
        {
            params.push_back(Value::null);
        }
        else
        {
            Array jkeys;
            std::copy(keys.begin(), keys.end(), std::back_inserter(jkeys));

            params.push_back(jkeys);
        }

        Object reply = xbridge::CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "signrawtransaction", params);

        // Parse reply
        Value result = find_value(reply, "result");
        const Value & error = find_value(reply, "error");

        if (error.type() != null_type)
        {
            // For newer bitcoin clients try signrawtransactionwithwallet
            reply = xbridge::CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "signrawtransactionwithwallet", params);

            const Value & error2 = find_value(reply, "error");
            if (error2.type() != null_type) {
                LOG() << "error: " << write_string(error, false) << " " << write_string(error2, false);
                return false;
            }

            result = find_value(reply, "result");
        }

        if (result.type() != obj_type)
        {
            // Result
            LOG() << "result not an object " <<
                     (result.type() == null_type ? "" :
                      result.type() == str_type  ? result.get_str() :
                                                   write_string(result, true));
            return false;
        }

        Object obj = result.get_obj();
        const Value  & tx = find_value(obj, "hex");
        const Value & cpl = find_value(obj, "complete");

        if (tx.type() != str_type || cpl.type() != bool_type)
        {
            LOG() << "bad hex " <<
                     (tx.type() == null_type ? "" :
                      tx.type() == str_type  ? tx.get_str() :
                                                   write_string(tx, true));
            return false;
        }

        rawtx    = tx.get_str();
        complete = cpl.get_bool();

    }
    catch (std::exception & e)
    {
        LOG() << "signrawtransaction exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool sendRawTransaction(const std::string & rpcuser,
                        const std::string & rpcpasswd,
                        const std::string & rpcip,
                        const std::string & rpcport,
                        const std::string & rawtx)
{
    try
    {
        LOG() << "rpc call <sendrawtransaction>";

        Array params;
        params.push_back(rawtx);
        Object reply = xbridge::CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "sendrawtransaction", params);

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
    }
    catch (std::exception & e)
    {
        LOG() << "sendrawtransaction exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool getNewPubKey(const std::string & rpcuser,
                  const std::string & rpcpasswd,
                  const std::string & rpcip,
                  const std::string & rpcport,
                  std::string & key)
{
    try
    {
        LOG() << "rpc call <getnewpubkey>";

        Array params;
        Object reply = xbridge::CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "getnewpubkey", params);

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
        else if (result.type() != str_type)
        {
            // Result
            LOG() << "result not an string " <<
                     (result.type() == null_type ? "" :
                      result.type() == str_type  ? result.get_str() :
                                                   write_string(result, true));
            return false;
        }

        key = result.get_str();
    }
    catch (std::exception & e)
    {
        LOG() << "getnewpubkey exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool dumpPrivKey(const std::string & rpcuser,
                 const std::string & rpcpasswd,
                 const std::string & rpcip,
                 const std::string & rpcport,
                 const std::string & address,
                 string & key)
{
    try
    {
        LOG() << "rpc call <dumpprivkey>";

        Array params;
        params.push_back(address);

        Object reply = xbridge::CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "dumpprivkey", params);

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
        else if (result.type() != str_type)
        {
            // Result
            LOG() << "result not an string " <<
                     (result.type() == null_type ? "" :
                      result.type() == str_type  ? result.get_str() :
                                                   write_string(result, true));
            return false;
        }

        key = result.get_str();
    }
    catch (std::exception & e)
    {
        LOG() << "dumpprivkey exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool importPrivKey(const std::string & rpcuser,
                   const std::string & rpcpasswd,
                   const std::string & rpcip,
                   const std::string & rpcport,
                   const std::string & key,
                   const std::string & label,
                   const bool & noScanWallet)
{
    try
    {
        LOG() << "rpc call <importprivkey>";

        Array params;
        params.push_back(key);
        params.push_back(label);
        if (noScanWallet)
        {
            params.push_back(false);
        }


        Object reply = xbridge::CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "importprivkey", params);

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
    }
    catch (std::exception & e)
    {
        LOG() << "importprivkey exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool getNewAddress(const std::string & rpcuser,
                   const std::string & rpcpasswd,
                   const std::string & rpcip,
                   const std::string & rpcport,
                   std::string & addr)
{
    try
    {
        LOG() << "rpc call <getnewaddress>";

        Array params;
        Object reply = xbridge::CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "getnewaddress", params);

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
        else if (result.type() != str_type)
        {
            // Result
            LOG() << "result not an string " <<
                     (result.type() == null_type ? "" :
                      result.type() == str_type  ? result.get_str() :
                                                   write_string(result, true));
            return false;
        }

        addr = result.get_str();
    }
    catch (std::exception & e)
    {
        LOG() << "getnewaddress exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool addMultisigAddress(const std::string & rpcuser,
                        const std::string & rpcpasswd,
                        const std::string & rpcip,
                        const std::string & rpcport,
                        const std::vector<std::string> & keys,
                        std::string & addr)
{
    try
    {
        LOG() << "rpc call <addmultisigaddress>";

        Array params;
        params.push_back(static_cast<int>(keys.size()));

        Array paramKeys;
        for (const std::string & key : keys)
        {
            paramKeys.push_back(key);
        }
        params.push_back(paramKeys);

        Object reply = xbridge::CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "addmultisigaddress", params);

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
        else if (result.type() != str_type)
        {
            // Result
            LOG() << "result not an string " <<
                     (result.type() == null_type ? "" :
                      result.type() == str_type  ? result.get_str() :
                                                   write_string(result, true));
            return false;
        }

        addr = result.get_str();
    }
    catch (std::exception & e)
    {
        LOG() << "addmultisigaddress exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool getTransaction(const std::string & rpcuser,
                    const std::string & rpcpasswd,
                    const std::string & rpcip,
                    const std::string & rpcport,
                    const std::string & txid)
                    // std::string & tx)
{
    try
    {
        LOG() << "rpc call <gettransaction>";

        Array params;
        params.push_back(txid);
        Object reply = xbridge::CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "gettransaction", params);

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

        // transaction exists, success
    }
    catch (std::exception & e)
    {
        LOG() << "signrawtransaction exception " << e.what();
        return false;
    }

    return true;

}


//*****************************************************************************
//*****************************************************************************
bool eth_gasPrice(const std::string & rpcip,
                  const std::string & rpcport,
                  uint64_t & gasPrice)
{
    try
    {
        LOG() << "rpc call <eth_gasPrice>";

        Array params;
        Object reply = xbridge::CallRPC("rpcuser", "rpcpasswd", rpcip, rpcport, "eth_gasPrice", params);

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

        const Value & result = find_value(reply, "result");

        if (result.type() != str_type)
        {
            // Result
            LOG() << "result not an array " <<
                     (result.type() == null_type ? "" :
                       write_string(result, true));
            return false;
        }

        std::string value = result.get_str();
//        gasPrice = strtoll(value.substr(2).c_str(), nullptr, 16); // TODO Blocknet use non locale func strtoll
    }
    catch (std::exception & e)
    {
        LOG() << "eth_accounts exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool eth_accounts(const std::string   & rpcip,
                  const std::string   & rpcport,
                  std::vector<string> & addresses)
{
    try
    {
        LOG() << "rpc call <eth_accounts>";

        Array params;
        Object reply = xbridge::CallRPC("rpcuser", "rpcpasswd", rpcip, rpcport, "eth_accounts", params);

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

        const Value & result = find_value(reply, "result");

        if (result.type() != array_type)
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
        LOG() << "sendrawtransaction exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool eth_getBalance(const std::string & rpcip,
                    const std::string & rpcport,
                    const std::string & account,
                    uint64_t & amount)
{
    try
    {
        LOG() << "rpc call <eth_getBalance>";

//        std::vector<std::string> accounts;
//        rpc::eth_accounts(rpcip, rpcport, accounts);

//        for (const std::string & account : accounts)
        {
            Array params;
            params.push_back(account);
            params.push_back("latest");
            Object reply = xbridge::CallRPC("rpcuser", "rpcpasswd", rpcip, rpcport, "eth_getBalance", params);

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

            const Value & result = find_value(reply, "result");

            if (result.type() != str_type)
            {
                // Result
                LOG() << "result not an string " <<
                         (result.type() == null_type ? "" :
                          write_string(result, true));
                return false;
            }

            std::string value = result.get_str();
//            amount = strtoll(value.substr(2).c_str(), nullptr, 16); // TODO Blocknet use non locale func strtoll
        }
    }
    catch (std::exception & e)
    {
        LOG() << "sendrawtransaction exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool eth_sendTransaction(const std::string & rpcip,
                         const std::string & rpcport,
                         const std::string & from,
                         const std::string & to,
                         const uint64_t & amount,
                         const uint64_t & /*fee*/)
{
    try
    {
        LOG() << "rpc call <eth_sendTransaction>";

        Array params;
        // params.push_back(rawtx);

        Object o;
        o.push_back(Pair("from",       from));
        o.push_back(Pair("to",         to));
        o.push_back(Pair("gas",        "0x76c0"));
        o.push_back(Pair("gasPrice",   "0x9184e72a000"));
        // o.push_back(Pair("value",      "0x9184e72a"));

        char buf[64];
//        sprintf(buf, "%lullx", amount); // TODO Blocknet use non-locale based func
        o.push_back(Pair("value", buf));

        // o.push_back(Pair("data",       "0xd46e8dd67c5d32be8d46e8dd67c5d32be8058bb8eb970870f072445675058bb8eb970870f072445675"));

        params.push_back(o);

        Object reply = xbridge::CallRPC("rpcuser", "rpcpasswd", rpcip, rpcport, "eth_sendTransaction", params);

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
    }
    catch (std::exception & e)
    {
        LOG() << "sendrawtransaction exception " << e.what();
        return false;
    }

    return true;
}

} // namespace rpc
