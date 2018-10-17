//******************************************************************************
//******************************************************************************

#include "xrouterserver.h"
#include "xrouterlogger.h"
#include "xbridge/util/settings.h"
#include "xrouterapp.h"
#include "servicenodeman.h"
#include "activeservicenode.h"
#include "rpcserver.h"

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

#include <boost/chrono/chrono.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/range/adaptor/map.hpp>
#include <boost/asio.hpp>

#include <algorithm>
#include <iostream>
#include <chrono>
#include <future>

namespace xrouter
{  

//*****************************************************************************
//*****************************************************************************
bool XRouterServer::start()
{
    // start xrouter
    try
    {
        // sessions
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

            this->addConnector(conn);
        }
    }
    catch (std::exception & e)
    {
        //ERR() << e.what();
        //ERR() << __FUNCTION__;
    }

    return true;
}

void XRouterServer::addConnector(const WalletConnectorXRouterPtr & conn)
{
    boost::mutex::scoped_lock l(m_connectorsLock);
    m_connectors.push_back(conn);
    m_connectorCurrencyMap[conn->currency] = conn;
}

WalletConnectorXRouterPtr XRouterServer::connectorByCurrency(const std::string & currency) const
{
    boost::mutex::scoped_lock l(m_connectorsLock);
    if (m_connectorCurrencyMap.count(currency))
    {
        return m_connectorCurrencyMap.at(currency);
    }

    return WalletConnectorXRouterPtr();
}

void XRouterServer::sendPacketToClient(const XRouterPacketPtr& packet, CNode* pnode)
{
    pnode->PushMessage("xrouter", packet->body());
}

//*****************************************************************************
//*****************************************************************************
static bool verifyBlockRequirement(const XRouterPacketPtr& packet)
{
    if (packet->size() < 36) {
        LOG() << "Packet not big enough";
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
            LOG() << "Invalid vout index " << vout;
            return false;
        }

        txOut = coins.vout[vout];
    } else if (GetTransaction(txHash, txval, hashBlock, true)) {
        txOut = txval.vout[vout];
    } else {
        LOG() << "Could not find " << txHash.ToString();
        return false;
    }

    if (txOut.nValue < MIN_BLOCK) {
        LOG() << "Insufficient BLOCK " << txOut.nValue;
        return false;
    }

    CTxDestination destination;
    if (!ExtractDestination(txOut.scriptPubKey, destination)) {
        LOG() << "Unable to extract destination";
        return false;
    }

    auto txKeyID = boost::get<CKeyID>(&destination);
    if (!txKeyID) {
        LOG() << "destination must be a single address";
        return false;
    }

    CPubKey packetKey(packet->pubkey(),
        packet->pubkey() + XRouterPacket::pubkeySize);

    if (packetKey.GetID() != *txKeyID) {
        LOG() << "Public key provided doesn't match UTXO destination.";
        return false;
    }

    return true;
}

inline void XRouterServer::sendReply(CNode* node, std::string uuid, std::string reply)
{
    XRouterPacketPtr rpacket(new XRouterPacket(xrReply));
    rpacket->append(uuid);
    rpacket->append(reply);
    sendPacketToClient(rpacket, node);
    LOG() << reply;
}

