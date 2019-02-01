#include "xrouterconnectorbtc.h"
#include "xroutererror.h"

namespace xrouter
{

static Value getResult(Object obj)
{
    for (Object::size_type i = 0; i != obj.size(); i++ ) {
        if (obj[i].name_ == "result") {
            return obj[i].value_;
        }
    }
    return Value();
}

static bool getResultOrError(Object obj, Value& res)
{
    const Value & result = find_value(obj, "result");
    const Value & error  = find_value(obj, "error");
    
    if (error.type() != null_type)
    {
        res = error;
        return false;
    }
    else if (result.type() != null_type)
    {
        res = result;
        return true;
    }
    else
    {
        res = Value();
        return false;
    }
}

static double parseVout(Value vout, std::string account)
{
    double result = 0.0;
    double val = find_value(vout.get_obj(), "value").get_real();
    Object src = find_value(vout.get_obj(), "scriptPubKey").get_obj();
    const Value & addr_val = find_value(src, "addresses");
    if (addr_val.is_null())
        return 0.0;
    Array addr = addr_val.get_array();

    for (unsigned int k = 0; k != addr.size(); k++ ) {
        std::string cur_addr = Value(addr[k]).get_str();
        if (cur_addr == account)
            result += val;
    }

    return result;
}

double BtcWalletConnectorXRouter::getBalanceChange(Object tx, std::string account) const
{
    std::string commandGRT("getrawtransaction");
    std::string commandDRT("decoderawtransaction");

    double result = 0.0;

    Array vout = find_value(tx, "vout").get_array();
    for (unsigned int j = 0; j != vout.size(); j++ )
    {
        result += parseVout(vout[j], account);
    }

    Array vin = find_value(tx, "vin").get_array();
    for (unsigned int j = 0; j != vin.size(); j++ )
    {
        const Value& txid_val = find_value(vin[j].get_obj(), "txid");
        if (txid_val.is_null())
            continue;

        std::string txid = txid_val.get_str();
        int voutid = find_value(vin[j].get_obj(), "vout").get_int();

        Array paramsGRT { Value(txid) };
        Object rawTr = CallRPC(m_user, m_passwd, m_ip, m_port, commandGRT, paramsGRT);
        std::string txdata = getResult(rawTr).get_str();

        Array paramsDRT { Value(txdata) };
        Object decRawTr = CallRPC(m_user, m_passwd, m_ip, m_port, commandDRT, paramsDRT);
        Object prev_tx = getResult(decRawTr).get_obj();
        Array prev_vouts = find_value(prev_tx, "vout").get_array();

        result -= parseVout(prev_vouts[voutid], account);
    }

    return result;
}

std::string BtcWalletConnectorXRouter::getBlockCount() const
{
    std::string command("getblockcount");
    Array params;

    Object resp = CallRPC(m_user, m_passwd, m_ip, m_port, command, params);

    return std::to_string(getResult(resp).get_int());
}

Object BtcWalletConnectorXRouter::getBlockHash(const std::string & blockId) const
{
    std::string command("getblockhash");
    Array params { std::stoi(blockId) };

    return CallRPC(m_user, m_passwd, m_ip, m_port, command, params);
}

Object BtcWalletConnectorXRouter::getBlock(const std::string & blockHash) const
{
    std::string command("getblock");
    Array params { blockHash };

    return CallRPC(m_user, m_passwd, m_ip, m_port, command, params);
}

Object BtcWalletConnectorXRouter::getTransaction(const std::string & trHash) const
{
    std::string commandGRT("getrawtransaction");
    Array paramsGRT { trHash };

    Object rawTr = CallRPC(m_user, m_passwd, m_ip, m_port, commandGRT, paramsGRT);

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

        Object decTr = CallRPC(m_user, m_passwd, m_ip, m_port, commandDRT, paramsDRT);

        Value resValue = getResult(decTr);
        Object wrap;
        wrap.emplace_back(Pair("result", resValue));

        return wrap;
    }
}

Array BtcWalletConnectorXRouter::getAllBlocks(const int number, int blocklimit) const
{
    std::string commandGBC("getblockcount");
    std::string commandGBH("getblockhash");
    std::string commandGB("getblock");

    Object blockCountObj = CallRPC(m_user, m_passwd, m_ip, m_port, commandGBC, Array());

    Value res = getResult(blockCountObj);
    int blockcount = res.get_int();

    Array result;
    if ((blocklimit > 0) && (blockcount - number > blocklimit)) {
        throw XRouterError("Too many blocks requested", xrouter::INVALID_PARAMETERS);
    }
    
    for (int id = number; id <= blockcount; id++)
    {
        Array paramsGBH { id };
        Object blockHashObj = CallRPC(m_user, m_passwd, m_ip, m_port, commandGBH, paramsGBH);

        std::string hash = getResult(blockHashObj).get_str();

        Array paramsGB { hash };
        Object blockObj = CallRPC(m_user, m_passwd, m_ip, m_port, commandGB, paramsGB);

        result.push_back(getResult(blockObj));
    }

    return result;
}

