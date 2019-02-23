//******************************************************************************
//******************************************************************************

#include "xrouterserver.h"
#include "xrouterlogger.h"
#include "xrouterapp.h"
#include "xroutererror.h"
#include "xrouterutils.h"
#include "xbridge/util/settings.h"

#include "servicenodeman.h"
#include "activeservicenode.h"
#include "rpcserver.h"
#include "init.h"

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
                LOG() << "Skipping currency " << wp.method << " because of missing credentials, COIN or BlockTime parameters";
                continue;
            }

            LOG() << "Adding connector to " << wp.currency;

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
    LOCK(_lock);
    connectors[conn->currency] = conn;
    connectorLocks[conn->currency] = boost::shared_ptr<boost::mutex>(new boost::mutex());
}

WalletConnectorXRouterPtr XRouterServer::connectorByCurrency(const std::string & currency) const
{
    LOCK(_lock);

    if (connectors.count(currency))
        return connectors.at(currency);

    return WalletConnectorXRouterPtr();
}

void XRouterServer::sendPacketToClient(std::string uuid, std::string reply, CNode* pnode)
{
    LOG() << "Sending reply to client with query id " << uuid << ": " << reply;
    XRouterPacketPtr rpacket(new XRouterPacket(xrReply));
    rpacket->append(uuid);
    rpacket->append(reply);
    pnode->PushMessage("xrouter", rpacket->body());
}

bool XRouterServer::processPayment(CNode* node, std::string feetx, CAmount fee)
{
    const auto nodeAddr = node->addr.ToString();
    if (feetx.empty() && fee > 0) {
        ERR() << "Client sent a bad feetx: " << nodeAddr;
        return false; // do not process bad fees
    }
    else if (feetx.empty())
        return true; // do not process empty fees if we're not expecting any payment

    std::vector<std::string> parts;
    boost::split(parts, feetx, boost::is_any_of(";"));
    if (parts.size() < 3) {
        ERR() << "Incorrect payment data format from client: " << nodeAddr;
        return false;
    }

    bool usehash = parts[0] == "hash";
    CAmount fee_part1 = fee;
    if (usehash)
        fee_part1 = fee / 2;

    if (parts[1] == "single") {
        // Direct payment, no CLTV channel
        const auto & addr = getMyPaymentAddress();
        if (addr.empty())
            throw XRouterError("Bad payment address", xrouter::BAD_ADDRESS);

        CAmount paid = to_amount(getTxValue(parts[2], addr));
        if (paid < fee_part1) {
            ERR() << "Client failed to send enough fees: " << nodeAddr;
            return false;
        }

        std::string txid;
        bool res = sendTransactionBlockchain(parts[2], txid);
        if (!res) {
            ERR() << "Failed to spend client fee: Could not send transaction " + parts[2] + " " << nodeAddr;
            return false;
        }

        LOG() << "Received direct payment: value = " << paid << " tx = " << parts[2] + " " << nodeAddr;
        return true;
    }

    throw XRouterError("Unsupported payment format: " + parts[0], xrouter::INVALID_PARAMETERS);
}

