#include "xrouterconnectoreth.h"

namespace xrouter
{

Object EthWalletConnectorXRouter::getBlockCount() const
{
    std::string command("getblockcount");
    Array params;

    return rpc::CallRPC(m_user, m_passwd, m_ip, m_port, command, params);
}

Object EthWalletConnectorXRouter::getBlockHash(const std::string & blockId) const
{
    std::string command("getblockhash");
    Array params { std::stoi(blockId) };

    return rpc::CallRPC(m_user, m_passwd, m_ip, m_port, command, params);
}

Object EthWalletConnectorXRouter::getBlock(const std::string & blockHash) const
{
    std::string command("getblock");
    Array params { blockHash };

    return rpc::CallRPC(m_user, m_passwd, m_ip, m_port, command, params);
}

Object EthWalletConnectorXRouter::getTransaction(const std::string & trHash) const
{
    std::string commandGRT("getrawtransaction");
    Array paramsGRT { hash };

    Object rawTr = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandGRT, paramsGRT);

    Value raw;
    bool code = getResultOrError(rawTr, raw);

    if (!code)
    {
        return rawTr;
    }
    else
    {
        std::string txdata = raw.get_str();

        std::string commandDRT("decoderawtransaction");
        Array paramsDRT { txdata };

        Object decTr = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandDRT, paramsDRT);

        Value resValue = getResult(decTr);
        Object wrap;
        wrap.emplace_back(Pair("result", resValue));

        return wrap;
    }
}

Array EthWalletConnectorXRouter::getAllBlocks(const int number) const
{
    std::string commandGBC("getblockcount");
    std::string commandGBH("getblockhash");
    std::string commandGB("getblock");

    Object blockCountObj = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandGBC, Array());

    Value res = getResult(blockCountObj);
    int blockcount = res.get_int();

    Array result;

    for (int id = number; id <= blockcount; id++)
    {
        Array paramsGBH { id };
        Object blockHashObj = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandGBH, paramsGBH);

        std::string hash = getResult(blockHashObj).get_str();

        Array paramsGB { hash };
        Object blockObj = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandGB, paramsGB);

        result.push_back(getResult(blockObj));
    }

    return result;
}

Array EthWalletConnectorXRouter::getAllTransactions(const std::string & account, const int number) const
{
    std::string commandGBC("getblockcount");
    std::string commandGBH("getblockhash");
    std::string commandGB("getblock");
    std::string commandGRT("getrawtransaction");
    std::string commandDRT("decoderawtransaction");

    Object blockCountObj = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandGBC, Array());
    int blockcount = getResult(blockCountObj).get_int();

    Array result;
    for (int id = number; id <= blockcount; id++)
    {
        Array paramsGBH { id };
        Object blockHashObj = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandGBH, paramsGBH);
        std::string hash = getResult(blockHashObj).get_str();

        Array paramsGB { hash };
        Object blockObj = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandGB, paramsGB);
        Object block = getResult(blockObj).get_obj();

        Array txs = find_value(block, "tx").get_array();
        std::cout << "block " << id << " " << txs.size() << std::endl;

        for (uint j = 0; j < txs.size(); j++)
        {
            std::string txid = Value(txs[j]).get_str();

            Array paramsGRT { txid };
            Object rawTrObj = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandGRT, paramsGRT);
            std::string txdata = getResult(rawTrObj).get_str();

            Array paramsDRT { txdata };
            Object decRawTrObj = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandDRT, paramsDRT);
            Object tx = getResult(decRawTrObj).get_obj();

            if (getBalanceChange(tx, account) != 0.0)
                result.push_back(Value(tx));
        }
    }

    return result;
}

