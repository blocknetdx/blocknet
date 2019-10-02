//******************************************************************************
//******************************************************************************
#include "xrouterutils.h"
#include "xrouterlogger.h"
#include "xrouterdef.h"
#include "xroutererror.h"

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

#include "rpcserver.h"
#include "rpcprotocol.h"
#include "rpcclient.h"
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

bool createAndSignTransaction(Array txparams, std::string & raw_tx)
{
    LOCK2(cs_rpcBlockchainStore, pwalletMain->cs_wallet);
    
    int         errCode = 0;
    std::string errMessage;
    std::string rawtx;

    try
    {
        Value result;

        {
            // call create
            result = createrawtransaction(txparams, false);
            LOG() << "Create transaction: " << json_spirit::write_string(Value(result), true);
            if (result.type() != str_type)
            {
                throw std::runtime_error("Create transaction command finished with error");
            }

            rawtx = result.get_str();
        }

        {
            Array params;
            params.push_back(rawtx);
            Object options;
            options.push_back(Pair("lockUnspents", true));
            params.push_back(options);

            // call fund
            result = fundrawtransaction(params, false);
            LOG() << "Fund transaction: " << json_spirit::write_string(Value(result), true);
            if (result.type() != obj_type)
            {
                throw std::runtime_error("Could not fund the transaction. Please check that you have enough funds.");
            }

            Object obj = result.get_obj();
            const Value  & tx = find_value(obj, "hex");
            if (tx.type() != str_type)
            {
                throw std::runtime_error("Could not fund the transaction. Please check that you have enough funds.");
            }

            rawtx = tx.get_str();
        }

        {
            Array params;
            params.push_back(rawtx);

            result = signrawtransaction(params, false);
            LOG() << "Sign transaction: " << json_spirit::write_string(Value(result), true);
            if (result.type() != obj_type)
            {
                throw std::runtime_error("Sign transaction command finished with error");
            }

            Object obj = result.get_obj();
            const Value  & tx = find_value(obj, "hex");
            const Value & cpl = find_value(obj, "complete");

            if (tx.type() != str_type)
            {
                throw std::runtime_error("Sign transaction error");
            }

            if (cpl.type() != bool_type || !cpl.get_bool())
            {
                throw std::runtime_error("Sign transaction not complete");
            }
            
            rawtx = tx.get_str();
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
        LOG() << "xdata signrawtransaction " << rawtx;
        LOG() << "error sign transaction, code " << errCode << " " << errMessage << " " << __FUNCTION__;
        return false;
    }

    raw_tx = rawtx;
    
    return true;
}

bool createAndSignTransaction(std::string address, CAmount amount, string & raw_tx)
{
    Array outputs;
    Object out;
    out.push_back(Pair("address", address));
    out.push_back(Pair("amount", ValueFromAmount(amount)));
    outputs.push_back(out);
    Array inputs;
    Value result;

    Array params;
    params.push_back(inputs);
    params.push_back(outputs);
    return createAndSignTransaction(params, raw_tx);
}

bool createAndSignTransaction(boost::container::map<std::string, CAmount> addrs, string & raw_tx)
{
    Array outputs;
    typedef boost::container::map<std::string, CAmount> addr_map;
    BOOST_FOREACH( addr_map::value_type &it, addrs ) {
        Object out;
        out.push_back(Pair("address", it.first));
        out.push_back(Pair("amount", ValueFromAmount(it.second)));
        outputs.push_back(out);
    }
    Array inputs;
    Value result;

    Array params;
    params.push_back(inputs);
    params.push_back(outputs);
    return createAndSignTransaction(params, raw_tx);
}

void unlockOutputs(std::string tx) {
    CMutableTransaction txobj = decodeTransaction(tx);
    for (size_t i = 0; i < txobj.vin.size(); i++) {
        pwalletMain->UnlockCoin(txobj.vin[0].prevout);
    }
}

std::string signTransaction(std::string& raw_tx)
{
    LOCK2(cs_rpcBlockchainStore, pwalletMain->cs_wallet);
    
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

        LOG() << "sendrawtransaction " << raw_tx;
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

bool sendTransactionBlockchain(std::string address, CAmount amount, std::string & txid)
{
    std::string raw_tx;
    bool res = createAndSignTransaction(address, amount, raw_tx);
    if (!res) {
        return false;
    }
    
    res = sendTransactionBlockchain(raw_tx, txid);
    return res;
}
    
} // namespace xrouter