//*****************************************************************************
//*****************************************************************************
void XRouterServer::onMessageReceived(CNode* node, XRouterPacketPtr& packet, CValidationState& state)
{
    clearHashedQueries(); // clean up

    std::string uuid, reply;
    const auto nodeAddr = node->addr.ToString();

    try {
        if (packet->version() != static_cast<boost::uint32_t>(XROUTER_PROTOCOL_VERSION))
            throw XRouterError("You are using a different version of XRouter protocol. This node runs version " + std::to_string(XROUTER_PROTOCOL_VERSION), xrouter::BAD_VERSION);

        if (!packet->verify()) {
            state.DoS(10, error("XRouter: unsigned packet or signature error"), REJECT_INVALID, "xrouter-error");
            throw XRouterError("Unsigned packet or signature error", xrouter::BAD_REQUEST);
        }

        App & app = App::instance();
        uint32_t offset = 0;

        // id
        uuid = std::string((const char *)packet->data()+offset);
        offset += uuid.size() + 1;

        // wallet currency
        std::string currency((const char *)packet->data()+offset);
        offset += currency.size() + 1;

        const auto command = packet->command();
        std::string commandStr(XRouterCommand_ToString(command));
        const auto commandKey = app.buildCommandKey(currency, commandStr);

        LOG() << "XRouter command: " << commandStr;
        if (!app.xrSettings()->isAvailableCommand(packet->command(), currency))
            throw XRouterError("Unsupported xrouter command " + commandKey, xrouter::UNSUPPORTED_BLOCKCHAIN);

        bool usehash = false;
        CAmount fee = 0;

        // Handle calls to XRouter plugins
        if (packet->command() == xrCustomCall) {
            // Check rate limit
            XRouterPluginSettingsPtr psettings = app.xrSettings()->getPluginSettings(currency);
            auto rateLimit = psettings->clientRequestLimit();
            if (rateLimit >= 0 && rateLimitExceeded(nodeAddr, currency, rateLimit)) {
                std::string err_msg = "Rate limit exceeded: " + commandKey;
                state.DoS(20, error(err_msg.c_str()), REJECT_INVALID, "xrouter-error");
            }

            // Spend client payment
            std::string feetx((const char *)packet->data()+offset);
            offset += feetx.size() + 1;
            fee = to_amount(psettings->getFee());
            if (!this->processPayment(node, feetx, fee)) {
                std::string err_msg = "Bad fee payment from client: " + commandKey;
                state.DoS(20, error(err_msg.c_str()), REJECT_INVALID, "xrouter-error");
                throw XRouterError(err_msg, xrouter::INSUFFICIENT_FEE);
            }

            std::vector<std::string> params;
            int count = psettings->maxParamCount();
            std::string p;
            for (int i = 0; i < count; i++) {
                p = (const char *)packet->data()+offset;
                params.push_back(p);
                offset += p.size() + 1;
            }
            
            reply = processCustomCall(currency, params);
        }
        else { // Handle default XRouter calls
            std::string feetx((const char *)packet->data()+offset);
            offset += feetx.size() + 1;
            
            std::vector<std::string> parts;
            boost::split(parts, feetx, boost::is_any_of(";"));
            usehash = parts[0] == "hash"; // TODO Support hashes

            fee = to_amount(app.xrSettings()->getCommandFee(command, currency));
            
            LOG() << "Fee = " << fee;
            LOG() << "Feetx = " << feetx;

            try {
                CAmount fee_part1 = fee;
                if (command == xrFetchReply)
                    fee_part1 = getQueryFee(uuid);

                int rateLimit = app.xrSettings()->clientRequestLimit(command, currency);
                if (rateLimit >= 0 && rateLimitExceeded(nodeAddr, commandKey, rateLimit)) {
                    std::string err_msg = "Rate limit exceeded: " + commandKey;
                    state.DoS(20, error(err_msg.c_str()), REJECT_INVALID, "xrouter-error");
                }

                if (!app.xrSettings()->isAvailableCommand(command, currency))
                    throw XRouterError("Unsupported command: " + commandKey, xrouter::INVALID_PARAMETERS);

                // Spend client payment if supported command
                switch (command) {
                    case xrGetBalance:
                    case xrTimeToBlockNumber:
                        break; // commands not supported, do not charge client
                    default: {
                        if (!this->processPayment(node, feetx, fee_part1)) {
                            std::string err_msg = "Bad fee payment from client: " + commandKey;
                            state.DoS(20, error(err_msg.c_str()), REJECT_INVALID, "xrouter-error");
                            throw XRouterError(err_msg, xrouter::INSUFFICIENT_FEE);
                        }
                    }
                }

                switch (command) {
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
                    throw XRouterError("This call is not supported: " + commandKey, xrouter::INVALID_PARAMETERS);
                    //reply = processGetBalance(packet, offset, currency);
                    break;
                case xrGetBalanceUpdate:
                    reply = processGetBalanceUpdate(packet, offset, currency);
                    break;
                case xrGetTransactionsBloomFilter:
                    reply = processGetTransactionsBloomFilter(packet, offset, currency);
                    break;
                case xrTimeToBlockNumber:
                    throw XRouterError("This call is not supported: " + commandKey, xrouter::INVALID_PARAMETERS);
                    //reply = processConvertTimeToBlockCount(packet, offset, currency);
                    break;
                case xrFetchReply:
                    reply = processFetchReply(uuid);
                    break;
                case xrSendTransaction:
                    reply = processSendTransaction(packet, offset, currency);
                    break;
                default:
                    throw XRouterError("Unknown packet command", xrouter::INVALID_PARAMETERS);
                }
            }
            catch (XRouterError & e)
            {
                throw XRouterError("Failed to process your request: " + e.msg, e.code);
            }
        }

        // TODO Create func to indicate commands that don't support hashes (e.g. xrSendTransaction, xrFetchReply)
        if (usehash && packet->command() != xrSendTransaction && packet->command() != xrFetchReply) {
            hashedQueries[uuid] = std::pair<std::string, CAmount>(reply, fee - fee/2);
            std::string hash = Hash160(reply.begin(), reply.end()).ToString();
            hashedQueriesDeadlines[uuid] = std::chrono::system_clock::now();
            reply = hash;
        }

    } catch (XRouterError & e) {
        LOG() << e.msg;
        Object error;
        error.emplace_back("error", e.msg);
        error.emplace_back("code", e.code);
        reply = json_spirit::write_string(Value(error), true);
    }

    sendPacketToClient(uuid, reply, node);
}

