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

#include "xbridge/xkey.h"
#include "xbridge/util/settings.h"
#include "xbridge/xbridgewallet.h"
#include "xbridge/xbridgewalletconnector.h"

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
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

static const CAmount minBlock = 2;

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
                else if (wp.method == "BTC")
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

std::string App::sendPacketAndWait(const XRouterPacketPtr & packet, std::string id, std::string currency, int timeout)
{
    boost::shared_ptr<boost::mutex> m(new boost::mutex());
    boost::shared_ptr<boost::condition_variable> cond(new boost::condition_variable());
    boost::mutex::scoped_lock lock(*m);
    sendPacket(packet, currency);

    queriesLocks[id] = std::pair<boost::shared_ptr<boost::mutex>, boost::shared_ptr<boost::condition_variable> >(m, cond);
    if(!cond->timed_wait(lock, boost::posix_time::milliseconds(timeout))) {
        return "Failed to get response";
    } else {
        return queries[id];
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

    // timestamp
    boost::posix_time::ptime timestamp = boost::posix_time::microsec_clock::universal_time();
    uint64_t timestampValue = xrouter::util::timeToInt(timestamp);
    unsigned char* ptr = reinterpret_cast<unsigned char*>(&timestampValue);
    msg.insert(msg.end(), ptr, ptr + sizeof(uint64_t));

    // body
    msg.insert(msg.end(), message.begin(), message.end());
    
    if (wallet.empty()) {
        // TODO: here send only back to the sender
        for (CNode* pnode : vNodes) {
            pnode->PushMessage("xrouter", msg);
        }
    }
    
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
    int offset = 32;
    uint32_t vout = *static_cast<uint32_t*>(static_cast<void*>(packet->data() + offset));

    CCoins coins;
    if (!pcoinsTip->GetCoins(txHash, coins)) {
        std::clog << "Could not find " << txHash.ToString() << "\n";
        return false;
    }

    if (vout > coins.vout.size()) {
        std::clog << "Invalid vout index " << vout << "\n";
        return false;
    }

    auto& txOut = coins.vout[vout];

    if (txOut.nValue < minBlock) {
        std::clog << "Insufficient BLOCK " << coins.vout[vout].nValue << "\n";
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
    std::cout << "Processing GetBlocks\n";
    if (!packet->verify())
    {
      std::clog << "unsigned packet or signature error " << __FUNCTION__;
        return false;
    }
    
    if (!verifyBlockRequirement(packet)) {
        std::clog << "Block requirement not satisfied\n";
        return false;
    }
    
    uint32_t offset = 36;

    std::string uuid((const char *)packet->data()+offset);
    offset += uuid.size() + 1;
    std::string currency((const char *)packet->data()+offset);
    offset += currency.size() + 1;
    std::cout << uuid << " "<< currency << std::endl;
    
    std::string result = "query reply";
    //
    // SEND THE QUERY TO WALLET CONNECTOR HERE
    //
    
    xbridge::WalletConnectorPtr conn = connectorByCurrency(currency);
    if (conn)
    {
        Object res = conn->executeRpcCall("getblockcount", Array());
        const Value& res_val(res);
        result = json_spirit::write_string(res_val, true);
    }

    XRouterPacketPtr rpacket(new XRouterPacket(xrReply));

    rpacket->append(uuid);
    rpacket->append(result);
    sendPacket(rpacket);

    return true;
}

bool App::processGetBlockHash(XRouterPacketPtr packet) {
    std::cout << "Processing GetBlocks\n";
    if (!packet->verify())
    {
        std::clog << "unsigned packet or signature error " << __FUNCTION__;
        return false;
    }
    
    if (!verifyBlockRequirement(packet)) {
        std::clog << "Block requirement not satisfied\n";
        return false;
    }
    
    uint32_t offset = 36;

    std::string uuid((const char *)packet->data()+offset);
    offset += uuid.size() + 1;
    std::string currency((const char *)packet->data()+offset);
    offset += currency.size() + 1;
    std::string blockId((const char *)packet->data()+offset);
    offset += blockId.size() + 1;
    std::cout << uuid << " "<< currency << " " << blockId << std::endl;
    
    std::string result = "query reply";
    //
    // SEND THE QUERY TO WALLET CONNECTOR HERE
    //
    
    xbridge::WalletConnectorPtr conn = connectorByCurrency(currency);
    if (conn)
    {
        Array a {blockId};
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

bool App::processGetBlock(XRouterPacketPtr packet) {
    std::cout << "Processing GetBlocks\n";
    if (!packet->verify())
    {
        std::clog << "unsigned packet or signature error " << __FUNCTION__;
        return false;
    }
    
    if (!verifyBlockRequirement(packet)) {
        std::clog << "Block requirement not satisfied\n";
        return false;
    }
    
    uint32_t offset = 36;

    std::string uuid((const char *)packet->data()+offset);
    offset += uuid.size() + 1;
    std::string currency((const char *)packet->data()+offset);
    offset += currency.size() + 1;
    std::string blockHash((const char *)packet->data()+offset);
    offset += blockHash.size() + 1;
    std::cout << uuid << " "<< currency << " " << blockHash << std::endl;
    
    std::string result = "query reply";
    //
    // SEND THE QUERY TO WALLET CONNECTOR HERE
    //
    
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

bool App::processGetTransaction(XRouterPacketPtr packet) {
    std::cout << "Processing GetTransaction\n";
    if (!packet->verify())
    {
        std::clog << "unsigned packet or signature error " << __FUNCTION__;
        return false;
    }
    
    if (!verifyBlockRequirement(packet)) {
        std::clog << "Block requirement not satisfied\n";
        return false;
    }
    
    uint32_t offset = 36;

    std::string uuid((const char *)packet->data()+offset);
    offset += uuid.size() + 1;
    std::string currency((const char *)packet->data()+offset);
    offset += currency.size() + 1;
    std::string hash((const char *)packet->data()+offset);
    offset += hash.size() + 1;
    std::cout << uuid << " "<< currency << " " << hash << std::endl;
    
    std::string result = "query reply";
    //
    // SEND THE QUERY TO WALLET CONNECTOR HERE
    //
    
    xbridge::WalletConnectorPtr conn = connectorByCurrency(currency);
    if (conn)
    {
        Array a {hash};
        Object res = conn->executeRpcCall("gettransaction", a);
        const Value& res_val(res);
        result = json_spirit::write_string(res_val, true);
    }

    XRouterPacketPtr rpacket(new XRouterPacket(xrReply));

    rpacket->append(uuid);
    rpacket->append(result);
    sendPacket(rpacket);

    return true;
}

bool App::processGetAllBlocks(XRouterPacketPtr packet) {
    std::cout << "Processing GetTransaction\n";
    if (!packet->verify())
    {
        std::clog << "unsigned packet or signature error " << __FUNCTION__;
        return false;
    }
    
    if (!verifyBlockRequirement(packet)) {
        std::clog << "Block requirement not satisfied\n";
        return false;
    }
    
    uint32_t offset = 36;

    std::string uuid((const char *)packet->data()+offset);
    offset += uuid.size() + 1;
    std::string currency((const char *)packet->data()+offset);
    offset += currency.size() + 1;
    std::string number_s((const char *)packet->data()+offset);
    offset += number_s.size() + 1;
    std::cout << uuid << " "<< currency << " " << number_s << std::endl;
    int number = std::stoi(number_s);
    
    //
    // SEND THE QUERY TO WALLET CONNECTOR HERE
    //
    
    xbridge::WalletConnectorPtr conn = connectorByCurrency(currency);
    Array result;
    if (conn)
    {
        Object res = conn->executeRpcCall("getblockcount", Array());
        int blockcount = Value(res).get_int();
        Value res_val;
        for (int id = number; id <= blockcount; id++) {
            Array a { Value(id) };
            std::string hash = Value(conn->executeRpcCall("getblockhash", a)).get_str();
            Array b { Value(hash) };
            Object block = conn->executeRpcCall("getblock", b);
            result.push_back(Value(res));
        }
    }

    XRouterPacketPtr rpacket(new XRouterPacket(xrReply));

    rpacket->append(uuid);
    rpacket->append(json_spirit::write_string(Value(result), true));
    sendPacket(rpacket);

    return true;
}

static double parseVout(Value vout, std::string account) {
    double result = 0.0;
    mValue voutjmap;
    json_spirit::read_string(json_spirit::write_string(Value(vout), true), voutjmap);
    mObject voutj = voutjmap.get_obj();
    mObject src = voutj.find("scriptPubKey")->second.get_obj();
    mArray addr = mValue(src.find("addresses")->second).get_array();
    
    for (uint k = 0; k != addr.size(); k++ ) {
        std::string cur_addr = mValue(addr[k]).get_str();
        if (cur_addr == account)
            result += mValue(voutj.find("value")->second).get_real();
    }
    
    return result;
}

static double getBalanceChange(xbridge::WalletConnectorPtr conn, Object tx, std::string account) {
    double result = 0.0;
    for (Object::size_type i = 0; i != tx.size(); i++ ) {
        if (tx[i].name_ == "vout") {
            Array vout = Value(tx[i].value_).get_array();
            for (uint j = 0; j != vout.size(); i++ ) {
                result += parseVout(vout[j], account);
            }
        }
        
        if (tx[i].name_ == "vin") {
            Array vin = Value(tx[i].value_).get_array();
            for (uint j = 0; j != vin.size(); i++ ) {
                std::string txid;
                int voutid;
                Object vinj = vin[j].get_obj();
                for (Object::size_type k = 0; k != vinj.size(); k++ ) {
                    if (vinj[k].name_ == "txid")
                        txid = Value(vinj[k].value_).get_str();
                    if (vinj[k].name_ == "vout")
                        voutid = Value(vinj[k].value_).get_int();
                }
                
                Array c { Value(txid) };
                std::string txdata = Value(conn->executeRpcCall("getrawtransaction", c)).get_str();
                Array d { Value(txdata) };
                Object prev_tx = conn->executeRpcCall("decoderawtransaction", d);
                for (Object::size_type i = 0; i != prev_tx.size(); i++ ) {
                    if (prev_tx[i].name_ == "vout") {
                        Array prev_vouts = Value(prev_tx[i].value_).get_array();
                        result -= parseVout(prev_vouts[voutid], account);
                    }
                }
            }
        }
    }
    
    return result;
}

bool App::processGetAllTransactions(XRouterPacketPtr packet) {
    std::cout << "Processing GetTransaction\n";
    if (!packet->verify())
    {
        std::clog << "unsigned packet or signature error " << __FUNCTION__;
        return false;
    }
    
    if (!verifyBlockRequirement(packet)) {
        std::clog << "Block requirement not satisfied\n";
        return false;
    }
    
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
    
    //
    // SEND THE QUERY TO WALLET CONNECTOR HERE
    //
    
    xbridge::WalletConnectorPtr conn = connectorByCurrency(currency);
    Array result;
    if (conn)
    {
        Object res = conn->executeRpcCall("getblockcount", Array());
        int blockcount = Value(res).get_int();
        Value res_val;
        for (int id = number; id <= blockcount; id++) {
            Array a { Value(id) };
            std::string hash = Value(conn->executeRpcCall("getblockhash", a)).get_str();
            Array b { Value(hash) };
            Object block = conn->executeRpcCall("getblock", b);
            for (Object::size_type i = 0; i != block.size(); i++ ) {
                if(block[i].name_ == "tx") {
                    Array txs = block[i].value_.get_array();
                    for (uint j = 0; j < txs.size(); i++) {
                        std::string txid = Value(txs[i]).get_str();
                        Array c { Value(txid) };
                        std::string txdata = Value(conn->executeRpcCall("getrawtransaction", c)).get_str();
                        Array d { Value(txdata) };
                        Object tx = conn->executeRpcCall("decoderawtransaction", d);
                        if (getBalanceChange(conn, tx, account) != 0.0)
                            result.push_back(Value(tx));
                    }
                    break;
                }
            }
            
            //result.push_back(Value(res));
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
    std::cout << "Processing GetBalances\n";
    if (!packet->verify())
    {
      std::clog << "unsigned packet or signature error " << __FUNCTION__;
        return false;
    }
    
    if (!verifyBlockRequirement(packet)) {
        std::clog << "Block requirement not satisfied\n";
        return false;
    }
    
    uint32_t offset = 36;

    std::string uuid((const char *)packet->data()+offset);
    offset += uuid.size() + 1;
    std::string currency((const char *)packet->data()+offset);
    offset += currency.size() + 1;
    std::string account((const char *)packet->data()+offset);
    offset += account.size() + 1;
    std::cout << uuid << " "<< currency << " " << account << std::endl;
    
    std::string result = "query reply";
    //
    // SEND THE QUERY TO WALLET CONNECTOR HERE
    //
    
    xbridge::WalletConnectorPtr conn = connectorByCurrency(currency);
    if (conn)
    {
        Array a;
        Object res = conn->executeRpcCall("getbalance", a);
        const Value& res_val(res);
        result = json_spirit::write_string(res_val, true);
    }

    XRouterPacketPtr rpacket(new XRouterPacket(xrReply));

    rpacket->append(uuid);
    rpacket->append(result);
    sendPacket(rpacket);

    return true;
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
    // TODO: check uuid is in queriesLock keys
    boost::mutex::scoped_lock l(*queriesLocks[uuid].first);
    queries[uuid] = reply;
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

    /* std::clog << "received message to " << util::base64_encode(std::string((char*)&id[0], 20)).c_str() */
    /*           << " command " << packet->command(); */

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
std::string App::xrouterCall(enum XRouterCommand command, const std::string & currency, std::string param1, std::string param2)
{
    std::cout << "process Query" << std::endl;
    XRouterPacketPtr packet(new XRouterPacket(command));

    uint256 txHash;
    uint32_t vout;
    CKey key;
    if (!satisfyBlockRequirement(txHash, vout, key)) {
        std::cerr << "Minimum block requirement not satisfied\n";
        return "Minimum block";
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
    
    return sendPacketAndWait(packet, id, currency);
}

std::string App::getBlockCount(const std::string & currency)
{
    return this->xrouterCall(xrGetBlockCount, currency);
}

std::string App::getBlockHash(const std::string & currency, const std::string & blockId)
{
    return this->xrouterCall(xrGetBlockHash, currency, blockId);
}

std::string App::getBlock(const std::string & currency, const std::string & blockHash)
{
    return this->xrouterCall(xrGetBlock, currency, blockHash);
}

std::string App::getTransaction(const std::string & currency, const std::string & hash)
{
    return this->xrouterCall(xrGetTransaction, currency, hash);
}

std::string App::getAllBlocks(const std::string & currency, const std::string & number)
{
    return this->xrouterCall(xrGetAllBlocks, currency, number);
}

std::string App::getAllTransactions(const std::string & currency, const std::string & account, const std::string & number)
{
    return this->xrouterCall(xrGetAllTransactions, currency, account, number);
}

std::string App::getBalance(const std::string & currency, const std::string & account)
{
    return this->xrouterCall(xrGetBalance, currency, account);
}
} // namespace xrouter
