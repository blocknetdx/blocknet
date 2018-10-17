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
    stream.expires_from_now(boost::posix_time::seconds(60));
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

std::string EthWalletConnectorXRouter::getBlockCount() const
{
    std::string command("eth_blockNumber");

    Object blockNumberObj = rpc::CallRPC(m_ip, m_port, command, Array());

    Value blockNumberVal = getResult(blockNumberObj);

    if(!blockNumberVal.is_null())
    {
        std::string hexValue = blockNumberVal.get_str();
        uint256 bigInt(hexValue);

        std::stringstream ss;

        bigInt.Serialize(ss, 0, 0);

        return ss.str();
    }

    return std::string();
}

std::string EthWalletConnectorXRouter::getBlockHash(const std::string & blockId) const
{
    uint256 blockId256(std::stoi(blockId));

    std::string command("eth_getBlockByNumber");
    Array params { blockId256.ToString(), false };

    Object resp = rpc::CallRPC(m_ip, m_port, command, params);

    Object blockHashObj = getResult(resp).get_obj();

    return find_value(blockHashObj, "hash").get_str();
}

Object EthWalletConnectorXRouter::getBlock(const std::string & blockHash) const
{
    std::string command("eth_getBlockByHash");
    Array params { blockHash, true };

    Object resp = rpc::CallRPC(m_ip, m_port, command, params);

    return getResult(resp).get_obj();
}

Object EthWalletConnectorXRouter::getTransaction(const std::string & trHash) const
{
    std::string command("eth_getTransactionByHash");
    Array params { trHash };

    Object resp = rpc::CallRPC(m_ip, m_port, command, params);

    return getResult(resp).get_obj();
}

Array EthWalletConnectorXRouter::getAllBlocks(const int number) const
{
    std::string commandBN("eth_blockNumber");
    std::string commandgGBBN("eth_getBlockByNumber");

    Object blockCountObj = rpc::CallRPC(m_ip, m_port, commandBN, Array());
    std::string hexValueStr = getResult(blockCountObj).get_str();
    uint256 blockCount(hexValueStr);

    Array result;

    for(uint256 id = number; id <= blockCount; id++)
    {
        Array params { id.ToString(), true };

        Object resp = rpc::CallRPC(m_ip, m_port, commandgGBBN, params);

        result.push_back(getResult(resp));
    }

    return result;
}

Array EthWalletConnectorXRouter::getAllTransactions(const std::string & account, const int number, const int time) const
{
    std::string commandBN("eth_blockNumber");
    std::string commandgGBBN("eth_getBlockByNumber");

    Object blockCountObj = rpc::CallRPC(m_ip, m_port, commandBN, Array());
    std::string hexValueStr = getResult(blockCountObj).get_str();
    uint256 blockCount(hexValueStr);

    Array result;

    for(uint256 id = number; id <= blockCount; id++)
    {
        Array params { id.ToString(), true };

        Object resp = rpc::CallRPC(m_ip, m_port, commandgGBBN, params);
        Object blockObj = getResult(resp).get_obj();

        const Array & transactionsInBlock = find_value(blockObj, "transactions").get_array();

        for(const Value & transaction : transactionsInBlock)
        {
            Object transactionObj = transaction.get_obj();

            std::string from = find_value(transactionObj, "from").get_str();

            if(from == account)
                result.push_back(transaction);
        }
    }

    return result;
}

std::string EthWalletConnectorXRouter::getBalance(const std::string & account, const int time) const
{
    std::string commandBN("eth_blockNumber");
    std::string commandgGBBN("eth_getBlockByNumber");

    Object blockCountObj = rpc::CallRPC(m_ip, m_port, commandBN, Array());
    std::string hexValueStr = getResult(blockCountObj).get_str();
    uint256 blockCount(hexValueStr);

    uint256 result;
    bool isPositive = true;

    for(uint256 id = 0; id <= blockCount; id++)
    {
        Array params { id.ToString(), true };

        Object resp = rpc::CallRPC(m_ip, m_port, commandgGBBN, params);
        Object blockObj = getResult(resp).get_obj();

        const Array & transactionsInBlock = find_value(blockObj, "transactions").get_array();

        for(const Value & transaction : transactionsInBlock)
        {
            Object transactionObj = transaction.get_obj();

            std::string from = find_value(transactionObj, "from").get_str();
            std::string to = find_value(transactionObj, "to").get_str();

            if(from == account)
            {
                uint256 value(find_value(transactionObj, "value").get_str());

                if(result < value)
                {
                    isPositive = false;
                    result = value - result;
                }
            }
            else if(to == account)
            {
                uint256 value(find_value(transactionObj, "value").get_str());

                if(isPositive)
                {
                    result += value;
                }
                else
                {
                    if(result > value)
                    {
                        result -= value;
                    }
                    else
                    {
                        result = value - result;
                        isPositive = true;
                    }
                }
            }
        }
    }

    std::stringstream ss;

    if(!isPositive)
        ss << "-";

    result.Serialize(ss, 0, 0);

    return ss.str();
}

std::string EthWalletConnectorXRouter::getBalanceUpdate(const std::string & account, const int number, const int time) const
{
    std::string commandBN("eth_blockNumber");
    std::string commandgGBBN("eth_getBlockByNumber");

    Object blockCountObj = rpc::CallRPC(m_ip, m_port, commandBN, Array());
    std::string hexValueStr = getResult(blockCountObj).get_str();
    uint256 blockCount(hexValueStr);

    uint256 result;
    bool isPositive = true;

    for(uint256 id = number; id <= blockCount; id++)
    {
        Array params { id.ToString(), true };

        Object resp = rpc::CallRPC(m_ip, m_port, commandgGBBN, params);
        Object blockObj = getResult(resp).get_obj();

        const Array & transactionsInBlock = find_value(blockObj, "transactions").get_array();

        for(const Value & transaction : transactionsInBlock)
        {
            Object transactionObj = transaction.get_obj();

            std::string from = find_value(transactionObj, "from").get_str();
            std::string to = find_value(transactionObj, "to").get_str();

            if(from == account)
            {
                uint256 value(find_value(transactionObj, "value").get_str());

                if(result < value)
                {
                    isPositive = false;
                    result = value - result;
                }
            }
            else if(to == account)
            {
                uint256 value(find_value(transactionObj, "value").get_str());

                if(isPositive)
                {
                    result += value;
                }
                else
                {
                    if(result > value)
                    {
                        result -= value;
                    }
                    else
                    {
                        result = value - result;
                        isPositive = true;
                    }
                }
            }
        }
    }

    std::stringstream ss;

    if(!isPositive)
        ss << "-";

    result.Serialize(ss, 0, 0);

    return ss.str();
}

Array EthWalletConnectorXRouter::getTransactionsBloomFilter(const int, CDataStream &) const
{
    // not realized for Ethereum
    return Array();
}

Object EthWalletConnectorXRouter::sendTransaction(const std::string &) const
{
    return Object();
}

Object EthWalletConnectorXRouter::getPaymentAddress() const
{
    return Object();
}

} // namespace xrouter