void XRouterServer::processPayment(CNode* node, std::string feetx, CAmount fee)
{
    if (fee > 0) {
        if (!paymentChannels.count(node)) {
            // There is no payment channel with this node
            
            if (feetx.find(";") == std::string::npos) {
                // Direct payment, no CLTV channel
                std::string txid;
                
                CAmount paid = to_amount(getTxValue(feetx, getMyPaymentAddress()));
                if (paid < fee) {
                    LOG() << "Fee paid is not enough: paid" << paid << "; fee = " << fee;
                    throw std::runtime_error("Fee paid is not enough");
                }

                bool res = sendTransactionBlockchain(feetx, txid);
                if (!res) {
                    LOG() << "Could not send transaction to blockchain";
                    throw std::runtime_error("Could not send transaction " + feetx + " to blockchain");
                }
                
                LOG() << "Got direct payment; value = " << paid << " tx = " << feetx; 
            } else {
                std::vector<std::string> parts;
                boost::split(parts, feetx, boost::is_any_of(";"));
                if (parts.size() != 4) {
                    throw std::runtime_error("Incorrect channel creation parameters");
                }
                
                paymentChannels[node] = PaymentChannel();
                paymentChannels[node].value = CAmount(0);
                paymentChannels[node].raw_tx = parts[0];
                paymentChannels[node].txid = parts[1];
                std::vector<unsigned char> script = ParseHex(parts[2]);
                paymentChannels[node].redeemScript = CScript(script.begin(), script.end());
                feetx = parts[3];
                
                // TODO: verify the channel's correctness

                // Send the closing tx 5 seconds before the deadline
                int date = getChannelExpiryTime(paymentChannels[node].raw_tx);
                int deadline = date - std::time(0) - 5000;
                
                LOG() << "Created payment channel date = " << date << " expiry = " << deadline << " ms"; 
                
                boost::thread([deadline, this, node]() {
                    boost::this_thread::sleep_for(boost::chrono::milliseconds(deadline));
                    std::string txid;
                    LOG() << "Closing payment channel: " << this->paymentChannels[node].txid << " Value = " << this->paymentChannels[node].value;
                    
                    sendTransactionBlockchain(this->paymentChannels[node].latest_tx, txid);
                    this->paymentChannels.erase(node);
                }).detach();
            }
        }
        
        if (paymentChannels.count(node)) {
            CAmount paid = to_amount(getTxValue(feetx, getMyPaymentAddress()));
            LOG() << "Got payment via channel; value = " << paid - paymentChannels[node].value << " total value = " << paid << " tx = " << feetx; 
            if (paid - paymentChannels[node].value < fee) {
                throw std::runtime_error("Fee paid is not enough");
            }
            
            finalizeChannelTransaction(paymentChannels[node], this->getMyPaymentAddressKey(), feetx, paymentChannels[node].latest_tx);
            paymentChannels[node].value = paid;
        }
    }

    return;
}

