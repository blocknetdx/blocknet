// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <xrouter/xrouterserver.h>

#include <servicenode/servicenodemgr.h>
#include <xbridge/util/settings.h>
#include <xrouter/xrouterapp.h>
#include <xrouter/xroutererror.h>
#include <xrouter/xrouterlogger.h>
#include <xrouter/xrouterutils.h>

#include <algorithm>
#include <iostream>
#include <chrono>
#include <future>

#include <json/json_spirit_reader_template.h>
#include <json/json_spirit_writer_template.h>
#include <json/json_spirit_utils.h>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>

namespace xrouter
{  

//*****************************************************************************
//*****************************************************************************
bool XRouterServer::start()
{
    if (!initKeyPair())
        return false;

    createConnectors();

    LOCK(_lock);
    started = true;

    return true;
}

bool XRouterServer::stop()
{
    LOCK(_lock);
    connectors.clear();
    connectorLocks.clear();
    return true;
}

bool XRouterServer::createConnectors() {
    try {
        Settings & s = settings();
        std::vector<std::string> wallets = App::instance().xrSettings()->getWallets();
        for (std::vector<std::string>::iterator i = wallets.begin(); i != wallets.end(); ++i)
        {
            WalletParam wp;
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
            wp.jsonver                     = s.get<std::string>(*i + ".JSONVersion", "");
            wp.contenttype                 = s.get<std::string>(*i + ".ContentType", "");

            if (wp.m_user.empty() || wp.m_passwd.empty())
                LOG() << "Warning currency " << wp.method << " has empty credentials";

            if (wp.m_ip.empty() || wp.m_port.empty() || wp.COIN == 0 || wp.blockTime == 0) {
                LOG() << "Skipping currency " << wp.method << " because of bad Ip, Port, COIN or BlockTime parameters";
                continue;
            }

            LOG() << "Adding connector to " << wp.currency;

            xrouter::WalletConnectorXRouterPtr conn;
            if (wp.method == "ETH" || wp.method == "ETHER" || wp.method == "ETHEREUM") {
                conn.reset(new EthWalletConnectorXRouter);
                *conn = wp;
            } else {
                conn.reset(new BtcWalletConnectorXRouter);
                *conn = wp;
            }

            if (!conn)
                continue;

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
    LOCK(_lock);
    connectors[conn->currency] = conn;
    connectorLocks[conn->currency] = std::make_shared<boost::mutex>();
}

WalletConnectorXRouterPtr XRouterServer::connectorByCurrency(const std::string & currency) const
{
    LOCK(_lock);

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
    xrouter::PushXRouterMessage(pnode, rpacket.body());
}

bool XRouterServer::processPayment(const std::string & feetx)
{
    std::string txid;
    bool res = sendTransactionBlockchain(feetx, txid);
    if (!res) {
        ERR() << "Failed to spend client fee:\n" + feetx;
        return false;
    }
    return true;
}

bool XRouterServer::checkFeePayment(const NodeAddr & nodeAddr, const std::string & paymentAddress,
        const std::string & feetx, const CAmount & requiredFee)
{
    if (feetx.empty()) {
        ERR() << "Client sent a bad feetx: " << nodeAddr;
        return false; // do not process bad fees
    }

    if (paymentAddress.empty()) // check payment address
        return false;

    checkPayment(feetx, paymentAddress, requiredFee);
    return true;
}

//*****************************************************************************
//*****************************************************************************
void XRouterServer::onMessageReceived(CNode* node, XRouterPacketPtr packet, CValidationState& state)
{
    clearHashedQueries(); // clean up

    // Make sure this node is designated as an xrouter node
    node->fXRouter = true;

    const auto & nodeAddr = node->GetAddrName();
    const auto & uuid = packet->suuid();
    std::string reply;

    try {
        if (packet->version() != static_cast<boost::uint32_t>(XROUTER_PROTOCOL_VERSION))
            throw XRouterError("You are using a different version of XRouter protocol. This node runs version " +
                               std::to_string(XROUTER_PROTOCOL_VERSION), xrouter::BAD_VERSION);

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
            app.updateSentRequest(nodeAddr, commandStr);

            // Prep reply (serialize config)
            reply = app.parseConfig(cfg);
            LOG() << "Sending config to client " << nodeAddr << " for query " << uuid;

            XRouterPacket rpacket(xrConfigReply, uuid);
            rpacket.append(reply);
            rpacket.sign(spubkey, sprivkey);
            xrouter::PushXRouterMessage(node, rpacket.body());
            return;
        }

        uint32_t offset = 0;

        // wallet/service name
        const auto service = packet->service();
        offset += service.size() + 1;

        const auto & fqService = (command == xrService) ? pluginCommandKey(service)
                                                        : walletCommandKey(service, commandStr);

        if (!app.xrSettings()->isAvailableCommand(command, service))
            throw XRouterError("Unsupported xrouter command: " + fqService, xrouter::UNSUPPORTED_SERVICE);

        // Store fee tx
        const std::string feetx((const char *)packet->data()+offset);
        offset += feetx.size() + 1;

        // Params count
        const auto paramsCount = *static_cast<uint32_t *>(static_cast<void *>(packet->data()+offset));
        offset += sizeof(uint32_t);
        const auto & fetchLimit = app.xrSettings()->commandFetchLimit(command, service);
        if (paramsCount > fetchLimit)
            throw XRouterError("Too many parameters from client, max is " +
                               std::to_string(fetchLimit) + ": " + fqService, xrouter::BAD_REQUEST);

        auto handlePayment = [this](const bool & expectingPayment, const std::string & feeTransaction,
                const std::string & fqService, CValidationState & state, const NodeAddr & nodeAddr)
        {
            if (!expectingPayment)
                return;
            try {
                if (!processPayment(feeTransaction)) {
                    const std::string err_msg = strprintf("Bad fee payment from client %s service %s", nodeAddr, fqService);
                    state.DoS(50, error(err_msg.c_str()), REJECT_INVALID, "xrouter-error");
                    throw XRouterError(err_msg, xrouter::INSUFFICIENT_FEE);
                }
                LOG() << "Received payment for service " << fqService << " from node " << nodeAddr << "\n" << feeTransaction;
            } catch (XRouterError & e) {
                state.DoS(1, error("XRouter: bad request"), REJECT_INVALID, "xrouter-error"); // prevent abuse
                throw e;
            } catch (std::exception & e) {
                state.DoS(1, error("XRouter: server error"), REJECT_INVALID, "xrouter-error"); // prevent abuse
                throw XRouterError(e.what(), xrouter::INTERNAL_SERVER_ERROR);
            }
        };

        // Handle calls to XRouter plugins
        if (command == xrService) {
            if (!app.xrSettings()->hasPlugin(service))
                throw XRouterError("Service not supported: " + fqService, xrouter::BAD_REQUEST);

            // Check rate limit
            XRouterPluginSettingsPtr psettings = app.xrSettings()->getPluginSettings(service);
            auto rateLimit = app.xrSettings()->clientRequestLimit(command, service);
            if (rateLimit >= 0 && rateLimitExceeded(nodeAddr, fqService, rateLimit)) {
                std::string err_msg = "Rate limit exceeded: " + fqService;
                state.DoS(20, error(err_msg.c_str()), REJECT_INVALID, "xrouter-error");
            }
            app.updateSentRequest(nodeAddr, fqService); // Record request time

            if (!app.xrSettings()->isAvailableCommand(command, service))
                throw XRouterError("Unsupported command: " + fqService, xrouter::UNSUPPORTED_SERVICE);

            // Get parameters from packet
            std::vector<std::string> params;
            if (!processParameters(packet, paramsCount, params, offset)) {
                state.DoS(1, error("XRouter: too many parameters in query"), REJECT_INVALID, "xrouter-error"); // prevent abuse
                throw XRouterError("XRouter: too many parameters in call " + fqService + " query " + uuid +
                                   " from node " + nodeAddr, xrouter::BAD_REQUEST);
            }

            // Check payment
            const auto dfee = app.xrSettings()->commandFee(command, service);
            const auto fee = to_amount(dfee);
            bool expectingPayment = fee > 0;
            if (expectingPayment) {
                if (!checkFeePayment(nodeAddr, app.xrSettings()->paymentAddress(command, service), feetx, fee)) {
                    const std::string err_msg = strprintf("Bad fee payment from client %s service %s", nodeAddr, fqService);
                    state.DoS(25, error(err_msg.c_str()), REJECT_INVALID, "xrouter-error");
                    throw XRouterError(err_msg, xrouter::INSUFFICIENT_FEE);
                }
                LOG() << "XRouter command: " << fqService << " expecting fee " << dfee << " for query " << uuid;
            }

            try {
                reply = processServiceCall(service, params);
            } catch (XRouterError & e) {
                state.DoS(1, error("XRouter: bad request"), REJECT_INVALID, "xrouter-error"); // prevent abuse
                throw e;
            } catch (std::exception & e) {
                ERR() << "Error: " << fqService << " : " << e.what();
                state.DoS(1, error("XRouter: server error"), REJECT_INVALID, "xrouter-error"); // prevent abuse
                throw XRouterError("Unknown server error in " + fqService, xrouter::INTERNAL_SERVER_ERROR);
            }

            // Spend client payment
            handlePayment(expectingPayment, feetx, fqService, state, nodeAddr);

        } else { // Handle default XRouter calls
            const auto dfee = app.xrSettings()->commandFee(command, service);
            const auto fee = to_amount(dfee); // convert to satoshi

            // Rate limit check
            int rateLimit = app.xrSettings()->clientRequestLimit(command, service);
            if (rateLimit >= 0 && rateLimitExceeded(nodeAddr, fqService, rateLimit)) {
                std::string err_msg = "Rate limit exceeded: " + fqService;
                state.DoS(20, error(err_msg.c_str()), REJECT_INVALID, "xrouter-error");
            }
            app.updateSentRequest(nodeAddr, fqService); // Record request time

            if (!app.xrSettings()->isAvailableCommand(command, service))
                throw XRouterError("Unsupported command: " + fqService, xrouter::UNSUPPORTED_SERVICE);

            std::vector<std::string> params;
            if (!processParameters(packet, paramsCount, params, offset)) {
                state.DoS(1, error("XRouter: too many parameters in query"), REJECT_INVALID, "xrouter-error"); // prevent abuse
                throw XRouterError("XRouter: too many parameters in call " + fqService + " query " + uuid +
                                   " from node " + nodeAddr, xrouter::BAD_REQUEST);
            }

            // Check payment
            bool expectingPayment = fee > 0;
            if (expectingPayment) {
                if (!checkFeePayment(nodeAddr, app.xrSettings()->paymentAddress(command, service), feetx, fee)) {
                    const std::string err_msg = strprintf("Bad fee payment from client %s service %s", nodeAddr, fqService);
                    state.DoS(25, error(err_msg.c_str()), REJECT_INVALID, "xrouter-error");
                    throw XRouterError(err_msg, xrouter::INSUFFICIENT_FEE);
                }
                LOG() << "XRouter command: " << fqService << " expecting fee " << dfee << " for query " << uuid;
            }

            try {
                switch (command) {
                    case xrGetBlockCount:
                        reply = parseResult(processGetBlockCount(service, params));
                        break;
                    case xrGetBlockHash:
                        reply = parseResult(processGetBlockHash(service, params));
                        break;
                    case xrGetBlock:
                        reply = parseResult(processGetBlock(service, params));
                        break;
                    case xrGetTransaction:
                        reply = parseResult(processGetTransaction(service, params));
                        break;
                    case xrGetBlocks:
                        reply = parseResult(processGetBlocks(service, params));
                        break;
                    case xrGetTransactions:
                        reply = parseResult(processGetTransactions(service, params));
                        break;
                    case xrDecodeRawTransaction:
                        reply = parseResult(processDecodeRawTransaction(service, params));
                        break;
                    case xrGetBalance:
                        throw XRouterError("This call is not supported: " + fqService, xrouter::UNSUPPORTED_SERVICE);
//                        reply = parseResult(processGetBalance(service, params));
                        break;
                    case xrGetTxBloomFilter:
                        throw XRouterError("This call is not supported: " + fqService, xrouter::UNSUPPORTED_SERVICE);
//                        reply = parseResult(processGetTxBloomFilter(service, params));
                        break;
                    case xrGenerateBloomFilter:
                        throw XRouterError("This call is not supported: " + fqService, xrouter::UNSUPPORTED_SERVICE);
//                        reply = parseResult(processGenerateBloomFilter(service, params));
                        break;
                    case xrGetBlockAtTime:
                        throw XRouterError("This call is not supported: " + fqService, xrouter::UNSUPPORTED_SERVICE);
//                        reply = parseResult(processConvertTimeToBlockCount(service, params));
                        break;
                    case xrGetReply:
                        reply = parseResult(processFetchReply(uuid));
                        break;
                    case xrSendTransaction:
                        reply = parseResult(processSendTransaction(service, params));
                        break;
                    default:
                        throw XRouterError("Unknown command " + fqService, xrouter::UNSUPPORTED_SERVICE);
                }
            } catch (XRouterError & e) {
                state.DoS(1, error("XRouter: bad request"), REJECT_INVALID, "xrouter-error"); // prevent abuse
                ERR() << "Failed to process " << fqService << "from node " << nodeAddr << " msg: " << e.msg << " code: " << e.code;
                throw e;
            } catch (std::exception & e) {
                state.DoS(1, error("XRouter: server error"), REJECT_INVALID, "xrouter-error"); // prevent abuse
                ERR() << "Failed to process " << fqService << " from node " << nodeAddr << " " << e.what();
                throw XRouterError("Internal Server Error: Bad connector for " + fqService, xrouter::BAD_CONNECTOR);
            }

            // Spend client payment
            handlePayment(expectingPayment, feetx, fqService, state, nodeAddr);
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
std::string XRouterServer::processGetBlockCount(const std::string & currency, const std::vector<std::string> & params) {
    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);
    if (conn && hasConnectorLock(currency)) {
        boost::mutex::scoped_lock l(*getConnectorLock(currency));
        return conn->getBlockCount();
    }

    throw XRouterError("Internal Server Error: No connector for " + currency, xrouter::BAD_CONNECTOR);
}

std::string XRouterServer::processGetBlockHash(const std::string & currency, const std::vector<std::string> & params) {
    const auto & blockId = params[0];

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);
    if (conn && hasConnectorLock(currency)) {
        boost::mutex::scoped_lock l(*getConnectorLock(currency));
        uint32_t block_n{0};
        if (boost::algorithm::starts_with(blockId, "0x")) { // handle hex values (specifically for eth)
            try {
                block_n = boost::lexical_cast<uint32_t>(blockId);
            } catch(...) {
                throw XRouterError("Failed to parse hex into block number", xrouter::INVALID_PARAMETERS);
            }
        } else { // handle integer values
            try {
                block_n = boost::lexical_cast<uint32_t>(blockId);
            } catch (...) {
                throw XRouterError("Problem with the specified block number, is it a number?", xrouter::INVALID_PARAMETERS);
            }
        }
        return conn->getBlockHash(block_n);
    }

    throw XRouterError("Internal Server Error: No connector for " + currency, xrouter::BAD_CONNECTOR);
}

std::string XRouterServer::processGetBlock(const std::string & currency, const std::vector<std::string> & params) {
    const auto & blockHash = params[0];

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);
    if (conn && hasConnectorLock(currency)) {
        boost::mutex::scoped_lock l(*getConnectorLock(currency));
        return conn->getBlock(blockHash);
    }

    throw XRouterError("Internal Server Error: No connector for " + currency, xrouter::BAD_CONNECTOR);
}

std::vector<std::string> XRouterServer::processGetBlocks(const std::string & currency, const std::vector<std::string> & params) {
    if (params.empty())
        throw XRouterError("Missing block hashes for " + currency, xrouter::BAD_REQUEST);

    App & app = App::instance();
    const auto & fetchlimit = app.xrSettings()->commandFetchLimit(xrGetBlocks, currency);
    if (params.size() > fetchlimit)
        throw XRouterError("Too many blocks requested for " + currency + " limit is " +
                           std::to_string(fetchlimit) + " received " + std::to_string(params.size()), xrouter::BAD_REQUEST);

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);
    if (conn && hasConnectorLock(currency)) {
        boost::mutex::scoped_lock l(*getConnectorLock(currency));
        return conn->getBlocks(params);
    }

    throw XRouterError("Internal Server Error: No connector for " + currency, xrouter::BAD_CONNECTOR);
}


std::string XRouterServer::processGetTransaction(const std::string & currency, const std::vector<std::string> & params) {
    const auto & hash = params[0];

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);
    if (conn && hasConnectorLock(currency)) {
        boost::mutex::scoped_lock l(*getConnectorLock(currency));
        return conn->getTransaction(hash);
    }

