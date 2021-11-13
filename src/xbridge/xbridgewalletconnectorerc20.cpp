//*****************************************************************************
//*****************************************************************************

#include "xbridgewalletconnectorerc20.h"
#include "xbridgewalletconnectorbtc.h"
#include "util/settings.h"

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

//*****************************************************************************
//*****************************************************************************
namespace xbridge
{

using namespace json_spirit;

//*****************************************************************************
//*****************************************************************************
struct Block
{
    uint256  hash;
    uint64_t timestamp;
};
                            
//*****************************************************************************
//*****************************************************************************
namespace rpc
{

using namespace std;
using namespace boost;
using namespace boost::asio;

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
        // filter.push_back(Pair("topics", Array{Value(), as0xString(topic)}));

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

//*****************************************************************************
//*****************************************************************************
bool eth_call(const std::string & rpcip, const std::string & rpcport,
              const uint160 & fromAddress,
              const uint160 & contractAddress, 
              const uint256 & gas,
              const uint256 & value,
              const bytes & data,
              Value & result)
{
    try
    {
        LOG() << "rpc call <eth_call>";

        Array params;

        Object transaction;
        if (!fromAddress.IsNull())
        {
            transaction.push_back(Pair("from", as0xString(fromAddress)));
        }
        transaction.push_back(Pair("to",   as0xString(contractAddress)));
        transaction.push_back(Pair("data", as0xString(data)));
        if(!gas.IsNull())
        {
            transaction.push_back(Pair("gas", as0xStringNumber(gas)));
        }
        if(!value.IsNull())
        {
            transaction.push_back(Pair("value", as0xStringNumber(value)));
        }

        params.push_back(transaction);
        params.push_back("latest");

        Object reply = CallRPC(rpcip, rpcport,
                               "eth_call", params);

        // Parse reply
        const Value & error  = find_value(reply, "error");

        if (error.type() != null_type)
        {
            // Error
            LOG() << "error: " << write_string(error, false);
            return false;
        }

        result = find_value(reply, "result");
    }
    catch (std::exception & e)
    {
        LOG() << "eth_call exception " << e.what();
        return false;
    }

    return true;
}

} // namespace

} // namespace rpc