//*****************************************************************************
//*****************************************************************************
void XRouterServer::onMessageReceived(CNode* node, XRouterPacketPtr& packet, CValidationState& state)
{
    LOG() << "Processing packet on server side";
    App& app = App::instance();
    
    if (!packet->verify()) {
        LOG() << "unsigned packet or signature error " << __FUNCTION__;
        state.DoS(10, error("XRouter: unsigned packet or signature error"), REJECT_INVALID, "xrouter-error");
        return;
    }

    if (!verifyBlockRequirement(packet)) {
        LOG() << "Block requirement not satisfied\n";
        state.DoS(10, error("XRouter: block requirement not satisfied"), REJECT_INVALID, "xrouter-error");
        return;
    }

    std::string reply;
    uint32_t offset = 36;
    std::string uuid((const char *)packet->data()+offset);
    offset += uuid.size() + 1;
    std::string currency((const char *)packet->data()+offset);
    offset += currency.size() + 1;
    LOG() << "XRouter command: " << std::string(XRouterCommand_ToString(packet->command()));
    if (!app.xrouter_settings.isAvailableCommand(packet->command(), currency)) {
        LOG() << "This command is blocked in xrouter.conf";
        return;
    }
    
    std::chrono::time_point<std::chrono::system_clock> time = std::chrono::system_clock::now();
    if (packet->command() == xrCustomCall) {
        XRouterPluginSettings psettings = app.xrouter_settings.getPluginSettings(currency);
        
        std::string keystr = currency;
        double timeout = psettings.get<double>("timeout", -1.0);
        if (timeout >= 0) {
            if (lastPacketsReceived.count(node)) {
                if (lastPacketsReceived[node].count(keystr)) {
                    std::chrono::time_point<std::chrono::system_clock> prev_time = lastPacketsReceived[node][keystr];
                    std::chrono::system_clock::duration diff = time - prev_time;
                    if (std::chrono::duration_cast<std::chrono::milliseconds>(diff) < std::chrono::milliseconds((int)(timeout * 1000))) {
                        std::string err_msg = "XRouter: too many requests to plugin " + keystr; 
                        state.DoS(100, error(err_msg.c_str()), REJECT_INVALID, "xrouter-error");
                    }
                    if (!lastPacketsReceived.count(node))
                        lastPacketsReceived[node] = boost::container::map<std::string, std::chrono::time_point<std::chrono::system_clock> >();
                    lastPacketsReceived[node][keystr] = time;
                } else {
                    lastPacketsReceived[node][keystr] = time;
                }
            } else {
                lastPacketsReceived[node] = boost::container::map<std::string, std::chrono::time_point<std::chrono::system_clock> >();
                lastPacketsReceived[node][keystr] = time;
            }
        }
        
        std::string feetx((const char *)packet->data()+offset);
        offset += feetx.size() + 1;
        
        CAmount fee = to_amount(psettings.getFee());
        
        this->processPayment(node, feetx, fee);
        
        std::vector<std::string> params;
        int count = psettings.getMaxParamCount();
        std::string p;
        for (int i = 0; i < count; i++) {
            p = (const char *)packet->data()+offset;
            params.push_back(p);
            offset += p.size() + 1;
        }
        
        reply = processCustomCall(currency, params);
    } else {
        std::string feetx((const char *)packet->data()+offset);
        offset += feetx.size() + 1;
        
        CAmount fee = to_amount(app.xrouter_settings.getCommandFee(packet->command(), currency));
        
        LOG() << "Fee = " << fee;
        LOG() << "Feetx = " << feetx;
        try {
            this->processPayment(node, feetx, fee);
        
            std::string keystr = currency + "::" + XRouterCommand_ToString(packet->command());
            double timeout = app.xrouter_settings.getCommandTimeout(packet->command(), currency);
            if (lastPacketsReceived.count(node)) {
                if (lastPacketsReceived[node].count(keystr)) {
                    std::chrono::time_point<std::chrono::system_clock> prev_time = lastPacketsReceived[node][keystr];
                    std::chrono::system_clock::duration diff = time - prev_time;
                    if (std::chrono::duration_cast<std::chrono::milliseconds>(diff) < std::chrono::milliseconds((int)(timeout * 1000))) {
                        std::string err_msg = "XRouter: too many requests of type " + keystr; 
                        state.DoS(100, error(err_msg.c_str()), REJECT_INVALID, "xrouter-error");
                    }
                    if (!lastPacketsReceived.count(node))
                        lastPacketsReceived[node] = boost::container::map<std::string, std::chrono::time_point<std::chrono::system_clock> >();
                    lastPacketsReceived[node][keystr] = time;
                } else {
                    lastPacketsReceived[node][keystr] = time;
                }
            } else {
                lastPacketsReceived[node] = boost::container::map<std::string, std::chrono::time_point<std::chrono::system_clock> >();
                lastPacketsReceived[node][keystr] = time;
            }
            
            switch (packet->command()) {
            case xrGetBlockCount:
                reply = processGetBlockCount(packet, offset, currency);
                break;
            case xrGetBlockHash:
                reply = processGetBlockHash(packet, offset, currency);
                break;
            case xrGetBlock:
                reply = processGetBlock(packet, offset, currency);
                break;
            case xrGetTransaction:
                reply = processGetTransaction(packet, offset, currency);
                break;
            case xrGetAllBlocks:
                reply = processGetAllBlocks(packet, offset, currency);
                break;
            case xrGetAllTransactions:
                reply = processGetAllTransactions(packet, offset, currency);
                break;
            case xrGetBalance:
                reply = processGetBalance(packet, offset, currency);
                break;
            case xrGetBalanceUpdate:
                reply = processGetBalanceUpdate(packet, offset, currency);
                break;
            case xrGetTransactionsBloomFilter:
                reply = processGetTransactionsBloomFilter(packet, offset, currency);
                break;
            case xrSendTransaction:
                reply = processSendTransaction(packet, offset, currency);
                break;
            default:
                LOG() << "Unknown packet";
                return;
            }
        }
        catch (std::runtime_error & e)
        {
            reply = e.what();
        }
        
    }

    XRouterPacketPtr rpacket(new XRouterPacket(xrReply));

    rpacket->append(uuid);
    rpacket->append(reply);
    sendPacketToClient(rpacket, node);
    return;
}