Array BtcWalletConnectorXRouter::getAllTransactions(const std::string & account, const int number, const int time, int blocklimit) const
{
    std::string commandGBC("getblockcount");
    std::string commandGBH("getblockhash");
    std::string commandGB("getblock");
    std::string commandGRT("getrawtransaction");
    std::string commandDRT("decoderawtransaction");

    Object blockCountObj = CallRPC(m_user, m_passwd, m_ip, m_port, commandGBC, Array());
    int blockcount = getResult(blockCountObj).get_int();

    Array result;
    if ((blocklimit > 0) && (blockcount - number > blocklimit)) {
        throw XRouterError("Too many blocks requested", xrouter::INVALID_PARAMETERS);
    }
    
    for (int id = blockcount; id >= number; id--)
    {
        Array paramsGBH { id };
        Object blockHashObj = CallRPC(m_user, m_passwd, m_ip, m_port, commandGBH, paramsGBH);
        std::string hash = getResult(blockHashObj).get_str();

        Array paramsGB { hash };
        Object blockObj = CallRPC(m_user, m_passwd, m_ip, m_port, commandGB, paramsGB);
        Object block = getResult(blockObj).get_obj();

        Array txs = find_value(block, "tx").get_array();
        int blocktime = find_value(block, "time").get_int();
        if (blocktime < time)
            break;

        for (unsigned int j = 0; j < txs.size(); j++)
        {
            std::string txid = Value(txs[j]).get_str();

            Array paramsGRT { txid };
            Object rawTrObj = CallRPC(m_user, m_passwd, m_ip, m_port, commandGRT, paramsGRT);
            std::string txdata = getResult(rawTrObj).get_str();

            Array paramsDRT { txdata };
            Object decRawTrObj = CallRPC(m_user, m_passwd, m_ip, m_port, commandDRT, paramsDRT);
            Object tx = getResult(decRawTrObj).get_obj();

            if (getBalanceChange(tx, account) != 0.0)
                result.push_back(Value(tx));
        }
    }

    return result;
}

std::string BtcWalletConnectorXRouter::getBalance(const std::string & account, const int time, int blocklimit) const
{
    return getBalanceUpdate(account, 0, time, blocklimit);
}

std::string BtcWalletConnectorXRouter::getBalanceUpdate(const std::string & account, const int number, const int time, int blocklimit) const
{
    std::string commandGBC("getblockcount");
    std::string commandGBH("getblockhash");
    std::string commandGB("getblock");
    std::string commandGRT("getrawtransaction");
    std::string commandDRT("decoderawtransaction");

    double result = 0.0;

    Object blockCountObj = CallRPC(m_user, m_passwd, m_ip, m_port, commandGBC, Array());
    int blockcount = getResult(blockCountObj).get_int();

    if ((blocklimit > 0) && (blockcount - number > blocklimit)) {
        throw XRouterError("Too many blocks requested", xrouter::INVALID_PARAMETERS);
    }
    
    for (int id = blockcount; id >= number; id--)
    {
        Array paramsGBH { id };
        Object blockHashObj = CallRPC(m_user, m_passwd, m_ip, m_port, commandGBH, paramsGBH);
        std::string hash = getResult(blockHashObj).get_str();

        Array paramsGB { hash };
        Object blockObj = CallRPC(m_user, m_passwd, m_ip, m_port, commandGB, paramsGB);
        Object block = getResult(blockObj).get_obj();

        Array txs = find_value(block, "tx").get_array();
        int blocktime = find_value(block, "time").get_int();
        if (blocktime < time)
            break;

        for (unsigned int j = 0; j < txs.size(); j++)
        {
            std::string txid = Value(txs[j]).get_str();

            Array paramsGRT { txid };
            Object rawTrObj = CallRPC(m_user, m_passwd, m_ip, m_port, commandGRT, paramsGRT);
            std::string txdata = getResult(rawTrObj).get_str();

            Array paramsDRT { txdata };
            Object decRawTrObj = CallRPC(m_user, m_passwd, m_ip, m_port, commandDRT, paramsDRT);
            Object tx = getResult(decRawTrObj).get_obj();

            result += getBalanceChange(tx, account);
        }
    }

    return std::to_string(result);
}

