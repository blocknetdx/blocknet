//*****************************************************************************
//*****************************************************************************

#include "xbridgewalletconnectoreth.h"
#include "xbridgewalletconnectorbtc.h"

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

#include "base58.h"
#include "uint256.h"

#include "util/logger.h"
#include "util/txlog.h"

#include "xbridgecryptoproviderbtc.h"
#include "xbitcoinaddress.h"
#include "xbitcointransaction.h"

#include "rpc/server.h"
#include "rpc/protocol.h"
#include "rpc/client.h"
#include "tinyformat.h"

#include <boost/asio.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/asio/ssl.hpp>
#include <stdio.h>

// int ReadHTTPStatus(std::basic_istream<char>& stream, int &proto);
// int ReadHTTPHeader(std::basic_istream<char>& stream, std::map<std::string, std::string>& mapHeadersRet);


//*****************************************************************************
//*****************************************************************************
namespace xbridge
{

//*****************************************************************************
//*****************************************************************************
struct Block
{
    uint256  hash;
    uint64_t timestamp;
};

//*****************************************************************************
//*****************************************************************************
// static
// json_spirit::Object CallRPC(const std::string & rpcuser, const std::string & rpcpasswd,
//                             const std::string & rpcip, const std::string & rpcport,
//                             const std::string & strMethod, const json_spirit::Array & params,
//                             const std::string & jsonver="", const std::string & contenttype="");

//*****************************************************************************
//*****************************************************************************
json_spirit::Object CallRPC(const std::string & rpcip, const std::string & rpcport,
                            const std::string & strMethod, const json_spirit::Array & params)
{
    return CallRPC("", "", rpcip, rpcport, strMethod, params, "", "application/json");
}

//*****************************************************************************
//*****************************************************************************
namespace rpc
{

using namespace json_spirit;
using namespace std;
using namespace boost;
using namespace boost::asio;

// int readHTTPEth(std::basic_istream<char>& stream, map<string, string>& mapHeadersRet, string& strMessageRet)
// {
//     mapHeadersRet.clear();
//     strMessageRet = "";

//     // Read status
//     int nProto = 0;
//     int nStatus = ReadHTTPStatus(stream, nProto);

//     // Read header
//     int nLen = ReadHTTPHeader(stream, mapHeadersRet);
//     if (nLen < 0 || nLen > (int)MAX_SIZE)
//         return HTTP_INTERNAL_SERVER_ERROR;

//     // Read message
//     if (nLen > 0)
//     {
//         vector<char> vch(nLen);
//         stream.read(&vch[0], nLen);
//         strMessageRet = string(vch.begin(), vch.end());
//     }
//     else
//     {
//         nLen = ReadHTTPChunk(stream);
//         while (nLen)
//         {
//             vector<char> vch(nLen);
//             stream.read(&vch[0], nLen);
//             strMessageRet += string(vch.begin(), vch.end());

//             nLen = ReadHTTPChunk(stream);
//         }
//     }

//     string sConHdr = mapHeadersRet["connection"];

//     if ((sConHdr != "close") && (sConHdr != "keep-alive"))
//     {
//         if (nProto >= 1)
//             mapHeadersRet["connection"] = "keep-alive";
//         else
//             mapHeadersRet["connection"] = "close";
//     }

//     return nStatus;
// }

// Object CallRPC(const std::string & rpcip, const std::string & rpcport,
//                const std::string & strMethod, const Array & params)
// {
//     // Connect to localhost
//     bool fUseSSL = false;
//     asio::io_service io_service;
//     ssl::context context(io_service, ssl::context::sslv23);
//     context.set_options(ssl::context::no_sslv2);
//     asio::ssl::stream<asio::ip::tcp::socket> sslStream(io_service, context);
//     SSLIOStreamDevice<asio::ip::tcp> d(sslStream, fUseSSL);
//     iostreams::stream< SSLIOStreamDevice<asio::ip::tcp> > stream(d);
//     if (!d.connect(rpcip, rpcport))
//         throw runtime_error("couldn't connect to server");

//     // Send request
//     string strRequest = JSONRPCRequest(strMethod, params, 1);
//     map<string, string> mapRequestHeaders;

//     string strPost = HTTPPost(strRequest, mapRequestHeaders);
//     stream << strPost << std::flush;

//     // Receive reply
//     map<string, string> mapHeaders;
//     string strReply;
//     int nStatus = readHTTPEth(stream, mapHeaders, strReply);

//     if (nStatus == HTTP_UNAUTHORIZED)
//         throw runtime_error("incorrect rpcuser or rpcpassword (authorization failed)");
//     else if (nStatus >= 400 && nStatus != HTTP_BAD_REQUEST && nStatus != HTTP_NOT_FOUND && nStatus != HTTP_INTERNAL_SERVER_ERROR)
//         throw runtime_error(strprintf("server returned HTTP error %d", nStatus));
//     else if (strReply.empty())
//         throw runtime_error("no response from server");

//     // Parse reply
//     Value valReply;
//     if (!read_string(strReply, valReply))
//         throw runtime_error("couldn't parse reply from server");
//     const Object& reply = valReply.get_obj();
//     if (reply.empty())
//         throw runtime_error("expected reply to have result, error and id properties");

//     return reply;
// }

//*****************************************************************************
//*****************************************************************************
namespace
{

//*****************************************************************************
//*****************************************************************************
bool getAccounts(const std::string & rpcip,
                 const std::string & rpcport,
                 std::vector<std::string> & accounts)
{
    try
    {
        LOG() << "rpc call <eth_accounts>";

        Array params;
        Object reply = CallRPC(rpcip, rpcport,
                               "eth_accounts", params);

        // Parse reply
        const Value & result = find_value(reply, "result");
        const Value & error  = find_value(reply, "error");

        if (error.type() != null_type)
        {
            // Error
            LOG() << "error: " << write_string(error, false);
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
                accounts.push_back(v.get_str());
            }
        }
    }
    catch (std::exception & e)
    {
        LOG() << "getAccounts exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool getBalance(const std::string & rpcip,
                const std::string & rpcport,
                const uint160 & account,
                uint256 & balance)
{
    try
    {
        LOG() << "rpc call <eth_getBalance>";

        Array params;
        params.emplace_back(as0xString(account));
        params.emplace_back("latest");
        Object reply = CallRPC(rpcip, rpcport,
                               "eth_getBalance", params);

        // Parse reply
        const Value & result = find_value(reply, "result");
        const Value & error  = find_value(reply, "error");

        if (error.type() != null_type)
        {
            // Error
            LOG() << "error: " << write_string(error, false);
            return false;
        }
        else if (result.type() != str_type)
        {
            // Result
            LOG() << "result not an string " << write_string(result, true);
            return false;
        }

        balance = uint256(result.get_str());
    }
    catch (std::exception & e)
    {
        LOG() << "getAccounts exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool sendTransaction(const std::string & rpcip,
                     const std::string & rpcport,
                     const uint160 & from,
                     const uint160 & to,
                     const uint256 & gas,
                     const uint256 & value,
                     const bytes & data,
                     uint256 & transactionHash)
{
    try
    {
        LOG() << "rpc call <personal_sendTransaction>";

        Array params;

        Object transaction;
        transaction.push_back(Pair("from", as0xString(from)));
        transaction.push_back(Pair("to", as0xString(to)));
        if(!gas.IsNull())
            transaction.push_back(Pair("gas", as0xStringNumber(gas)));
        if(!value.IsNull())
            transaction.push_back(Pair("value", as0xStringNumber(value)));
        transaction.push_back(Pair("data", as0xString(data)));

        params.push_back(transaction);

        // TODO empty password
        params.push_back("");

        Object reply = CallRPC(rpcip, rpcport,
                               "personal_sendTransaction", params);

        // Parse reply
        const Value & result = find_value(reply, "result");
        const Value & error  = find_value(reply, "error");

        if (error.type() != null_type)
        {
            // Error
            LOG() << "error: " << write_string(error, false);
            return false;
        }
        else if (result.type() != str_type)
        {
            // Result
            LOG() << "result not a string ";
            return false;
        }

        transactionHash = uint256(result.get_str());
    }
    catch (std::exception & e)
    {
        LOG() << "sendTransaction exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool getTransactionByHash(const std::string & rpcip,
                          const std::string & rpcport,
                          const uint256 & txHash,
                          uint256 & txBlockNumber)
{
    try
    {
        LOG() << "rpc call <eth_getTransactionByHash>";

        Array params;
        params.push_back(as0xString(txHash));
        Object reply = CallRPC(rpcip, rpcport,
                               "eth_getTransactionByHash", params);

        // Parse reply
        const Value & error  = find_value(reply, "error");
        if (error.type() != null_type)
        {
            // Error
            LOG() << "error: " << write_string(error, false);
            int errorCode = find_value(error.get_obj(), "code").get_int();
            return false;
        }

        const Value & result = find_value(reply, "result");
        if (result.type() != obj_type)
        {
            // Result
            LOG() << "result not an string " << write_string(result, true);
            return false;
        }

        const Value & blockNumber = find_value(result.get_obj(), "blockNumber");
        if(blockNumber.type() != str_type)
        {
            LOG() << "blockNumber not an string " << write_string(blockNumber, true);
            return false;
        }

        txBlockNumber = uint256(blockNumber.get_str());
    }
    catch (std::exception & e)
    {
        LOG() << "getTransactionByHash exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool getBlockNumber(const std::string & rpcip,
                    const std::string & rpcport,
                    uint64_t & blockNumber)
{
    try
    {
        LOG() << "rpc call <eth_blockNumber>";

        Array params;
        Object reply = CallRPC(rpcip, rpcport,
                               "eth_blockNumber", params);

        // Parse reply
        const Value & result = find_value(reply, "result");
        const Value & error  = find_value(reply, "error");

        if (error.type() != null_type)
        {
            // Error
            LOG() << "error: " << write_string(error, false);
            return false;
        }
        else if (result.type() != str_type)
        {
            // Result
            LOG() << "result not an string " << write_string(result, true);
            return false;
        }

        // 0x
        blockNumber = std::stoi(result.get_str(), nullptr, 16);
    }
    catch (std::exception & e)
    {
        LOG() << "getBlockNumber exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool getNetVersion(const std::string & rpcip,
                    const std::string & rpcport,
                    int & networkVersion)
{
    try
    {
        LOG() << "rpc call <net_version>";

        Array params;
        Object reply = CallRPC(rpcip, rpcport,
                               "net_version", params);

        // Parse reply
        const Value & result = find_value(reply, "result");
        const Value & error  = find_value(reply, "error");

        if (error.type() != null_type)
        {
            // Error
            LOG() << "error: " << write_string(error, false);
            return false;
        }
        else if (result.type() != str_type)
        {
            // Result
            LOG() << "result not an string " << write_string(result, true);
            return false;
        }

        networkVersion = stoi(result.get_str());
    }
    catch (std::exception & e)
    {
        LOG() << "getNetVersion exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool getBlock(const std::string & rpcip,
              const std::string & rpcport,
              const uint64_t blockNumber,
              Block & block)
{
    try
    {
        LOG() << "rpc call <eth_getBlock>";

        Object reply = CallRPC(rpcip, rpcport,
                               "eth_getBlockByNumber", Array{as0xString(blockNumber), true});

        // Parse reply
        const Value & result = find_value(reply, "result");
        const Value & error  = find_value(reply, "error");

        if (error.type() != null_type)
        {
            // Error
            LOG() << "error: " << write_string(error, false);
            return false;
        }
        else if (result.type() != obj_type)
        {
            // Result
            LOG() << "result not an object " << write_string(result, true);
            return false;
        }

        const Value & blockHashValue = find_value(result.get_obj(), "hash");
        if(blockHashValue.type() != str_type)
        {
            LOG() << "hash not an string " << write_string(blockHashValue, true);
            return false;
        }

        block.hash = uint256S(blockHashValue.get_str());

        const Value & blockTimestampValue = find_value(result.get_obj(), "timestamp");
        if(blockTimestampValue.type() != str_type)
        {
            LOG() << "timestamp not an integer " << write_string(blockTimestampValue, true);
            return false;
        }

        block.timestamp = std::stoi(blockTimestampValue.get_str(), nullptr, 16);
    }
    catch (std::exception & e)
    {
        LOG() << "getBlockNumber exception " << e.what();
        return false;
    }

    return true;

}

//*****************************************************************************
//*****************************************************************************
bool getLastBlockTime(const std::string & rpcip,
                      const std::string & rpcport,
                      uint256 & blockTimestamp)
{
    try
    {
        LOG() << "rpc call <eth_blockNumber>";

        Object reply = CallRPC(rpcip, rpcport,
                               "eth_getBlockByNumber", Array{"latest", true});

        // Parse reply
        const Value & result = find_value(reply, "result");
        const Value & error  = find_value(reply, "error");

        if (error.type() != null_type)
        {
            // Error
            LOG() << "error: " << write_string(error, false);
            return false;
        }
        else if (result.type() != obj_type)
        {
            // Result
            LOG() << "result not an object " << write_string(result, true);
            return false;
        }

        const Value & blockTimestampValue = find_value(result.get_obj(), "timestamp");
        if(blockTimestampValue.type() != str_type)
        {
            LOG() << "timestamp not an string " << write_string(blockTimestampValue, true);
            return false;
        }

        blockTimestamp = uint256(blockTimestampValue.get_str());
    }
    catch (std::exception & e)
    {
        LOG() << "getBlockNumber exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool getGasPrice(const std::string & rpcip,
                 const std::string & rpcport,
                 uint256 & gasPrice)
{
    try
    {
        LOG() << "rpc call <eth_gasPrice>";

        Array params;
        Object reply = CallRPC(rpcip, rpcport,
                               "eth_gasPrice", params);

        // Parse reply
        const Value & result = find_value(reply, "result");
        const Value & error  = find_value(reply, "error");

        if (error.type() != null_type)
        {
            // Error
            LOG() << "error: " << write_string(error, false);
            return false;
        }
        else if (result.type() != str_type)
        {
            // Result
            LOG() << "result not a string ";
            return false;
        }

        gasPrice = uint256(result.get_str());
    }
    catch (std::exception & e)
    {
        LOG() << "getGasPrice exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool getEstimateGas(const std::string & rpcip,
                    const std::string & rpcport,
                    const uint160 & from,
                    const uint160 & to,
                    const uint256 & value,
                    const bytes & data,
                    uint256 & estimateGas)
{
    try
    {
        LOG() << "rpc call <eth_estimateGas>";

        Array params;

        Object transaction;
        transaction.push_back(Pair("from", as0xString(from)));
        transaction.push_back(Pair("to", as0xString(to)));
        if(!value.IsNull())
            transaction.push_back(Pair("value", as0xStringNumber(value)));
        transaction.push_back(Pair("data", as0xString(data)));

        params.push_back(transaction);

        Object reply = CallRPC(rpcip, rpcport,
                               "eth_estimateGas", params);

        // Parse reply
        const Value & result = find_value(reply, "result");
        const Value & error  = find_value(reply, "error");

        if (error.type() != null_type)
        {
            // Error
            LOG() << "error: " << write_string(error, false);
            return false;
        }
        else if (result.type() != str_type)
        {
            // Result
            LOG() << "result not a string ";
            return false;
        }

        estimateGas = uint256(result.get_str());
    }
    catch (std::exception & e)
    {
        LOG() << "getEstimateGas exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool getLogs(const std::string & rpcip,
             const std::string & rpcport,
             const uint160 & address,
             const uint64_t & fromBlock,
             const std::string & topic,
             std::vector<std::string> & events,
             std::vector<std::string> & data)
{
    try
    {
        LOG() << "rpc call <eth_getLogs>";

        Array params;

        Object filter;
        filter.push_back(Pair("fromBlock", as0xString(fromBlock)));
        filter.push_back(Pair("toBlock", "latest"));
        filter.push_back(Pair("address", as0xString(address)));
        filter.push_back(Pair("topics", Array{Value(), as0xString(topic)}));

        params.push_back(filter);

        Object reply = CallRPC(rpcip, rpcport,
                               "eth_getLogs", params);

        // Parse reply
        const Value & result = find_value(reply, "result");
        const Value & error  = find_value(reply, "error");

        if (error.type() != null_type)
        {
            // Error
            LOG() << "error: " << write_string(error, false);
            return false;
        }
        else if (result.type() != array_type)
        {
            // Result
            LOG() << "result not a array";
            return false;
        }

        for(const Value & logValue : result.get_array())
        {
            if(logValue.type() != obj_type)
            {
                LOG() << "log not a object";
                return false;
            }

            const Object & log = logValue.get_obj();

            const Value & dataValue = find_value(log, "data");
            if(dataValue.type() != str_type)
            {
                LOG() << "data not a string";
                return false;
            }

            data.emplace_back(dataValue.get_str());


            const Value & topicsValue = find_value(log, "topics");
            if(topicsValue.type() != array_type)
            {
                LOG() << "data not a array";
                return false;
            }

            const Value & eventValue = topicsValue.get_array().at(0);
            if(eventValue.type() != str_type)
            {
                LOG() << "event not a string";
                return false;
            }

            events.emplace_back(eventValue.get_str());
        }
    }
    catch (std::exception & e)
    {
        LOG() << "newFilter exception " << e.what();
        return false;
    }

    return true;
}

} // namespace

} // namespace rpc

//*****************************************************************************
//*****************************************************************************
bool EthWalletConnector::init()
{
    if (!rpc::getBlockNumber(m_ip, m_port, m_fromBlock))
    {
        return false;
    }

    int netVersion = 0;
    if (!rpc::getNetVersion(m_ip, m_port, netVersion))
    {
        return false;
    }

    m_networkType = netVersion == 1 ? MAINNET : TESTNET;

    return true;
}

//*****************************************************************************
//*****************************************************************************
std::string EthWalletConnector::fromXAddr(const std::vector<unsigned char> & xaddr) const
{
    std::string result("0x");
    result.append(HexStr(xaddr));
    return result;
}

//*****************************************************************************
//*****************************************************************************
std::vector<unsigned char> EthWalletConnector::toXAddr(const std::string & addr) const
{
    std::string addressWithout0x(addr.begin() + 2, addr.end());
    std::vector<unsigned char> vch = ParseHex(addressWithout0x);
    return vch;
}

//*****************************************************************************
//*****************************************************************************
bool EthWalletConnector::getNewAddress(std::string & addr, const std::string & /*type*/) 
{ 
    std::vector<std::string> accounts;
    if (!rpc::getAccounts(m_ip, m_port, accounts))
    {
        return false;
    }

    if (accounts.empty())
    {
        addr = "NOT CREATED";
        return false;
    }

    addr = accounts[0]; 
    return true; 
}

//*****************************************************************************
//*****************************************************************************
bool EthWalletConnector::requestAddressBook(std::vector<wallet::AddressBookEntry> & entries)
{
    std::vector<std::string> accounts;
    if (!rpc::getAccounts(m_ip, m_port, accounts))
        return false;

    entries.push_back(std::make_pair("default", accounts));

    return true;
}

//*****************************************************************************
//*****************************************************************************
amount_t EthWalletConnector::getWalletBalance(const std::set<wallet::UtxoEntry> & excluded, const std::string &addr) const
{
    uint256 balance;

    if(addr.empty())
    {
        std::vector<std::string> accounts;
        if(!rpc::getAccounts(m_ip, m_port, accounts))
        {
            return 0;
        }

        for(const std::string & account : accounts)
        {
            uint256 accountBalance;
            if(!rpc::getBalance(m_ip, m_port, uint160(account), accountBalance))
            {
                return 0;
            }

            balance += accountBalance;
        }
    }
    else
    {
        if(!rpc::getBalance(m_ip, m_port, uint160(addr), balance))
        {
            return 0;
        }
    }

    return balance.getdouble();
}

//******************************************************************************
//******************************************************************************
bool EthWalletConnector::getBlockHash(const uint32_t & blockNumber, std::string & blockHash)
{
    Block block;
    if (!rpc::getBlock(m_ip, m_port, blockNumber, block))
    {
        LOG() << "getBlock failed";
        return false;
    }

    blockHash = block.hash.ToString();
    return true;
}

//******************************************************************************
//******************************************************************************
bool EthWalletConnector::getBlockCount(uint32_t & blockCount)
{
    // TODO make uint64 in param
    uint64_t count = 0;
    if (!rpc::getBlockNumber(m_ip, m_port, count))
    {
        LOG() << "getBlockNumber failed";
        return false;
    }

    blockCount = static_cast<uint32_t>(count);
    return true;
}

//******************************************************************************
//******************************************************************************
bool EthWalletConnector::newKeyPair(std::vector<unsigned char> & pubkey,
                                    std::vector<unsigned char> & privkey)
{
    m_cp->makeNewKey(privkey);
    return m_cp->getPubKey(privkey, pubkey);
}

//******************************************************************************
//******************************************************************************
std::vector<unsigned char> EthWalletConnector::getKeyId(const std::vector<unsigned char> & pubkey)
{
    uint160 id = Hash160(&pubkey[0], &pubkey[0] + pubkey.size());
    return std::vector<unsigned char>(id.begin(), id.end());
}

//******************************************************************************
// return false if deposit tx not found (need wait tx)
// true if tx found and checked
// isGood == true id depost tx is OK
//******************************************************************************
bool EthWalletConnector::checkDepositTransaction(const std::string& depositTxId, 
                                                 const std::string&, 
                                                 amount_t & amount, 
                                                 amount_t & p2shAmount,
                                                 uint32_t & depositTxVout,
                                                 const std::string & expectedScript,
                                                 amount_t & excessAmount,
                                                 bool& isGood)
{
    isGood  = false;

   uint256 txBlockNumber;
   if (!rpc::getTransactionByHash(m_ip, m_port, uint256(depositTxId), txBlockNumber))
   {
       LOG() << "no tx found " << depositTxId << " " << __FUNCTION__;
       return false;
   }

   uint64_t lastBlockNumber;
   if (!rpc::getBlockNumber(m_ip, m_port, lastBlockNumber))
   {
       LOG() << "can't get last block number " << depositTxId << " " << __FUNCTION__;
       return false;
   }

   if (requiredConfirmations > 0 && requiredConfirmations > (lastBlockNumber - txBlockNumber))
   {
       LOG() << "tx " << depositTxId << " unconfirmed, need " << requiredConfirmations << " " << __FUNCTION__;
       return false;
   }

    // TODO check amount in tx

    isGood = true;

    return true;
}

//******************************************************************************
//******************************************************************************
uint32_t EthWalletConnector::lockTime(const char role) const
{
    uint32_t lt = 0;
    if (role == 'A')
    {
        // 2h in seconds
        lt = 7200;
    }
    else if (role == 'B')
    {
        // 1h in seconds
        lt = 3600;
    }

    return lt;
}

//******************************************************************************
//******************************************************************************
bool EthWalletConnector::acceptableLockTimeDrift(const char role, const uint32_t lckTime) const
{
    auto lt = lockTime(role);
    if (lt == 0 || lt >= LOCKTIME_THRESHOLD || lckTime >= LOCKTIME_THRESHOLD)
    {
        return false;
    }

    const int64_t diff = static_cast<int64_t>(lt) - static_cast<int64_t>(lckTime);
    // Locktime drift is at minimum XLOCKTIME_DRIFT_SECONDS. In cases with slow chains
    // the locktime drift will increase to block time multiplied by the number of
    // blocks representing the maximum allowed locktime drift.
    //
    // If drift determination changes here, update wallet validation checks and unit
    // tests.
    int64_t drift = std::max<int64_t>(XLOCKTIME_DRIFT_SECONDS, XMAX_LOCKTIME_DRIFT_BLOCKS * blockTime);
    return diff * static_cast<int64_t>(blockTime) <= drift;
}

//*****************************************************************************
//*****************************************************************************
bool EthWalletConnector::getAccounts(std::vector<std::string> & accounts)
{
    if(!rpc::getAccounts(m_ip, m_port, accounts))
    {
        LOG() << "can't get accounts" << __FUNCTION__;
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool EthWalletConnector::getBalance(const bytes & account, uint256 & balance) const
{
    if(!rpc::getBalance(m_ip, m_port, uint160(HexStr(account)), balance))
    {
        LOG() << "can't get balance" << __FUNCTION__;
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool EthWalletConnector::getGasPrice(uint256 & gasPrice) const
{
    if(!rpc::getGasPrice(m_ip, m_port, gasPrice))
    {
        LOG() << "can't get gasPrice" << __FUNCTION__;
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool EthWalletConnector::getEstimateGas(const bytes & myAddress,
                                        const bytes & data,
                                        const uint256 & value,
                                        uint256 & estimateGas) const
{
    if(!rpc::getEstimateGas(m_ip, m_port,
                            uint160(HexStr(myAddress)),
                            uint160(m_networkType == MAINNET ? m_contractAddress : m_contractAddressTestnet),
                            value, data,
                            estimateGas))
    {
        LOG() << "can't get estimate gas" << __FUNCTION__;
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool EthWalletConnector::getLastBlockTime(uint256 & blockTime) const
{
    if (!rpc::getLastBlockTime(m_ip, m_port, blockTime))
    {
        LOG() << "can't get last block time " << __FUNCTION__;
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool splitEventParams(const std::string & paramsString, std::vector<std::string> & paramsVector)
{
    const unsigned int paramSize = 64;

    if(paramsString.size() < paramSize)
    {
        return false;
    }

    if((paramsString.size() - 2) % paramSize != 0)
    {
        return false;
    }

    //first 2 chars is 0x, so skip it
    for(unsigned int i = 2; i < paramsString.size(); i += paramSize)
    {
        paramsVector.emplace_back(paramsString.substr(i, paramSize));
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bytes EthWalletConnector::createInitiateData(const bytes & hashedSecret,
                                             const bytes & responderAddress,
                                             const uint256 & refundDuration) const
{
    bytes initiateMethodSignature = EthEncoder::encodeSig("initiate(bytes20,address,uint256)");
    bytes data = initiateMethodSignature +
            EthEncoder::encode(hashedSecret, false) +
            EthEncoder::encode(responderAddress) +
            EthEncoder::encode(refundDuration);

    return data;
}

//*****************************************************************************
//*****************************************************************************
bytes EthWalletConnector::createRespondData(const bytes & hashedSecret,
                                            const bytes & initiatorAddress,
                                            const uint256 & refundDuration) const
{
    bytes respondMethodSignature = EthEncoder::encodeSig("respond(bytes20,address,uint256)");
    bytes data = respondMethodSignature +
            EthEncoder::encode(hashedSecret, false) +
            EthEncoder::encode(initiatorAddress) +
            EthEncoder::encode(refundDuration);

    return data;
}

//*****************************************************************************
//*****************************************************************************
bytes EthWalletConnector::createRefundData(const bytes & hashedSecret) const
{
    bytes refundMethodSignature = EthEncoder::encodeSig("refund(bytes20)");
    bytes data = refundMethodSignature +
            EthEncoder::encode(hashedSecret, false);

    return data;
}

//*****************************************************************************
//*****************************************************************************
bytes EthWalletConnector::createRedeemData(const bytes & hashedSecret, const bytes & secret) const
{
    bytes redeemMethodSignature = EthEncoder::encodeSig("redeem(bytes20,bytes)");
    bytes data = redeemMethodSignature +
            EthEncoder::encode(hashedSecret, false) +
            EthEncoder::encode(64) +            // secret data offset
            EthEncoder::encode(secret.size()) + // size of secret data
            EthEncoder::encode(secret, false);

    return data;
}

//*****************************************************************************
//*****************************************************************************
bool EthWalletConnector::callContractMethod(const bytes & myAddress,
                                            const bytes & data,
                                            const uint256 & value,
                                            const uint256 & gas,
                                            uint256 & transactionHash) const
{
    if(!rpc::sendTransaction(m_ip, m_port,
                             uint160(HexStr(myAddress)),
                             uint160(m_networkType == MAINNET ? m_contractAddress : m_contractAddressTestnet),
                             gas, value, data,
                             transactionHash))
    {
        LOG() << "can't call contract method" << __FUNCTION__;
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool EthWalletConnector::isInitiated(const bytes & hashedSecret,
                                     bytes & initiatorAddress,
                                     const bytes & responderAddress,
                                     const uint256 value) const
{
    std::string initiatedEventSignature = as0xString(HexStr(EthEncoder::encodeSig("Initiated(bytes20,address,address,uint256,uint256)")));

    std::vector<std::string> events;
    std::vector<std::string> data;
    if(!rpc::getLogs(m_ip, m_port,
                     uint160(m_networkType == MAINNET ? m_contractAddress : m_contractAddressTestnet),
                     m_fromBlock,
                     HexStr(EthEncoder::encode(hashedSecret, false)),
                     events, data))
    {
        LOG() << "can't get logs" << __FUNCTION__;
        return false;
    }

    for(unsigned int i = 0; i < events.size(); ++i)
    {
        std::string event(events.at(i).begin(), events.at(i).begin() + 10);

        if(event == initiatedEventSignature)
        {
            std::vector<std::string> params;
            if(!splitEventParams(data.at(i), params))
            {
                LOG() << "can't split params" << __FUNCTION__;
                return false;
            }

            if(params.size() < 3)
            {
                LOG() << "wrong params count" << __FUNCTION__;
                return false;
            }

            if(params.at(1) == HexStr(EthEncoder::encode(responderAddress)) &&
               params.at(2) == value.ToString())
            {
                initiatorAddress = toBigEndian(uint160(params.at(0)));
                return true;
            }
        }
    }

    return false;
}

//*****************************************************************************
//*****************************************************************************
bool EthWalletConnector::isResponded(const bytes & hashedSecret,
                                     const bytes & initiatorAddress,
                                     bytes & responderAddress,
                                     const uint256 value) const
{
    std::string respondedEventSignature = as0xString(HexStr(EthEncoder::encodeSig("Responded(bytes20,address,address,uint256,uint256)")));

    std::vector<std::string> events;
    std::vector<std::string> data;
    if(!rpc::getLogs(m_ip, m_port,
                     uint160(m_networkType == MAINNET ? m_contractAddress : m_contractAddressTestnet),
                     m_fromBlock,
                     HexStr(EthEncoder::encode(hashedSecret, false)),
                     events, data))
    {
        LOG() << "can't get logs" << __FUNCTION__;
        return false;
    }

    for(unsigned int i = 0; i < events.size(); ++i)
    {
        std::string event(events.at(i).begin(), events.at(i).begin() + 10);

        if(event == respondedEventSignature)
        {
            std::vector<std::string> params;
            if(!splitEventParams(data.at(i), params))
            {
                LOG() << "can't split params" << __FUNCTION__;
                return false;
            }

            if(params.size() < 3)
            {
                LOG() << "wrong params count" << __FUNCTION__;
                return false;
            }

            if(params.at(0) == HexStr(EthEncoder::encode(initiatorAddress)) &&
               params.at(2) == value.ToString())
            {
                responderAddress = toBigEndian(uint160(params.at(1)));
                return true;
            }
        }
    }

    return false;
}

//*****************************************************************************
//*****************************************************************************
bool EthWalletConnector::isRefunded(const bytes & hashedSecret,
                                    const bytes & recipientAddress,
                                    const uint256 value) const
{
    std::string refundedEventSignature = as0xString(HexStr(EthEncoder::encodeSig("Refunded(bytes20,address,uint256)")));

    std::vector<std::string> events;
    std::vector<std::string> data;
    if(!rpc::getLogs(m_ip, m_port,
                     uint160(m_networkType == MAINNET ? m_contractAddress : m_contractAddressTestnet),
                     m_fromBlock,
                     HexStr(EthEncoder::encode(hashedSecret, false)),
                     events, data))
    {
        LOG() << "can't get logs" << __FUNCTION__;
        return false;
    }

    for(unsigned int i = 0; i < events.size(); ++i)
    {
        std::string event(events.at(i).begin(), events.at(i).begin() + 10);

        if(event == refundedEventSignature)
        {
            std::vector<std::string> params;
            if(!splitEventParams(data.at(i), params))
            {
                LOG() << "can't split params" << __FUNCTION__;
                return false;
            }

            if(params.size() < 2)
            {
                LOG() << "wrong params count" << __FUNCTION__;
                return false;
            }

            if(params.at(0) == HexStr(EthEncoder::encode(recipientAddress)) &&
               params.at(1) == value.ToString())
            {
                return true;
            }
        }
    }

    return false;
}

//*****************************************************************************
//*****************************************************************************
bool EthWalletConnector::isRedeemed(const bytes& hashedSecret,
                                    const bytes & recipientAddress,
                                    const uint256 value) const
{
    std::string redeemedEventSignature = as0xString(HexStr(EthEncoder::encodeSig("Redeemed(bytes20,bytes,address,uint256)")));

    std::vector<std::string> events;
    std::vector<std::string> data;
    if(!rpc::getLogs(m_ip, m_port,
                     uint160(m_networkType == MAINNET ? m_contractAddress : m_contractAddressTestnet),
                     m_fromBlock,
                     HexStr(EthEncoder::encode(hashedSecret, false)),
                     events, data))
    {
        LOG() << "can't get logs" << __FUNCTION__;
        return false;
    }

    for(unsigned int i = 0; i < events.size(); ++i)
    {
        std::string event(events.at(i).begin(), events.at(i).begin() + 10);

        if(event == redeemedEventSignature)
        {
            std::vector<std::string> params;
            if(!splitEventParams(data.at(i), params))
            {
                LOG() << "can't split params" << __FUNCTION__;
                return false;
            }

            if(params.size() < 2)
            {
                LOG() << "wrong params count" << __FUNCTION__;
                return false;
            }

            if(params.at(1) == HexStr(EthEncoder::encode(recipientAddress)) &&
               params.at(2) == value.ToString())
            {
                return true;
            }
        }
    }

    return false;
}

} //namespace xbridge