    throw XRouterError("Internal Server Error: No connector for " + currency, xrouter::BAD_CONNECTOR);
}

std::vector<std::string> XRouterServer::processGetTransactions(const std::string & currency, const std::vector<std::string> & params) {
    if (params.empty())
        throw XRouterError("Missing transaction hashes for " + currency, xrouter::BAD_REQUEST);

    App & app = App::instance();
    const auto & fetchlimit = app.xrSettings()->commandFetchLimit(xrGetTransactions, currency);
    if (params.size() > fetchlimit)
        throw XRouterError("Too many transactions requested for " + currency + " limit is " +
                           std::to_string(fetchlimit) + " received " + std::to_string(params.size()), xrouter::BAD_REQUEST);
    
    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);
    if (conn && hasConnectorLock(currency)) {
        boost::mutex::scoped_lock l(*getConnectorLock(currency));
        return conn->getTransactions(params);
    }

    throw XRouterError("Internal Server Error: No connector for " + currency, xrouter::BAD_CONNECTOR);
}

std::string XRouterServer::processDecodeRawTransaction(const std::string & currency, const std::vector<std::string> & params) {
    const auto & hex = params[0];

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);
    if (conn && hasConnectorLock(currency)) {
        boost::mutex::scoped_lock l(*getConnectorLock(currency));
        return conn->decodeRawTransaction(hex);
    }

    throw XRouterError("Internal Server Error: No connector for " + currency, xrouter::BAD_CONNECTOR);
}

