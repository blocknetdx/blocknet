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
    std::string command("getblock");
    Array params { blockHash };

    return CallRPC(m_user, m_passwd, m_ip, m_port, command, params);
}

Array BtcWalletConnectorXRouter::getBlocks(const std::set<std::string> & blockHashes) const
{
    Array result;
    static const std::string commandGB("getblock");
    for (const auto & hash : blockHashes) {
        Array paramsGB { hash };
        Object blockObj = CallRPC(m_user, m_passwd, m_ip, m_port, commandGB, paramsGB);
        result.push_back(getResult(blockObj));
    }
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

Array BtcWalletConnectorXRouter::getTransactions(const std::set<std::string> & txHashes) const
{
    static const std::string commandGRT("getrawtransaction");
    static const std::string commandDRT("decoderawtransaction");

    Array result;
    for (const auto & hash : txHashes) {
        Array paramsGRT { hash };
        Object rawTrObj = CallRPC(m_user, m_passwd, m_ip, m_port, commandGRT, paramsGRT);
        const std::string & txdata = getResult(rawTrObj).get_str();
        result.push_back(txdata);
    }
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

Array BtcWalletConnectorXRouter::getTransactionsBloomFilter(const int & number, CDataStream & stream, const int & blocklimit) const
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