//*****************************************************************************
//*****************************************************************************
std::string XRouterServer::processGetBlockCount(XRouterPacketPtr packet, uint32_t offset, std::string currency) {
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

    return json_spirit::write_string(Value(result), true);
}

std::string XRouterServer::processGetBlockHash(XRouterPacketPtr packet, uint32_t offset, std::string currency) {
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

    return json_spirit::write_string(Value(result), true);
}

std::string XRouterServer::processGetBlock(XRouterPacketPtr packet, uint32_t offset, std::string currency) {
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
    
    return json_spirit::write_string(Value(result), true);
}

std::string XRouterServer::processGetTransaction(XRouterPacketPtr packet, uint32_t offset, std::string currency) {
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

    return json_spirit::write_string(Value(result), true);
}

std::string XRouterServer::processGetAllBlocks(XRouterPacketPtr packet, uint32_t offset, std::string currency) {
    std::string number_s((const char *)packet->data()+offset);
    offset += number_s.size() + 1;
    int number = std::stoi(number_s);

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);
    Array result;
    if (conn)
    {
        result = conn->getAllBlocks(number);
    }

    return json_spirit::write_string(Value(result), true);
}

std::string XRouterServer::processGetAllTransactions(XRouterPacketPtr packet, uint32_t offset, std::string currency) {
    std::string account((const char *)packet->data()+offset);
    offset += account.size() + 1;
    std::string number_s((const char *)packet->data()+offset);
    offset += number_s.size() + 1;
    int number = std::stoi(number_s);

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);
    int time = 0;
    if (account.find(":") != string::npos) {
        time = std::stoi(account.substr(account.find(":")+1));
        account = account.substr(0, account.find(":"));
    }
    Array result;
    if (conn)
    {
        result = conn->getAllTransactions(account, number, time);
    }

    return json_spirit::write_string(Value(result), true);
}

//*****************************************************************************
//*****************************************************************************
std::string XRouterServer::processGetBalance(XRouterPacketPtr packet, uint32_t offset, std::string currency) {
    std::string account((const char *)packet->data()+offset);
    offset += account.size() + 1;

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);
    int time = 0;
    if (account.find(":") != string::npos) {
        time = std::stoi(account.substr(account.find(":")+1));
        account = account.substr(0, account.find(":"));
    }
    std::string result;
    if (conn)
    {
        result = conn->getBalance(account, time);
    }

    return result;
}

std::string XRouterServer::processGetBalanceUpdate(XRouterPacketPtr packet, uint32_t offset, std::string currency) {
    std::string account((const char *)packet->data()+offset);
    offset += account.size() + 1;
    std::string number_s((const char *)packet->data()+offset);
    offset += number_s.size() + 1;
    int number = std::stoi(number_s);

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);
    int time = 0;
    if (account.find(":") != string::npos) {
        time = std::stoi(account.substr(account.find(":")+1));
        account = account.substr(0, account.find(":"));
    }

    std::string result;
    if (conn)
    {
        result = conn->getBalanceUpdate(account, number, time);
    }

    return result;
}

std::string XRouterServer::processGetTransactionsBloomFilter(XRouterPacketPtr packet, uint32_t offset, std::string currency) {
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

    return json_spirit::write_string(Value(result), true);
}

std::string XRouterServer::processSendTransaction(XRouterPacketPtr packet, uint32_t offset, std::string currency) {
    std::string transaction((const char *)packet->data()+offset);
    offset += transaction.size() + 1;

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);

    Object result;
    Object error;
    
    if (conn)
    {
        result = conn->sendTransaction(transaction);
    }
    else
    {
        error.emplace_back(Pair("error", "No connector for currency " + currency));
        error.emplace_back(Pair("errorcode", "-100"));
        result = error;
    }
    
    return json_spirit::write_string(Value(result), true);
}

