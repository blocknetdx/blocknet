#include "xrouterconnectoreth.h"
#include "uint256.h"
#include "rpcserver.h"
#include "rpcprotocol.h"
#include "rpcclient.h"
#include "tinyformat.h"
#include <boost/asio.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/asio/ssl.hpp>
#include <stdio.h>
#include <cstdint>
#include "xroutererror.h"

namespace rpc
{

using namespace json_spirit;
using namespace std;
using namespace boost;
using namespace boost::asio;

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

Object CallRPC(const std::string & rpcip, const std::string & rpcport,
               const std::string & strMethod, const Array & params)
{
    boost::asio::ip::tcp::iostream stream;
    stream.expires_from_now(boost::posix_time::seconds(GetArg("-rpcxroutertimeout", 60)));
    stream.connect(rpcip, rpcport);
    if (stream.error() != boost::system::errc::success) {
        LogPrint("net", "Failed to make rpc connection to %s:%s error %d: %s", rpcip, rpcport, stream.error(), stream.error().message());
        throw runtime_error(strprintf("no response from server %s:%s - %s", rpcip.c_str(), rpcport.c_str(),
                                      stream.error().message().c_str()));
    }

    // Send request
    string strRequest = JSONRPCRequest(strMethod, params, 1);
    map<string, string> mapRequestHeaders;

    string strPost = HTTPPost(strRequest, mapRequestHeaders);
    stream << strPost << std::flush;

    // Receive reply
    map<string, string> mapHeaders;
    string strReply;
    int nStatus = readHTTP(stream, mapHeaders, strReply);

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

}

namespace xrouter
{

static Value getResult(Object obj) {
    for (Object::size_type i = 0; i != obj.size(); i++ ) {
        if (obj[i].name_ == "result") {
            return obj[i].value_;
        }
    }
    return Value();
}

static std::string dec2hex(const int & s) {
    std::stringstream ss;
    ss << std::hex << s;
    return "0x" + ss.str();
}

static std::string dec2hex(const std::string & s) {
    return dec2hex(std::stoi(s));
}

static int hex2dec(std::string s) {
    std::stringstream ss;
    unsigned int result;
    ss << std::hex << s;
    ss >> result;
    return result;
}

std::string EthWalletConnectorXRouter::getBlockCount() const
{
    std::string command("eth_blockNumber");

    Object blockNumberObj = rpc::CallRPC(m_ip, m_port, command, Array());

    Value blockNumberVal = getResult(blockNumberObj);

    if(!blockNumberVal.is_null())
    {
        return std::to_string(hex2dec(blockNumberVal.get_str()));
    }

    return std::string();
}

Object EthWalletConnectorXRouter::getBlockHash(const int & block) const
{
    std::string command("eth_getBlockByNumber");
      
    Array params { dec2hex(block), false };

    Object resp = rpc::CallRPC(m_ip, m_port, command, params);

    Object blockResult = getResult(resp).get_obj();
    std::string hash = find_value(blockResult, "hash").get_str();

    Object result;
    result.emplace_back(Pair("result", hash));
    return result;
}

Object EthWalletConnectorXRouter::getBlock(const std::string & blockHash) const
{
    std::string command("eth_getBlockByHash");
    Array params { blockHash, true };

    Object resp = rpc::CallRPC(m_ip, m_port, command, params);

    return getResult(resp).get_obj();
}

Array EthWalletConnectorXRouter::getBlocks(const std::vector<std::string> & blockHashes) const
{
    static const std::string commandBN("eth_getBlockByHash");

    Array result;
    for (const auto & hash : blockHashes) {
        Array params { hash, true };
        Object resp = rpc::CallRPC(m_ip, m_port, commandBN, params);
        result.push_back(getResult(resp));
    }
    return result;
}

Object EthWalletConnectorXRouter::getTransaction(const std::string & trHash) const
{
    std::string command("eth_getTransactionByHash");
    Array params { trHash };

    Object resp = rpc::CallRPC(m_ip, m_port, command, params);

    return getResult(resp).get_obj();
}

Object EthWalletConnectorXRouter::decodeRawTransaction(const std::string & trHash) const
{
    Object unsupported; unsupported.emplace_back("error", "Unsupported");
    return unsupported;
}

Array EthWalletConnectorXRouter::getTransactions(const std::vector<std::string> & txHashes) const
{
    Array result;
    for (const auto & hash : txHashes) {
        result.push_back(getTransaction(hash));
    }
    return result;
}

Array EthWalletConnectorXRouter::getTransactionsBloomFilter(const int &, CDataStream &, const int &) const
{
    // TODO Implement
    return Array();
}

Object EthWalletConnectorXRouter::sendTransaction(const std::string &) const
{
    // TODO: implement
    return Object();
}

std::string EthWalletConnectorXRouter::convertTimeToBlockCount(const std::string & timestamp) const
{
    // TODO: implement
    return "";
}

std::string EthWalletConnectorXRouter::getBalance(const std::string & address) const
{
    return "0";
}

} // namespace xrouter