std::string XRouterServer::processSendTransaction(const std::string & currency, const std::vector<std::string> & params) {
    const auto & transaction = params[0];

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);
    if (conn && hasConnectorLock(currency)) {
        boost::mutex::scoped_lock l(*getConnectorLock(currency));
        return conn->sendTransaction(transaction);
    }

    throw XRouterError("Internal Server Error: No connector for " + currency, xrouter::BAD_CONNECTOR);
}

//*****************************************************************************
//*****************************************************************************

std::vector<std::string> XRouterServer::processGetTxBloomFilter(const std::string & currency, const std::vector<std::string> & params) {
    const auto & filter = params[0];

    // 10 is a constant for bloom filters currently
    if (!is_hash(filter) || (filter.size() % 10 != 0))
        throw XRouterError("Incorrect bloom filter: " + filter, xrouter::INVALID_PARAMETERS);
    
    CBloomFilter f(filter.size(), 0.1, 5, 0);
//    f.from_hex(filter); // TODO Blocknet XRouter fixme
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << f;

    std::string number_s(params[1]);
    if (!is_number(number_s))
        throw XRouterError("Incorrect block number: " + number_s, xrouter::INVALID_PARAMETERS);
    int number = boost::lexical_cast<int>(number_s);

    App & app = App::instance();
    int fetchlimit = app.xrSettings()->commandFetchLimit(xrGetTxBloomFilter, currency);

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);
    if (conn && hasConnectorLock(currency)) {
        boost::mutex::scoped_lock l(*getConnectorLock(currency));
        return conn->getTransactionsBloomFilter(number, stream, fetchlimit);
    }

    throw XRouterError("Internal Server Error: No connector for " + currency, xrouter::BAD_CONNECTOR);
}