std::string EthWalletConnectorXRouter::getBalance(const std::string & account) const
{
    std::string commandGBC("getblockcount");
    std::string commandGBH("getblockhash");
    std::string commandGB("getblock");
    std::string commandGRT("getrawtransaction");
    std::string commandDRT("decoderawtransaction");

    double result = 0.0;

    Object blockCountObj = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandGBC, Array());
    int blockcount = getResult(blockCountObj).get_int();

    for (int id = 0; id <= blockcount; id++)
    {
        Array paramsGBH { id };
        Object blockHashObj = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandGBH, paramsGBH);
        std::string hash = getResult(blockHashObj).get_str();

        Array paramsGB { hash };
        Object blockObj = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandGB, paramsGB);
        Object block = getResult(blockObj).get_obj();

        Array txs = find_value(block, "tx").get_array();
        std::cout << "block " << id << " " << txs.size() << std::endl;

        for (uint j = 0; j < txs.size(); j++)
        {
            std::string txid = Value(txs[j]).get_str();

            Array paramsGRT { txid };
            Object rawTrObj = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandGRT, paramsGRT);
            std::string txdata = getResult(rawTrObj).get_str();

            Array paramsDRT { txdata };
            Object decRawTrObj = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandDRT, paramsDRT);
            Object tx = getResult(decRawTrObj).get_obj();

            result += getBalanceChange(tx, account);
        }
    }

    return std::to_string(result);
}

std::string EthWalletConnectorXRouter::getBalanceUpdate(const std::string & account, const int number) const
{
    std::string commandGBC("getblockcount");
    std::string commandGBH("getblockhash");
    std::string commandGB("getblock");
    std::string commandGRT("getrawtransaction");
    std::string commandDRT("decoderawtransaction");

    double result = 0.0;

    Object blockCountObj = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandGBC, Array());
    int blockcount = getResult(blockCountObj).get_int();

    for (int id = number; id <= blockcount; id++)
    {
        Array paramsGBH { id };
        Object blockHashObj = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandGBH, paramsGBH);
        std::string hash = getResult(blockHashObj).get_str();

        Array paramsGB { hash };
        Object blockObj = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandGB, paramsGB);
        Object block = getResult(blockObj).get_obj();

        Array txs = find_value(block, "tx").get_array();
        std::cout << "block " << id << " " << txs.size() << std::endl;

        for (uint j = 0; j < txs.size(); j++)
        {
            std::string txid = Value(txs[j]).get_str();

            Array paramsGRT { txid };
            Object rawTrObj = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandGRT, paramsGRT);
            std::string txdata = getResult(rawTrObj).get_str();

            Array paramsDRT { txdata };
            Object decRawTrObj = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandDRT, paramsDRT);
            Object tx = getResult(decRawTrObj).get_obj();

            result += getBalanceChange(tx, account);
        }
    }

    return std::to_string(result);
}

Array EthWalletConnectorXRouter::getTransactionsBloomFilter(const int number) const
{
    std::string commandGBC("getblockcount");
    std::string commandGBH("getblockhash");
    std::string commandGB("getblock");
    std::string commandGRT("getrawtransaction");
    std::string commandDRT("decoderawtransaction");

    CBloomFilter ft;
    stream >> ft;
    CBloomFilter filter(ft);
    filter.UpdateEmptyFull();

    Array result;

    Object blockCountObj = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandGBC, Array());
    int blockcount = getResult(blockCountObj).get_int();

    for (int id = number; id <= blockcount; id++)
    {
        Array paramsGBH { id };
        Object blockHashObj = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandGBH, paramsGBH);
        std::string hash = getResult(blockHashObj).get_str();

        Array paramsGB { hash };
        Object blockObj = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandGB, paramsGB);
        Object block = getResult(blockObj).get_obj();

        Array txs = find_value(block, "tx").get_array();
        std::cout << "block " << id << " " << txs.size() << std::endl;

        for (uint j = 0; j < txs.size(); j++)
        {
            std::string txid = Value(txs[j]).get_str();

            Array paramsGRT { txid };
            Object rawTrObj = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandGRT, paramsGRT);
            std::string txdata = getResult(rawTrObj).get_str();

            Array paramsDRT { txdata };
            Object decRawTrObj = rpc::CallRPC(m_user, m_passwd, m_ip, m_port, commandDRT, paramsDRT);
            Object tx = getResult(decRawTrObj).get_obj();

            if (checkFilterFit(tx, filter))
                result.push_back(Value(tx));
        }
    }
}

Object EthWalletConnectorXRouter::sendTransaction(const std::string & transaction) const
{

}

Object EthWalletConnectorXRouter::getPaymentAddress() const
{

}

} // namespace xrouter