//*****************************************************************************
//*****************************************************************************
std::string XRouterServer::processGetBlockCount(XRouterPacketPtr packet, uint32_t offset, std::string currency) {
    Object result;

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);
    if (conn)
    {
        boost::mutex::scoped_lock l(*connectorLocks[currency]);
        result.emplace_back("result", conn->getBlockCount());
    }
    else
    {
        throw XRouterError("No connector for currency " + currency, xrouter::BAD_CONNECTOR);
    }

    return json_spirit::write_string(Value(result), true);
}

std::string XRouterServer::processGetBlockHash(XRouterPacketPtr packet, uint32_t offset, std::string currency) {
    std::string blockId((const char *)packet->data()+offset);
    offset += blockId.size() + 1;

    Object result;

    if (!is_number(blockId))
        throw XRouterError("Incorrect block number: " + blockId, xrouter::INVALID_PARAMETERS);
    
    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);
    if (conn)
    {
        boost::mutex::scoped_lock l(*connectorLocks[currency]);
        result = conn->getBlockHash(blockId);
    }
    else
    {
        throw XRouterError("No connector for currency " + currency, xrouter::BAD_CONNECTOR);
    }

    return json_spirit::write_string(Value(result), true);
}

std::string XRouterServer::processGetBlock(XRouterPacketPtr packet, uint32_t offset, std::string currency) {
    std::string blockHash((const char *)packet->data()+offset);
    offset += blockHash.size() + 1;

    Object result;

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);
    if (conn)
    {
        boost::mutex::scoped_lock l(*connectorLocks[currency]);
        result = conn->getBlock(blockHash);
    }
    else
    {
        throw XRouterError("No connector for currency " + currency, xrouter::BAD_CONNECTOR);
    }
    
    return json_spirit::write_string(Value(result), true);
}

std::string XRouterServer::processGetTransaction(XRouterPacketPtr packet, uint32_t offset, std::string currency) {
    std::string hash((const char *)packet->data()+offset);
    offset += hash.size() + 1;

    Object result;

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);
    if (conn)
    {
        boost::mutex::scoped_lock l(*connectorLocks[currency]);
        result = conn->getTransaction(hash);
    }
    else
    {
        throw XRouterError("No connector for currency " + currency, xrouter::BAD_CONNECTOR);
    }

    return json_spirit::write_string(Value(result), true);
}