std::string XRouterServer::processGenerateBloomFilter(const std::string & currency, const std::vector<std::string> & params) {
    CBloomFilter f(10 * static_cast<unsigned int>(params.size()), 0.1, 5, 0);

    Object result;
    // TODO Blocknet XRouter fixme
//    Array invalid;
//
//    std::vector<unsigned char> data;
//    for (const auto & paddress : params) {
//        xrouter::UnknownChainAddress address(paddress);
//        if (!address.IsValid()) {
//            // This is a hash
//            data = ParseHex(paddress);
//            CPubKey pubkey(data);
//            if (!pubkey.IsValid()) {
//                invalid.push_back(Value(paddress));
//                continue;
//            }
//            f.insert(data);
//        } else {
//            // This is a bitcoin address
//            CKeyID keyid;
//            address.GetKeyID(keyid);
//            data = std::vector<unsigned char>(keyid.begin(), keyid.end());
//            f.insert(data);
//        }
//    }
//
//    if (!invalid.empty()) {
//        result.emplace_back("skipped-invalid", invalid);
//    }
//
//    if (invalid.size() == params.size()) {
//        result.emplace_back("error", "No valid addresses");
//        result.emplace_back("code", xrouter::INVALID_PARAMETERS);
//    }

    return json_spirit::write_string(Value(result), true);
}

