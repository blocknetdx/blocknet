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
    if (!initKeyPair())
        return false;

    createConnectors();

    WaitableLock l(_lock);
    started = true;

    return true;
}

bool XRouterServer::stop()
{
    WaitableLock l(_lock);
    connectors.clear();
    connectorLocks.clear();
    return true;
}

bool XRouterServer::createConnectors() {
    try {
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
    catch (std::exception & e) {
        return false;
    }

    return true;
}

void XRouterServer::addConnector(const WalletConnectorXRouterPtr & conn)
{
    WaitableLock l(_lock);
    connectors[conn->currency] = conn;
    connectorLocks[conn->currency] = std::make_shared<boost::mutex>();
}

WalletConnectorXRouterPtr XRouterServer::connectorByCurrency(const std::string & currency) const
{
    WaitableLock l(_lock);

    if (connectors.count(currency))
        return connectors.at(currency);

    return WalletConnectorXRouterPtr();
}

void XRouterServer::sendPacketToClient(const std::string & uuid, const std::string & reply, CNode* pnode)
{
    LOG() << "Sending reply to client for query " << uuid;
    XRouterPacket rpacket(xrReply, uuid);
    rpacket.append(reply);
    rpacket.sign(spubkey, sprivkey);
    pnode->PushMessage("xrouter", rpacket.body());
}

bool XRouterServer::processPayment(CNode *node, const std::string & feetx, const CAmount requiredFee)
{
    const auto nodeAddr = node->NodeAddress();
    if (feetx.empty() && requiredFee > 0) {
        ERR() << "Client sent a bad feetx: " << nodeAddr;
        return false; // do not process bad fees
    }
    else if (feetx.empty())
        return true; // do not process empty fees if we're not expecting any payment

    // Direct payment, no CLTV channel
    const auto & addr = getMyPaymentAddress();
    if (addr.empty())
        throw XRouterError("Bad payment address", xrouter::BAD_ADDRESS);

    CAmount paid = to_amount(checkPayment(feetx, addr));
    if (paid < requiredFee) {
        auto requiredDbl = static_cast<double>(requiredFee) / static_cast<double>(COIN);
        auto paidDbl = static_cast<double>(paid) / static_cast<double>(COIN);
        ERR() << "Client failed to send enough fees, required " << std::to_string(requiredDbl)
              << " received: " << std::to_string(paidDbl) << " "
              << nodeAddr;
        return false;
    }

    std::string txid;
    bool res = sendTransactionBlockchain(feetx, txid);
    if (!res) {
        ERR() << "Failed to spend client fee: Could not send transaction " + feetx + " " << nodeAddr;
        return false;
    }

    LOG() << "Received direct payment: value = " << static_cast<double>(paid)/static_cast<double>(COIN) << " tx = "
          << feetx + " " << nodeAddr;
    return true;
}

//*****************************************************************************
//*****************************************************************************
void XRouterServer::onMessageReceived(CNode* node, XRouterPacketPtr packet, CValidationState& state)
{
    clearHashedQueries(); // clean up

    // Make sure this node is designated as an xrouter node
    node->setXRouter();

    const auto & nodeAddr = node->NodeAddress();
    const auto & uuid = packet->suuid();
    std::string reply;

    try {
        if (packet->version() != static_cast<boost::uint32_t>(XROUTER_PROTOCOL_VERSION))
            throw XRouterError("You are using a different version of XRouter protocol. This node runs version " + std::to_string(XROUTER_PROTOCOL_VERSION), xrouter::BAD_VERSION);

        if (!packet->verify(packet->vpubkey())) {
            state.DoS(20, error("XRouter: unsigned packet or signature error"), REJECT_INVALID, "xrouter-error");
            throw XRouterError("Unsigned packet or signature error", xrouter::BAD_REQUEST);
        }

        const auto command = packet->command();
        std::string commandStr = XRouterCommand_ToString(command);
        App & app = App::instance();

        // Handle config requests
        if (packet->command() == xrGetConfig) {
            if (packet->size() > 200) {
                state.DoS(20, error("XRouter: packet larger than expected"), REJECT_INVALID, "xrouter-error");
                throw XRouterError("Packet is too large, must be smaller than 200 bytes", xrouter::BAD_REQUEST);
            }

            XRouterSettingsPtr cfg = app.xrSettings();

            // Check request rate
            if (!app.needConfigUpdate(nodeAddr, true))
                state.DoS(10, error("XRouter: too many config requests"), REJECT_INVALID, "xrouter-error");
            auto time = std::chrono::system_clock::now();
            app.updateConfigTime(nodeAddr, time);

            // Prep reply (serialize config)
            reply = app.parseConfig(cfg);
            LOG() << "Sending config to client " << nodeAddr << " for query " << uuid;

            XRouterPacket rpacket(xrConfigReply, uuid);
            rpacket.append(reply);
            rpacket.sign(spubkey, sprivkey);
            node->PushMessage("xrouter", rpacket.body());
            return;
        }

        uint32_t offset = 0;

        // wallet/service name
        const auto service = packet->service();
        offset += service.size() + 1;

        const auto & fqService = (command == xrService) ? pluginCommandKey(service)
                                                        : walletCommandKey(service, commandStr);

        if (!app.xrSettings()->isAvailableCommand(command, service))
            throw XRouterError("Unsupported xrouter command: " + fqService, xrouter::UNSUPPORTED_BLOCKCHAIN);

        // Store fee tx
        CAmount fee = 0;
        std::string feetx((const char *)packet->data()+offset);
        offset += feetx.size() + 1;

        // Params count
        const auto paramsCount = *static_cast<uint32_t *>(static_cast<void *>(packet->data()+offset));
        offset += sizeof(uint32_t);
        if (paramsCount > XROUTER_MAX_PARAMETERS)
            throw XRouterError("Too many parameters from client, max is " +
                               std::to_string(XROUTER_MAX_PARAMETERS) + ": " + fqService, xrouter::BAD_REQUEST);

        // Handle calls to XRouter plugins
        if (command == xrService) {
            if (!app.xrSettings()->hasPlugin(service))
                throw XRouterError("Service not supported: " + fqService, xrouter::BAD_REQUEST);

            // Check rate limit
            XRouterPluginSettingsPtr psettings = app.xrSettings()->getPluginSettings(service);
            auto rateLimit = psettings->clientRequestLimit();
            if (rateLimit >= 0 && rateLimitExceeded(nodeAddr, service, rateLimit)) {
                std::string err_msg = "Rate limit exceeded: " + fqService;
                state.DoS(20, error(err_msg.c_str()), REJECT_INVALID, "xrouter-error");
            }

            // Get parameters from packet
            std::vector<std::string> params;
            for (int i = 0; i < static_cast<int>(paramsCount); ++i) {
                if (offset >= packet->allSize())
                    break;
                std::string p = (const char *)packet->data()+offset;
                params.push_back(p);
                offset += p.size() + 1;
            }

            try {
                reply = processServiceCall(service, params);
            } catch (XRouterError & e) {
                state.DoS(1, error("XRouter: bad request"), REJECT_INVALID, "xrouter-error"); // prevent abuse
                throw e;
            } catch (std::exception & e) {
                state.DoS(1, error("XRouter: server error"), REJECT_INVALID, "xrouter-error"); // prevent abuse
                throw XRouterError(e.what(), xrouter::INTERNAL_SERVER_ERROR);
            }

            // Spend client payment after processing reply to avoid charging client on errors
            fee = to_amount(psettings->fee());
            LOG() << "XRouter command: " << fqService << " expecting fee = " << fee << " for query " << uuid;
            if (!processPayment(node, feetx, fee)) {
                std::string err_msg = strprintf("Bad fee payment from client %s service %s", node, fqService);
                state.DoS(20, error(err_msg.c_str()), REJECT_INVALID, "xrouter-error");
                throw XRouterError(err_msg, xrouter::INSUFFICIENT_FEE);
            }

        } else { // Handle default XRouter calls
            const auto dfee = app.xrSettings()->commandFee(command, service);
            LOG() << "XRouter command: " << fqService << " expecting fee = " << dfee << " for query " << uuid;

            // convert to satoshi
            fee = to_amount(dfee);
            CAmount cmdFee = fee;
            if (command == xrGetReply)
                cmdFee = getQueryFee(uuid);

            int rateLimit = app.xrSettings()->clientRequestLimit(command, service);
            if (rateLimit >= 0 && rateLimitExceeded(nodeAddr, fqService, rateLimit)) {
                std::string err_msg = "Rate limit exceeded: " + fqService;
                state.DoS(20, error(err_msg.c_str()), REJECT_INVALID, "xrouter-error");
            }

            if (!app.xrSettings()->isAvailableCommand(command, service))
                throw XRouterError("Unsupported command: " + fqService, xrouter::INVALID_PARAMETERS);

            try {
                switch (command) {
                    case xrGetBlockCount:
                        reply = processGetBlockCount(packet, offset, service);
                        break;
                    case xrGetBlockHash:
                        reply = processGetBlockHash(packet, offset, service);
                        break;
                    case xrGetBlock:
                        reply = processGetBlock(packet, offset, service);
                        break;
                    case xrGetTransaction:
                        reply = processGetTransaction(packet, offset, service);
                        break;
                    case xrGetBlocks:
                        reply = processGetAllBlocks(packet, offset, service);
                        break;
                    case xrGetTransactions:
                        reply = processGetAllTransactions(packet, offset, service);
                        break;
                    case xrGetBalance:
                        throw XRouterError("This call is not supported: " + fqService, xrouter::INVALID_PARAMETERS);
//                    reply = processGetBalance(packet, offset, currency);
                        break;
                    case xrGetBalanceUpdate:
                        throw XRouterError("This call is not supported: " + fqService, xrouter::INVALID_PARAMETERS);
//                    reply = processGetBalanceUpdate(packet, offset, service);
                        break;
                    case xrGetTxBloomFilter:
                        reply = processGetTransactionsBloomFilter(packet, offset, service);
                        break;
                    case xrGenerateBloomFilter:
                        throw XRouterError("This call is not supported: " + fqService, xrouter::INVALID_PARAMETERS);
                        break;
                    case xrGetBlockAtTime:
                        throw XRouterError("This call is not supported: " + fqService, xrouter::INVALID_PARAMETERS);
//                    reply = processConvertTimeToBlockCount(packet, offset, currency);
                        break;
                    case xrGetReply:
                        reply = processFetchReply(uuid);
                        break;
                    case xrSendTransaction:
                        reply = processSendTransaction(packet, offset, service);
                        break;
                    default:
                        throw XRouterError("Unknown packet command", xrouter::INVALID_PARAMETERS);
                }

                // Spend client payment if supported command
                switch (command) {
                    case xrGetBalance:
                    case xrGetBalanceUpdate:
                    case xrGetBlockAtTime:
                    case xrGenerateBloomFilter:
                        break; // commands not supported, do not charge client
                    default: {
                        if (!processPayment(node, feetx, cmdFee)) {
                            std::string err_msg = "Bad fee payment from client: " + fqService;
                            state.DoS(20, error(err_msg.c_str()), REJECT_INVALID, "xrouter-error");
                            throw XRouterError(err_msg, xrouter::INSUFFICIENT_FEE);
                        }
                    }
                }

            } catch (XRouterError & e) {
                state.DoS(1, error("XRouter: bad request"), REJECT_INVALID, "xrouter-error"); // prevent abuse
                throw e;
            } catch (std::exception & e) {
                state.DoS(1, error("XRouter: server error"), REJECT_INVALID, "xrouter-error"); // prevent abuse
                throw XRouterError(e.what(), xrouter::INTERNAL_SERVER_ERROR);
            }
        }

    } catch (XRouterError & e) {
        LOG() << e.msg;
        Object error;
        error.emplace_back("error", e.msg);
        error.emplace_back("code", e.code);
        reply = json_spirit::write_string(Value(error), true);
    } catch (std::exception & e) {
        LOG() << "Exception: " << e.what();
        Object error;
        error.emplace_back("error", "Internal Server Error");
        error.emplace_back("code", xrouter::INTERNAL_SERVER_ERROR);
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
    int blocklimit = app.xrSettings()->commandBlockLimit(packet->command(), currency);
    
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
    int blocklimit = app.xrSettings()->commandBlockLimit(packet->command(), currency);
    
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
    int blocklimit = app.xrSettings()->commandBlockLimit(packet->command(), currency);
    
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
    int blocklimit = app.xrSettings()->commandBlockLimit(packet->command(), currency);
    
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
    int blocklimit = app.xrSettings()->commandBlockLimit(packet->command(), currency);
    
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

std::string XRouterServer::processServiceCall(const std::string & name, const std::vector<std::string> & params)
{
    App & app = App::instance();
    if (!app.xrSettings()->hasPlugin(name))
        throw XRouterError("Service not found", UNSUPPORTED_SERVICE);
    
    XRouterPluginSettingsPtr psettings = app.xrSettings()->getPluginSettings(name);
    const std::string & callType = psettings->type();
    LOG() << "Calling plugin " << name << " with type = " << callType;

    // Check if parameters matches expected
    const auto & expectedParams = psettings->parameters();
    if (expectedParams.size() != params.size())
        throw XRouterError(strprintf("Received parameters count %ld do not match expected %ld",
                params.size(), expectedParams.size()), INVALID_PARAMETERS);

    if (callType == "rpc") {
        Array jsonparams;
        for (int i = 0; i < static_cast<int>(expectedParams.size()); ++i) {
            const auto & p = expectedParams[i];
            const auto & rec = params[i];
            if (p == "bool") {
                jsonparams.push_back(!(rec == "false" || rec == "0"));
            } else if (p == "int") {
                try {
                    jsonparams.push_back(boost::lexical_cast<int64_t>(rec));
                } catch (...) {
                    throw XRouterError("Parameter " + std::to_string(i + 1) + " cannot be converted to integer", INVALID_PARAMETERS);
                }
            } else if (p == "double") {
                try {
                    jsonparams.push_back(boost::lexical_cast<double>(rec));
                } catch (...) {
                    throw XRouterError("Parameter " + std::to_string(i + 1) + " cannot be converted to double", INVALID_PARAMETERS);
                }
            } else { // string
                jsonparams.push_back(rec);
            }
        }

        std::string result;
        try {
            const auto & user     = psettings->stringParam("rpcuser");
            const auto & passwd   = psettings->stringParam("rpcpassword");
            const auto & ip       = psettings->stringParam("rpcip", "127.0.0.1");
            const auto & port     = psettings->stringParam("rpcport");
            const auto & command  = psettings->stringParam("rpccommand");
            Object o = CallRPC(user, passwd, ip, port, command, jsonparams);
            result = json_spirit::write_string(Value(o), true);
        } catch (...) {
            throw XRouterError("Internal Server Error in command " + name, INTERNAL_SERVER_ERROR);
        }
        return result;

    } else if (callType == "shell") {
        throw XRouterError("shell calls are unsupported at this time", UNSUPPORTED_SERVICE);

        std::string cmd = psettings->stringParam("cmd");
        for (int i = 0; i < static_cast<int>(expectedParams.size()); ++i) {
            const auto & p = expectedParams[i];
            const auto & rec = params[i];
            if (p == "bool") {
                cmd += " " + std::string(rec == "false" || rec == "0" ? "0" : "1");
            } else if (rec.find(' ') != std::string::npos) {
                cmd += " \"" + rec + "\"";
            }
        }

        std::string result;
        try {
            LOG() << "Executing shell command " << cmd;
            result = CallCMD(cmd);
        } catch (...) {
            throw XRouterError("Internal Server Error in command " + name, INTERNAL_SERVER_ERROR);
        }
        return result;

    } else if (callType == "url") {
        throw XRouterError("url calls are unsupported at this time", UNSUPPORTED_SERVICE);

        const std::string & ip = psettings->stringParam("ip");
        const std::string & port = psettings->stringParam("port");
        std::string cmd = psettings->stringParam("url");
        // Replace params in command
        for (const auto & p : params) {
            cmd = cmd.replace(cmd.find("%s"), 2, p);
        }

        std::string result;
        try {
            LOG() << "Executing url command " << cmd;
            result = CallURL(ip, port, cmd);
        } catch (...) {
            throw XRouterError("Internal Server Error in Url command " + name, INTERNAL_SERVER_ERROR);
        }
        return result;
    }
    
    return "";
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
    WaitableLock l(_lock);

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
    WaitableLock l(_lock);
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
        
        TESTLOG() << "xrGetBlockAtTime";
        time = std::chrono::system_clock::now();
        conn->convertTimeToBlockCount("1241469643");
        diff = std::chrono::system_clock::now() - time;
        TESTLOG() << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << " ms";
    }
}

bool XRouterServer::rateLimitExceeded(const std::string & nodeAddr, const std::string & key, const int & rateLimit) {
    WaitableLock l(_lock);

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

bool XRouterServer::initKeyPair() {
    std::string secret = GetArg("-servicenodeprivkey", "");
    if (secret.empty()) {
        ERR() << "Failed to initiate xrouter keypair, is servicenodeprivkey config entry set correctly?";
        return false;
    }

    CBitcoinSecret vchSecret;
    if (!vchSecret.SetString(secret)) {
        ERR() << "Failed to initiate xrouter keypair, there is a problem with servicenodeprivkey config entry";
        return false;
    }

    CKey key = vchSecret.GetKey();
    CPubKey pubkey = key.GetPubKey();
    if (!pubkey.IsCompressed())
        pubkey.Compress();

    spubkey = std::vector<unsigned char>(pubkey.begin(), pubkey.end());
    sprivkey = std::vector<unsigned char>(key.begin(), key.end());

    return true;
}

} // namespace xrouter
