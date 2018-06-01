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
#include "wallet.h"
#include "bloom.h"

#include "xbridge/xkey.h"
#include "xbridge/util/settings.h"
#include "xbridge/xbridgewallet.h"
#include "xbridge/xbridgewalletconnector.h"


#include "xrouterconnector.h"
#include "xrouterconnectorbtc.h"
#include "xrouterconnectoreth.h"

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
#include <iostream>
#include <sstream>
#include <vector>

static const CAmount minBlock = 200;

#ifdef _WIN32
#include <objbase.h>
    
static std::string generateUUID()
{
    GUID guid;
	CoCreateGuid(&guid);
    char guid_string[37];
    sprintf(guid_string, sizeof(guid_string) / sizeof(guid_string[0]),
          "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
          guid.Data1, guid.Data2, guid.Data3,
          guid.Data4[0], guid.Data4[1], guid.Data4[2],
          guid.Data4[3], guid.Data4[4], guid.Data4[5],
          guid.Data4[6], guid.Data4[7]);
    return guid_string;
}
    
#else

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

static std::string generateUUID()
{
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    return boost::uuids::to_string(uuid);
}

#endif 

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
    xrouter::Connectors                                         m_connectors;
    xrouter::ConnectorsAddrMap                                  m_connectorAddressMap;
    xrouter::ConnectorsCurrencyMap                              m_connectorCurrencyMap;

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
    : m_p(new Impl), queries(), req_cnt(0)
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
    
    std::string path(GetDataDir(false).string());
    std::string xbridgepath = path + "/xbridge.conf";
    s.read(xbridgepath.c_str());
    s.parseCmdLine(argc, argv);

    std::string xrouterpath = path + "/xrouter.conf";
    this->xrouter_settings.read(xrouterpath.c_str());
    
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

                xrouter::WalletConnectorXRouterPtr conn;
                if ((wp.method == "ETH") || (wp.method == "ETHER"))
                {
                    conn.reset(new EthWalletConnectorXRouter);
                    *conn = wp;
                }
                else if ((wp.method == "BTC") || (wp.method == "BLOCK"))
                {
                    conn.reset(new BtcWalletConnectorXRouter);
                    *conn = wp;
                }
                else
                {
                    conn.reset(new BtcWalletConnectorXRouter);
                    *conn = wp;
                }
                if (!conn)
                {
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

void App::addConnector(const WalletConnectorXRouterPtr & conn)
{
    boost::mutex::scoped_lock l(m_p->m_connectorsLock);
    m_p->m_connectors.push_back(conn);
    m_p->m_connectorCurrencyMap[conn->currency] = conn;
}

WalletConnectorXRouterPtr App::connectorByCurrency(const std::string & currency) const
{
    boost::mutex::scoped_lock l(m_p->m_connectorsLock);
    if (m_p->m_connectorCurrencyMap.count(currency))
    {
        return m_p->m_connectorCurrencyMap.at(currency);
    }

    return xrouter::WalletConnectorXRouterPtr();
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
        error.emplace_back(Pair("uuid", id));
        return json_spirit::write_string(Value(error), true);
    }
    else
    {
        for (unsigned int i = 0; i < queries[id].size(); i++)
        {
            std::string cand = queries[id][i];
            int cnt = 0;
            for (unsigned int j = 0; j < queries[id].size(); j++)
            {
                if (queries[id][j] == cand)
                {
                    cnt++;
                    if (cnt > confirmations / 2)
                        return cand;
                }
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

    Object result;
    Object error;

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);
    if (conn)
    {
        result.push_back(Pair("result", conn->getBlockCount()));
    }
    else
    {
        error.emplace_back(Pair("error", "No connector for currency " + currency));
        result = error;
    }

    XRouterPacketPtr rpacket(new XRouterPacket(xrReply));

    rpacket->append(uuid);
    rpacket->append(json_spirit::write_string(Value(result), true));
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

    Object result;
    Object error;

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);
    if (conn)
    {
        result.push_back(Pair("result", conn->getBlockHash(blockId)));
    }
    else
    {
        error.emplace_back(Pair("error", "No connector for currency " + currency));
        result = error;
    }

    XRouterPacketPtr rpacket(new XRouterPacket(xrReply));

    rpacket->append(uuid);
    rpacket->append(json_spirit::write_string(Value(result), true));
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

    Object result;
    Object error;

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);
    if (conn)
    {
        result = conn->getBlock(blockHash);
    }
    else
    {
        error.emplace_back(Pair("error", "No connector for currency " + currency));
        result = error;
    }

    XRouterPacketPtr rpacket(new XRouterPacket(xrReply));

    rpacket->append(uuid);
    rpacket->append(json_spirit::write_string(Value(result), true));
    sendPacket(rpacket);

    return true;
}

