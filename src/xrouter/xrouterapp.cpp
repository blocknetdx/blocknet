//*****************************************************************************
//*****************************************************************************

#include "xrouterapp.h"
#include "init.h"
#include "keystore.h"
#include "main.h"
#include "net.h"
#include "servicenodeconfig.h"
#include "servicenodeman.h"
#include "addrman.h"
#include "script/standard.h"
#include "util/xutil.h"
#include "wallet.h"
#include "xrouterrpc.h"
#include "bloom.h"

#include "xbridge/xkey.h"
#include "xbridge/util/settings.h"
#include "xbridge/xbridgewallet.h"
#include "xbridge/xbridgewalletconnector.h"

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"
#include <assert.h>

#include <boost/chrono/chrono.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <iostream>
#include <sstream>
#include <vector>

static const CAmount minBlock = 200;

//*****************************************************************************
//*****************************************************************************
namespace xrouter
{
//*****************************************************************************
//*****************************************************************************
class App::Impl
{
    friend class App;

    mutable boost::mutex                                        m_connectorsLock;
    xbridge::Connectors                                         m_connectors;
    xbridge::ConnectorsAddrMap                                  m_connectorAddressMap;
    xbridge::ConnectorsCurrencyMap                              m_connectorCurrencyMap;

protected:
    /**
     * @brief Impl - default constructor, init
     * services and timer
     */
    Impl();

    /**
     * @brief start - run sessions, threads and services
     * @return true, if run succesfull
     */
    bool start();

    /**
     * @brief stop stopped service, timer, secp stop
     * @return true
     */
    bool stop();