std::string XRouterServer::processGetAllBlocks(XRouterPacketPtr packet, uint32_t offset, std::string currency) {
    std::string number_s((const char *)packet->data()+offset);
    offset += number_s.size() + 1;
    
    if (!is_number(number_s))
        throw XRouterError("Incorrect block number: " + number_s, xrouter::INVALID_PARAMETERS);
    
    int number = std::stoi(number_s);

    App& app = App::instance();
    int blocklimit = app.xrSettings()->getCommandBlockLimit(packet->command(), currency);
    
    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);
    Array result;
    if (conn)
    {
        boost::mutex::scoped_lock l(*connectorLocks[currency]);
        result = conn->getAllBlocks(number, blocklimit);
    }
    else
    {
        throw XRouterError("No connector for currency " + currency, xrouter::BAD_CONNECTOR);
    }

    return json_spirit::write_string(Value(result), true);
}

std::string XRouterServer::processGetAllTransactions(XRouterPacketPtr packet, uint32_t offset, std::string currency) {
    std::string account((const char *)packet->data()+offset);
    offset += account.size() + 1;
    std::string number_s((const char *)packet->data()+offset);
    offset += number_s.size() + 1;
    if (!is_number(number_s))
        throw XRouterError("Incorrect block number: " + number_s, xrouter::INVALID_PARAMETERS);
    
    int number = std::stoi(number_s);

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);
    int time = 0;
    if (account.find(":") != string::npos) {
        time = std::stoi(account.substr(account.find(":")+1));
        account = account.substr(0, account.find(":"));
    }
    Array result;
    
    App& app = App::instance();
    int blocklimit = app.xrSettings()->getCommandBlockLimit(packet->command(), currency);
    
    if (conn)
    {
        boost::mutex::scoped_lock l(*connectorLocks[currency]);
        result = conn->getAllTransactions(account, number, time, blocklimit);
    }
    else
    {
        throw XRouterError("No connector for currency " + currency, xrouter::BAD_CONNECTOR);
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
    App& app = App::instance();
    int blocklimit = app.xrSettings()->getCommandBlockLimit(packet->command(), currency);
    
    if (conn)
    {
        boost::mutex::scoped_lock l(*connectorLocks[currency]);
        result = conn->getBalance(account, time, blocklimit);
    }
    else
    {
        throw XRouterError("No connector for currency " + currency, xrouter::BAD_CONNECTOR);
    }

    return result;
}

std::string XRouterServer::processGetBalanceUpdate(XRouterPacketPtr packet, uint32_t offset, std::string currency) {
    std::string account((const char *)packet->data()+offset);
    offset += account.size() + 1;
    std::string number_s((const char *)packet->data()+offset);
    offset += number_s.size() + 1;
    if (!is_number(number_s))
        throw XRouterError("Incorrect block number: " + number_s, xrouter::INVALID_PARAMETERS);
    
    int number = std::stoi(number_s);

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);
    int time = 0;
    if (account.find(":") != string::npos) {
        time = std::stoi(account.substr(account.find(":")+1));
        account = account.substr(0, account.find(":"));
    }
    
    App& app = App::instance();
    int blocklimit = app.xrSettings()->getCommandBlockLimit(packet->command(), currency);
    
    std::string result;
    if (conn)
    {
        boost::mutex::scoped_lock l(*connectorLocks[currency]);
        result = conn->getBalanceUpdate(account, number, time, blocklimit);
    }
    else
    {
        throw XRouterError("No connector for currency " + currency, xrouter::BAD_CONNECTOR);
    }

    return result;
}

