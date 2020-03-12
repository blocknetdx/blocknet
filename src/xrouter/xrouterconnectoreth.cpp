// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <xrouter/xrouterconnectoreth.h>

#include <tinyformat.h>
#include <uint256.h>

#include <json/json_spirit.h>
#include <json/json_spirit_reader_template.h>
#include <json/json_spirit_writer_template.h>

#include <boost/lexical_cast.hpp>

using namespace json_spirit;

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

static std::string dec2hex(const unsigned int & n) {
    std::stringstream ss;
    ss << std::hex << n;
    return "0x" + ss.str();
}

static std::string dec2hex(const std::string & s) {
    return dec2hex(boost::lexical_cast<unsigned int>(s));
}

static unsigned int hex2dec(const std::string & s) {
    unsigned int result;
    std::stringstream ss;
    ss << std::hex << s;
    ss >> result;
    return result;
}

std::string EthWalletConnectorXRouter::getBlockCount() const
{
    static const std::string command("eth_blockNumber");
    const auto & data = CallRPC(m_user, m_passwd, m_ip, m_port, command, Array(), jsonver, contenttype);

    Value data_val; read_string(data, data_val);
    if (data_val.type() != obj_type)
        return data;

    const auto & result_val = getResult(data);
    if (result_val.type() != str_type)
        return data;

    auto blockCount = hex2dec(result_val.get_str());

    auto o = data_val.get_obj();
    for (int i = 0; i < o.size(); ++i) {
        const auto & item = o[i];
        if (item.name_ == "result") {
            o.erase(o.begin()+i);
            o.insert(o.begin()+i, Pair("result", static_cast<int>(blockCount)));
            return write_string(Value(o));
        }
    }

    return data;
}

std::string EthWalletConnectorXRouter::getBlockHash(const int & block) const
{
    static const std::string command("eth_getBlockByNumber");
    return CallRPC(m_user, m_passwd, m_ip, m_port, command, { dec2hex(block), false }, jsonver, contenttype);
}

std::string EthWalletConnectorXRouter::getBlock(const std::string & blockHash) const
{
    static const std::string command("eth_getBlockByHash");
    return CallRPC(m_user, m_passwd, m_ip, m_port, command, { blockHash, false }, jsonver, contenttype);
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
    return CallRPC(m_user, m_passwd, m_ip, m_port, command, { trHash }, jsonver, contenttype);
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
    return CallRPC(m_user, m_passwd, m_ip, m_port, command, { rawtx }, jsonver, contenttype);
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
