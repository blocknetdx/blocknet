
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

static CCriticalSection cs_rpcBlockchainStore;

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

    if(fDebug)
        LOG() << "HTTP: req  " << strMethod << " " << strRequest;

    string strPost = HTTPPost(strRequest, mapRequestHeaders);
    stream << strPost << std::flush;

    // Receive reply
    map<string, string> mapHeaders;
    string strReply;
    int nStatus = readHTTP(stream, mapHeaders, strReply);

    if(fDebug)
        LOG() << "HTTP: resp " << nStatus << " " << strReply;

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
bool storeDataIntoBlockchain(const std::vector<unsigned char> & dstAddress,
                             const double amount,
                             const std::vector<unsigned char> & data,
                             string & txid)
{
    LOCK(cs_rpcBlockchainStore);

    const static std::string createCommand("createrawtransaction");
    const static std::string fundCommand("fundrawtransaction");
    const static std::string signCommand("signrawtransaction");
    const static std::string sendCommand("sendrawtransaction");

    int         errCode = 0;
    std::string errMessage;
    std::string rawtx;

    try
    {
        Object outputs;

        std::string strdata = HexStr(data.begin(), data.end());
        outputs.push_back(Pair("data", strdata));

        uint160 id(dstAddress);
        CBitcoinAddress addr;
        addr.Set(CKeyID(id));
        outputs.push_back(Pair(addr.ToString(), amount));

        std::vector<COutput> used;

        Array inputs;
        for (const COutput & out : used)
        {
            Object tmp;
            tmp.push_back(Pair("txid", out.tx->GetHash().ToString()));
            tmp.push_back(Pair("vout", out.i));
            inputs.push_back(tmp);
        }

        Value result;

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
            Array params;
            params.push_back(rawtx);

            // call fund
            result = tableRPC.execute(fundCommand, params);
            if (result.type() != obj_type)
            {
                throw std::runtime_error("Fund transaction command finished with error");
            }

            Object obj = result.get_obj();
            const Value  & tx = find_value(obj, "hex");
            if (tx.type() != str_type)
            {
                throw std::runtime_error("Fund transaction error or not completed");
            }

            rawtx = tx.get_str();
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
        TXERR() << "xdata sendrawtransaction " << rawtx;
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
