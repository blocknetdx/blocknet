
#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

#include <boost/asio.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/asio/ssl.hpp>
#include <stdio.h>

#include "bitcoinrpcconnector.h"
#include "util/xutil.h"
#include "util/logger.h"
#include "util/txlog.h"

#include "base58.h"
// #include "bignum.h"
#include "rpcserver.h"
#include "rpcprotocol.h"
#include "rpcclient.h"
#include "wallet.h"
#include "init.h"
#include "key.h"

#define HTTP_DEBUG

//*****************************************************************************
//*****************************************************************************
namespace xbridge
{

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
int readHTTP(std::basic_istream<char>& stream, map<string, string>& mapHeadersRet, string& strMessageRet)
{
    mapHeadersRet.clear();
    strMessageRet = "";

    // Read status
    int nProto = 0;
    int nStatus = ReadHTTPStatus(stream, nProto);

    // Read header
    int nLen = ReadHTTPHeaders(stream, mapHeadersRet);
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

//******************************************************************************
//******************************************************************************
Object CallRPC(const std::string & rpcuser, const std::string & rpcpasswd,
               const std::string & rpcip, const std::string & rpcport,
               const std::string & strMethod, const Array & params)
{
//    if (mapArgs["-rpcuser"] == "" && mapArgs["-rpcpassword"] == "")
//        throw runtime_error(strprintf(
//            _("You must set rpcpassword=<password> in the configuration file:\n%s\n"
//              "If the file does not exist, create it with owner-readable-only file permissions."),
//                GetConfigFile().string().c_str()));

    // Connect to localhost
    bool fUseSSL = false;//GetBoolArg("-rpcssl");
    asio::io_service io_service;
    ssl::context context(io_service, ssl::context::sslv23);
    context.set_options(ssl::context::no_sslv2);
    asio::ssl::stream<asio::ip::tcp::socket> sslStream(io_service, context);
    SSLIOStreamDevice<asio::ip::tcp> d(sslStream, fUseSSL);
    iostreams::stream< SSLIOStreamDevice<asio::ip::tcp> > stream(d);
    if (!d.connect(rpcip, rpcport))
        throw runtime_error("couldn't connect to server");

    // HTTP basic authentication
    string strUserPass64 = util::base64_encode(rpcuser + ":" + rpcpasswd);
    map<string, string> mapRequestHeaders;
    mapRequestHeaders["Authorization"] = string("Basic ") + strUserPass64;

    // Send request
    string strRequest = JSONRPCRequest(strMethod, params, 1);

#ifdef HTTP_DEBUG
    LOG() << "HTTP: req  " << strMethod << " " << strRequest;
#endif

    string strPost = HTTPPost(strRequest, mapRequestHeaders);
    stream << strPost << std::flush;

    // Receive reply
    map<string, string> mapHeaders;
    string strReply;
    int nStatus = readHTTP(stream, mapHeaders, strReply);

#ifdef HTTP_DEBUG
    LOG() << "HTTP: resp " << nStatus << " " << strReply;
#endif

    if (nStatus == HTTP_UNAUTHORIZED)
        throw runtime_error("incorrect rpcuser or rpcpassword (authorization failed)");
    else if (nStatus >= 400 && nStatus != HTTP_BAD_REQUEST && nStatus != HTTP_NOT_FOUND && nStatus != HTTP_INTERNAL_SERVER_ERROR)
        throw runtime_error(strprintf("server returned HTTP error %d", nStatus));
    else if (strReply.empty())
        throw runtime_error("no response from server");

    // Parse reply
    Value valReply;
    if (!read_string(strReply, valReply))
        throw runtime_error("couldn't parse reply from server");
    const Object& reply = valReply.get_obj();
    if (reply.empty())
        throw runtime_error("expected reply to have result, error and id properties");

    return reply;
}

//*****************************************************************************
//*****************************************************************************
//bool eth_gasPrice(const std::string & rpcip,
//                  const std::string & rpcport,
//                  uint64_t & gasPrice)
//{
//    try
//    {
//        LOG() << "rpc call <eth_gasPrice>";

//        Array params;
//        Object reply = CallRPC("rpcuser", "rpcpasswd", rpcip, rpcport,
//                               "eth_gasPrice", params);

//        // Parse reply
//        // const Value & result = find_value(reply, "result");
//        const Value & error  = find_value(reply, "error");

//        if (error.type() != null_type)
//        {
//            // Error
//            LOG() << "error: " << write_string(error, false);
//            // int code = find_value(error.get_obj(), "code").get_int();
//            return false;
//        }

//        const Value & result = find_value(reply, "result");

//        if (result.type() != str_type)
//        {
//            // Result
//            LOG() << "result not an array " <<
//                     (result.type() == null_type ? "" :
//                       write_string(result, true));
//            return false;
//        }

//        std::string value = result.get_str();
//        gasPrice = strtoll(value.substr(2).c_str(), nullptr, 16);
//    }
//    catch (std::exception & e)
//    {
//        LOG() << "eth_accounts exception " << e.what();
//        return false;
//    }

//    return true;
//}

//*****************************************************************************
//*****************************************************************************
//bool eth_accounts(const std::string   & rpcip,
//                  const std::string   & rpcport,
//                  std::vector<string> & addresses)
//{
//    try
//    {
//        LOG() << "rpc call <eth_accounts>";

//        Array params;
//        Object reply = CallRPC("rpcuser", "rpcpasswd", rpcip, rpcport,
//                               "eth_accounts", params);

//        // Parse reply
//        // const Value & result = find_value(reply, "result");
//        const Value & error  = find_value(reply, "error");

//        if (error.type() != null_type)
//        {
//            // Error
//            LOG() << "error: " << write_string(error, false);
//            // int code = find_value(error.get_obj(), "code").get_int();
//            return false;
//        }

//        const Value & result = find_value(reply, "result");

//        if (result.type() != array_type)
//        {
//            // Result
//            LOG() << "result not an array " <<
//                     (result.type() == null_type ? "" :
//                      result.type() == str_type  ? result.get_str() :
//                                                   write_string(result, true));
//            return false;
//        }

//        Array arr = result.get_array();
//        for (const Value & v : arr)
//        {
//            if (v.type() == str_type)
//            {
//                addresses.push_back(v.get_str());
//            }
//        }

//    }
//    catch (std::exception & e)
//    {
//        LOG() << "sendrawtransaction exception " << e.what();
//        return false;
//    }

//    return true;
//}

//*****************************************************************************
//*****************************************************************************
//bool eth_getBalance(const std::string & rpcip,
//                    const std::string & rpcport,
//                    const std::string & account,
//                    uint64_t & amount)
//{
//    try
//    {
//        LOG() << "rpc call <eth_getBalance>";

////        std::vector<std::string> accounts;
////        rpc::eth_accounts(rpcip, rpcport, accounts);

////        for (const std::string & account : accounts)
//        {
//            Array params;
//            params.push_back(account);
//            params.push_back("latest");
//            Object reply = CallRPC("rpcuser", "rpcpasswd", rpcip, rpcport,
//                                   "eth_getBalance", params);

//            // Parse reply
//            // const Value & result = find_value(reply, "result");
//            const Value & error  = find_value(reply, "error");

//            if (error.type() != null_type)
//            {
//                // Error
//                LOG() << "error: " << write_string(error, false);
//                // int code = find_value(error.get_obj(), "code").get_int();
//                return false;
//            }

//            const Value & result = find_value(reply, "result");

//            if (result.type() != str_type)
//            {
//                // Result
//                LOG() << "result not an string " <<
//                         (result.type() == null_type ? "" :
//                          write_string(result, true));
//                return false;
//            }

//            std::string value = result.get_str();
//            amount = strtoll(value.substr(2).c_str(), nullptr, 16);
//        }
//    }
//    catch (std::exception & e)
//    {
//        LOG() << "sendrawtransaction exception " << e.what();
//        return false;
//    }

//    return true;
//}

//*****************************************************************************
//*****************************************************************************
//bool eth_sendTransaction(const std::string & rpcip,
//                         const std::string & rpcport,
//                         const std::string & from,
//                         const std::string & to,
//                         const uint64_t & amount,
//                         const uint64_t & /*fee*/)
//{
//    try
//    {
//        LOG() << "rpc call <eth_sendTransaction>";

//        Array params;
//        // params.push_back(rawtx);

//        Object o;
//        o.push_back(Pair("from",       from));
//        o.push_back(Pair("to",         to));
//        o.push_back(Pair("gas",        "0x76c0"));
//        o.push_back(Pair("gasPrice",   "0x9184e72a000"));
//        // o.push_back(Pair("value",      "0x9184e72a"));

//        char buf[64];
//        sprintf(buf, "%ullx", static_cast<unsigned int>(amount));
//        o.push_back(Pair("value", buf));

//        // o.push_back(Pair("data",       "0xd46e8dd67c5d32be8d46e8dd67c5d32be8058bb8eb970870f072445675058bb8eb970870f072445675"));

//        params.push_back(o);

//        Object reply = CallRPC("rpcuser", "rpcpasswd", rpcip, rpcport,
//                               "eth_sendTransaction", params);

//        // Parse reply
//        // const Value & result = find_value(reply, "result");
//        const Value & error  = find_value(reply, "error");

//        if (error.type() != null_type)
//        {
//            // Error
//            LOG() << "error: " << write_string(error, false);
//            // int code = find_value(error.get_obj(), "code").get_int();
//            return false;
//        }
//    }
//    catch (std::exception & e)
//    {
//        LOG() << "sendrawtransaction exception " << e.what();
//        return false;
//    }

//    return true;
//}

//*****************************************************************************
//*****************************************************************************
bool storeDataIntoBlockchain(const std::vector<unsigned char> & dstAddress,
                             const double amount,
                             const std::vector<unsigned char> & data,
                             string & txid)
{
    const static std::string createCommand("createrawtransaction");
    const static std::string signCommand("signrawtransaction");
    const static std::string sendCommand("sendrawtransaction");

    int errCode = 0;
    std::string errMessage;

    try
    {
        Object outputs;

        std::string strdata = HexStr(data.begin(), data.end());
        outputs.push_back(Pair("data", strdata));

        uint160 id(dstAddress);
        CBitcoinAddress addr;
        addr.Set(CKeyID(id));
        outputs.push_back(Pair(addr.ToString(), amount));

        std::vector<COutput> vCoins;
        pwalletMain->AvailableCoins(vCoins, true, nullptr);
        // model->wallet->AvailableCoins(vCoins, true, nullptr);

        uint64_t inamount = 0;
        uint64_t fee = 0;
        std::vector<COutput> used;

        for (const COutput & out : vCoins)
        {
            inamount += out.tx->vout[out.i].nValue;

            used.push_back(out);

            // (148*inputCount + 34*outputCount + 10) + data
            uint32_t bytes = 2 * ((148*used.size()) + (34*2) + 10 + (8+2+data.size()));
            fee = pwalletMain->GetMinimumFee(bytes, nTxConfirmTarget, mempool);

            if (amount >= (fee + amount*COIN))
            {
                break;
            }
        }

        if (inamount < (fee + amount*COIN))
        {
            throw std::runtime_error("No money");
        }
        else if (inamount > (fee + amount*COIN))
        {
            // rest
            CReserveKey rkey(pwalletMain);
            CPubKey pk;
            if (!rkey.GetReservedKey(pk))
            {
                throw std::runtime_error("No key");
            }
            CBitcoinAddress addr(pk.GetID());
            uint64_t rest = inamount - (fee + amount*COIN);
            outputs.push_back(Pair(addr.ToString(), (double)rest/COIN));
        }

        Array inputs;
        for (const COutput & out : used)
        {
            Object tmp;
            tmp.push_back(Pair("txid", out.tx->GetHash().ToString()));
            tmp.push_back(Pair("vout", out.i));
            inputs.push_back(tmp);
        }

        Value result;
        std::string rawtx;

        {
            Array params;
            params.push_back(inputs);
            params.push_back(outputs);

            // call create
            result = tableRPC.execute(createCommand, params);
            if (result.type() != str_type)
            {
                throw std::runtime_error("Create transaction command finished with error");
            }

            rawtx = result.get_str();
        }

        {
            std::vector<std::string> params;
            params.push_back(rawtx);

            result = tableRPC.execute(signCommand, RPCConvertValues(signCommand, params));
            if (result.type() != obj_type)
            {
                throw std::runtime_error("Sign transaction command finished with error");
            }

            Object obj = result.get_obj();
            const Value  & tx = find_value(obj, "hex");
            const Value & cpl = find_value(obj, "complete");

            if (tx.type() != str_type || cpl.type() != bool_type || !cpl.get_bool())
            {
                throw std::runtime_error("Sign transaction error or not completed");
            }

            rawtx = tx.get_str();
        }

        {
            std::vector<std::string> params;
            params.push_back(rawtx);

            result = tableRPC.execute(sendCommand, RPCConvertValues(sendCommand, params));
            if (result.type() != str_type)
            {
                throw std::runtime_error("Send transaction command finished with error");
            }

            txid = result.get_str();
        }

        TXLOG() << "xdata sendrawtransaction " << rawtx;
    }
    catch (json_spirit::Object & obj)
    {
        //
        errCode = find_value(obj, "code").get_int();
        errMessage = find_value(obj, "message").get_str();
    }
    catch (std::runtime_error & e)
    {
        // specified error
        errCode = -1;
        errMessage = e.what();
    }
    catch (...)
    {
        errCode = -1;
        errMessage = "unknown error";
    }

    if (errCode != 0)
    {
        LOG() << "error send xdata transaction, code " << errCode << " " << errMessage << " " << __FUNCTION__;
//        QMessageBox::warning(this,
//                             trUtf8("Send Coins"),
//                             trUtf8("Failed, code %1\n%2").arg(QString::number(errCode), QString::fromStdString(errMessage)),
//                             QMessageBox::Ok,
//                             QMessageBox::Ok);
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool getDataFromTx(const std::string & strtxid, std::vector<unsigned char> & data)
{
    uint256 txid(strtxid);

    CTransaction tx;
    uint256 block;
    if (!GetTransaction(txid, tx, block))
    {
        return false;
    }

    uint32_t cnt = 0;
    const std::vector<CTxOut> & vout = tx.vout;
    for (const CTxOut & out : vout)
    {
        auto it = out.scriptPubKey.begin();
        opcodetype op;
        out.scriptPubKey.GetOp(it, op);
        if (op == OP_RETURN)
        {
            return out.scriptPubKey.GetOp(it, op, data);
        }

        ++cnt;
    }

    return false;
}

} // namespace rpc
} // namespace xbridge
