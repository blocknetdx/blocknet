//******************************************************************************
//******************************************************************************

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

#include "xbridgewalletconnectordgb.h"

#include "util/logger.h"
#include "util/txlog.h"

#include "xkey.h"
#include "xbitcoinsecret.h"
#include "xbitcoinaddress.h"
#include "xbitcointransaction.h"

//*****************************************************************************
//*****************************************************************************
namespace xbridge
{

//*****************************************************************************
//*****************************************************************************
namespace rpc
{

using namespace json_spirit;

Object CallRPC(const std::string & rpcuser, const std::string & rpcpasswd,
               const std::string & rpcip, const std::string & rpcport,
               const std::string & strMethod, const Array & params);

//*****************************************************************************
//*****************************************************************************
bool createRawTransaction(const std::string & rpcuser,
                          const std::string & rpcpasswd,
                          const std::string & rpcip,
                          const std::string & rpcport,
                          const std::vector<std::pair<string, int> > & inputs,
                          const std::vector<std::pair<std::string, double> > & outputs,
                          const uint32_t lockTime,
                          std::string & tx);

//*****************************************************************************
//*****************************************************************************
bool decodeRawTransaction(const std::string & rpcuser, const std::string & rpcpasswd,
                          const std::string & rpcip, const std::string & rpcport,
                          const std::string & rawtx,
                          std::string & txid, std::string & tx);

//*****************************************************************************
//*****************************************************************************
namespace
{

//*****************************************************************************
//*****************************************************************************
bool signRawTransactionWithWallet(const std::string & rpcuser,
                                  const std::string & rpcpasswd,
                                  const std::string & rpcip,
                                  const std::string & rpcport,
                                  std::string & rawtx,
                                  bool & complete)
{
    try
    {
        LOG() << "rpc call <signrawtransactionwithwallet>";

        Array params;
        params.push_back(rawtx);

        Object reply = CallRPC(rpcuser, rpcpasswd, rpcip, rpcport,
                               "signrawtransactionwithwallet", params);

        // Parse reply
        const Value & result = find_value(reply, "result");
        const Value & error  = find_value(reply, "error");

        if (error.type() != null_type)
        {
            // Error
            LOG() << "error: " << write_string(error, false);
            // int code = find_value(error.get_obj(), "code").get_int();
            return false;
        }
        else if (result.type() != obj_type)
        {
            // Result
            LOG() << "result not an object " <<
                     (result.type() == null_type ? "" :
                      result.type() == str_type  ? result.get_str() :
                                                   write_string(result, true));
            return false;
        }

        Object obj = result.get_obj();
        const Value  & tx = find_value(obj, "hex");
        const Value & cpl = find_value(obj, "complete");

        if (tx.type() != str_type || cpl.type() != bool_type)
        {
            LOG() << "bad hex " <<
                     (tx.type() == null_type ? "" :
                      tx.type() == str_type  ? tx.get_str() :
                                                   write_string(tx, true));
            return false;
        }

        rawtx    = tx.get_str();
        complete = cpl.get_bool();

    }
    catch (std::exception & e)
    {
        LOG() << "signrawtransactionwithwallet exception " << e.what();
        return false;
    }

    return true;
}

} // namespace
} // namespace rpc

//******************************************************************************
//******************************************************************************
DgbWalletConnector::DgbWalletConnector()
    : BtcWalletConnector()
{

}

//******************************************************************************
//******************************************************************************
bool DgbWalletConnector::createDepositTransaction(const std::vector<std::pair<std::string, int> > & inputs,
                                                  const std::vector<std::pair<std::string, double> > & outputs,
                                                  std::string & txId,
                                                  std::string & rawTx)
{
    if (!rpc::createRawTransaction(m_user, m_passwd, m_ip, m_port,
                                   inputs, outputs, 0, rawTx))
    {
        // cancel transaction
        LOG() << "create transaction error, transaction canceled " << __FUNCTION__;
        return false;
    }

    // sign
    bool complete = false;
    if (!rpc::signRawTransactionWithWallet(m_user, m_passwd, m_ip, m_port, rawTx, complete))
    {
        // do not sign, cancel
        LOG() << "sign transaction error, transaction canceled " << __FUNCTION__;
        return false;
    }

    if(!complete)
    {
        LOG() << "transaction not fully signed" << __FUNCTION__;
        return false;
    }

    std::string txid;
    std::string json;
    if (!rpc::decodeRawTransaction(m_user, m_passwd, m_ip, m_port, rawTx, txid, json))
    {
        LOG() << "decode signed transaction error, transaction canceled " << __FUNCTION__;
        return false;
    }

    txId = txid;

    return true;
}

} // namespace xbridge
