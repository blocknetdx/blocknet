#include "xrouterconnectoreth.h"
#include "xroutererror.h"

#include "uint256.h"
#include "tinyformat.h"
#include "rpcserver.h"
#include "rpcprotocol.h"
#include "rpcclient.h"

#include "json/json_spirit.h"
#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"

#include <stdio.h>
#include <cstdint>

#include <boost/asio.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/asio/ssl.hpp>

using namespace json_spirit;
using namespace std;
using namespace boost;
using namespace boost::asio;

namespace rpc
{

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

std::string CallRPC(const std::string & rpcip, const std::string & rpcport,
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

    return strReply;
}

}

namespace xrouter
{

static Value getResult(const std::string & obj)
{
    Value obj_val; read_string(obj, obj_val);
    if (obj_val.type() == null_type)
        return Value(obj);
    const Value & r = find_value(obj_val.get_obj(), "result");
    if (r.type() == null_type)
        return Value(obj);
    return r;
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
    static const std::string command("eth_blockNumber");
    return rpc::CallRPC(m_ip, m_port, command, Array());
}

std::string EthWalletConnectorXRouter::getBlockHash(const int & block) const
{
    static const std::string command("eth_getBlockByNumber");
    return rpc::CallRPC(m_ip, m_port, command, { dec2hex(block), false });
}

std::string EthWalletConnectorXRouter::getBlock(const std::string & blockHash) const
{
    static const std::string command("eth_getBlockByHash");
    return rpc::CallRPC(m_ip, m_port, command, { blockHash, true });
}

std::vector<std::string> EthWalletConnectorXRouter::getBlocks(const std::vector<std::string> & blockHashes) const
{
    std::vector<std::string> results;
    for (const auto & hash : blockHashes)
        results.push_back(getBlock(hash));
    return results;
}

std::string EthWalletConnectorXRouter::getTransaction(const std::string & trHash) const
{
    static const std::string command("eth_getTransactionByHash");
    return rpc::CallRPC(m_ip, m_port, command, { trHash });
}

std::string EthWalletConnectorXRouter::decodeRawTransaction(const std::string & trHash) const
{
    Object unsupported; unsupported.emplace_back("error", "Unsupported");
    return write_string(Value(unsupported), pretty_print);
}

std::vector<std::string> EthWalletConnectorXRouter::getTransactions(const std::vector<std::string> & txHashes) const
{
    std::vector<std::string> results;
    for (const auto & hash : txHashes)
        results.push_back(getTransaction(hash));
    return results;
}

std::vector<std::string> EthWalletConnectorXRouter::getTransactionsBloomFilter(const int &, CDataStream &, const int &) const
{
    Object unsupported; unsupported.emplace_back("error", "Unsupported");
    return std::vector<std::string>{write_string(Value(unsupported), pretty_print)};
}

std::string EthWalletConnectorXRouter::sendTransaction(const std::string & rawtx) const
{
    static const std::string command("eth_sendRawTransaction");
    return rpc::CallRPC(m_ip, m_port, command, { rawtx });
}

std::string EthWalletConnectorXRouter::convertTimeToBlockCount(const std::string & timestamp) const
{
    Object unsupported; unsupported.emplace_back("error", "Unsupported");
    return write_string(Value(unsupported), pretty_print);
}

std::string EthWalletConnectorXRouter::getBalance(const std::string & address) const
{
    Object unsupported; unsupported.emplace_back("error", "Unsupported");
    return write_string(Value(unsupported), pretty_print);
}

} // namespace xrouter