std::string XRouterServer::processConvertTimeToBlockCount(const std::string & currency, const std::vector<std::string> & params) {
    const std::string timestamp(params[0]);

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);
    if (conn && hasConnectorLock(currency)) {
        boost::mutex::scoped_lock l(*getConnectorLock(currency));
        return conn->convertTimeToBlockCount(timestamp);
    }

    throw XRouterError("Internal Server Error: No connector for " + currency, xrouter::BAD_CONNECTOR);
}

std::string XRouterServer::processGetBalance(const std::string & currency, const std::vector<std::string> & params) {
    throw XRouterError("Internal Server Error: Not implemented for " + currency, xrouter::BAD_CONNECTOR);
}

std::string XRouterServer::processServiceCall(const std::string & name, const std::vector<std::string> & params)
{
    App & app = App::instance();
    if (!app.xrSettings()->hasPlugin(name))
        throw XRouterError("Service not found", UNSUPPORTED_SERVICE);
    
    XRouterPluginSettingsPtr psettings = app.xrSettings()->getPluginSettings(name);
    const std::string & callType = psettings->type();
    LOG() << "Calling plugin " << name << " with type " << callType;

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
        const auto & user     = psettings->stringParam("rpcuser");
        const auto & passwd   = psettings->stringParam("rpcpassword");
        const auto & ip       = psettings->stringParam("rpcip", "127.0.0.1");
        const auto & port     = psettings->stringParam("rpcport");
        const auto & command  = psettings->stringParam("rpccommand");
        const auto & jsonver  = psettings->stringParam("rpcjsonversion");
        const auto & contype  = psettings->stringParam("rpccontenttype");
        result = CallRPC(user, passwd, ip, port, command, jsonparams, jsonver, contype);

        if (psettings->hasCustomResponse())
            return psettings->customResponse();
        else
            return result;

    } else if (callType == "docker") {
        auto replace = [](std::string & s, size_t pos, const size_t & spos, const std::string & from, const std::string & to) -> bool {
            pos = s.find(from, spos);
            if (pos == std::string::npos)
                return false;
            s.replace(pos, from.length(), to);
            return true;
        };

        const auto & container = psettings->container();
        const auto & exe = psettings->command();

        if (container.empty()) {
            ERR() << "Failed to run plugin " + name + " \"containername\" cannot be empty";
            throw XRouterError("Internal Server Error in command " + name, INTERNAL_SERVER_ERROR);
        }
        if (exe.empty()) {
            ERR() << "Failed to run plugin " + name + " \"command\" cannot be empty";
            throw XRouterError("Internal Server Error in command " + name, INTERNAL_SERVER_ERROR);
        }
        if (psettings->commandArgs().empty() && expectedParams.size() > 0) {
            ERR() << "Failed to run plugin " + name + " \"args\" cannot be empty when parameters= is set";
            throw XRouterError("Internal Server Error in command " + name, INTERNAL_SERVER_ERROR);
        }

        std::string cmdargs = psettings->commandArgs();
        size_t spos = 0;

        for (int i = 0; i < static_cast<int>(expectedParams.size()); ++i) {
            const auto & p = expectedParams[i];
            const auto & rec = params[i];
            const auto & arg = "$" + std::to_string(i + 1);
            std::string argv;

            if (psettings->quoteArgs())
                argv = std::string("\""+rec+"\"");
            else
                argv = rec;

            size_t pos;
            replace(cmdargs, pos, spos, arg, argv);

            // prevent backtracking
            if (pos + argv.size() > spos)
                spos = pos + argv.size();
        }
        // Insert docker command info
        const auto & cmd = strprintf("docker exec %s %s %s", container, exe, cmdargs);

        // parses the container result into Value
        auto parseR = [](const std::string & res) -> Value {
            Value cmd_val;
            try {
                json_spirit::read_string(res, cmd_val);
            } catch (...) { // ignore errors on json parse
                throw XRouterError("Failed to read the plugin response data", INTERNAL_SERVER_ERROR);
            }
            if (cmd_val.type() != null_type)
                return cmd_val;
            else
                return Value(res); // raw string
        };

        LOG() << "Executing docker plugin " << name << " with command: " << cmd;
        Value val;
        int nexit;
        const auto & r = CallCMD(cmd, nexit);
        if (nexit != 0) {
            ERR() << "docker command reported non-zero exit status (" << std::to_string(nexit) << ") on command: "
                  << cmd << "\n" << r;
            if (nexit == 1 || nexit == 2 || (nexit >= 126 && nexit <= 165) || nexit == 255)
                throw std::runtime_error("Failed to execute command " + name);
            Value r_val = parseR(r);
            Object o; o.emplace_back("error", r_val);
            val = Value(o);
        } else {
            val = parseR(r);
        }

        if (psettings->hasCustomResponse())
            return psettings->customResponse();
        else
            return json_spirit::write_string(val, false);

    } else if (callType == "url") {
        throw XRouterError("url calls are unsupported at this time", UNSUPPORTED_SERVICE);
//
//        const std::string & ip = psettings->stringParam("ip");
//        const std::string & port = psettings->stringParam("port");
//        std::string cmd = psettings->stringParam("url");
//
//        // Replace params in command
//        for (const auto & p : params) {
//            cmd = cmd.replace(cmd.find("%s"), 2, p);
//        }
//
//        std::string result;
//        LOG() << "Executing url command " << cmd;
//        result = CallURL(ip, port, cmd);
//        return result;
    }
    
    return "";
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

