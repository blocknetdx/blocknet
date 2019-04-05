//******************************************************************************
//******************************************************************************
#include "xrouterutils.h"
#include "xrouterlogger.h"
#include "xrouterdef.h"
#include "xroutererror.h"

#include "xbridge/xbridgewallet.h"
#include "xbridge/xbridgeapp.h"

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

#include "rpcserver.h"
#include "rpcprotocol.h"
#include "rpcclient.h"
#include "rpcserver.h"
#include "base58.h"
#include "wallet.h"
#include "init.h"
#include "key.h"
#include "core_io.h"

using namespace json_spirit;

//*****************************************************************************
//*****************************************************************************
namespace xrouter
{


CMutableTransaction decodeTransaction(std::string tx)
{
    vector<unsigned char> txData(ParseHex(tx));
    CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
    CMutableTransaction result;
    ssData >> result;
    return result;
}

// TODO: check that this variable is static across xbridge and xrouter
static CCriticalSection cs_rpcBlockchainStore;

bool createAndSignTransaction(const std::string & toaddress, const CAmount & toamount, string & raw_tx) {
    LOCK(cs_rpcBlockchainStore);

    raw_tx.clear(); // clean ret transaction tx
    // Exclude the used uxtos
    const auto excludedUtxos = xbridge::App::instance().getAllLockedUtxos("BLOCK");

    // Available utxos from from wallet
    std::vector<xbridge::wallet::UtxoEntry> inputs;
    std::vector<xbridge::wallet::UtxoEntry> outputsForUse;

    try {
        Array defaults;
        const auto & unspents = listunspent(defaults, false);
        if (unspents.type() != array_type)
            return false; // not enough inputs
        for (const auto & utxo_val : unspents.get_array()) {
            const auto & utxo = utxo_val.get_obj();
            const auto & txid_val = find_value(utxo, "txid");
            const auto & vout_val = find_value(utxo, "vout");
            const auto & amount_val = find_value(utxo, "amount");
            const auto & address_val = find_value(utxo, "address");
            const auto & scriptpk_val = find_value(utxo, "scriptPubKey");
            const auto & confirmations_val = find_value(utxo, "confirmations");
            xbridge::wallet::UtxoEntry entry;
            entry.txId = txid_val.get_str();
            entry.vout = vout_val.get_int();
            entry.amount = amount_val.get_real();
            entry.address = address_val.get_str();
            entry.scriptPubKey = scriptpk_val.get_str();
            entry.confirmations = confirmations_val.get_int();
            inputs.emplace_back(entry);
        }
    } catch (...) {
        ERR() << "Failed to created feetx, listunspent returned error";
        return false;
    }

    // Remove all the excluded utxos
    inputs.erase(
            std::remove_if(inputs.begin(), inputs.end(), [&excludedUtxos](xbridge::wallet::UtxoEntry & u) {
                if (excludedUtxos.count(u))
                    return true; // remove if in excluded list

                // Only accept p2pkh (like 76a91476bba472620ff0ecbfbf93d0d3909c6ca84ac81588ac)
                std::vector<unsigned char> script = ParseHex(u.scriptPubKey);
                if (script.size() == 25 &&
                    script[0] == 0x76 && script[1] == 0xa9 && script[2] == 0x14 &&
                    script[23] == 0x88 && script[24] == 0xac)
                {
                    return false; // keep
                }

                return true; // remove if script invalid
            }),
            inputs.end()
    );


    // Select utxos
    uint64_t utxoAmount{0};
    uint64_t fee1{0};
    uint64_t fee2{0};
    auto minTxFee1 = [](const uint32_t & inputs, const uint32_t & outputs) -> double {
        uint64_t fee = (192*inputs + 34*2) * 20;
        return static_cast<double>(fee) / COIN;
    };
    auto minTxFee2 = [](const uint32_t & inputs, const uint32_t & outputs) -> double {
        return 0;
    };
    if (!xbridge::App::instance().selectUtxos("", inputs, minTxFee1, minTxFee2, toamount,
                                              COIN, outputsForUse, utxoAmount, fee1, fee2))
    {
        ERR() << "Insufficient funds for fee tx";
        return false;
    }

    Array params;

    Array inputs_o;
    CAmount change = utxoAmount - toamount - fee1;
    std::string largestInputAddress;
    double largestInput{0};
    for (const auto & a : outputsForUse) {
        Object ao;
        ao.emplace_back("txid", a.txId);
        ao.emplace_back("vout", static_cast<int>(a.vout));
        if (a.amount > largestInput) {
            largestInputAddress = a.address;
            largestInput = a.amount;
        }
        inputs_o.push_back(ao);
    }
    params.push_back(inputs_o);

    Array outputs_o;
    // Payment
    double paymentAmount = boost::lexical_cast<double>(toamount) / boost::lexical_cast<double>(COIN);
    Object payment_o;
    payment_o.emplace_back("address", toaddress);
    payment_o.emplace_back("amount", paymentAmount);
    outputs_o.push_back(payment_o);
    // Change
    double changePayment = boost::lexical_cast<double>(change) / boost::lexical_cast<double>(COIN);
    Object change_o;
    change_o.emplace_back("address", largestInputAddress);
    change_o.emplace_back("amount", changePayment);
    outputs_o.push_back(change_o);

    params.push_back(outputs_o);

    // Create the transaction
    std::string rawtx;
    try {
        const auto & tx_val = createrawtransaction(params, false);
        rawtx = tx_val.get_str();
    } catch (...) {
        ERR() << "Failed to created feetx, createrawtransaction returned error";
        return false;
    }
    // Sign transaction
    std::string signedtx;
    try {
        const auto & signtx_val = signrawtransaction(Array{rawtx}, false);
        if (signtx_val.type() == obj_type) {
            const auto & signedtx_o = signtx_val.get_obj();
            const auto & hex_val = find_value(signedtx_o, "hex");
            const auto & complete_val = find_value(signedtx_o, "complete");
            if (hex_val.type() != str_type || (complete_val.type() == bool_type && !complete_val.get_bool())) {
                ERR() << "Failed to created feetx, signrawtransaction failed to sign all inputs";
                return false;
            }
            signedtx = hex_val.get_str();
        } else {
            ERR() << "Failed to created feetx, signrawtransaction returned null";
            return false;
        }
    } catch (...) {
        ERR() << "Failed to created feetx, signrawtransaction returned error";
        return false;
    }

    // lock used coins
    std::set<xbridge::wallet::UtxoEntry> feeUtxos{outputsForUse.begin(), outputsForUse.end()};
    xbridge::App::instance().lockFeeUtxos(feeUtxos);

    raw_tx = signedtx;
    return true;
}

void unlockOutputs(const std::string & tx) {
    CMutableTransaction txobj = decodeTransaction(tx);
    std::set<xbridge::wallet::UtxoEntry> coins;
    for (const auto & vin : txobj.vin) {
        xbridge::wallet::UtxoEntry entry;
        entry.txId = vin.prevout.hash.ToString();
        entry.vout = vin.prevout.n;
        coins.insert(entry);
    }
    xbridge::App::instance().unlockFeeUtxos(coins);
}

std::string signTransaction(std::string& raw_tx)
{
    LOCK(cs_rpcBlockchainStore);
    
    std::vector<std::string> params;
    params.push_back(raw_tx);

    const static std::string signCommand("signrawtransaction");
    Value result = tableRPC.execute(signCommand, RPCConvertValues(signCommand, params));
    LOG() << "Sign transaction: " << json_spirit::write_string(Value(result), true);
    if (result.type() != obj_type)
    {
        throw std::runtime_error("Sign transaction command finished with error");
    }

    Object obj = result.get_obj();
    const Value& tx = find_value(obj, "hex");
    return tx.get_str();
}

bool sendTransactionBlockchain(std::string raw_tx, std::string & txid)
{
    LOCK(cs_rpcBlockchainStore);

    const static std::string sendCommand("sendrawtransaction");

    int         errCode = 0;
    std::string errMessage;
    Value result;
    
    try
    {
        {
            std::vector<std::string> params;
            params.push_back(raw_tx);

            result = tableRPC.execute(sendCommand, RPCConvertValues(sendCommand, params));
            if (result.type() != str_type)
            {
                throw std::runtime_error("Send transaction command finished with error");
            }

            txid = result.get_str();
        }
    }
    catch (json_spirit::Object & obj)
    {
        //
        errCode = find_value(obj, "code").get_int();
        errMessage = find_value(obj, "message").get_str();
    }
    catch (std::runtime_error & e)
    {
        // specified error
        errCode = -1;
        errMessage = e.what();
    }
    catch (...)
    {
        errCode = -1;
        errMessage = "unknown error";
    }

    if (errCode != 0)
    {
        LOG() << "xdata sendrawtransaction " << raw_tx;
        LOG() << "error send xdata transaction, code " << errCode << " " << errMessage << " " << __FUNCTION__;
        return false;
    }

    return true;
}

double checkPayment(const std::string & rawtx, const std::string & address)
{
    const static std::string decodeCommand("decoderawtransaction");
    std::vector<std::string> params;
    params.push_back(rawtx);

    Value result = tableRPC.execute(decodeCommand, RPCConvertValues(decodeCommand, params));
    if (result.type() != obj_type)
        throw std::runtime_error("Check payment failed: Decode transaction command finished with error");

    Object obj = result.get_obj();
    Array vouts = find_value(obj, "vout").get_array();
    for (const auto & vout : vouts) {
        // Validate tx type
        auto & scriptPubKey = find_value(vout.get_obj(), "scriptPubKey").get_obj();
        const auto & vouttype = find_value(scriptPubKey, "type").get_str();
        if (vouttype != "pubkeyhash")
            throw std::runtime_error("Check payment failed: Only pubkeyhash payments are accepted");

        // Validate payment address
        const auto & addr_val = find_value(scriptPubKey, "addresses");
        if (addr_val.type() != array_type)
            continue;

        auto & addrs = addr_val.get_array();
        if (addrs.size() <= 0)
            continue;

        if (addrs[0].get_str() != address) // check address
            continue;

        return find_value(vout.get_obj(), "value").get_real();
    }

    return 0.0;
}
    
} // namespace xrouter