bool App::processGetTransaction(XRouterPacketPtr packet) {
    uint32_t offset = 36;

    std::string uuid((const char *)packet->data()+offset);
    offset += uuid.size() + 1;
    std::string currency((const char *)packet->data()+offset);
    offset += currency.size() + 1;
    std::string hash((const char *)packet->data()+offset);
    offset += hash.size() + 1;

    Object result;
    Object error;

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);
    if (conn)
    {
        result = conn->getTransaction(hash);
    }
    else
    {
        error.emplace_back(Pair("error", "No connector for currency " + currency));
        result = error;
    }

    XRouterPacketPtr rpacket(new XRouterPacket(xrReply));

    rpacket->append(uuid);
    rpacket->append(json_spirit::write_string(Value(result), true));
    sendPacket(rpacket);

    return true;
}

bool App::processGetAllBlocks(XRouterPacketPtr packet) {
    uint32_t offset = 36;

    std::string uuid((const char *)packet->data()+offset);
    offset += uuid.size() + 1;
    std::string currency((const char *)packet->data()+offset);
    offset += currency.size() + 1;
    std::string number_s((const char *)packet->data()+offset);
    offset += number_s.size() + 1;
    int number = std::stoi(number_s);

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);
    Array result;
    if (conn)
    {
        result = conn->getAllBlocks(number);
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
    int number = std::stoi(number_s);

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);

    Array result;
    if (conn)
    {
        result = conn->getAllTransactions(account, number);
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

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);
    std::string result;
    if (conn)
    {
        result = conn->getBalance(account);
    }

    XRouterPacketPtr rpacket(new XRouterPacket(xrReply));

    rpacket->append(uuid);
    rpacket->append(result);
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
    int number = std::stoi(number_s);

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);

    std::string result;
    if (conn)
    {
        result = conn->getBalanceUpdate(account, number);
    }

    XRouterPacketPtr rpacket(new XRouterPacket(xrReply));

    rpacket->append(uuid);
    rpacket->append(result);
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

    int number = std::stoi(number_s);

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);

    Array result;
    if (conn)
    {
        result = conn->getTransactionsBloomFilter(number, stream);
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

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);

    Object result;
    if (conn)
    {
        result = conn->sendTransaction(transaction);
    }

    XRouterPacketPtr rpacket(new XRouterPacket(xrReply));

    rpacket->append(uuid);
    rpacket->append(json_spirit::write_string(Value(result), true));
    sendPacket(rpacket);

    return true;
}

bool App::processGetPaymentAddress(XRouterPacketPtr packet) {

}