std::string XRouterServer::processGetTransactionsBloomFilter(XRouterPacketPtr packet, uint32_t offset, std::string currency) {
    std::string filter((const char *)packet->data()+offset);

    // 10 is a constant for bloom filters currently
    if (!is_hash(filter) || (filter.size() % 10 != 0))
        throw XRouterError("Incorrect bloom filter: " + filter, xrouter::INVALID_PARAMETERS);
    
    CBloomFilter f(filter.size(), 0.1, 5, 0);
    f.from_hex(filter);
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << f;
    offset += filter.size() + 1;

    std::string number_s((const char *)packet->data()+offset);
    offset += number_s.size() + 1;
    if (!is_number(number_s))
        throw XRouterError("Incorrect block number: " + number_s, xrouter::INVALID_PARAMETERS);
    int number = std::stoi(number_s);

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);
    App& app = App::instance();
    int blocklimit = app.xrSettings()->getCommandBlockLimit(packet->command(), currency);
    
    Array result;
    if (conn)
    {
        boost::mutex::scoped_lock l(*connectorLocks[currency]);
        result = conn->getTransactionsBloomFilter(number, stream, blocklimit);
    }
    else
    {
        throw XRouterError("No connector for currency " + currency, xrouter::BAD_CONNECTOR);
    }

    return json_spirit::write_string(Value(result), true);
}

std::string XRouterServer::processConvertTimeToBlockCount(XRouterPacketPtr packet, uint32_t offset, std::string currency) {
    std::string timestamp((const char *)packet->data()+offset);
    offset += timestamp.size() + 1;

    Object result;
    Object error;

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);
    if (conn)
    {
        boost::mutex::scoped_lock l(*connectorLocks[currency]);
        result.emplace_back("result", conn->convertTimeToBlockCount(timestamp));
    }
    else
    {
        throw XRouterError("No connector for currency " + currency, xrouter::BAD_CONNECTOR);
    }

    return json_spirit::write_string(Value(result), true);
}

std::string XRouterServer::processFetchReply(const std::string & uuid) {
    if (hasQuery(uuid))
        return getQuery(uuid);
    else {
        Object error;
        error.emplace_back("error", "Unknown query id: " + uuid);
        error.emplace_back("code", xrouter::INVALID_PARAMETERS);
        return json_spirit::write_string(Value(error), true);
    }
}
    

std::string XRouterServer::processSendTransaction(XRouterPacketPtr packet, uint32_t offset, std::string currency) {
    std::string transaction((const char *)packet->data()+offset);
    offset += transaction.size() + 1;

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);

    Object result;
    Object error;
    
    if (conn && hasConnectorLock(currency)) {
        boost::mutex::scoped_lock l(*getConnectorLock(currency));
        result = conn->sendTransaction(transaction);
    } else {
        throw XRouterError("No connector for currency " + currency, xrouter::BAD_CONNECTOR);
    }
    
    return json_spirit::write_string(Value(result), true);
}

std::string XRouterServer::processCustomCall(std::string name, std::vector<std::string> params)
{
    App & app = App::instance();
    if (!app.xrSettings()->hasPlugin(name))
        return "Custom call not found";
    
    XRouterPluginSettingsPtr psettings = app.xrSettings()->getPluginSettings(name);
    std::string callType = psettings->getParam("type");
    LOG() << "Plugin call " << name << " type = " << callType; 
    if (callType == "rpc") {
        Array jsonparams;
        int count = psettings->maxParamCount();
        std::vector<std::string> paramtypes;
        std::string typestring = psettings->getParam("paramsType");
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
        user = psettings->getParam("rpcUser");
        passwd = psettings->getParam("rpcPassword");
        ip = psettings->getParam("rpcIp", "127.0.0.1");
        port = psettings->getParam("rpcPort");
        command = psettings->getParam("rpcCommand");
        Object result;
        try {
            result = CallRPC(user, passwd, ip, port, command, jsonparams);
        } catch (...) {
            return "Internal error in the plugin";
        }
        return json_spirit::write_string(Value(result), true);
    } else if (callType == "shell") {
        return "Shell plugins are currently disabled";
        
        std::string cmd = psettings->getParam("cmd");
        int count = psettings->maxParamCount();
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
        ip = psettings->getParam("ip");
        port = psettings->getParam("port");
        std::string cmd = psettings->getParam("url");
        int count = psettings->maxParamCount();
        for (int i = 0; i < count; i++) {
            cmd = cmd.replace(cmd.find("%s"), 2, params[i]);
        }
        
        LOG() << "Executing url command " << cmd;
        std::string result = CallURL(ip, port, cmd);
        return result;
    }
    
    return "Unknown type";
}