    /**
     * @brief onSend  send packet to xrouter network to specified id,
     *  or broadcast, when id is empty
     * @param id
     * @param message
     */
    void onSend(const std::vector<unsigned char>& id, const std::vector<unsigned char>& message, std::string wallet="");
};

//*****************************************************************************
//*****************************************************************************
App::Impl::Impl()
{
}

//*****************************************************************************
//*****************************************************************************
App::App()
    : m_p(new Impl), queries()
{
}

//*****************************************************************************
//*****************************************************************************
App::~App()
{
    stop();
}

//*****************************************************************************
//*****************************************************************************
// static
App& App::instance()
{
    static App app;
    return app;
}

//*****************************************************************************
//*****************************************************************************
// static
bool App::isEnabled()
{
    // enabled by default
    return true;
}

bool App::init(int argc, char *argv[])
{
    // init xbridge settings
    Settings & s = settings();
    {
        std::string path(GetDataDir(false).string());
        path += "/xbridge.conf";
        s.read(path.c_str());
        s.parseCmdLine(argc, argv);
        std::cout << "Finished loading config" << path << std::endl;
    }
    
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool App::start()
{
    return m_p->start();
}

//*****************************************************************************
//*****************************************************************************
bool App::Impl::start()
{
    // start xbrige
    try
    {
        // sessions
        xrouter::App & app = xrouter::App::instance();
        {
            Settings & s = settings();
            std::vector<std::string> wallets = s.exchangeWallets();
            for (std::vector<std::string>::iterator i = wallets.begin(); i != wallets.end(); ++i)
            {
                xbridge::WalletParam wp;
                wp.currency                    = *i;
                std::cout << "currency " << wp.currency << std::endl;
                wp.title                       = s.get<std::string>(*i + ".Title");
                wp.address                     = s.get<std::string>(*i + ".Address");
                wp.m_ip                        = s.get<std::string>(*i + ".Ip");
                wp.m_port                      = s.get<std::string>(*i + ".Port");
                wp.m_user                      = s.get<std::string>(*i + ".Username");
                wp.m_passwd                    = s.get<std::string>(*i + ".Password");
                wp.addrPrefix[0]               = s.get<int>(*i + ".AddressPrefix", 0);
                wp.scriptPrefix[0]             = s.get<int>(*i + ".ScriptPrefix", 0);
                wp.secretPrefix[0]             = s.get<int>(*i + ".SecretPrefix", 0);
                wp.COIN                        = s.get<uint64_t>(*i + ".COIN", 0);
                wp.txVersion                   = s.get<uint32_t>(*i + ".TxVersion", 1);
                wp.minTxFee                    = s.get<uint64_t>(*i + ".MinTxFee", 0);
                wp.feePerByte                  = s.get<uint64_t>(*i + ".FeePerByte", 200);
                wp.method                      = s.get<std::string>(*i + ".CreateTxMethod");
                wp.blockTime                   = s.get<int>(*i + ".BlockTime", 0);
                wp.requiredConfirmations       = s.get<int>(*i + ".Confirmations", 0);

                if (wp.m_ip.empty() || wp.m_port.empty() ||
                    wp.m_user.empty() || wp.m_passwd.empty() ||
                    wp.COIN == 0 || wp.blockTime == 0)
                {
                    continue;
                }

                xbridge::WalletConnectorPtr conn;
                if (wp.method == "ETHER")
                {
                    //LOG() << "wp.method ETHER not implemented" << __FUNCTION__;
                    // session.reset(new XBridgeSessionEthereum(wp));
                }
                else if ((wp.method == "BTC") || (wp.method == "BLOCK"))
                {
                    conn.reset(new BtcWalletConnectorXRouter);
                    *conn = wp;
                }
                else if (wp.method == "BCC")
                {
                    conn.reset(new BccWalletConnectorXRouter);
                    *conn = wp;
                }
                else if (wp.method == "SYS")
                {
                    conn.reset(new SysWalletConnectorXRouter);
                    *conn = wp;
                }
//                else if (wp.method == "RPC")
//                {
//                    LOG() << "wp.method RPC not implemented" << __FUNCTION__;
//                    // session.reset(new XBridgeSessionRpc(wp));
//                }
                else
                {
                    // session.reset(new XBridgeSession(wp));
                    //ERR() << "unknown session type " << __FUNCTION__;
                }
                if (!conn)
                {
                    continue;
                }

                if (!conn->init())
                {
                    //ERR() << "connection not initialized " << *i << " " << __FUNCTION__;
                    continue;
                }

                app.addConnector(conn);
            }
        }
    }
    catch (std::exception & e)
    {
        //ERR() << e.what();
        //ERR() << __FUNCTION__;
    }

    return true;
}


//*****************************************************************************
//*****************************************************************************
bool App::stop()
{
    return m_p->stop();
}

//*****************************************************************************
//*****************************************************************************
bool App::Impl::stop()
{
    return true;
}

void App::addConnector(const xbridge::WalletConnectorPtr & conn)
{
    boost::mutex::scoped_lock l(m_p->m_connectorsLock);
    m_p->m_connectors.push_back(conn);
    m_p->m_connectorCurrencyMap[conn->currency] = conn;
}

xbridge::WalletConnectorPtr App::connectorByCurrency(const std::string & currency) const
{
    boost::mutex::scoped_lock l(m_p->m_connectorsLock);
    if (m_p->m_connectorCurrencyMap.count(currency))
    {
        return m_p->m_connectorCurrencyMap.at(currency);
    }

    return xbridge::WalletConnectorPtr();
}

//*****************************************************************************
//*****************************************************************************
void App::sendPacket(const XRouterPacketPtr& packet, std::string wallet)
{
    static std::vector<unsigned char> addr(20, 0);
    m_p->onSend(addr, packet->body(), wallet);
}

std::string App::sendPacketAndWait(const XRouterPacketPtr & packet, std::string id, std::string currency, int confirmations, int timeout)
{
    boost::shared_ptr<boost::mutex> m(new boost::mutex());
    boost::shared_ptr<boost::condition_variable> cond(new boost::condition_variable());
    boost::mutex::scoped_lock lock(*m);
    queriesLocks[id] = std::pair<boost::shared_ptr<boost::mutex>, boost::shared_ptr<boost::condition_variable> >(m, cond);
    sendPacket(packet, currency);

    int confirmation_count = 0;
    while ((confirmation_count < confirmations) && cond->timed_wait(lock, boost::posix_time::milliseconds(timeout)))
        confirmation_count++;
    
    Object error;
    
    if(confirmation_count <= confirmations / 2) {
        error.emplace_back(Pair("error", "Failed to get response"));
        return json_spirit::write_string(Value(error), true);
    } else {
        for (uint i = 0; i < queries[id].size(); i++) {
            std::string cand = queries[id][i];
            int cnt = 0;
            for (uint j = 0; j < queries[id].size(); j++)
                if (queries[id][j] == cand) {
                    cnt++;
                    if (cnt > confirmations / 2)
                        return cand;
                }
        }
        
        error.emplace_back(Pair("error", "No consensus between responses"));
        return json_spirit::write_string(Value(error), true);
    }
}


//*****************************************************************************
// send packet to xrouter network to specified id,
// or broadcast, when id is empty
//*****************************************************************************
void App::Impl::onSend(const std::vector<unsigned char>& id, const std::vector<unsigned char>& message, std::string wallet)
{
    std::vector<unsigned char> msg(id);
    if (msg.size() != 20) {
        std::cerr << "bad send address " << __FUNCTION__;
        return;
    }

    // body
    msg.insert(msg.end(), message.begin(), message.end());
    
    if (wallet.empty()) {
        // TODO: here send only back to the sender
        for (CNode* pnode : vNodes) {
            pnode->PushMessage("xrouter", msg);
        }
        
        return;
    }
    
    for (CNode* pnode : vNodes) {
        pnode->PushMessage("xrouter", msg);
    }
    return;
    
    // Send only to the service nodes that have the required wallet
    int nHeight;
    {
        LOCK(cs_main);
        CBlockIndex* pindex = chainActive.Tip();
        if(!pindex) return;
        nHeight = pindex->nHeight;
    }
    std::vector<pair<int, CServicenode> > vServicenodeRanks = mnodeman.GetServicenodeRanks(nHeight);
    
    LOCK(cs_vNodes);
    for (CNode* pnode : vNodes) {
        BOOST_FOREACH (PAIRTYPE(int, CServicenode) & s, vServicenodeRanks) {
            if (s.second.addr.ToString() == pnode->addr.ToString()) {
                // This node is a service node
                std::cout << "sn " << s.second.addr.ToString() << " " << s.second.GetConnectedWalletsStr() << std::endl;
                std::vector<string> wallets;
                std::string wstr = s.second.GetConnectedWalletsStr();
                boost::split(wallets, wstr, boost::is_any_of(","));
                if (std::find(wallets.begin(), wallets.end(), wallet) != wallets.end()) {
                    pnode->PushMessage("xrouter", msg);
                }
            }
            
        }
    }
}

//*****************************************************************************
//*****************************************************************************
void App::sendPacket(const std::vector<unsigned char>& id, const XRouterPacketPtr& packet, std::string wallet)
{
    m_p->onSend(id, packet->body(), wallet);
}

//*****************************************************************************
//*****************************************************************************
static bool verifyBlockRequirement(const XRouterPacketPtr& packet)
{
    if (packet->size() < 36) {
        std::clog << "packet not big enough\n";
        return false;
    }

    uint256 txHash(packet->data());
    CTransaction txval;
    uint256 hashBlock;
    int offset = 32;
    uint32_t vout = *static_cast<uint32_t*>(static_cast<void*>(packet->data() + offset));

    CCoins coins;
    CTxOut txOut;
    if (pcoinsTip->GetCoins(txHash, coins)) {
        if (vout > coins.vout.size()) {
            std::clog << "Invalid vout index " << vout << "\n";
            return false;
        }   
        
        txOut = coins.vout[vout];
    } else if (GetTransaction(txHash, txval, hashBlock, true)) {
        txOut = txval.vout[vout];
    } else {
        std::clog << "Could not find " << txHash.ToString() << "\n";
        return false;   
    }

    if (txOut.nValue < minBlock) {
        std::clog << "Insufficient BLOCK " << txOut.nValue << "\n";
        return false;
    }

    CTxDestination destination;
    if (!ExtractDestination(txOut.scriptPubKey, destination)) {
        std::clog << "Unable to extract destination\n";
        return false;
    }

    auto txKeyID = boost::get<CKeyID>(&destination);
    if (!txKeyID) {
        std::clog << "destination must be a single address\n";
        return false;
    }

    CPubKey packetKey(packet->pubkey(),
        packet->pubkey() + XRouterPacket::pubkeySize);

    if (packetKey.GetID() != *txKeyID) {
        std::clog << "Public key provided doesn't match UTXO destination.\n";
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool App::processGetBlockCount(XRouterPacketPtr packet) {
    uint32_t offset = 36;

    std::string uuid((const char *)packet->data()+offset);
    offset += uuid.size() + 1;
    std::string currency((const char *)packet->data()+offset);
    offset += currency.size() + 1;
    std::cout << uuid << " "<< currency << std::endl;
    
    std::string result = "query reply";
    
    xbridge::WalletConnectorPtr conn = connectorByCurrency(currency);
    if (conn)
    {
        Object res = conn->executeRpcCall("getblockcount", Array());
        const Value& res_val(res);
        result = json_spirit::write_string(res_val, true);
        std::cout << result << std::endl;
    }

    XRouterPacketPtr rpacket(new XRouterPacket(xrReply));

    rpacket->append(uuid);
    rpacket->append(result);
    sendPacket(rpacket);

    return true;
}

bool App::processGetBlockHash(XRouterPacketPtr packet) {
    uint32_t offset = 36;

    std::string uuid((const char *)packet->data()+offset);
    offset += uuid.size() + 1;
    std::string currency((const char *)packet->data()+offset);
    offset += currency.size() + 1;
    std::string blockId((const char *)packet->data()+offset);
    offset += blockId.size() + 1;
    std::cout << uuid << " "<< currency << " " << blockId << std::endl;
    
    std::string result = "query reply";
    
    xbridge::WalletConnectorPtr conn = connectorByCurrency(currency);
    if (conn)
    {
        Array a { std::stoi(blockId) };
        Object res = conn->executeRpcCall("getblockhash", a);
        const Value& res_val(res);
        result = json_spirit::write_string(res_val, true);
    }

    XRouterPacketPtr rpacket(new XRouterPacket(xrReply));

    rpacket->append(uuid);
    rpacket->append(result);
    sendPacket(rpacket);

    return true;
}

bool App::processGetBlock(XRouterPacketPtr packet) {
    uint32_t offset = 36;

    std::string uuid((const char *)packet->data()+offset);
    offset += uuid.size() + 1;
    std::string currency((const char *)packet->data()+offset);
    offset += currency.size() + 1;
    std::string blockHash((const char *)packet->data()+offset);
    offset += blockHash.size() + 1;
    std::cout << uuid << " "<< currency << " " << blockHash << std::endl;
    
    std::string result = "query reply";
    
    xbridge::WalletConnectorPtr conn = connectorByCurrency(currency);
    if (conn)
    {
        Array a {blockHash};
        Object res = conn->executeRpcCall("getblock", a);
        const Value& res_val(res);
        result = json_spirit::write_string(res_val, true);
    }

    XRouterPacketPtr rpacket(new XRouterPacket(xrReply));

    rpacket->append(uuid);
    rpacket->append(result);
    sendPacket(rpacket);

    return true;
}

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

bool App::processGetTransaction(XRouterPacketPtr packet) {
    uint32_t offset = 36;

    std::string uuid((const char *)packet->data()+offset);
    offset += uuid.size() + 1;
    std::string currency((const char *)packet->data()+offset);
    offset += currency.size() + 1;
    std::string hash((const char *)packet->data()+offset);
    offset += hash.size() + 1;
    std::cout << uuid << " "<< currency << " " << hash << std::endl;
    
    std::string result = "query reply";
    
    xbridge::WalletConnectorPtr conn = connectorByCurrency(currency);
    if (conn)
    {
        Array a { hash };
        Value raw;
        bool code = getResultOrError(conn->executeRpcCall("getrawtransaction", a), raw);
        if (!code) {
            result = raw.get_str();
        } else {
            std::string txdata = raw.get_str();
            Array d { Value(txdata) };
            Value res_val = getResult(conn->executeRpcCall("decoderawtransaction", d));
            Object wrap;
            wrap.emplace_back(Pair("result", res_val));
            result = json_spirit::write_string(Value(wrap), true);
        }
    }

    XRouterPacketPtr rpacket(new XRouterPacket(xrReply));

    rpacket->append(uuid);
    rpacket->append(result);
    sendPacket(rpacket);

    return true;
}

static double parseVout(Value vout, std::string account) {
    double result = 0.0;
    double val = find_value(vout.get_obj(), "value").get_real();
    Object src = find_value(vout.get_obj(), "scriptPubKey").get_obj();
    const Value & addr_val = find_value(src, "addresses");
    if (addr_val.is_null())
        return 0.0;
    Array addr = addr_val.get_array();
    
    for (uint k = 0; k != addr.size(); k++ ) {
        std::string cur_addr = Value(addr[k]).get_str();
        if (cur_addr == account)
            result += val;
    }
    
    return result;
}

static double getBalanceChange(xbridge::WalletConnectorPtr conn, Object tx, std::string account) {
    double result = 0.0;

    Array vout = find_value(tx, "vout").get_array();
    for (uint j = 0; j != vout.size(); j++ ) {
        result += parseVout(vout[j], account);
    }

    Array vin = find_value(tx, "vin").get_array();
    for (uint j = 0; j != vin.size(); j++ ) {
        const Value& txid_val = find_value(vin[j].get_obj(), "txid");
        if (txid_val.is_null())
            continue;
        std::string txid = txid_val.get_str();
        int voutid = find_value(vin[j].get_obj(), "vout").get_int();
        Array c { Value(txid) };
        std::string txdata = getResult(conn->executeRpcCall("getrawtransaction", c)).get_str();
        Array d { Value(txdata) };
        Object prev_tx = getResult(conn->executeRpcCall("decoderawtransaction", d)).get_obj();
        Array prev_vouts = find_value(prev_tx, "vout").get_array();
        result -= parseVout(prev_vouts[voutid], account);
    }
    
    return result;
}

static bool checkFilterFit(xbridge::WalletConnectorPtr conn, Object tx, CBloomFilter filter) {
    Array vout = find_value(tx, "vout").get_array();
    for (uint j = 0; j != vout.size(); j++ ) {
        Object src = find_value(vout[j].get_obj(), "scriptPubKey").get_obj();
        std::string outkey = find_value(src, "hex").get_str();
        std::vector<unsigned char> outkeyv(outkey.begin(), outkey.end());
        CScript vouts(outkeyv.begin(), outkeyv.end());
        CScript::const_iterator pc = vouts.begin();
        vector<unsigned char> data;
        while (pc < vouts.end()) {
            opcodetype opcode;
            if (!vouts.GetOp(pc, opcode, data))
                break;
            if (data.size() != 0 && filter.contains(data)) {
                return true;
            }
        }
    }

    Array vin = find_value(tx, "vin").get_array();
    for (uint j = 0; j != vin.size(); j++ ) {
        const Value& txid_val = find_value(vin[j].get_obj(), "scriptSig");
        if (txid_val.is_null())
            continue;
        std::string inkey = find_value(txid_val.get_obj(), "hex").get_str();
        std::vector<unsigned char> inkeyv(inkey.begin(), inkey.end());
        CScript vins(inkeyv.begin(), inkeyv.end());
        CScript::const_iterator pc = vins.begin();
        vector<unsigned char> data;
        while (pc < vins.end()) {
            opcodetype opcode;
            if (!vins.GetOp(pc, opcode, data))
                break;
            if (data.size() != 0 && filter.contains(data))
                return true;
        }
    }
    
    return false;
}

bool App::processGetAllBlocks(XRouterPacketPtr packet) {
    uint32_t offset = 36;

    std::string uuid((const char *)packet->data()+offset);
    offset += uuid.size() + 1;
    std::string currency((const char *)packet->data()+offset);
    offset += currency.size() + 1;
    std::string number_s((const char *)packet->data()+offset);
    offset += number_s.size() + 1;
    std::cout << uuid << " "<< currency << " " << number_s << std::endl;
    int number = std::stoi(number_s);
    
    xbridge::WalletConnectorPtr conn = connectorByCurrency(currency);
    Array result;
    if (conn)
    {
        Value res = getResult(conn->executeRpcCall("getblockcount", Array()));
        int blockcount = res.get_int();
        Value res_val;
        for (int id = number; id <= blockcount; id++) {
            Array a { Value(id) };
            std::string hash = getResult(conn->executeRpcCall("getblockhash", a)).get_str();
            Array b { Value(hash) };
            result.push_back(getResult(conn->executeRpcCall("getblock", b)));
        }
    }

    XRouterPacketPtr rpacket(new XRouterPacket(xrReply));

    rpacket->append(uuid);
    rpacket->append(json_spirit::write_string(Value(result), true));
    sendPacket(rpacket);

    return true;
}

bool App::processGetAllTransactions(XRouterPacketPtr packet) {
    uint32_t offset = 36;

    std::string uuid((const char *)packet->data()+offset);
    offset += uuid.size() + 1;
    std::string currency((const char *)packet->data()+offset);
    offset += currency.size() + 1;
    std::string account((const char *)packet->data()+offset);
    offset += account.size() + 1;
    std::string number_s((const char *)packet->data()+offset);
    offset += number_s.size() + 1;
    std::cout << uuid << " "<< currency << " " << number_s << std::endl;
    int number = std::stoi(number_s);
    
    xbridge::WalletConnectorPtr conn = connectorByCurrency(currency);
    Array result;
    if (conn)
    {
        Value res = getResult(conn->executeRpcCall("getblockcount", Array()));
        int blockcount = res.get_int();
        for (int id = number; id <= blockcount; id++) {
            Array a { Value(id) };
            std::string hash = getResult(conn->executeRpcCall("getblockhash", a)).get_str();
            Array b { Value(hash) };
            Object block = getResult(conn->executeRpcCall("getblock", b)).get_obj();
            Array txs = find_value(block, "tx").get_array();
            std::cout << "block " << id << " " << txs.size() << std::endl;
            for (uint j = 0; j < txs.size(); j++) {
                std::string txid = Value(txs[j]).get_str();
                Array c { Value(txid) };
                std::string txdata = getResult(conn->executeRpcCall("getrawtransaction", c)).get_str();
                Array d { Value(txdata) };
                Object tx = getResult(conn->executeRpcCall("decoderawtransaction", d)).get_obj();
                if (getBalanceChange(conn, tx, account) != 0.0)
                    result.push_back(Value(tx));
            }
        }
    }

    XRouterPacketPtr rpacket(new XRouterPacket(xrReply));

    rpacket->append(uuid);
    rpacket->append(json_spirit::write_string(Value(result), true));
    sendPacket(rpacket);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool App::processGetBalance(XRouterPacketPtr packet) {
    uint32_t offset = 36;

    std::string uuid((const char *)packet->data()+offset);
    offset += uuid.size() + 1;
    std::string currency((const char *)packet->data()+offset);
    offset += currency.size() + 1;
    std::string account((const char *)packet->data()+offset);
    offset += account.size() + 1;
    std::cout << uuid << " "<< currency << " " << account << std::endl;
    
    xbridge::WalletConnectorPtr conn = connectorByCurrency(currency);
    double result = 0.0;
    if (conn)
    {
        Value res = getResult(conn->executeRpcCall("getblockcount", Array()));
        int blockcount = res.get_int();
        for (int id = 0; id <= blockcount; id++) {
            Array a { Value(id) };
            std::string hash = getResult(conn->executeRpcCall("getblockhash", a)).get_str();
            Array b { Value(hash) };
            Object block = getResult(conn->executeRpcCall("getblock", b)).get_obj();
            Array txs = find_value(block, "tx").get_array();
            std::cout << "block " << id << " " << txs.size() << std::endl;
            for (uint j = 0; j < txs.size(); j++) {
                std::string txid = Value(txs[j]).get_str();
                Array c { Value(txid) };
                std::string txdata = getResult(conn->executeRpcCall("getrawtransaction", c)).get_str();
                Array d { Value(txdata) };
                Object tx = getResult(conn->executeRpcCall("decoderawtransaction", d)).get_obj();
                result += getBalanceChange(conn, tx, account);
            }
        }
    }
    
    XRouterPacketPtr rpacket(new XRouterPacket(xrReply));

    rpacket->append(uuid);
    rpacket->append(std::to_string(result));
    sendPacket(rpacket);

    return true;
}

bool App::processGetBalanceUpdate(XRouterPacketPtr packet) {
    uint32_t offset = 36;

    std::string uuid((const char *)packet->data()+offset);
    offset += uuid.size() + 1;
    std::string currency((const char *)packet->data()+offset);
    offset += currency.size() + 1;
    std::string account((const char *)packet->data()+offset);
    offset += account.size() + 1;
    std::string number_s((const char *)packet->data()+offset);
    offset += number_s.size() + 1;
    std::cout << uuid << " "<< currency << " " << number_s << std::endl;
    int number = std::stoi(number_s);
    
    xbridge::WalletConnectorPtr conn = connectorByCurrency(currency);
    double result = 0.0;
    if (conn)
    {
        Value res = getResult(conn->executeRpcCall("getblockcount", Array()));
        int blockcount = res.get_int();
        for (int id = number; id <= blockcount; id++) {
            Array a { Value(id) };
            std::string hash = getResult(conn->executeRpcCall("getblockhash", a)).get_str();
            Array b { Value(hash) };
            Object block = getResult(conn->executeRpcCall("getblock", b)).get_obj();
            Array txs = find_value(block, "tx").get_array();
            std::cout << "block " << id << " " << txs.size() << std::endl;
            for (uint j = 0; j < txs.size(); j++) {
                std::string txid = Value(txs[j]).get_str();
                Array c { Value(txid) };
                std::string txdata = getResult(conn->executeRpcCall("getrawtransaction", c)).get_str();
                Array d { Value(txdata) };
                Object tx = getResult(conn->executeRpcCall("decoderawtransaction", d)).get_obj();
                result += getBalanceChange(conn, tx, account);
            }
        }
    }
    
    XRouterPacketPtr rpacket(new XRouterPacket(xrReply));

    rpacket->append(uuid);
    rpacket->append(std::to_string(result));
    sendPacket(rpacket);

    return true;
}

bool App::processGetTransactionsBloomFilter(XRouterPacketPtr packet) {
    uint32_t offset = 36;

    std::string uuid((const char *)packet->data()+offset);
    offset += uuid.size() + 1;
    std::string currency((const char *)packet->data()+offset);
    offset += currency.size() + 1;
    std::string number_s((const char *)packet->data()+offset);
    offset += number_s.size() + 1;

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream.resize(packet->size() - offset);
    memcpy(&stream[0], packet->data()+offset, packet->size() - offset);
    std::cout << uuid << " "<< currency << " " << stream.str() << " " << stream.size() << " " << number_s << std::endl;
    int number = std::stoi(number_s);
    
    xbridge::WalletConnectorPtr conn = connectorByCurrency(currency);
    CBloomFilter ft;
    stream >> ft;
    CBloomFilter filter(ft);
    filter.UpdateEmptyFull();
    
    Array result;
    if (conn)
    {
        Value res = getResult(conn->executeRpcCall("getblockcount", Array()));
        int blockcount = res.get_int();
        for (int id = number; id <= blockcount; id++) {
            Array a { Value(id) };
            std::string hash = getResult(conn->executeRpcCall("getblockhash", a)).get_str();
            Array b { Value(hash) };
            Object block = getResult(conn->executeRpcCall("getblock", b)).get_obj();
            Array txs = find_value(block, "tx").get_array();
            std::cout << "block " << id << " " << txs.size() << std::endl;
            for (uint j = 0; j < txs.size(); j++) {
                std::string txid = Value(txs[j]).get_str();
                Array c { Value(txid) };
                std::string txdata = getResult(conn->executeRpcCall("getrawtransaction", c)).get_str();
                Array d { Value(txdata) };
                Object tx = getResult(conn->executeRpcCall("decoderawtransaction", d)).get_obj();
                if (checkFilterFit(conn, tx, filter))
                    result.push_back(Value(tx));
            }
        }
    }

    XRouterPacketPtr rpacket(new XRouterPacket(xrReply));

    rpacket->append(uuid);
    rpacket->append(json_spirit::write_string(Value(result), true));
    sendPacket(rpacket);

    return true;
}

bool App::processSendTransaction(XRouterPacketPtr packet) {
    uint32_t offset = 36;

    std::string uuid((const char *)packet->data()+offset);
    offset += uuid.size() + 1;
    std::string currency((const char *)packet->data()+offset);
    offset += currency.size() + 1;
    std::string transaction((const char *)packet->data()+offset);
    offset += transaction.size() + 1;
    
    std::string result = "sent";
    
    XRouterPacketPtr rpacket(new XRouterPacket(xrReply));

    rpacket->append(uuid);
    rpacket->append(result);
    sendPacket(rpacket);

    return true;
}

bool App::processGetPaymentAddress(XRouterPacketPtr packet) {
    
}

//*****************************************************************************
//*****************************************************************************
bool App::processReply(XRouterPacketPtr packet) {
    std::cout << "Processing Reply\n";
    
    uint32_t offset = 0;

    std::string uuid((const char *)packet->data()+offset);
    offset += uuid.size() + 1;
    std::string reply((const char *)packet->data()+offset);
    offset += reply.size() + 1;
    std::cout << uuid << " " << reply << std::endl;
    
    // check uuid is in queriesLock keys
    if (!queriesLocks.count(uuid))
        return true;
    
    boost::mutex::scoped_lock l(*queriesLocks[uuid].first);
    if (!queries.count(uuid))
        queries[uuid] = vector<std::string>();
    queries[uuid].push_back(reply);
    queriesLocks[uuid].second->notify_all();
    return true;
}

//*****************************************************************************
//*****************************************************************************
void App::onMessageReceived(const std::vector<unsigned char>& id,
    const std::vector<unsigned char>& message,
    CValidationState& /*state*/)
{
    std::cerr << "Received xrouter packet\n";

    XRouterPacketPtr packet(new XRouterPacket);
    if (!packet->copyFrom(message)) {
        std::clog << "incorrect packet received " << __FUNCTION__;
        return;
    }

    if ((packet->command() != xrReply) && !packet->verify()) {
        std::clog << "unsigned packet or signature error " << __FUNCTION__;
        return;
    }
    
    if ((packet->command() != xrReply) && !verifyBlockRequirement(packet)) {
        std::clog << "Block requirement not satisfied\n";
        return;
    }

    switch (packet->command()) {
      case xrGetBlockCount:
        processGetBlockCount(packet);
        break;
      case xrGetBlockHash:
        processGetBlockHash(packet);
        break;
      case xrGetBlock:
        processGetBlock(packet);
        break;
      case xrGetTransaction:
        processGetTransaction(packet);
        break;
      case xrGetAllBlocks:
        processGetAllBlocks(packet);
        break;
      case xrGetAllTransactions:
        processGetAllTransactions(packet);
        break;
      case xrGetBalance:
        processGetBalance(packet);
        break;
      case xrGetBalanceUpdate:
        processGetBalanceUpdate(packet);
        break;
      case xrGetTransactionsBloomFilter:
        processGetTransactionsBloomFilter(packet);
        break;
      case xrReply:
        processReply(packet);
        break;
      default:
        std::clog << "Unknown packet\n";
        break;
    }
}

//*****************************************************************************
//*****************************************************************************
static bool satisfyBlockRequirement(uint256& txHash, uint32_t& vout, CKey& key)
{
    if (!pwalletMain) {
        return false;
    }
    for (auto& addressCoins : pwalletMain->AvailableCoinsByAddress()) {
        for (auto& output : addressCoins.second) {
            if (output.Value() >= minBlock) {
                CKeyID keyID;
                if (!addressCoins.first.GetKeyID(keyID)) {
                    std::cerr << "GetKeyID failed\n";
                    continue;
                }
                if (!pwalletMain->GetKey(keyID, key)) {
                    std::cerr << "GetKey failed\n";
                    continue;
                }
                txHash = output.tx->GetHash();
                vout = output.i;
                return true;
            }
        }
    }
    return false;
}

//*****************************************************************************
//*****************************************************************************
std::string App::xrouterCall(enum XRouterCommand command, const std::string & currency, std::string param1, std::string param2, std::string confirmations)
{
    std::cout << "process Query" << std::endl;
    XRouterPacketPtr packet(new XRouterPacket(command));

    uint256 txHash;
    uint32_t vout;
    CKey key;
    if (!satisfyBlockRequirement(txHash, vout, key)) {
        std::cerr << "Minimum block requirement not satisfied\n";
        return "Minimum block requirement not satisfied";
    }
    std::cout << "txHash = " << txHash.ToString() << "\n";
    std::cout << "vout = " << vout << "\n";
    std::cout << "Sending xrGetBlock packet...\n";
    
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    std::string id = boost::uuids::to_string(uuid);
    
    packet->append(txHash.begin(), 32);
    packet->append(vout);
    packet->append(id);
    packet->append(currency);
    if (!param1.empty())
        packet->append(param1);
    if (!param2.empty())
        packet->append(param2);
    packet->sign(key);
    
    if (!confirmations.empty())
        return sendPacketAndWait(packet, id, currency, std::stoi(confirmations), 300000);
    else
        return sendPacketAndWait(packet, id, currency);
}

std::string App::getBlockCount(const std::string & currency, const std::string & confirmations)
{
    return this->xrouterCall(xrGetBlockCount, currency, "", "", confirmations);
}

std::string App::getBlockHash(const std::string & currency, const std::string & blockId, const std::string & confirmations)
{
    return this->xrouterCall(xrGetBlockHash, currency, blockId, "", confirmations);
}

std::string App::getBlock(const std::string & currency, const std::string & blockHash, const std::string & confirmations)
{
    return this->xrouterCall(xrGetBlock, currency, blockHash, "", confirmations);
}

std::string App::getTransaction(const std::string & currency, const std::string & hash, const std::string & confirmations)
{
    return this->xrouterCall(xrGetTransaction, currency, hash, "", confirmations);
}

std::string App::getAllBlocks(const std::string & currency, const std::string & number, const std::string & confirmations)
{
    return this->xrouterCall(xrGetAllBlocks, currency, number, "", confirmations);
}

std::string App::getAllTransactions(const std::string & currency, const std::string & account, const std::string & number, const std::string & confirmations)
{
    return this->xrouterCall(xrGetAllTransactions, currency, account, number, confirmations);
}

std::string App::getBalance(const std::string & currency, const std::string & account, const std::string & confirmations)
{
    return this->xrouterCall(xrGetBalance, currency, account, "", confirmations);
}

std::string App::getBalanceUpdate(const std::string & currency, const std::string & account, const std::string & number, const std::string & confirmations)
{
    return this->xrouterCall(xrGetBalanceUpdate, currency, account, number, confirmations);
}

std::string App::getTransactionsBloomFilter(const std::string & currency, const std::string & number, const std::string & filter, const std::string & confirmations)
{
    return this->xrouterCall(xrGetTransactionsBloomFilter, currency, number, filter, confirmations);
}

std::string App::sendTransaction(const std::string & currency, const std::string & transaction)
{
    return this->xrouterCall(xrSendTransaction, currency, transaction, "", "1");
}

std::string App::getPaymentAddress(CNode* node)
{
    
}
} // namespace xrouter
