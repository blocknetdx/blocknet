#include "xrouterconnectorbtc.h"
#include "xroutererror.h"

namespace xrouter
{

static Value getResult(const Object & obj)
{
    const Value & r = find_value(obj, "result");
    if (r.type() == null_type)
        return Value(obj);
    return r;
}

static bool getResultOrError(const Object & obj, Value & res)
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

std::string BtcWalletConnectorXRouter::getBlockCount() const
{
    std::string command("getblockcount");
    Array params;

    Object resp = CallRPC(m_user, m_passwd, m_ip, m_port, command, params);

    return std::to_string(getResult(resp).get_int());
}

Object BtcWalletConnectorXRouter::getBlockHash(const int & block) const
{
    std::string command("getblockhash");
    Array params { block };

    return CallRPC(m_user, m_passwd, m_ip, m_port, command, params);
}

Object BtcWalletConnectorXRouter::getBlock(const std::string & blockHash) const
{
    static const std::string command("getblock");
    Array params { blockHash };

    return CallRPC(m_user, m_passwd, m_ip, m_port, command, params);
}

Array BtcWalletConnectorXRouter::getBlocks(const std::vector<std::string> & blockHashes) const
{
    static const std::string commandGB("getblock");

    std::set<std::string> unique{blockHashes.begin(), blockHashes.end()};
    std::map<std::string, Value> results;
    Array result;

    for (const auto & hash : unique) {
        Array paramsGB { hash };
        results[hash] = getResult(getBlock(hash));
    }

    for (const auto & hash : blockHashes)
        result.push_back(results[hash]);

    return result;
}

Object BtcWalletConnectorXRouter::getTransaction(const std::string & hash) const
{
    static const std::string commandGRT("getrawtransaction");
    Array paramsGRT { hash };

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

        static const std::string commandDRT("decoderawtransaction");
        Array paramsDRT { txdata };

        Object decTr = CallRPC(m_user, m_passwd, m_ip, m_port, commandDRT, paramsDRT);

        Value resValue = getResult(decTr);
        Object wrap;
        wrap.emplace_back(Pair("result", resValue));

        return wrap;
    }
}

Array BtcWalletConnectorXRouter::getTransactions(const std::vector<std::string> & txHashes) const
{
    static const std::string commandGRT("getrawtransaction");
    static const std::string commandDRT("decoderawtransaction");

    std::set<std::string> unique{txHashes.begin(), txHashes.end()};
    std::map<std::string, Value> results;
    Array result;

    for (const auto & hash : unique) {
        Array paramsGRT { hash };
        Object rawtxObj = CallRPC(m_user, m_passwd, m_ip, m_port, commandGRT, paramsGRT);
        Object txObj = CallRPC(m_user, m_passwd, m_ip, m_port, commandDRT, { getResult(rawtxObj).get_str() });
        results[hash] = getResult(txObj);
    }

    for (const auto & hash : txHashes)
        result.push_back(results[hash]);

    return result;
}

Object BtcWalletConnectorXRouter::decodeRawTransaction(const std::string & hex) const
{
    static const std::string commandDRT("decoderawtransaction");

    Object decTr = CallRPC(m_user, m_passwd, m_ip, m_port, commandDRT, { hex });
    Value resValue;
    getResultOrError(decTr, resValue);
    Object o; o.emplace_back(Pair("result", resValue));
    return o;
}

Array BtcWalletConnectorXRouter::getTransactionsBloomFilter(const int & number, CDataStream & stream, const int & fetchlimit) const
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

    if ((fetchlimit > 0) && (blockcount - number > fetchlimit)) {
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

        for (const auto & j : txs) {
            std::string txid = Value(j).get_str();

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

std::string BtcWalletConnectorXRouter::getBalance(const std::string & address) const
{
    return "0"; // TODO Implement
}

} // namespace xrouter