bool App::processGetXrouterConfig(XRouterPacketPtr packet) {
    uint32_t offset = 36;

    std::string uuid((const char *)packet->data()+offset);
    offset += uuid.size() + 1;

    XRouterPacketPtr rpacket(new XRouterPacket(xrReply));
    rpacket->append(uuid);
    rpacket->append(this->xrouter_settings.rawText());
    sendPacket(rpacket);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool App::processReply(XRouterPacketPtr packet) {
    uint32_t offset = 0;

    std::string uuid((const char *)packet->data()+offset);
    offset += uuid.size() + 1;
    std::string reply((const char *)packet->data()+offset);
    offset += reply.size() + 1;

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

    // If Main.xrouter == 0, xrouter is turned offf on this snode
    int xrouter_on = xrouter_settings.get<int>("Main.xrouter", 0);
    if (!xrouter_on)
        return;
    
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
      case xrGetXrouterConfig:
        processGetXrouterConfig(packet);
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
    XRouterPacketPtr packet(new XRouterPacket(command));

    uint256 txHash;
    uint32_t vout;
    CKey key;
    if (!satisfyBlockRequirement(txHash, vout, key)) {
        std::cerr << "Minimum block requirement not satisfied\n";
        return "Minimum block requirement not satisfied";
    }

    //boost::uuids::uuid uuid = boost::uuids::random_generator()();
    //std::string id = boost::uuids::to_string(uuid);
    std::string id = generateUUID(); //"request" + std::to_string(req_cnt);
    req_cnt++;

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
        return sendPacketAndWait(packet, id, currency, std::stoi(confirmations), 20000);
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

std::string App::getReply(const std::string & id)
{
    Object result;

    if(queries[id].size() == 0) {
        result.emplace_back(Pair("error", "No replies found"));
        result.emplace_back(Pair("uuid", id));
        return json_spirit::write_string(Value(result), true);
    } else {
        for (unsigned int i = 0; i < queries[id].size(); i++) {
            std::string cand = queries[id][i];
            result.emplace_back(Pair("reply" + std::to_string(i+1), cand));
        }

        return json_spirit::write_string(Value(result), true);
    }
}

std::string App::sendTransaction(const std::string & currency, const std::string & transaction)
{
    return this->xrouterCall(xrSendTransaction, currency, transaction, "", "1");
}

std::string App::getPaymentAddress(CNode* node)
{

}

std::string App::getXrouterConfig(CNode* node) {
    XRouterPacketPtr packet(new XRouterPacket(xrGetXrouterConfig));

    uint256 txHash;
    uint32_t vout;
    CKey key;
    if (!satisfyBlockRequirement(txHash, vout, key)) {
        std::cerr << "Minimum block requirement not satisfied\n";
        return "Minimum block requirement not satisfied";
    }

    //boost::uuids::uuid uuid = boost::uuids::random_generator()();
    //std::string id = boost::uuids::to_string(uuid);
    std::string id = generateUUID(); //"request" + std::to_string(req_cnt);
    req_cnt++;

    packet->append(txHash.begin(), 32);
    packet->append(vout);
    packet->append(id);
    packet->sign(key);

    boost::shared_ptr<boost::mutex> m(new boost::mutex());
    boost::shared_ptr<boost::condition_variable> cond(new boost::condition_variable());
    boost::mutex::scoped_lock lock(*m);
    queriesLocks[id] = std::pair<boost::shared_ptr<boost::mutex>, boost::shared_ptr<boost::condition_variable> >(m, cond);

    static std::vector<unsigned char> addr(20, 0);
    std::vector<unsigned char> msg(addr);
    msg.insert(msg.end(), packet->body().begin(), packet->body().end());
    node->PushMessage("xrouter", msg);
    
    if (!cond->timed_wait(lock, boost::posix_time::milliseconds(30000)))
        return "Could not get XRouter config";

    std::string reply = queries[id][0];
    
    int nHeight;
    {
        LOCK(cs_main);
        CBlockIndex* pindex = chainActive.Tip();
        if(!pindex) return;
        nHeight = pindex->nHeight;
    }
    std::vector<pair<int, CServicenode> > vServicenodeRanks = mnodeman.GetServicenodeRanks(nHeight);

    LOCK(cs_vNodes);
    BOOST_FOREACH (PAIRTYPE(int, CServicenode) & s, vServicenodeRanks) {
        if (s.second.addr.ToString() == node->addr.ToString()) {
            s.second.xrouterConfig = reply;
        }

    }
    return reply;
}

} // namespace xrouter