//*****************************************************************************
//*****************************************************************************
bool ERC20WalletConnector::init()
{
    boost::property_tree::ptree section = settings().getSection(currency);
    m_networkId            = section.get<uint32_t>   ("NetworkId", 0);
    m_contractAddress      = section.get<std::string>("ContractAddress", "");
    m_erc20contractAddress = section.get<std::string>("ERC20ContractAddress", "");

    if (!rpc::getBlockNumber(m_ip, m_port, m_fromBlock))
    {
        return false;
    }

    int netVersion = 0;
    if (!rpc::getNetVersion(m_ip, m_port, netVersion))
    {
        return false;
    }

    if (m_networkId != netVersion)
    {
        LOG() << "wrong network settings, network id in config " << m_networkId << " vs node reply " << netVersion << __FUNCTION__;
        return false;
    }

    if (m_contractAddress.empty())
    {
        LOG() << "empty contract address " << __FUNCTION__;
        return false;
    }

    if (m_erc20contractAddress.empty())
    {
        LOG() << "empty contract address " << __FUNCTION__;
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
std::string ERC20WalletConnector::fromXAddr(const std::vector<unsigned char> & xaddr) const
{
    std::string result("0x");
    result.append(HexStr(xaddr));
    return result;
}

//*****************************************************************************
//*****************************************************************************
std::vector<unsigned char> ERC20WalletConnector::toXAddr(const std::string & addr) const
{
    std::string addressWithout0x(addr.begin() + 2, addr.end());
    std::vector<unsigned char> vch = ParseHex(addressWithout0x);
    return vch;
}

//*****************************************************************************
//*****************************************************************************
bool ERC20WalletConnector::getNewAddress(std::string & addr, const std::string & /*type*/) 
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
bool ERC20WalletConnector::requestAddressBook(std::vector<wallet::AddressBookEntry> & entries)
{
    std::vector<std::string> accounts;
    if (!rpc::getAccounts(m_ip, m_port, accounts))
        return false;

    entries.push_back(std::make_pair("default", accounts));

    return true;
}

//*****************************************************************************
//*****************************************************************************
amount_t ERC20WalletConnector::getWalletBalance(const std::set<wallet::UtxoEntry> & /*excluded*/, const std::string & addr) const
{
    // if addr is empty - use only address from settings
    // TODO check for other addresses
    amount_t amount = 0;
    if (!getBalance(toXAddr(addr.empty() ? address : addr), amount))
    {
        return 0;
    }

    return amount;
}

//******************************************************************************
//******************************************************************************
bool ERC20WalletConnector::getBlockHash(const uint32_t & blockNumber, std::string & blockHash)
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
bool ERC20WalletConnector::getBlockCount(uint32_t & blockCount) const
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
bool ERC20WalletConnector::isValidAddress(const std::string & /*addr*/) const  
{
    // TODO validate address 
    return true; 
}

//******************************************************************************
//******************************************************************************
bool ERC20WalletConnector::isValidAmount(const amount_t & amount) const
{
    // TODO check wallet balance?
    // TODO check maximum?
    return !isDustAmount(amount);
}

//******************************************************************************
//******************************************************************************
bool ERC20WalletConnector::canAcceptTransactions() const
{
    // need to pay gas
    uint256 amount = 0;
    if (!rpc::getBalance(m_ip, m_port, uint160(address), amount))
    {
        return false;
    }

    // simple check
    return amount > 0;
}

//******************************************************************************
//******************************************************************************
bool ERC20WalletConnector::newKeyPair(std::vector<unsigned char> & pubkey,
                                    std::vector<unsigned char> & privkey)
{
    m_cp->makeNewKey(privkey);
    return m_cp->getPubKey(privkey, pubkey);
}

//******************************************************************************
//******************************************************************************
std::vector<unsigned char> ERC20WalletConnector::getKeyId(const std::vector<unsigned char> & pubkey)
{
    uint160 id = Hash160(&pubkey[0], &pubkey[0] + pubkey.size());
    return std::vector<unsigned char>(id.begin(), id.end());
}

//******************************************************************************
// return false if deposit tx not found (need wait tx)
// true if tx found and checked
// isGood == true id depost tx is OK
//******************************************************************************
bool ERC20WalletConnector::checkDepositTransaction(const std::string& depositTxId, 
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
uint32_t ERC20WalletConnector::lockTime(const char role) const
{
    uint32_t blockCount = 0;
    if (!getBlockCount(blockCount))
    {
        LOG() << "wrong block count " << __FUNCTION__;
        return 0;
    }

    // lock time
    uint32_t lt = 0;
    if (role == 'A')
    {
        uint32_t makerTime = XMAKER_LOCKTIME_TARGET_SECONDS;
        uint32_t blocks = makerTime / blockTime;
        if (blocks < XMIN_LOCKTIME_BLOCKS) blocks = XMIN_LOCKTIME_BLOCKS;
        lt = blockCount + blocks;
    }
    else if (role == 'B')
    {
        uint32_t takerTime = XTAKER_LOCKTIME_TARGET_SECONDS;
        if (blockTime >= XSLOW_BLOCKTIME_SECONDS)
        {
            // allow more time for slower chains
            takerTime = XSLOW_TAKER_LOCKTIME_TARGET_SECONDS;
        }
        uint32_t blocks = takerTime / blockTime;
        if (blocks < XMIN_LOCKTIME_BLOCKS) blocks = XMIN_LOCKTIME_BLOCKS;
        lt = blockCount + blocks;
    }
    return lt;
}

//******************************************************************************
//******************************************************************************
bool ERC20WalletConnector::acceptableLockTimeDrift(const char role, const uint32_t lckTime) const
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
bool ERC20WalletConnector::getAccounts(std::vector<std::string> & accounts)
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
bool ERC20WalletConnector::getBalance(const bytes & account, uint256 & balance) const
{
    bytes methodSignature = EthEncoder::encodeSig("balanceOf(address)");
    bytes data = methodSignature + EthEncoder::encode(account);

    Value result;
    if (!rpc::eth_call(m_ip, m_port, uint160(), uint160(m_contractAddress), uint256(), uint256(), data, result))
    {
        LOG() << "can't get balance for address " << HexStr(account) << " " << __FUNCTION__;
        return false;
    }

    if (result.type() != str_type)
    {
        // Result
        LOG() << "result not a string ";
        return false;
    }

    balance = uint256(result.get_str());

    return true;
}

//*****************************************************************************
//*****************************************************************************
std::string ERC20WalletConnector::contractAddress() const
{
    return m_contractAddress;
}

//*****************************************************************************
//*****************************************************************************
bool ERC20WalletConnector::getGasPrice(uint256 & gasPrice) const
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
bool ERC20WalletConnector::getEstimateGas(const bytes & myAddress,
                                          const bytes & data,
                                          const uint256 & value,
                                          uint256 & estimateGas) const
{
    if(!rpc::getEstimateGas(m_ip, m_port,
                            uint160(HexStr(myAddress)),
                            uint160(m_contractAddress),
                            value, data,
                            estimateGas))
    {
        LOG() << "can't get estimate gas " << __FUNCTION__;
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool ERC20WalletConnector::getLastBlockTime(uint256 & blockTime) const
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
bool splitEventParams(const std::string & paramsString, std::vector<std::string> & paramsVector);

//*****************************************************************************
//*****************************************************************************
bytes ERC20WalletConnector::createInitiateData(const uint256 & amount,
                                               const bytes & hashedSecret,
                                               const bytes & responderAddress,
                                               const uint256 & refundDuration) const
{
    bytes initiateMethodSignature = EthEncoder::encodeSig("initiate(uint256,bytes20,address,uint256)");
    bytes data = initiateMethodSignature +
            EthEncoder::encode(amount) +
            EthEncoder::encode(hashedSecret, false) +
            EthEncoder::encode(responderAddress) +
            EthEncoder::encode(refundDuration);

    return data;
}

//*****************************************************************************
//*****************************************************************************
bytes ERC20WalletConnector::createRespondData(const uint256 & amount,
                                              const bytes & hashedSecret,
                                              const bytes & initiatorAddress,
                                              const uint256 & refundDuration) const
{
    bytes respondMethodSignature = EthEncoder::encodeSig("respond(uint256,bytes20,address,uint256)");
    bytes data = respondMethodSignature +
            EthEncoder::encode(amount) +
            EthEncoder::encode(hashedSecret, false) +
            EthEncoder::encode(initiatorAddress) +
            EthEncoder::encode(refundDuration);

    return data;
}

//*****************************************************************************
//*****************************************************************************
bytes ERC20WalletConnector::createRefundData(const bytes & hashedSecret) const
{
    bytes refundMethodSignature = EthEncoder::encodeSig("refund(bytes20)");
    bytes data = refundMethodSignature +
            EthEncoder::encode(hashedSecret, false);

    return data;
}

//*****************************************************************************
//*****************************************************************************
bytes ERC20WalletConnector::createRedeemData(const bytes & hashedSecret, const bytes & secret) const
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
bool ERC20WalletConnector::approve(const uint256 & amount) const
{
    // check allowance
    {
        bytes sig = EthEncoder::encodeSig("allowance(address,address)");
        bytes data = sig +
                     EthEncoder::encode(toXAddr(address)) +
                     EthEncoder::encode(toXAddr(m_contractAddress));         

        Value result;
        if (!rpc::eth_call(m_ip, m_port,
                             uint160(address),
                             uint160(m_erc20contractAddress),
                             uint256(), uint256(), data, result))
        {
            WARN() << "failed check allowance, try without " << __FUNCTION__;
        }
        else if (result.type() != str_type)
        {
            WARN() << "allowance, result not a string " << __FUNCTION__;
        }
        else
        {
            uint256 allowed = uint256(result.get_str());

            if (allowed > 0)
            {
                if (amount > allowed)
                {
                    LOG() << "amount not allowed " << __FUNCTION__;
                    return false;
                }
                else
                {
                    // already approved
                    LOG() << "amount already approved " << __FUNCTION__;
                    return true;
                }
            }
        }
    }

    bytes methodSignature = EthEncoder::encodeSig("approve(address,uint256)");
    bytes data = methodSignature +
            EthEncoder::encode(toXAddr(m_contractAddress)) +
            EthEncoder::encode(amount);

    uint256 estimateGas;
    if (!rpc::getEstimateGas(m_ip, m_port,
                             uint160(address),
                             uint160(m_erc20contractAddress),
                             0, data,
                             estimateGas))
    {
        LOG() << "can't process without estimate gas " << __FUNCTION__;
        return false;
    }

    // uint256 gasPrice;
    // if (!getGasPrice(gasPrice))
    // {
    //     LOG() << "can't process without gas price " << __FUNCTION__;
    //     return false;
    // }

    // Value result;
    uint256 result;
    if (!rpc::sendTransaction(m_ip, m_port, 
                       uint160(address), 
                       uint160(m_erc20contractAddress), 
                       estimateGas, 0, data, result))
    {
        LOG() << "can't call contract method" << __FUNCTION__;
        return false;
    }

    // if (result.type() != str_type)
    // {
    //     // Result
    //     LOG() << "result not a string ";
    //     return false;
    // }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool ERC20WalletConnector::callContractMethod(const bytes & myAddress,
                                            const bytes & data,
                                            const uint256 & value,
                                            const uint256 & gas,
                                            uint256 & transactionHash) const
{
    // Value result;
    uint256 result;
    if (!rpc::sendTransaction(m_ip, m_port, uint160(address), uint160(m_contractAddress), gas, uint256(), data, result))
    {
        LOG() << "can't call contract method" << __FUNCTION__;
        return false;
    }

    // if (result.type() != str_type)
    // {
    //     // Result
    //     LOG() << "result not a string ";
    //     return false;
    // }

    // transactionHash = uint256(result.get_str());
    transactionHash = result;

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool ERC20WalletConnector::isInitiated(const bytes & hashedSecret,
                                     bytes & initiatorAddress,
                                     const bytes & responderAddress,
                                     const uint256 value) const
{
    std::string initiatedEventSignature = as0xString(HexStr(EthEncoder::encodeSig("Initiated(bytes20,address,address,uint256,uint256)")));

    std::vector<std::string> events;
    std::vector<std::string> data;
    if(!rpc::getLogs(m_ip, m_port,
                     uint160(m_contractAddress),
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
bool ERC20WalletConnector::isResponded(const bytes & hashedSecret,
                                     const bytes & initiatorAddress,
                                     bytes & responderAddress,
                                     const uint256 value) const
{
    std::string respondedEventSignature = as0xString(HexStr(EthEncoder::encodeSig("Responded(bytes20,address,address,uint256,uint256)")));

    std::vector<std::string> events;
    std::vector<std::string> data;
    if(!rpc::getLogs(m_ip, m_port,
                     uint160(m_contractAddress),
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
bool ERC20WalletConnector::isRefunded(const bytes & hashedSecret,
                                    const bytes & recipientAddress,
                                    const uint256 value) const
{
    std::string refundedEventSignature = as0xString(HexStr(EthEncoder::encodeSig("Refunded(bytes20,address,uint256)")));

    std::vector<std::string> events;
    std::vector<std::string> data;
    if(!rpc::getLogs(m_ip, m_port,
                     uint160(m_contractAddress),
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
bool ERC20WalletConnector::isRedeemed(const bytes& hashedSecret,
                                    const bytes & recipientAddress,
                                    const uint256 value) const
{
    std::string redeemedEventSignature = as0xString(HexStr(EthEncoder::encodeSig("Redeemed(bytes20,bytes,address,uint256)")));

    std::vector<std::string> events;
    std::vector<std::string> data;
    if(!rpc::getLogs(m_ip, m_port,
                     uint160(m_contractAddress),
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