bool XRouterServer::processParameters(XRouterPacketPtr packet, const int & paramsCount,
                       std::vector<std::string> & params, uint32_t & offset)
{
    for (int i = 0; i < static_cast<int>(paramsCount); ++i) {
        if (offset >= packet->allSize())
            return false;
        std::string p = (const char *)packet->data()+offset;
        params.push_back(p);
        offset += p.size() + 1;
    }
    return true;
}

std::string XRouterServer::changeAddress() {
    return App::instance().changeAddress();
}

std::string XRouterServer::getMyPaymentAddress()
{
    std::string addr;
    try {
        if (sn::ServiceNodeMgr::instance().hasActiveSn()) {
            const auto & entry = sn::ServiceNodeMgr::instance().getActiveSn();
            const auto & snode = sn::ServiceNodeMgr::instance().getSn(entry.key.GetPubKey());
            addr = EncodeDestination(snode.getPaymentAddress());
        }
        else
            addr = changeAddress();
    } catch (...) { }
    return addr;
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
//    LOCK(_lock);
//    std::chrono::time_point<std::chrono::system_clock> time;
//    std::chrono::system_clock::duration diff;
//    for (const auto& it : this->connectors) {
//        std::string currency = it.first;
//        WalletConnectorXRouterPtr conn = it.second;
//        boost::mutex::scoped_lock l(*connectorLocks[currency]);
//        TESTLOG() << "Testing connector to currency " << currency;
//        TESTLOG() << "xrGetBlockCount";
//        time = std::chrono::system_clock::now();
//        int blocks = std::stoi(conn->getBlockCount());
//        diff = std::chrono::system_clock::now() - time;
//        TESTLOG() << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << " ms";
//
//        TESTLOG() << "xrGetBlockHash";
//        time = std::chrono::system_clock::now();
//        Object obj = conn->getBlockHash(blocks-1);
//        const Value & result = find_value(obj, "result");
//        const std::string & blockhash = result.get_str();
//        diff = std::chrono::system_clock::now() - time;
//        TESTLOG() << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << " ms";
//
//        TESTLOG() << "xrGetBlock";
//        time = std::chrono::system_clock::now();
//        conn->getBlock(blockhash);
//        diff = std::chrono::system_clock::now() - time;
//        TESTLOG() << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << " ms";
//
//        TESTLOG() << "xrGetBlocks - 10 blocks";
//        time = std::chrono::system_clock::now();
//        conn->getBlocks({ "302a309d6b6c4a65e4b9ff06c7ea81bb17e985d00abdb01978ace62cc5e18421",
//                          "175d2a428b5649c2a4732113e7f348ba22a0e69cc0a87631449d1d77cd6e1b04",
//                          "34989eca8ed66ff53631294519e147a12f4860123b4bdba36feac6da8db492ab",
//                          "504540bb0c63470e2007a1c0145d9c92513a1a309b6452445f21eb1f7223dc85",
//                          "309eff15ee88810ddb779bad676ba1ac13e0b92ef3d335e63f83633cc3fa3c78",
//                          "8bfe0b683a7cc5301f40252090f59c98850ee9d2337c446e7d64e7c141cbfdfa",
//                          "fe5fe66e026624c2cc39024212c0e27ecbb50f83b96da007c7e5f28af0d340ec",
//                          "4f4b6ab9094ff37edfd708834c70c9aa8e4c5bb72ba85ebe0d287018cbcbcbef",
//                          "22e379280b11bdd006229a0192116a16a7ce22628435fe035788e414ae0ce5fa",
//                          "f1450f0dac81bb08470217abe27a8303c34e31626b0a09afa851ffea5e35c788",
//                          "695038b66bc7b1bcd2c4e60ef1017209a832e94fe78bbcd81a5c8fc166817457" });
//        diff = std::chrono::system_clock::now() - time;
//        TESTLOG() << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << " ms";
//
//        TESTLOG() << "xrGetBlocks - 100 blocks";
//        time = std::chrono::system_clock::now();
//        conn->getBlocks({ });
//        diff = std::chrono::system_clock::now() - time;
//        TESTLOG() << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << " ms";
//
//        TESTLOG() << "xrGetBlocks - 1000 blocks";
//        time = std::chrono::system_clock::now();
//        conn->getBlocks({ });
//        diff = std::chrono::system_clock::now() - time;
//        TESTLOG() << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << " ms";
//
//        TESTLOG() << "xrGetTransactions - 10 blocks";
//        time = std::chrono::system_clock::now();
//        conn->getTransactions({ "24ff5506a30772acfb65012f1b3309d62786bc386be3b6ea853a798a71c010c8",
//                                "24b6bcb44f045d7a4cf8cd47c94a14cc609352851ea973f8a47b20578391629f",
//                                "66a5809c7090456965fe30280b88f69943e620894e1c4538a724ed9a89c769be",
//                                "b981a5f85354342ad9d627b367f070a688d8cd054eb9df6e7549c9d388dd6070",
//                                "7821c916acc487e019e11fab947aa3ace75b035831790f7d06796a92b6cb5f18",
//                                "34e876cbe8d4e97290db089c680bd4b6c5d4f7f56f916d0c9bd4393739f273eb",
//                                "e5d585f6d0fa35accda2113a55f5a090c746592b4afc1fae770bc1bb3dd0b6e7",
//                                "28de1d6012490c8421480644dd644eb9c800bad33f76c477f792ef13d24baac1",
//                                "fddd4e01390653b5cfea74ef717515738b04b63a33f646ed25210c1cc9a68385",
//                                "b8d9cb15d6a1d8e0c79e3a7140f974185d08ce78d38bfdfe705ce80f48e57e7b" });
//        diff = std::chrono::system_clock::now() - time;
//        TESTLOG() << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << " ms";
//
//        TESTLOG() << "xrGetTransactions - 100 blocks";
//        time = std::chrono::system_clock::now();
//        conn->getTransactions({ "" });
//        diff = std::chrono::system_clock::now() - time;
//        TESTLOG() << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << " ms";
//
//        TESTLOG() << "xrGetTransactions - 1000 blocks";
//        time = std::chrono::system_clock::now();
//        conn->getTransactions({ "" });
//        diff = std::chrono::system_clock::now() - time;
//        TESTLOG() << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << " ms";
//
//        TESTLOG() << "xrGetBlockAtTime";
//        time = std::chrono::system_clock::now();
//        conn->convertTimeToBlockCount("1241469643");
//        diff = std::chrono::system_clock::now() - time;
//        TESTLOG() << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << " ms";
//    }
}