std::string XRouterServer::processCustomCall(std::string name, std::vector<std::string> params)
{
    App& app = App::instance();    
    if (!app.xrouter_settings.hasPlugin(name))
        return "Custom call not found";
    
    XRouterPluginSettings psettings = app.xrouter_settings.getPluginSettings(name);
    std::string callType = psettings.getParam("type");
    LOG() << "Plugin call " << name << " type = " << callType; 
    if (callType == "rpc") {
        Array jsonparams;
        int count = psettings.getMaxParamCount();
        std::vector<std::string> paramtypes;
        std::string typestring = psettings.getParam("paramsType");
        boost::split(paramtypes, typestring, boost::is_any_of(","));
        std::string p;
        for (int i = 0; i < count; i++) {
            p = params[i];
            if (p == "")
                continue;
            if (paramtypes[i] == "string")
                jsonparams.push_back(p);
            else if (paramtypes[i] == "int") {
                try {
                    jsonparams.push_back(std::stoi(p));
                } catch (...) {
                    return "Parameter #" + std::to_string(i+1) + " can not be converted to integer";
                }
            } else if (paramtypes[i] == "bool") {
                if (params[i] == "true")
                    jsonparams.push_back(true);
                else if (params[i] == "false")
                    jsonparams.push_back(true);
                else
                    return "Parameter #" + std::to_string(i+1) + " can not be converted to bool";
            }
        }
        
        std::string user, passwd, ip, port, command;
        user = psettings.getParam("rpcUser");
        passwd = psettings.getParam("rpcPassword");
        ip = psettings.getParam("rpcIp", "127.0.0.1");
        port = psettings.getParam("rpcPort");
        command = psettings.getParam("rpcCommand");
        Object result;
        try {
            result = CallRPC(user, passwd, ip, port, command, jsonparams);
        } catch (...) {
            return "Internal error in the plugin";
        }
        return json_spirit::write_string(Value(result), true);
    } else if (callType == "shell") {
        return "Shell plugins are currently disabled";
        
        std::string cmd = psettings.getParam("cmd");
        int count = psettings.getMaxParamCount();
        std::string p;
        for (int i = 0; i < count; i++) {
            cmd += " " + params[i];
        }
        
        LOG() << "Executing shell command " << cmd;
        std::string result = CallCMD(cmd);
        return result;
    } else if (callType == "url") {
        return "Shell plugins are currently disabled";
        
        std::string ip, port;
        ip = psettings.getParam("ip");
        port = psettings.getParam("port");
        std::string cmd = psettings.getParam("url");
        int count = psettings.getMaxParamCount();
        for (int i = 0; i < count; i++) {
            cmd = cmd.replace(cmd.find("%s"), 2, params[i]);
        }
        
        LOG() << "Executing url command " << cmd;
        std::string result = CallURL(ip, port, cmd);
        return result;
    }
    
    return "Unknown type";
}

std::string XRouterServer::getMyPaymentAddress()
{
    try {
        CServicenode* pmn = mnodeman.Find(activeServicenode.vin);
        if (!pmn)
            return "yKQyDJ2CJLaQfZKdi8yM7nQHZZqGXYNhUt";
        std::string result = CBitcoinAddress(pmn->pubKeyCollateralAddress.GetID()).ToString();
        return result;
    } catch (...) {
        return "yKQyDJ2CJLaQfZKdi8yM7nQHZZqGXYNhUt";
    }
}

CKey XRouterServer::getMyPaymentAddressKey()
{
    CServicenode* pmn = mnodeman.Find(activeServicenode.vin);
    CKeyID keyid;
    if (!pmn)
        CBitcoinAddress(getMyPaymentAddress()).GetKeyID(keyid);
    else 
        keyid = pmn->pubKeyCollateralAddress.GetID();
    
    CKey result;
    pwalletMain->GetKey(keyid, result);
    return result;
}

Value XRouterServer::printPaymentChannels() {
    Array server;
    
    for (const auto& it : this->paymentChannels) {
        Object val;
        val.emplace_back("Node id", it.first->id);
        val.emplace_back("Deposit transaction", it.second.raw_tx);
        val.emplace_back("Deposit transaction id", it.second.txid);
        val.emplace_back("Latest redeem transaction", it.second.latest_tx);
        val.emplace_back("Received amount", it.second.value);
        server.push_back(Value(val));
    }
    
    return json_spirit::write_string(Value(server), true);
}

} // namespace xrouter