//******************************************************************************
//******************************************************************************

#include "xrouterserver.h"
#include "xrouterlogger.h"
#include "xbridge/util/settings.h"
#include "xrouterapp.h"
#include "xroutererror.h"
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
                LOG() << "Skipping currency " << wp.method << " because of missing credentials, COIN or BlockTime parameters";
                continue;
            }

            LOG() << "Adding connector to currency " << wp.method;
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
    connectors[conn->currency] = conn;
    connectorLocks[conn->currency] = boost::shared_ptr<boost::mutex>(new boost::mutex());
}

WalletConnectorXRouterPtr XRouterServer::connectorByCurrency(const std::string & currency) const
{
    if (connectors.count(currency))
    {
        return connectors.at(currency);
    }

    return WalletConnectorXRouterPtr();
}

void XRouterServer::sendPacketToClient(std::string uuid, std::string reply, CNode* pnode)
{
    LOG() << "Sending reply to query " << uuid << ": " << reply;
    XRouterPacketPtr rpacket(new XRouterPacket(xrReply));
    rpacket->append(uuid);
    rpacket->append(reply);
    pnode->PushMessage("xrouter", rpacket->body());
}

void XRouterServer::processPayment(CNode* node, std::string feetx, CAmount fee)
{
    if (fee > 0) {
        std::vector<std::string> parts;
        boost::split(parts, feetx, boost::is_any_of(";"));
        if (parts.size() < 3)
            throw XRouterError("Incorrect payment data format", xrouter::INVALID_PARAMETERS);
        bool usehash;
        if (parts[0] == "nohash")
            usehash = false;
        else if (parts[0] == "hash")
            usehash = true;
        else
            throw XRouterError("Incorrect hash/no hash field", xrouter::INVALID_PARAMETERS);
        
        CAmount fee_part1 = fee;
        if (usehash)
            fee_part1 = fee / 2;
        
        if (parts[1] == "single") {
            // Direct payment, no CLTV channel
            std::string txid;
            CAmount paid = to_amount(getTxValue(parts[2], getMyPaymentAddress()));
            if (paid < fee_part1) {
                throw XRouterError("Fee paid is not enough", xrouter::INSUFFICIENT_FEE);
            }

            bool res = sendTransactionBlockchain(parts[2], txid);
            if (!res) {
                throw XRouterError("Could not send transaction " + parts[2] + " to blockchain", xrouter::INTERNAL_SERVER_ERROR);
            }
            
            LOG() << "Received direct payment; value = " << paid << " tx = " << parts[2];
        } else if (parts[1] == "channel") {
            if (!paymentChannels.count(node)) {
                // There is no payment channel with this node
                if (parts.size() != 6) {
                    throw XRouterError("Incorrect channel creation parameters or expired channel", xrouter::EXPIRED_PAYMENT_CHANNEL);
                }
                
                paymentChannels[node] = PaymentChannel();
                paymentChannels[node].value = CAmount(0);
                paymentChannels[node].raw_tx = parts[2];
                paymentChannels[node].txid = parts[3];
                std::vector<unsigned char> script = ParseHex(parts[4]);
                paymentChannels[node].redeemScript = CScript(script.begin(), script.end());
                feetx = parts[5];

                int date = getChannelExpiryTime(paymentChannels[node].raw_tx);
                
                int deadline = date - std::time(0) - 5;
                LOG() << "Created payment channel date = " << date << " expiry = " << deadline << " seconds"; 
                
                boost::shared_ptr<boost::mutex> m(new boost::mutex());
                boost::shared_ptr<boost::condition_variable> cond(new boost::condition_variable());
                paymentChannelLocks[node] = std::pair<boost::shared_ptr<boost::mutex>, boost::shared_ptr<boost::condition_variable> >(m, cond);
                
                boost::thread([deadline, this, node]() {
                    // No need to check the result of timed_wait(): if it's true then it means a function to close channel early was called, otherwise it means that the deadline is reached.
                    boost::mutex::scoped_lock lock(*this->paymentChannelLocks[node].first);
                    this->paymentChannelLocks[node].second->timed_wait(lock, boost::posix_time::seconds(deadline));
                    
                    std::string txid;
                    LOG() << "Closing payment channel: " << this->paymentChannels[node].txid << " Value = " << this->paymentChannels[node].value;
                    
                    try {
                        bool res = sendTransactionBlockchain(this->paymentChannels[node].latest_tx, txid);
                        if (!res)
                            throw "";
                    } catch (...) {
                        LOG() << "Failed to submit finalizing transaction";
                    }
                    this->paymentChannels.erase(node);
                }).detach();
            } else {
                feetx = parts[2];
            }
        
            if (paymentChannels.count(node)) {
                //verifyChannelTransaction(feetx);
                CAmount paid = to_amount(getTxValue(feetx, getMyPaymentAddress()));
                LOG() << "Received payment via channel; value = " << paid - paymentChannels[node].value << " total value = " << paid << " tx = " << feetx;
                if (paid - paymentChannels[node].value < fee_part1) {
                    throw XRouterError("Fee paid is not enough", xrouter::INSUFFICIENT_FEE);
                }
                    
                {
                    boost::mutex::scoped_lock lock(*this->paymentChannelLocks[node].first);
                    finalizeChannelTransaction(paymentChannels[node], this->getMyPaymentAddressKey(), feetx, paymentChannels[node].latest_tx);
                    paymentChannels[node].value = paid;
                }
            }
            
        } else {
            throw XRouterError("Unknown payment format: " + parts[0], xrouter::INVALID_PARAMETERS);
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
    clearHashedQueries();
    std::string uuid="", reply="";
    try {
        if (!packet->verify()) {
            state.DoS(10, error("XRouter: unsigned packet or signature error"), REJECT_INVALID, "xrouter-error");
            throw XRouterError("Unsigned packet or signature error", xrouter::BAD_REQUEST);
            
        }
        
        uint32_t offset = 36;
        uuid = std::string((const char *)packet->data()+offset);
        
        if (packet->version() != static_cast<boost::uint32_t>(XROUTER_PROTOCOL_VERSION))
        {
            throw XRouterError("You are using a different version of XRouter protocol. This node runs version " + std::to_string(XROUTER_PROTOCOL_VERSION), xrouter::BAD_VERSION);
        }

        if (!verifyBlockRequirement(packet)) {
            //state.DoS(10, error("XRouter: block requirement not satisfied"), REJECT_INVALID, "xrouter-error");
            throw XRouterError("Block requirement not satisfied", xrouter::INSUFFICIENT_FUNDS);
        }

        offset += uuid.size() + 1;
        std::string currency((const char *)packet->data()+offset);
        offset += currency.size() + 1;
        LOG() << "XRouter command: " << std::string(XRouterCommand_ToString(packet->command()));
        if (!app.xrouter_settings.isAvailableCommand(packet->command(), currency)) {
            throw XRouterError("This command is blocked in xrouter.conf", xrouter::UNSUPPORTED_BLOCKCHAIN);
        }

        bool usehash = false;
        CAmount fee = 0;
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
            
            std::vector<std::string> parts;
            boost::split(parts, feetx, boost::is_any_of(";"));
            if (parts[0] == "hash")
                usehash = true;        
            
            if (usehash)
                throw XRouterError("Hashing replies is not available in this version.", xrouter::BAD_REQUEST);
            
            fee = to_amount(app.xrouter_settings.getCommandFee(packet->command(), currency));
            
            LOG() << "Fee = " << fee;
            LOG() << "Feetx = " << feetx;
            try {
                CAmount fee_part1 = fee;
                if (packet->command() == xrFetchReply)
                    fee_part1 = hashedQueries[uuid].second;
                
                this->processPayment(node, feetx, fee_part1);
                std::string keystr = currency + "::" + XRouterCommand_ToString(packet->command());
                double timeout = app.xrouter_settings.getCommandTimeout(packet->command(), currency);
                if (lastPacketsReceived.count(node)) {
                    if (lastPacketsReceived[node].count(keystr)) {
                        std::chrono::time_point<std::chrono::system_clock> prev_time = lastPacketsReceived[node][keystr];
                        std::chrono::system_clock::duration diff = time - prev_time;
                        if (std::chrono::duration_cast<std::chrono::milliseconds>(diff) < std::chrono::milliseconds((int)(timeout * 1000))) {
                            std::string err_msg = "XRouter: too many requests of type " + keystr; 
                            state.DoS(100, error(err_msg.c_str()), REJECT_INVALID, "xrouter-error");
                            throw XRouterError(err_msg, xrouter::TOO_MANY_REQUESTS);
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
                    throw XRouterError("Obsolete command", xrouter::INVALID_PARAMETERS);
                    //reply = processGetBalance(packet, offset, currency);
                    break;
                case xrGetBalanceUpdate:
                    reply = processGetBalanceUpdate(packet, offset, currency);
                    break;
                case xrGetTransactionsBloomFilter:
                    reply = processGetTransactionsBloomFilter(packet, offset, currency);
                    break;
                case xrTimeToBlockNumber:
                    reply = "This call is not implemented yet";
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
            catch (std::runtime_error & e)
            {
                throw XRouterError("Error happened while processing your request: " + std::string(e.what()), xrouter::INTERNAL_SERVER_ERROR);
            }
        }
        
        if (!usehash || (packet->command() == xrSendTransaction) || (packet->command() == xrFetchReply)) {
            // Send 'reply' string
        } else {
            hashedQueries[uuid] = std::pair<std::string, CAmount>(reply, fee - fee/2);
            std::string hash = Hash160(reply.begin(), reply.end()).ToString();
            hashedQueriesDeadlines[uuid] = std::chrono::system_clock::now();
            reply = hash;
        }
    } catch (XRouterError e) {
        Object error;
        error.emplace_back(Pair("error", e.msg));
        error.emplace_back(Pair("code", e.code));
        error.emplace_back(Pair("uuid", uuid));
        LOG() << e.msg;
        reply = json_spirit::write_string(Value(error), true);
    }

    sendPacketToClient(uuid, reply, node);
}

//*****************************************************************************
//*****************************************************************************
std::string XRouterServer::processGetBlockCount(XRouterPacketPtr packet, uint32_t offset, std::string currency) {
    Object result;
    Object error;

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);
    if (conn)
    {
        boost::mutex::scoped_lock l(*connectorLocks[currency]);
        result.push_back(Pair("result", conn->getBlockCount()));
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
    Object error;

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
    Object error;

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
    Object error;

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
    int blocklimit = app.xrouter_settings.getCommandBlockLimit(packet->command(), currency);
    
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
    int blocklimit = app.xrouter_settings.getCommandBlockLimit(packet->command(), currency);
    
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
    int blocklimit = app.xrouter_settings.getCommandBlockLimit(packet->command(), currency);
    
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
    int blocklimit = app.xrouter_settings.getCommandBlockLimit(packet->command(), currency);
    
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
    int blocklimit = app.xrouter_settings.getCommandBlockLimit(packet->command(), currency);
    
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
        result.push_back(Pair("result", conn->convertTimeToBlockCount(timestamp)));
    }
    else
    {
        throw XRouterError("No connector for currency " + currency, xrouter::BAD_CONNECTOR);
    }

    return json_spirit::write_string(Value(result), true);
}

std::string XRouterServer::processFetchReply(std::string uuid) {
    if (hashedQueries.count(uuid))
        return hashedQueries[uuid].first;
    else {
        Object error;
        error.emplace_back(Pair("error", "Unknown query ID"));
        error.emplace_back(Pair("errorcode", xrouter::INVALID_PARAMETERS));
        return json_spirit::write_string(Value(error), true);
    }
}
    

std::string XRouterServer::processSendTransaction(XRouterPacketPtr packet, uint32_t offset, std::string currency) {
    std::string transaction((const char *)packet->data()+offset);
    offset += transaction.size() + 1;

    xrouter::WalletConnectorXRouterPtr conn = connectorByCurrency(currency);

    Object result;
    Object error;
    
    if (conn)
    {
        boost::mutex::scoped_lock l(*connectorLocks[currency]);
        result = conn->sendTransaction(transaction);
    }
    else
    {
        throw XRouterError("No connector for currency " + currency, xrouter::BAD_CONNECTOR);
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
            return "yBW61mwkjuqFK1rVfm2Az2s2WU5Vubrhhw";
        std::string result = CBitcoinAddress(pmn->pubKeyCollateralAddress.GetID()).ToString();
        return result;
    } catch (...) {
        return "yBW61mwkjuqFK1rVfm2Az2s2WU5Vubrhhw";
    }
}

CKey XRouterServer::getMyPaymentAddressKey()
{
    CKeyID keyid;
    App& app = App::instance();
    std::string addr = app.xrouter_settings.get<std::string>("depositaddress", "");
    if (addr == "") {
        CServicenode* pmn = mnodeman.Find(activeServicenode.vin);
        if (!pmn)
            CBitcoinAddress(getMyPaymentAddress()).GetKeyID(keyid);
        else 
            keyid = pmn->pubKeyCollateralAddress.GetID();
    } else {
        CBitcoinAddress(addr).GetKeyID(keyid);
    }
    
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

void XRouterServer::clearHashedQueries() {
    typedef boost::container::map<std::string, std::chrono::time_point<std::chrono::system_clock> > queries_map;
    std::vector<std::string> to_remove;
    BOOST_FOREACH( queries_map::value_type &it, hashedQueriesDeadlines ) {
        std::chrono::time_point<std::chrono::system_clock> time = std::chrono::system_clock::now();
        std::chrono::system_clock::duration diff = time - it.second;
        // TODO: move 1000 seconds to settings?
        if (std::chrono::duration_cast<std::chrono::milliseconds>(diff) < std::chrono::milliseconds((int)(1000 * 1000))) {
            to_remove.push_back(it.first);
        }
    }
    
    for (size_t i = 0; i < to_remove.size(); i++) {
        hashedQueries.erase(to_remove[i]);
        hashedQueriesDeadlines.erase(to_remove[i]);
    }
}

void XRouterServer::closePaymentChannel(std::string id) {
    CNode* node = NULL;
    for (const auto& it : this->paymentChannels) {
        if (std::to_string(it.first->id) == id) {
            node = it.first;
            break;
        }
    }
    
    if (node) {
        boost::mutex::scoped_lock lock(*this->paymentChannelLocks[node].first);
        this->paymentChannelLocks[node].second->notify_all();
    }
}

void XRouterServer::closeAllPaymentChannels() {
    for (const auto& it : this->paymentChannelLocks) {
        it.second.second->notify_all();
    }
}

void XRouterServer::runPerformanceTests() {
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
        std::string blockhash = result.get_str();
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

} // namespace xrouter
