#include "xrouterconnector.h"
#include "xrouterlogger.h"

#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <ctime>
#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

#include <boost/asio.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/locale.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/insert_linebreaks.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/ostream_iterator.hpp>

#include <stdio.h>

#include "rpcserver.h"
#include "rpcprotocol.h"
#include "rpcclient.h"
#include "base58.h"
#include "wallet.h"
#include "init.h"
#include "key.h"

namespace xrouter
{

using namespace json_spirit;
using namespace std;
using namespace boost;
using namespace boost::asio;
    
WalletConnectorXRouter::WalletConnectorXRouter()
{

}

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

std::string base64_encode(const std::string& s)
{
    namespace bai = boost::archive::iterators;
    std::string base64_padding[] = {"", "==","="};


    std::stringstream os;

    // convert binary values to base64 characters
    typedef bai::base64_from_binary
    // retrieve 6 bit integers from a sequence of 8 bit bytes
    <bai::transform_width<const char *, 6, 8> > base64_enc; // compose all the above operations in to a new iterator

    std::copy(base64_enc(s.c_str()), base64_enc(s.c_str() + s.size()),
            std::ostream_iterator<char>(os));

    os << base64_padding[s.size() % 3];
    return os.str();
}

static CMutableTransaction decodeTransaction(std::string tx)
{
    vector<unsigned char> txData(ParseHex(tx));
    CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
    CMutableTransaction result;
    ssData >> result;
    return result;
}

Object CallRPC(const std::string & rpcuser, const std::string & rpcpasswd,
               const std::string & rpcip, const std::string & rpcport,
               const std::string & strMethod, const Array & params)
{
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
    string strUserPass64 = base64_encode(rpcuser + ":" + rpcpasswd);
    map<string, string> mapRequestHeaders;
    mapRequestHeaders["Authorization"] = string("Basic ") + strUserPass64;

    // Send request
    string strRequest = JSONRPCRequest(strMethod, params, 1);

    LOG() << "HTTP: req  " << strMethod << " " << strRequest;

    string strPost = HTTPPost(strRequest, mapRequestHeaders);
    stream << strPost << std::flush;

    // Receive reply
    map<string, string> mapHeaders;
    string strReply;
    int nStatus = readHTTP(stream, mapHeaders, strReply);

    LOG() << "HTTP: resp " << nStatus << " " << strReply;

    if (nStatus == HTTP_UNAUTHORIZED)
        throw runtime_error("incorrect rpcuser or rpcpassword (authorization failed)");
    else if (nStatus >= 400 && nStatus != HTTP_BAD_REQUEST && nStatus != HTTP_NOT_FOUND && nStatus != HTTP_INTERNAL_SERVER_ERROR)
        throw runtime_error("server returned HTTP error " + nStatus);
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

std::string CallURL(std::string ip, std::string port, std::string url)
{
    // Connect to localhost
    bool fUseSSL = false;//GetBoolArg("-rpcssl");
    asio::io_service io_service;
    ssl::context context(io_service, ssl::context::sslv23);
    context.set_options(ssl::context::no_sslv2);
    asio::ssl::stream<asio::ip::tcp::socket> sslStream(io_service, context);
    SSLIOStreamDevice<asio::ip::tcp> d(sslStream, fUseSSL);
    iostreams::stream< SSLIOStreamDevice<asio::ip::tcp> > stream(d);
    if (!d.connect(ip, port))
        throw runtime_error("couldn't connect to server");

    // Send request
    ostringstream s;
    s << "GET " << url << "\r\n" << "Host: 127.0.0.1\r\n";
    string strRequest = s.str();

    LOG() << "HTTP: req  " << strRequest;

    map<string, string> mapRequestHeaders;
    stream << strRequest << std::flush;

    // Receive reply
    map<string, string> mapHeaders;
    string strReply;
    int nStatus = readHTTP(stream, mapHeaders, strReply);

    LOG() << "HTTP: resp " << nStatus << " " << strReply;

    if (nStatus >= 400 && nStatus != HTTP_BAD_REQUEST && nStatus != HTTP_NOT_FOUND && nStatus != HTTP_INTERNAL_SERVER_ERROR)
        throw runtime_error("server returned HTTP error " + nStatus);
    else if (strReply.empty())
        throw runtime_error("no response from server");

    return strReply;
}

std::string CallCMD(std::string cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::shared_ptr<FILE> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) throw std::runtime_error("popen() failed!");
    while (!feof(pipe.get())) {
        if (fgets(buffer.data(), 128, pipe.get()) != nullptr)
            result += buffer.data();
    }
    return result;
}

// TODO: make this common with xbridge or use xbridge function for storing
static CCriticalSection cs_rpcBlockchainStore;

bool createAndSignTransaction(Array txparams, std::string & raw_tx, bool fund, bool check_complete)
{
    LOCK(cs_rpcBlockchainStore);

    const static std::string createCommand("createrawtransaction");
    const static std::string fundCommand("fundrawtransaction");
    const static std::string signCommand("signrawtransaction");

    int         errCode = 0;
    std::string errMessage;
    std::string rawtx;

    try
    {
        Value result;

        {
            // call create
            result = tableRPC.execute(createCommand, txparams);
            LOG() << "Create transaction: " << json_spirit::write_string(Value(result), true);
            if (result.type() != str_type)
            {
                throw std::runtime_error("Create transaction command finished with error");
            }

            rawtx = result.get_str();
        }

        if (fund)
        {
            Array params;
            params.push_back(rawtx);

            // call fund
            result = tableRPC.execute(fundCommand, params);
            LOG() << "Fund transaction: " << json_spirit::write_string(Value(result), true);
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
            LOG() << "Sign transaction: " << json_spirit::write_string(Value(result), true);
            if (result.type() != obj_type)
            {
                throw std::runtime_error("Sign transaction command finished with error");
            }

            Object obj = result.get_obj();
            const Value  & tx = find_value(obj, "hex");
            const Value & cpl = find_value(obj, "complete");

            if (tx.type() != str_type)
            {
                throw std::runtime_error("Sign transaction error");
            }

            if (check_complete) {
                if (cpl.type() != bool_type || !cpl.get_bool())
                {
                    throw std::runtime_error("Sign transaction not complete");
                }
            }
            
            rawtx = tx.get_str();
        }
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
        LOG() << "xdata signrawtransaction " << rawtx;
        LOG() << "error sign transaction, code " << errCode << " " << errMessage << " " << __FUNCTION__;
        return false;
    }

    raw_tx = rawtx;
    
    return true;
}

bool createAndSignTransaction(std::string address, const double amount, string & raw_tx)
{
    Array outputs;
    Object out;
    out.push_back(Pair("address", address));
    out.push_back(Pair("amount", amount));
    outputs.push_back(out);
    Array inputs;
    Value result;

    Array params;
    params.push_back(inputs);
    params.push_back(outputs);
    return createAndSignTransaction(params, raw_tx);
}

std::string signTransaction(std::string& raw_tx)
{
    std::vector<std::string> params;
    params.push_back(raw_tx);

    const static std::string signCommand("signrawtransaction");
    Value result = tableRPC.execute(signCommand, RPCConvertValues(signCommand, params));
    LOG() << "Sign transaction: " << json_spirit::write_string(Value(result), true);
    if (result.type() != obj_type)
    {
        throw std::runtime_error("Sign transaction command finished with error");
    }

    Object obj = result.get_obj();
    const Value& tx = find_value(obj, "hex");
    return tx.get_str();
}

bool sendTransactionBlockchain(std::string raw_tx, std::string & txid)
{
    LOCK(cs_rpcBlockchainStore);

    const static std::string sendCommand("sendrawtransaction");

    int         errCode = 0;
    std::string errMessage;
    Value result;
    
    try
    {
        {
            std::vector<std::string> params;
            params.push_back(raw_tx);

            result = tableRPC.execute(sendCommand, RPCConvertValues(sendCommand, params));
            if (result.type() != str_type)
            {
                throw std::runtime_error("Send transaction command finished with error");
            }

            txid = result.get_str();
        }

        LOG() << "sendrawtransaction " << raw_tx;
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
        LOG() << "xdata sendrawtransaction " << raw_tx;
        LOG() << "error send xdata transaction, code " << errCode << " " << errMessage << " " << __FUNCTION__;
        return false;
    }

    return true;
}

bool sendTransactionBlockchain(std::string address, const double amount, std::string & txid)
{
    std::string raw_tx;
    bool res = createAndSignTransaction(address, amount, raw_tx);
    if (!res) {
        return false;
    }
    
    res = sendTransactionBlockchain(raw_tx, txid);
    return res;
}

PaymentChannel createPaymentChannel(CPubKey address, double deposit, int date)
{
    PaymentChannel channel;
    CScript inner;
    std::string raw_tx, txid;
    int vout = 0;
    
    int locktime = std::time(0) + date;
    CPubKey my_pubkey = pwalletMain->GenerateNewKey();
    CKey mykey;
    CKeyID mykeyID = my_pubkey.GetID();
    pwalletMain->GetKey(mykeyID, mykey);
          
    inner << OP_IF
                << address.GetID() << OP_CHECKSIGVERIFY
          << OP_ELSE
                << locktime << OP_CHECKLOCKTIMEVERIFY << OP_DROP
          << OP_ENDIF
          << mykeyID << OP_CHECKSIG;

    CScriptID id = CScriptID(inner);
    CBitcoinAddress scriptaddr;
    scriptaddr.Set(id);
    std::string resultScript = scriptaddr.ToString();
    resultScript = HexStr(inner.begin(), inner.end());
    
    
    Array outputs;
    Object out;
    std::stringstream sstream;
    sstream << std::hex << locktime;
    std::string hexdate = sstream.str();
    out.push_back(Pair("data", hexdate));
    Object out2;
    out2.push_back(Pair("script", resultScript));
    out2.push_back(Pair("amount", deposit));
    outputs.push_back(out);
    outputs.push_back(out2);
    Array inputs;

    Array params;
    params.push_back(inputs);
    params.push_back(outputs);    
    bool res = createAndSignTransaction(params, raw_tx);
    if (!res)
        return channel;
    
    res = sendTransactionBlockchain(raw_tx, txid);
    
    // Get the CLTV vout number
    const static std::string decodeCommand("decoderawtransaction");
    std::vector<std::string> dparams;
    dparams.push_back(raw_tx);

    Value result = tableRPC.execute(decodeCommand, RPCConvertValues(decodeCommand, dparams));
    Object obj = result.get_obj();
    Array vouts = find_value(obj, "vout").get_array();
    int i = 0;
    for (Value vout : vouts) {
        std::cout << json_spirit::write_string(vout, true) << std::endl << std::flush;
        Object script = find_value(vout.get_obj(), "scriptPubKey").get_obj();
        std::string vouttype = find_value(script, "type").get_str();
        if (vouttype == "nonstandard") {
            vout = i;
            break;
        }
        
        i++;
    }
    
    channel.raw_tx = raw_tx;
    channel.txid = txid;
    channel.value = 0.0;
    channel.key = mykey;
    channel.keyid = mykeyID;
    channel.vout = vout;
    
    return channel;
}

bool createAndSignChannelTransaction(PaymentChannel channel, std::string address, double deposit, double amount, std::string& raw_tx)
{
    Array outputs;
    Object out_me;
    CPubKey my_pubkey = pwalletMain->GenerateNewKey();
    CKey mykey;
    pwalletMain->GetKey(my_pubkey.GetID(), mykey);
    CKeyID mykeyID = my_pubkey.GetID();
    out_me.push_back(Pair("address", CBitcoinAddress(mykeyID).ToString()));
    out_me.push_back(Pair("amount", deposit-amount));
    outputs.push_back(out_me);
    Object out_srv;
    out_srv.push_back(Pair("address", address));
    out_srv.push_back(Pair("amount", amount));
    outputs.push_back(out_srv);
    
    Array inputs;
    Object inp;
    inp.push_back(Pair("txid", channel.txid));
    inp.push_back(Pair("vout", channel.vout));
    inputs.push_back(inp);
    Value result;

    
    Array params;
    params.push_back(inputs);
    params.push_back(outputs);
    
    const static std::string createCommand("createrawtransaction");
    const static std::string signCommand("signrawtransaction");

    int         errCode = 0;
    std::string errMessage;
    std::string rawtx;

    try
    {
        Value result;

        {
            // call create
            result = tableRPC.execute(createCommand, params);
            LOG() << "Create transaction: " << json_spirit::write_string(Value(result), true);
            if (result.type() != str_type)
            {
                throw std::runtime_error("Create transaction command finished with error");
            }

            rawtx = result.get_str();
        }

        {
            Array params;
            params.push_back(rawtx);
            Array signs;
            Object sign;
            //sign.push_back(Pair("txid", parts[0]));
            //sign.push_back(Pair("vout", stoi(parts[1])));
            sign.push_back(Pair("scriptPubKey", ""));
            signs.push_back(sign);
            params.push_back(signs);

            result = tableRPC.execute(signCommand, params);
            LOG() << "Sign transaction: " << json_spirit::write_string(Value(result), true);
            if (result.type() != obj_type)
            {
                throw std::runtime_error("Sign transaction command finished with error");
            }

            Object obj = result.get_obj();
            const Value  & tx = find_value(obj, "hex");

            if (tx.type() != str_type)
            {
                throw std::runtime_error("Sign transaction error");
            }
            
            rawtx = tx.get_str();
        }
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
        LOG() << "xdata signrawtransaction " << rawtx;
        LOG() << "error sign transaction, code " << errCode << " " << errMessage << " " << __FUNCTION__;
        return false;
    }

    raw_tx = rawtx;
    
    return true;
}

double getTxValue(std::string rawtx, std::string address, std::string type) {
    const static std::string decodeCommand("decoderawtransaction");
    std::vector<std::string> params;
    params.push_back(rawtx);

    Value result = tableRPC.execute(decodeCommand, RPCConvertValues(decodeCommand, params));
    if (result.type() != obj_type)
    {
        throw std::runtime_error("Decode transaction command finished with error");
    }

    Object obj = result.get_obj();
    Array vouts = find_value(obj, "vout").get_array();
    for (Value vout : vouts) {
        double val = find_value(vout.get_obj(), "value").get_real();        
        Object script = find_value(vout.get_obj(), "scriptPubKey").get_obj();
        std::string vouttype = find_value(script, "type").get_str();
        if (type == "nulldata")
            if (vouttype == "nulldata")
                return val;
            
        if (type == "nonstandard")
            if (vouttype == "nonstandard")
                return val;
            
        const Value & addr_val = find_value(script, "addresses");
        if (addr_val.is_null())
            continue;
        Array addr = addr_val.get_array();

        for (unsigned int k = 0; k != addr.size(); k++ ) {
            std::string cur_addr = Value(addr[k]).get_str();
            if (cur_addr == address)
                return val;
        }
    }
    
    return 0.0;
}

int getChannelExpiryTime(std::string rawtx) {
    const static std::string decodeCommand("decoderawtransaction");
    std::vector<std::string> params;
    params.push_back(rawtx);

    Value result = tableRPC.execute(decodeCommand, RPCConvertValues(decodeCommand, params));
    if (result.type() != obj_type)
    {
        throw std::runtime_error("Decode transaction command finished with error");
    }

    Object obj = result.get_obj();
    Array vouts = find_value(obj, "vout").get_array();
    for (Value vout : vouts) {            
        std::cout << json_spirit::write_string(vout, true) << std::endl << std::flush;
        Object script = find_value(vout.get_obj(), "scriptPubKey").get_obj();
        std::string vouttype = find_value(script, "type").get_str();
        if (vouttype != "nulldata")
            continue;
        std::string asmscript = find_value(script, "hex").get_str();
        if (asmscript.substr(0, 4) != "6a04") {
            // TODO: a better check of the script?
            return -1;
        }
        
        std::string hexdate = "0x" + asmscript.substr(4);
        std::cout << hexdate << std::endl << std::flush;
        
        int res = stoi(hexdate, 0, 16);
        return res;    
    }
    
    return -1;
}

} // namespace xrouter