Array BtcWalletConnectorXRouter::getTransactionsBloomFilter(const int number, CDataStream & stream, int blocklimit) const
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

    Object blockCountObj = CallRPC(m_user, m_passwd, m_ip, m_port, commandGBC, Array());
    int blockcount = getResult(blockCountObj).get_int();

    if ((blocklimit > 0) && (blockcount - number > blocklimit)) {
        throw XRouterError("Too many blocks requested", xrouter::INVALID_PARAMETERS);
    }
    
    for (int id = number; id <= blockcount; id++)
    {
        Array paramsGBH { id };
        Object blockHashObj = CallRPC(m_user, m_passwd, m_ip, m_port, commandGBH, paramsGBH);
        std::string hash = getResult(blockHashObj).get_str();

        Array paramsGB { hash };
        Object blockObj = CallRPC(m_user, m_passwd, m_ip, m_port, commandGB, paramsGB);
        Object block = getResult(blockObj).get_obj();

        Array txs = find_value(block, "tx").get_array();

        for (unsigned int j = 0; j < txs.size(); j++)
        {
            std::string txid = Value(txs[j]).get_str();

            Array paramsGRT { txid };
            Object rawTrObj = CallRPC(m_user, m_passwd, m_ip, m_port, commandGRT, paramsGRT);
            std::string txData_str = getResult(rawTrObj).get_str();
            vector<unsigned char> txData(ParseHex(txData_str));
            CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
            CTransaction tx;
            ssData >> tx;
            
            if (filter.IsRelevantAndUpdate(tx)) {
                result.push_back(Value(txData_str));
            }
        }
    }
    
    return result;
}

Object BtcWalletConnectorXRouter::sendTransaction(const std::string & transaction) const
{
    std::string command("sendrawtransaction");
    
    Array params { transaction };

    return CallRPC(m_user, m_passwd, m_ip, m_port, command, params);
}

std::string BtcWalletConnectorXRouter::convertTimeToBlockCount(const std::string & timestamp) const
{
    Object blockCountObj = CallRPC(m_user, m_passwd, m_ip, m_port, "getblockcount", Array());

    Value res = getResult(blockCountObj);
    int blockcount = res.get_int();
    if (blockcount <= 51)
        return "0";
    
    // Estimate average block creation time from the last 10 blocks
    Array paramsGBH { blockcount };
    Object blockHashObj = CallRPC(m_user, m_passwd, m_ip, m_port, "getblockhash", paramsGBH);
    std::string hash = getResult(blockHashObj).get_str();
    Array paramsGB { hash };
    Object blockObj = CallRPC(m_user, m_passwd, m_ip, m_port, "getblock", paramsGB);
    Object block = getResult(blockObj).get_obj();
    int curblocktime = find_value(block, "time").get_int();
    
    Array paramsGBH2 { blockcount-50 };
    blockHashObj = CallRPC(m_user, m_passwd, m_ip, m_port, "getblockhash", paramsGBH2);
    hash = getResult(blockHashObj).get_str();
    Array paramsGB2 { hash };
    blockObj = CallRPC(m_user, m_passwd, m_ip, m_port, "getblock", paramsGB2);
    block = getResult(blockObj).get_obj();
    int blocktime50ago = find_value(block, "time").get_int();    
    
    int averageBlockTime = (curblocktime - blocktime50ago) / 50;

    int time = std::stoi(timestamp);
    
    if (time >= curblocktime)
        return std::to_string(blockcount);
    
    int blockEstimate = blockcount - (curblocktime - time) / averageBlockTime;
    if (blockEstimate <= 1)
        return "0";
    
    Array paramsGBH3 { blockEstimate };
    blockHashObj = CallRPC(m_user, m_passwd, m_ip, m_port, "getblockhash", paramsGBH3);
    hash = getResult(blockHashObj).get_str();
    Array paramsGB3 { hash };
    blockObj = CallRPC(m_user, m_passwd, m_ip, m_port, "getblock", paramsGB3);
    block = getResult(blockObj).get_obj();
    int cur_estimate = find_value(block, "time").get_int(); 

    int sign = 1;
    if (cur_estimate > time)
        sign = -1;
    
    while (true) {
        blockEstimate += sign;
        Array paramsGBH { blockEstimate };
        Object blockHashObj = CallRPC(m_user, m_passwd, m_ip, m_port, "getblockhash", paramsGBH);
        std::string hash = getResult(blockHashObj).get_str();
        Array paramsGB { hash };
        Object blockObj = CallRPC(m_user, m_passwd, m_ip, m_port, "getblock", paramsGB);
        Object block = getResult(blockObj).get_obj();
        int new_estimate = find_value(block, "time").get_int();
        //std::cout << blockEstimate << " " << new_estimate << " " << cur_estimate << " " << (new_estimate - time) << " " << (cur_estimate - time) << " " << (cur_estimate - time) * (new_estimate - time) << std::endl << std::flush;
        int sign1 = (cur_estimate - time > 0) ? 1 : -1;
        if (sign1 * (new_estimate - time) < 0) {
            if (sign > 0)
                return std::to_string(blockEstimate - 1);
            else
                return std::to_string(blockEstimate);
        }
        
        cur_estimate = new_estimate;
    }
    
    return "0";
}

} // namespace xrouter