std::string XRouterServer::changeAddress() {
    return App::instance().changeAddress();
}

std::string XRouterServer::getMyPaymentAddress()
{
    std::string addr;
    try {
        CServicenode* pmn = mnodeman.Find(activeServicenode.vin);
        if (pmn)
            addr = CBitcoinAddress(pmn->pubKeyCollateralAddress.GetID()).ToString();
        else
            addr = changeAddress();
    } catch (...) { }
    return addr;
}

CKey XRouterServer::getMyPaymentAddressKey()
{
    App& app = App::instance();
    std::string addr = app.xrSettings()->get<std::string>("depositaddress", "");
    if (addr.empty())
        addr = getMyPaymentAddress();
    if (addr.empty())
        throw XRouterError("Unable to get deposit address", xrouter::BAD_ADDRESS);

    CKeyID keyid;
    CBitcoinAddress(addr).GetKeyID(keyid);

    CKey result;
    pwalletMain->GetKey(keyid, result);
    return result;
}

void XRouterServer::clearHashedQueries() {
    LOCK(_lock);

    std::vector<std::string> to_remove;
    for (auto & it : hashedQueriesDeadlines) {
        std::chrono::time_point<std::chrono::system_clock> time = std::chrono::system_clock::now();
        std::chrono::system_clock::duration diff = time - it.second;
        // TODO: move 1000 seconds to settings?
        if (std::chrono::duration_cast<std::chrono::milliseconds>(diff) < std::chrono::milliseconds(1000 * 1000)) {
            to_remove.push_back(it.first);
        }
    }
    
    for (const auto & i : to_remove) {
        hashedQueries.erase(i);
        hashedQueriesDeadlines.erase(i);
    }
}