bool XRouterServer::rateLimitExceeded(const std::string & nodeAddr, const std::string & key, const int & rateLimit) {
    auto & app = App::instance();
    return app.rateLimitExceeded(nodeAddr, key, app.getLastRequest(nodeAddr, key), rateLimit);
}

bool XRouterServer::initKeyPair() {
    if (!sn::ServiceNodeMgr::instance().hasActiveSn())
        return error("XRouter server unable to init key pair, service node is not active");

    const auto & key = sn::ServiceNodeMgr::instance().getActiveSn().key;
    CPubKey pubkey = key.GetPubKey();
    if (!pubkey.IsCompressed())
        pubkey.Compress();

    spubkey = std::vector<unsigned char>(pubkey.begin(), pubkey.end());
    sprivkey = std::vector<unsigned char>(key.begin(), key.end());

    return true;
}

std::string XRouterServer::parseResult(const std::string & res) {
    UniValue uv;
    if (!uv.read(res))
        return res;
    if (uv.isObject()) {
        const auto & r_val = find_value(uv, "result");
        if (r_val.isNull())
            return res;
        return r_val.getValStr();
    }
    return res;
};

std::string XRouterServer::parseResult(const std::vector<std::string> & resv) {
    std::vector<std::string> parsed;
    for (const auto & r : resv)
        parsed.push_back(parseResult(r));
    return "[" + boost::algorithm::join(parsed, ",") + "]";
};

} // namespace xrouter
