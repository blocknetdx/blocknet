#include "xrouterconnectoreth.h"
#include "../uint256.h"

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

static bool getResultOrError(Object obj, Value& res) {
    for (Object::size_type i = 0; i != obj.size(); i++ ) {
        if (obj[i].name_ == "result") {
            res =  obj[i].value_;
            return true;
        }
    }

    for (Object::size_type i = 0; i != obj.size(); i++ ) {
        if (obj[i].name_ == "error") {
            res =  obj[i].value_;
            return false;
        }
    }
    res = Object();
    return false;
}

std::string EthWalletConnectorXRouter::getBlockCount() const
{
    std::string command("eth_blockNumber");

    Object blockNumberObj = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, command, Array());

    Value blockNumberVal = getResult(blockNumberObj);

    if(blockNumberVal)
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

    Object resp = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, command, params);

    Object blockHashObj = getResult(resp).get_obj();

    return find_value(blockHashObj, "hash").get_str();
}

Object EthWalletConnectorXRouter::getBlock(const std::string & blockHash) const
{
    std::string command("eth_getBlockByHash");
    Array params { blockHash, true };

    Object resp = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, command, params);

    return getResult(resp).get_obj();
}

Object EthWalletConnectorXRouter::getTransaction(const std::string & trHash) const
{
    std::string command("eth_getTransactionByHash");
    Array params { trHash };

    Object resp = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, command, params);

    return getResult(resp).get_obj();
}

Array EthWalletConnectorXRouter::getAllBlocks(const int number) const
{
    std::string commandBN("eth_blockNumber");
    std::string commandgGBBN("eth_getBlockByNumber");

    Object blockCountObj = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandBN, Array());
    std::string hexValueStr = getResult(blockCountObj).get_str();
    uint256 blockCount(hexValueStr);

    Array result;

    for(uint256 id = number; id <= blockCount; id++)
    {
        Array params { id.ToString(), true };

        Object resp = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandgGBBN, params);

        result.push_back(getResult(resp));
    }

    return result;
}

Array EthWalletConnectorXRouter::getAllTransactions(const std::string & account, const int number) const
{
    std::string commandBN("eth_blockNumber");
    std::string commandgGBBN("eth_getBlockByNumber");

    Object blockCountObj = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandBN, Array());
    std::string hexValueStr = getResult(blockCountObj).get_str();
    uint256 blockCount(hexValueStr);

    Array result;

    for(uint256 id = number; id <= blockCount; id++)
    {
        Array params { id.ToString(), true };

        Object resp = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandgGBBN, params);
        Object blockObj = getResult(resp).get_obj();

        const Array & transactionsInBlock = find_value(blockObj, "transactions").get_array();

        for(const Value & transaction : transactionsInBlock)
        {
            Object transactionObj = transaction.get_obj();

            std::string from = find_value(transactionObj, "from");

            if(from == account)
                result.push_back(transaction);
        }
    }

    return result;
}

std::string EthWalletConnectorXRouter::getBalance(const std::string & account) const
{
    std::string commandBN("eth_blockNumber");
    std::string commandgGBBN("eth_getBlockByNumber");

    Object blockCountObj = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandBN, Array());
    std::string hexValueStr = getResult(blockCountObj).get_str();
    uint256 blockCount(hexValueStr);

    uint256 result;
    bool isPositive = true;

    for(uint256 id = number; id <= blockCount; id++)
    {
        Array params { id.ToString(), true };

        Object resp = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandgGBBN, params);
        Object blockObj = getResult(resp).get_obj();

        const Array & transactionsInBlock = find_value(blockObj, "transactions").get_array();

        for(const Value & transaction : transactionsInBlock)
        {
            Object transactionObj = transaction.get_obj();

            std::string from = find_value(transactionObj, "from");
            std::string to = find_value(transactionObj, "to");

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

std::string EthWalletConnectorXRouter::getBalanceUpdate(const std::string & account, const int number) const
{
    std::string commandBN("eth_blockNumber");
    std::string commandgGBBN("eth_getBlockByNumber");

    Object blockCountObj = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandBN, Array());
    std::string hexValueStr = getResult(blockCountObj).get_str();
    uint256 blockCount(hexValueStr);

    uint256 result;
    bool isPositive = true;

    for(uint256 id = number; id <= blockCount; id++)
    {
        Array params { id.ToString(), true };

        Object resp = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandgGBBN, params);
        Object blockObj = getResult(resp).get_obj();

        const Array & transactionsInBlock = find_value(blockObj, "transactions").get_array();

        for(const Value & transaction : transactionsInBlock)
        {
            Object transactionObj = transaction.get_obj();

            std::string from = find_value(transactionObj, "from");
            std::string to = find_value(transactionObj, "to");

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

Array EthWalletConnectorXRouter::getTransactionsBloomFilter(const int) const
{
    // not realized for Ethereum
    return Array();
}

Object EthWalletConnectorXRouter::sendTransaction(const std::string & transaction) const
{

}

Object EthWalletConnectorXRouter::getPaymentAddress() const
{

}

} // namespace xrouter