void XRouterServer::runPerformanceTests() {
    LOCK(_lock);
    std::chrono::time_point<std::chrono::system_clock> time;
    std::chrono::system_clock::duration diff;
    for (const auto& it : this->connectors) {
        std::string currency = it.first;
        WalletConnectorXRouterPtr conn = it.second;
        boost::mutex::scoped_lock l(*connectorLocks[currency]);
        TESTLOG() << "Testing connector to currency " << currency;
        TESTLOG() << "xrGetBlockCount";
        time = std::chrono::system_clock::now();
        int blocks = std::stoi(conn->getBlockCount());
        diff = std::chrono::system_clock::now() - time;
        TESTLOG() << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << " ms";
        
        TESTLOG() << "xrGetBlockHash";
        time = std::chrono::system_clock::now();
        Object obj = conn->getBlockHash(std::to_string(blocks-1));
        const Value & result = find_value(obj, "result");
        const std::string & blockhash = result.get_str();
        diff = std::chrono::system_clock::now() - time;
        TESTLOG() << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << " ms";
        
        TESTLOG() << "xrGetBlock";
        time = std::chrono::system_clock::now();
        conn->getBlock(blockhash);
        diff = std::chrono::system_clock::now() - time;
        TESTLOG() << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << " ms";
        
        TESTLOG() << "xrGetBlocks - 10 blocks";
        time = std::chrono::system_clock::now();
        conn->getAllBlocks(blocks-10, 0);
        diff = std::chrono::system_clock::now() - time;
        TESTLOG() << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << " ms";
        
        TESTLOG() << "xrGetBlocks - 100 blocks";
        time = std::chrono::system_clock::now();
        conn->getAllBlocks(blocks-100, 0);
        diff = std::chrono::system_clock::now() - time;
        TESTLOG() << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << " ms";
        
        TESTLOG() << "xrGetBlocks - 1000 blocks";
        time = std::chrono::system_clock::now();
        conn->getAllBlocks(blocks-1000, 0);
        diff = std::chrono::system_clock::now() - time;
        TESTLOG() << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << " ms";
        
        TESTLOG() << "xrGetTransactions - 10 blocks";
        time = std::chrono::system_clock::now();
        conn->getAllTransactions("yKQyDJ2CJLaQfZKdi8yM7nQHZZqGXYNhUt", blocks-10, 0, 0);
        diff = std::chrono::system_clock::now() - time;
        TESTLOG() << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << " ms";
        
        TESTLOG() << "xrGetTransactions - 100 blocks";
        time = std::chrono::system_clock::now();
        conn->getAllTransactions("yKQyDJ2CJLaQfZKdi8yM7nQHZZqGXYNhUt", blocks-100, 0, 0);
        diff = std::chrono::system_clock::now() - time;
        TESTLOG() << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << " ms";
        
        TESTLOG() << "xrGetTransactions - 1000 blocks";
        time = std::chrono::system_clock::now();
        conn->getAllTransactions("yKQyDJ2CJLaQfZKdi8yM7nQHZZqGXYNhUt", blocks-1000, 0, 0);
        diff = std::chrono::system_clock::now() - time;
        TESTLOG() << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << " ms";
        
        TESTLOG() << "xrGetBalanceUpdate - 10 blocks";
        time = std::chrono::system_clock::now();
        conn->getBalanceUpdate("yKQyDJ2CJLaQfZKdi8yM7nQHZZqGXYNhUt", blocks-10, 0, 0);
        diff = std::chrono::system_clock::now() - time;
        TESTLOG() << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << " ms";
        
        TESTLOG() << "xrGetBalanceUpdate - 100 blocks";
        time = std::chrono::system_clock::now();
        conn->getBalanceUpdate("yKQyDJ2CJLaQfZKdi8yM7nQHZZqGXYNhUt", blocks-100, 0, 0);
        diff = std::chrono::system_clock::now() - time;
        TESTLOG() << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << " ms";
        
        TESTLOG() << "xrGetBalanceUpdate - 1000 blocks";
        time = std::chrono::system_clock::now();
        conn->getBalanceUpdate("yKQyDJ2CJLaQfZKdi8yM7nQHZZqGXYNhUt", blocks-1000, 0, 0);
        diff = std::chrono::system_clock::now() - time;
        TESTLOG() << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << " ms";
        
        TESTLOG() << "xrTimeToBlockNumber";
        time = std::chrono::system_clock::now();
        conn->convertTimeToBlockCount("1241469643");
        diff = std::chrono::system_clock::now() - time;
        TESTLOG() << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << " ms";
    }
}

bool XRouterServer::rateLimitExceeded(const std::string & nodeAddr, const std::string & key, const int & rateLimit) {
    LOCK(_lock);

    std::chrono::time_point<std::chrono::system_clock> time = std::chrono::system_clock::now();
    // Check if existing packets on node
    if (lastPacketsReceived.count(nodeAddr)) {
        // Check if existing packets on currency
        if (lastPacketsReceived[nodeAddr].count(key)) {
            std::chrono::time_point<std::chrono::system_clock> prev_time = lastPacketsReceived[nodeAddr][key];
            std::chrono::system_clock::duration diff = time - prev_time;
            // Check if rate limit exceeded
            if (std::chrono::duration_cast<std::chrono::milliseconds>(diff) < std::chrono::milliseconds(rateLimit))
                return true;
            lastPacketsReceived[nodeAddr][key] = time;
        } else {
            lastPacketsReceived[nodeAddr][key] = time;
        }
    } else {
        lastPacketsReceived[nodeAddr] = std::map<std::string, std::chrono::time_point<std::chrono::system_clock> >();
        lastPacketsReceived[nodeAddr][key] = time;
    }

    return false;
}

} // namespace xrouter
