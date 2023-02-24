// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//*****************************************************************************
//*****************************************************************************

#include <xbridge/xbridgewalletconnectorbtc.h>

#include <xbridge/util/logger.h>
#include <xbridge/util/xutil.h>
#include <xbridge/xbitcoinaddress.h>
#include <xbridge/xbitcointransaction.h>
#include <xbridge/xbridgecryptoproviderbtc.h>

#include <base58.h>
#include <primitives/transaction.h>

#include <json/json_spirit_reader_template.h>
#include <json/json_spirit_writer_template.h>
#include <json/json_spirit_utils.h>

#include <boost/iostreams/concepts.hpp>
#include <boost/lexical_cast.hpp>

//*****************************************************************************
//*****************************************************************************
namespace xbridge
{

//*****************************************************************************
//*****************************************************************************
namespace rpc
{

using namespace json_spirit;

//*****************************************************************************
//*****************************************************************************
bool getinfo(const std::string & rpcuser, const std::string & rpcpasswd,
             const std::string & rpcip, const std::string & rpcport,
             WalletInfo & info)
{
    try
    {
        // LOG() << "rpc call <getinfo>";

        Array params;
        Object reply = CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "getinfo", params);

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

        Object o = result.get_obj();

        const Value & relayFee = find_value(o, "relayfee");
        if (relayFee.type() != null_type)
            info.relayFee = relayFee.get_real();
        info.blocks   = find_value(o, "blocks").get_int();
    }
    catch (std::exception & e)
    {
        LOG() << "getinfo exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool getnetworkinfo(const std::string & rpcuser, const std::string & rpcpasswd,
                    const std::string & rpcip, const std::string & rpcport,
                    WalletInfo & info)
{
    try
    {
        // LOG() << "rpc call <getnetworkinfo>";

        Array params;
        Object reply = CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "getnetworkinfo", params);

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

        Object o = result.get_obj();
        const Value & relayFee = find_value(o, "relayfee");
        if (relayFee.type() != null_type)
            info.relayFee = relayFee.get_real();
    }
    catch (std::exception & e)
    {
        LOG() << "getinfo exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool getblockchaininfo(const std::string & rpcuser, const std::string & rpcpasswd,
                       const std::string & rpcip, const std::string & rpcport,
                       WalletInfo & info)
{
    try
    {
        Array params;
        Object reply = CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "getblockchaininfo", params);

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

        Object o = result.get_obj();

        info.blocks = find_value(o, "blocks").get_real();
        // median time
        auto mt = find_value(o, "mediantime");
        if (mt.type() != null_type)
            info.mediantime = mt.get_int64();
        // best block hash
        const auto & bbh = find_value(o, "bestblockhash");
        if (bbh.type() != null_type)
            info.bestblockhash = uint256S(bbh.get_str());
    }
    catch (std::exception & e)
    {
        LOG() << "getinfo exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool getblock(const std::string & rpcuser, const std::string & rpcpasswd,
                  const std::string & rpcip, const std::string & rpcport,
                  const std::string & blockHash, std::string & rawBlock)
{
    try
    {
        Array params;
        params.push_back(blockHash);
        Object reply = CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "getblock", params);

        // Parse reply
        const Value & result = find_value(reply, "result");
        const Value & error  = find_value(reply, "error");

        if (error.type() != null_type)
        {
            // Error
            LOG() << "getblock error: " << write_string(error, false);
            return false;
        }
        else if (result.type() != obj_type)
        {
            // Result
            LOG() << "getblock result not an object " <<
                     (result.type() == null_type ? "" :
                      result.type() == str_type  ? result.get_str() :
                                                   write_string(result, true));
            return false;
        }

        rawBlock = write_string(result, true);
    }
    catch (std::exception & e)
    {
        LOG() << "getblock exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool getblockhash(const std::string & rpcuser, const std::string & rpcpasswd,
                  const std::string & rpcip, const std::string & rpcport,
                  const uint32_t & block, std::string & blockHash)
{
    try
    {
        Array params;
        params.push_back(static_cast<int>(block));
        Object reply = CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "getblockhash", params);

        // Parse reply
        const Value & result = find_value(reply, "result");
        const Value & error  = find_value(reply, "error");

        if (error.type() != null_type)
        {
            // Error
            LOG() << "getblockhash error: " << write_string(error, false);
            return false;
        }
        else if (result.type() != str_type)
        {
            // Result
            LOG() << "getblockhash result is not a string";
            return false;
        }

        blockHash = result.get_str();
    }
    catch (std::exception & e)
    {
        LOG() << "getblockhash exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool getblockcount(const std::string & rpcuser, const std::string & rpcpasswd,
                   const std::string & rpcip, const std::string & rpcport, uint32_t & blockCount)
{
    try {
        Array params;
        Object reply = CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "getblockcount", params);

        const Value & result = find_value(reply, "result");
        const Value & error  = find_value(reply, "error");

        if (error.type() != null_type) {
            LOG() << "getblockcount error: " << write_string(error, false);
            return false;
        } else if (result.type() != int_type) {
            LOG() << "getblockcount result is not an int";
            return false;
        }
        blockCount = result.get_int();

    } catch (std::exception & e) {
        LOG() << "getblockcount exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool listaccounts(const std::string & rpcuser, const std::string & rpcpasswd,
                  const std::string & rpcip, const std::string & rpcport,
                  std::vector<std::string> & accounts)
{
    try
    {
        // LOG() << "rpc call <listaccounts>";

        Array params;
        Object reply = CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "listaccounts", params);

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

        Object acclist = result.get_obj();
        for (auto nameval : acclist)
        {
            accounts.push_back(nameval.name_);
        }
    }
    catch (std::exception & e)
    {
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool listaddressgroupings(const std::string & rpcuser, const std::string & rpcpasswd,
                           const std::string & rpcip, const std::string & rpcport,
                           std::vector<std::string> & addresses)
{
    try
    {
        LOG() << "rpc call <listaddressgroupings>";

        Array params;
        //params.push_back(addresses);
        Object reply = CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "listaddressgroupings", params);

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
        else if (result.type() != array_type)
        {
            // Result
            LOG() << "result not an array " <<
                     (result.type() == null_type ? "" :
                      result.type() == str_type  ? result.get_str() :
                                                   write_string(result, true));
            return false;
        }


        Array arr = result.get_array();
        for (const Value & v : arr)
        {
            Array varray = v.get_array();
            for (const Value & varr : varray)
            {
                Array vaddress = varr.get_array();
                if (!vaddress.empty() && vaddress[0].type() == str_type)
                {
                    addresses.push_back(vaddress[0].get_str());
                }
            }
        }
    }
    catch (std::exception & e)
    {
        LOG() << "listaddressgroupings exception" << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool getaddressesbyaccount(const std::string & rpcuser, const std::string & rpcpasswd,
                           const std::string & rpcip, const std::string & rpcport,
                           const std::string & account,
                           std::vector<std::string> & addresses)
{
    try
    {
        // LOG() << "rpc call <getaddressesbyaccount>";

        Array params;
        params.push_back(account);
        Object reply = CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "getaddressesbyaccount", params);

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
        else if (result.type() != array_type)
        {
            // Result
            LOG() << "result not an array " <<
                     (result.type() == null_type ? "" :
                      result.type() == str_type  ? result.get_str() :
                                                   write_string(result, true));
            return false;
        }

        Array arr = result.get_array();
        for (const Value & v : arr)
        {
            if (v.type() == str_type)
            {
                addresses.push_back(v.get_str());
            }
        }
    }
    catch (std::exception & e)
    {
        LOG() << "getaddressesbyaccount exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool validateaddress(const std::string & rpcuser, const std::string & rpcpasswd,
                     const std::string & rpcip, const std::string & rpcport,
                     const std::string & address)
{
    try
    {
        Array params;
        params.push_back(address);
        params.push_back(address);
        Object reply = CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "signmessage", params);

        // Parse reply
        Value result = find_value(reply, "result");
        Value error  = find_value(reply, "error");
        if (error.type() != null_type)
        {
            // Error
            LOG() << "signmessage failed for address:" << address << " error: " << write_string(error, false);
            return false;
        }

    }
    catch (std::exception & e)
    {
        LOG() << "signmessage exception " << e.what();
        return false;
    }
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool listUnspent(const std::string & rpcuser,
                 const std::string & rpcpasswd,
                 const std::string & rpcip,
                 const std::string & rpcport,
                 std::vector<wallet::UtxoEntry> & entries)
{
    const static std::string txid("txid");
    const static std::string vout("vout");
    const static std::string amount("amount");
    const static std::string scriptPubKey("scriptPubKey");
    const static std::string confirmations("confirmations");

    try
    {
        LOG() << "rpc call <listunspent>";

        Array params;
        Object reply = CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "listunspent", params);

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
        else if (result.type() != array_type)
        {
            // Result
            LOG() << "result not an array " <<
                     (result.type() == null_type ? "" :
                      result.type() == str_type  ? result.get_str() :
                                                   write_string(result, true));
            return false;
        }

        Array arr = result.get_array();
        for (const Value & v : arr)
        {
            if (v.type() == obj_type)
            {
                const Value & spendable = find_value(v.get_obj(), "spendable");
                if (spendable.type() == bool_type && !spendable.get_bool())
                    continue;

                wallet::UtxoEntry u;
                int confs = -1;

                Object o = v.get_obj();
                for (const auto & v : o)
                {
                    if (v.name_ == txid)
                    {
                        u.txId = v.value_.get_str();
                    }
                    else if (v.name_ == vout)
                    {
                        u.vout = v.value_.get_int();
                    }
                    else if (v.name_ == amount)
                    {
                        u.amount = v.value_.get_real();
                    }
                    else if (v.name_ == scriptPubKey)
                    {
                        u.scriptPubKey = v.value_.get_str();
                    }
                    else if (v.name_ == confirmations)
                    {
                        confs = v.value_.get_int();
                        u.confirmations = confs > 0 ? confs : 0;
                        u.hasConfirmations = confs > 0;
                    }
                }

                if (!u.txId.empty() && u.amount > 0 && (confs == -1 || confs > 0))
                {
                    entries.push_back(u);
                }
            }
        }
    }
    catch (std::exception & e)
    {
        LOG() << "listunspent exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool lockUnspent(const std::string & rpcuser,
                 const std::string & rpcpasswd,
                 const std::string & rpcip,
                 const std::string & rpcport,
                 const std::vector<wallet::UtxoEntry> & entries,
                 const bool lock)
{
    const static std::string txid("txid");
    const static std::string vout("vout");

    try
    {
        LOG() << "rpc call <lockunspent>";

        Array params;

        // 1. unlock
        params.push_back(lock ? false : true);

        // 2. txoutputs
        Array outputs;
        for (const wallet::UtxoEntry & entry : entries)
        {
            Object o;
            o.push_back(Pair(txid, entry.txId));
            o.push_back(Pair(vout, (int)entry.vout));

            outputs.push_back(o);
        }

        params.push_back(outputs);

        Object reply = CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "lockunspent", params);

        // Parse reply
        // const Value & result = find_value(reply, "result");
        const Value & error  = find_value(reply, "error");

        if (error.type() != null_type)
        {
            // Error
            LOG() << "error: " << write_string(error, false);
            // int code = find_value(error.get_obj(), "code").get_int();
            return false;
        }
//        else if (result.type() != array_type)
//        {
//            // Result
//            LOG() << "result not an array " <<
//                     (result.type() == null_type ? "" :
//                      result.type() == str_type  ? result.get_str() :
//                                                   write_string(result, true));
//            return false;
//        }

    }
    catch (std::exception & e)
    {
        LOG() << "lockunspent exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool gettxout(const std::string & rpcuser,
              const std::string & rpcpasswd,
              const std::string & rpcip,
              const std::string & rpcport,
              wallet::UtxoEntry & txout)
{
    try
    {
        LOG() << "rpc call <gettxout>";

        txout.amount = 0;

        Array params;
        params.push_back(txout.txId);
        params.push_back(static_cast<int>(txout.vout));
        Object reply = CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "gettxout", params);

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

        Object o = result.get_obj();
        txout.amount = find_value(o, "value").get_real();

        // Assign confirmations
        const auto & rconfs = find_value(o, "confirmations");
        if (rconfs.type() == int_type)
            txout.setConfirmations(rconfs.get_int());
    }
    catch (std::exception & e)
    {
        LOG() << "gettxout exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool gettransaction(const std::string & rpcuser,
                    const std::string & rpcpasswd,
                    const std::string & rpcip,
                    const std::string & rpcport,
                    wallet::UtxoEntry & txout)
{
    try
    {
        LOG() << "rpc call <gettransaction>";

        txout.amount = 0;

        Array params;
        params.push_back(txout.txId);
        Object reply = CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "getrawtransaction", params);

        // Parse reply
        const Value & result = find_value(reply, "result");
        const Value & error  = find_value(reply, "error");

        if (error.type() != null_type)
        {
            // Error
            LOG() << "error: " << write_string(error, false);
            return false;
        }
        else if (result.type() != str_type)
        {
            // Result
            LOG() << "result of getrawtransaction not a string " <<
                     (result.type() == null_type ? "" :
                      result.type() == str_type  ? result.get_str() :
                                                   write_string(result, true));
            return false;
        }


        Array d { Value(result.get_str()) };
        reply = CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "decoderawtransaction", d);

        const Value & result2 = find_value(reply, "result");
        const Value & error2  = find_value(reply, "error");

        if (error2.type() != null_type)
        {
            // Error
            LOG() << "error: " << write_string(error2, false);
            return false;
        }
        else if (result2.type() != obj_type)
        {
            // Result
            LOG() << "result of decoderawtransaction not an object " <<
                     (result2.type() == null_type ? "" :
                      result2.type() == str_type  ? result2.get_str() :
                                                   write_string(result2, true));
            return false;
        }

        Object o = result2.get_obj();

        const Value & vouts = find_value(o, "vout");
        if(vouts.type() != array_type)
        {
            LOG() << "vout not an array type";
            return false;
        }

        for(const Value & element : vouts.get_array())
        {
            if(element.type() != obj_type)
            {
                LOG() << "vouts element not an object type";
                return false;
            }

            Object elementObj = element.get_obj();

            uint32_t vout = find_value(elementObj, "n").get_int();
            if(vout == txout.vout)
            {
                txout.amount = find_value(elementObj, "value").get_real();
                break;
            }
        }
    }
    catch (std::exception & e)
    {
        LOG() << "gettransaction exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool getRawTransaction(const std::string & rpcuser,
                       const std::string & rpcpasswd,
                       const std::string & rpcip,
                       const std::string & rpcport,
                       const std::string & txid,
                       const bool verbose,
                       std::string & tx)
{
    try
    {
        LOG() << "rpc call <getrawtransaction>";

        Array params;
        params.push_back(txid);
        if (verbose)
        {
            params.push_back(1);
        }
        Object reply = CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "getrawtransaction", params);

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

        if (verbose)
        {
            if (result.type() != obj_type)
            {
                // Result
                LOG() << "result not an object " << write_string(result, true);
                return false;
            }

            // transaction exists, success
            tx = write_string(result, true);
        }
        else
        {
            if (result.type() != str_type)
            {
                // Result
                LOG() << "result not an string " << write_string(result, true);
                return false;
            }

            // transaction exists, success
            tx = result.get_str();
        }
    }
    catch (std::exception & e)
    {
        LOG() << "getrawtransaction exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool getNewAddress(const std::string & rpcuser,
                   const std::string & rpcpasswd,
                   const std::string & rpcip,
                   const std::string & rpcport,
                   std::string & addr)
{
    try
    {
        LOG() << "rpc call <getnewaddress>";

        Array params;
        Object reply = CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "getnewaddress", params);

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
        else if (result.type() != str_type)
        {
            // Result
            LOG() << "result not an string " <<
                     (result.type() == null_type ? "" :
                      result.type() == str_type  ? result.get_str() :
                                                   write_string(result, true));
            return false;
        }

        addr = result.get_str();
    }
    catch (std::exception & e)
    {
        LOG() << "getnewaddress exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool createRawTransaction(const std::string & rpcuser,
                          const std::string & rpcpasswd,
                          const std::string & rpcip,
                          const std::string & rpcport,
                          const std::vector<XTxIn> & inputs,
                          const std::vector<std::pair<std::string, double> > & outputs,
                          const uint32_t lockTime,
                          std::string & tx,
                          const bool cltv=false)
{
    try
    {
        LOG() << "rpc call <createrawtransaction>";

        // inputs
        Array i;
        for (const XTxIn & input : inputs)
        {
            Object tmp;
            tmp.push_back(Pair("txid", input.txid));
            tmp.push_back(Pair("vout", static_cast<int>(input.n)));
            if (cltv)
                tmp.push_back(Pair("sequence", static_cast<int256_t>(xbridge::SEQUENCE_FINAL)));
            i.push_back(tmp);
        }

        // outputs
        Object o;
        for (const std::pair<std::string, double> & dest : outputs)
        {
            o.push_back(Pair(dest.first, dest.second));
        }

        Array params;
        params.push_back(i);
        params.push_back(o);

        // locktime
        if (lockTime > 0)
        {
            params.push_back(int256_t(lockTime));
        }

        Object reply = CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "createrawtransaction", params);

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
        else if (result.type() != str_type)
        {
            // Result
            LOG() << "result not an string " <<
                     (result.type() == null_type ? "" :
                                                   write_string(result, true));
            return false;
        }

        tx = write_string(result, false);
        if (tx[0] == '\"')
        {
            tx.erase(0, 1);
        }
        if (tx[tx.size()-1] == '\"')
        {
            tx.erase(tx.size()-1, 1);
        }
    }
    catch (std::exception & e)
    {
        LOG() << "createrawtransaction exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
std::string prevtxsJson(const std::vector<std::tuple<std::string, int, std::string, std::string> > & prevtxs)
{
    // prevtxs
    if (!prevtxs.size())
    {
        return std::string();
    }

    Array arrtx;
    for (const std::tuple<std::string, int, std::string, std::string> & prev : prevtxs)
    {
        Object o;
        o.push_back(Pair("txid",         std::get<0>(prev)));
        o.push_back(Pair("vout",         std::get<1>(prev)));
        o.push_back(Pair("scriptPubKey", std::get<2>(prev)));
        std::string redeem = std::get<3>(prev);
        if (redeem.size())
        {
            o.push_back(Pair("redeemScript", redeem));
        }
        arrtx.push_back(o);
    }

    return write_string(Value(arrtx));
}

//*****************************************************************************
//*****************************************************************************
bool signRawTransaction(const std::string & rpcuser,
                        const std::string & rpcpasswd,
                        const std::string & rpcip,
                        const std::string & rpcport,
                        std::string & rawtx,
                        const std::string & prevtxs,
                        const std::vector<std::string> & keys,
                        bool & complete)
{
    try
    {
        LOG() << "rpc call <signrawtransaction>";

        Array params;
        params.push_back(rawtx);

        // prevtxs
        if (!prevtxs.size())
        {
            params.push_back(Value::null);
        }
        else
        {
            Value v;
            if (!read_string(prevtxs, v))
            {
                ERR() << "error read json " << __FUNCTION__;
                ERR() << prevtxs;
                params.push_back(Value::null);
            }
            else
            {
                params.push_back(v.get_array());
            }
        }

        // priv keys
        if (!keys.size())
        {
            params.push_back(Value::null);
        }
        else
        {
            Array jkeys;
            std::copy(keys.begin(), keys.end(), std::back_inserter(jkeys));

            params.push_back(jkeys);
        }

        Object reply = CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "signrawtransaction", params);

        // Parse reply
        Value result = find_value(reply, "result");
        const Value & error = find_value(reply, "error");

        if (error.type() != null_type)
        {
            // For newer bitcoin clients try signrawtransactionwithwallet
            reply = CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "signrawtransactionwithwallet", params);

            const Value & error2 = find_value(reply, "error");
            if (error2.type() != null_type) {
                LOG() << "error: " << write_string(error, false) << " " << write_string(error2, false);
                return false;
            }

            result = find_value(reply, "result");
        }

        if (result.type() != obj_type)
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
        LOG() << "signrawtransaction exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool signRawTransaction(const std::string & rpcuser,
                        const std::string & rpcpasswd,
                        const std::string & rpcip,
                        const std::string & rpcport,
                        std::string & rawtx,
                        bool & complete)
{
    std::vector<std::tuple<std::string, int, std::string, std::string> > arr;
    std::string prevtxs = prevtxsJson(arr);
    std::vector<std::string> keys;
    return signRawTransaction(rpcuser, rpcpasswd, rpcip, rpcport,
                              rawtx, prevtxs, keys, complete);
}

//*****************************************************************************
//*****************************************************************************
bool decodeRawTransaction(const std::string & rpcuser,
                          const std::string & rpcpasswd,
                          const std::string & rpcip,
                          const std::string & rpcport,
                          const std::string & rawtx,
                          std::string & txid,
                          std::string & tx)
{
    try
    {
        LOG() << "rpc call <decoderawtransaction>";

        Array params;
        params.push_back(rawtx);
        Object reply = CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "decoderawtransaction", params);

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

        tx   = write_string(result, false);

        const Value & vtxid = find_value(result.get_obj(), "txid");
        if (vtxid.type() == str_type)
        {
            txid = vtxid.get_str();
        }
    }
    catch (std::exception & e)
    {
        LOG() << "decoderawtransaction exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool sendRawTransaction(const std::string & rpcuser,
                        const std::string & rpcpasswd,
                        const std::string & rpcip,
                        const std::string & rpcport,
                        const std::string & rawtx,
                        std::string & txid,
                        int32_t & errorCode,
                        std::string & message)
{
    try
    {
        LOG() << "rpc call <sendrawtransaction>";

        errorCode = 0;

        Array params;
        params.push_back(rawtx);
        Object reply = CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "sendrawtransaction", params);

        // Parse reply
        // const Value & result = find_value(reply, "result");
        const Value & error  = find_value(reply, "error");
        if (error.type() != null_type)
        {
            // Error
            LOG() << "error: " << write_string(error, false);
            errorCode = find_value(error.get_obj(), "code").get_int();
            message = find_value(error.get_obj(), "message").get_str();

            return false;
        }

        const Value & result = find_value(reply, "result");
        if (result.type() != str_type)
        {
            // Result
            LOG() << "result not an string " << write_string(result, true);
            return false;
        }

        txid = result.get_str();
    }
    catch (std::exception & e)
    {
        errorCode = -1;

        LOG() << "sendrawtransaction exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool signMessage(const std::string & rpcuser, const std::string & rpcpasswd,
                 const std::string & rpcip,   const std::string & rpcport,
                 const std::string & address, const std::string & message,
                 std::string & signature)
{
    try
    {
        LOG() << "rpc call <signmessage>";

        Array params;
        params.push_back(address);
        params.push_back(message);
        const Object reply = CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "signmessage", params);

        // reply
        const Value & error  = find_value(reply, "error");
        if (error.type() != null_type)
        {
            // Error
            LOG() << "error: " << write_string(error, false);
            return false;
        }

        const Value & result = find_value(reply, "result");
        if (result.type() != str_type)
        {
            // Result
            LOG() << "result not an string " << write_string(result, true);
            return false;
        }

        const std::string base64result  = result.get_str();

        bool isInvalid = false;
        DecodeBase64(base64result.c_str(), &isInvalid);

        if (isInvalid)
        {
            signature.clear();
            return false;
        }

        signature = base64result;
    }
    catch (std::exception & e)
    {
        signature.clear();

        LOG() << "signmessage exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool verifyMessage(const std::string & rpcuser, const std::string & rpcpasswd,
                   const std::string & rpcip,   const std::string & rpcport,
                   const std::string & address, const std::string & message,
                   const std::string & signature)
{
    try
    {
        LOG() << "rpc call <verifymessage>";

        Array params;
        params.push_back(address);
        params.push_back(signature);
        params.push_back(message);
        const Object reply = CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "verifymessage", params);

        // reply
        const Value & error  = find_value(reply, "error");
        if (error.type() != null_type)
        {
            // Error
            LOG() << "error: " << write_string(error, false);
            return false;
        }

        const Value & result = find_value(reply, "result");
        if (result.type() != bool_type)
        {
            // Result
            LOG() << "result not an string " << write_string(result, true);
            return false;
        }

        return result.get_bool();
    }
    catch (std::exception & e)
    {
        LOG() << "verifymessage exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool getRawMempool(const std::string & rpcuser, const std::string & rpcpasswd,
                   const std::string & rpcip,   const std::string & rpcport,
                   std::vector<std::string> & txids)
{
    try
    {
        LOG() << "rpc call <getrawmempool>";

        Array params;
        const Object reply = CallRPC(rpcuser, rpcpasswd, rpcip, rpcport, "getrawmempool", params);

        // reply
        const Value & error = find_value(reply, "error");
        if (error.type() != null_type)
        {
            // Error
            LOG() << "getrawmempool error: " << write_string(error, false);
            return false;
        }

        const Value & result = find_value(reply, "result");
        if (result.type() != array_type)
        {
            // Result
            LOG() << "getrawmempool result is not an array " << write_string(result, true);
            return false;
        }

        txids.clear();
        auto & res = result.get_array();
        for (auto & tid : txids)
            txids.push_back(tid);
        return true;
    }
    catch (std::exception & e)
    {
        LOG() << "getrawmempool exception " << e.what();
        return false;
    }

    return true;
}

} // namespace rpc

namespace
{

/**
 * @brief SignatureHash  compute hash of transaction signature
 * @param scriptCode
 * @param txTo
 * @param nIn
 * @param nHashType
 * @return hash of transaction signature
 */
uint256 SignatureHash(const CScript& scriptCode, const CTransactionPtr & txTo,
                      unsigned int nIn, int nHashType
                      /*, const CAmount& amount,
                       * SigVersion sigversion,
                       * const PrecomputedTransactionData* cache*/)
{
//    if (sigversion == SIGVERSION_WITNESS_V0) {
//        uint256 hashPrevouts;
//        uint256 hashSequence;
//        uint256 hashOutputs;

//        if (!(nHashType & SIGHASH_ANYONECANPAY)) {
//            hashPrevouts = cache ? cache->hashPrevouts : GetPrevoutHash(txTo);
//        }

//        if (!(nHashType & SIGHASH_ANYONECANPAY) && (nHashType & 0x1f) != SIGHASH_SINGLE && (nHashType & 0x1f) != SIGHASH_NONE) {
//            hashSequence = cache ? cache->hashSequence : GetSequenceHash(txTo);
//        }


//        if ((nHashType & 0x1f) != SIGHASH_SINGLE && (nHashType & 0x1f) != SIGHASH_NONE) {
//            hashOutputs = cache ? cache->hashOutputs : GetOutputsHash(txTo);
//        } else if ((nHashType & 0x1f) == SIGHASH_SINGLE && nIn < txTo.vout.size()) {
//            CHashWriter ss(SER_GETHASH, 0);
//            ss << txTo.vout[nIn];
//            hashOutputs = ss.GetHash();
//        }

//        CHashWriter ss(SER_GETHASH, 0);
//        // Version
//        ss << txTo.nVersion;
//        // Input prevouts/nSequence (none/all, depending on flags)
//        ss << hashPrevouts;
//        ss << hashSequence;
//        // The input being signed (replacing the scriptSig with scriptCode + amount)
//        // The prevout may already be contained in hashPrevout, and the nSequence
//        // may already be contain in hashSequence.
//        ss << txTo.vin[nIn].prevout;
//        ss << static_cast<const CScriptBase&>(scriptCode);
//        ss << amount;
//        ss << txTo.vin[nIn].nSequence;
//        // Outputs (none/one/all, depending on flags)
//        ss << hashOutputs;
//        // Locktime
//        ss << txTo.nLockTime;
//        // Sighash type
//        ss << nHashType;

//        return ss.GetHash();
//    }

    static const uint256 one(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));
    if (nIn >= txTo->vin.size()) {
        //  nIn out of range
        return one;
    }

    // Check for invalid use of SIGHASH_SINGLE
    if ((nHashType & 0x1f) == SIGHASH_SINGLE) {
        if (nIn >= txTo->vout.size()) {
            //  nOut out of range
            return one;
        }
    }

    // Wrapper to serialize only the necessary parts of the transaction being signed
    CTransactionSignatureSerializer txTmp(*txTo, scriptCode, nIn, nHashType);

    // Serialize and hash
    CHashWriter ss(SER_GETHASH, 0);
    ss << txTmp << nHashType;
    return ss.GetHash();
}

} // namespace

//*****************************************************************************
//*****************************************************************************
template <class CryptoProvider>
bool BtcWalletConnector<CryptoProvider>::init()
{
    // convert prefixes
    addrPrefix   = static_cast<char>(boost::lexical_cast<int>(addrPrefix.data()));
    scriptPrefix = static_cast<char>(boost::lexical_cast<int>(scriptPrefix.data()));
    secretPrefix = static_cast<char>(boost::lexical_cast<int>(secretPrefix.data()));

    // wallet info
    rpc::WalletInfo info;
    if (!this->getInfo(info))
        return false;

    // Calculate dust
    dustAmount = info.relayFee > 0 ? 0.546 * info.relayFee * COIN : 5460;

    // Set median time
    mediantime = info.mediantime;

    return true;
}

//*****************************************************************************
//*****************************************************************************
template <class CryptoProvider>
std::string BtcWalletConnector<CryptoProvider>::fromXAddr(const std::vector<unsigned char> & xaddr) const
{
    xbridge::XBitcoinAddress addr;
    addr.Set(CKeyID(uint160(xaddr)), addrPrefix[0]);
    return addr.ToString();
}

//*****************************************************************************
//*****************************************************************************
template <class CryptoProvider>
std::vector<unsigned char> BtcWalletConnector<CryptoProvider>::toXAddr(const std::string & addr) const
{
    std::vector<unsigned char> vaddr;
    if (DecodeBase58Check(addr.c_str(), vaddr))
    {
        vaddr.erase(vaddr.begin());
    }
    return vaddr;
}

//*****************************************************************************
//*****************************************************************************
template <class CryptoProvider>
bool BtcWalletConnector<CryptoProvider>::requestAddressBook(std::vector<wallet::AddressBookEntry> & entries)
{
    std::vector<std::string> addresses;
    if (!rpc::listaddressgroupings(m_user, m_passwd, m_ip, m_port, addresses))
    {
        return false;
    }

    std::vector<std::string> copy;

    for (std::string & address : addresses)
    {
        if (!rpc::validateaddress(m_user, m_passwd, m_ip, m_port, address))
        {
            continue;
        }
        copy.emplace_back(address);

    }
    entries.emplace_back("none", copy);
    return true;
}

//*****************************************************************************
//*****************************************************************************
template <class CryptoProvider>
bool BtcWalletConnector<CryptoProvider>::getInfo(rpc::WalletInfo & info) const
{
    if (!rpc::getblockchaininfo(m_user, m_passwd, m_ip, m_port, info) ||
        !rpc::getnetworkinfo(m_user, m_passwd, m_ip, m_port, info))
    {
        if (!rpc::getinfo(m_user, m_passwd, m_ip, m_port, info))
        {
            WARN() << currency << " failed to respond to getblockchaininfo, getnetworkinfo, and getinfo. Is the wallet running? "
                   << __FUNCTION__;
            return false;
        }
    }

    return true;
}

//******************************************************************************
//******************************************************************************
template <class CryptoProvider>
bool BtcWalletConnector<CryptoProvider>::getUnspent(std::vector<wallet::UtxoEntry> & inputs,
                                                    const std::set<wallet::UtxoEntry> & excluded) const
{
    if (!rpc::listUnspent(m_user, m_passwd, m_ip, m_port, inputs))
    {
        LOG() << "rpc::listUnspent failed " << __FUNCTION__;
        return false;
    }

    // Remove all the excluded utxos
    inputs.erase(
        std::remove_if(inputs.begin(), inputs.end(), [&excluded, this](xbridge::wallet::UtxoEntry & u) {
            if (excluded.count(u))
                return true; // remove if in excluded list

            // Only accept p2pkh (like 76a91476bba472620ff0ecbfbf93d0d3909c6ca84ac81588ac)
            std::vector<unsigned char> script = ParseHex(u.scriptPubKey);
            if (script.size() == 25 &&
                script[0] == 0x76 && script[1] == 0xa9 && script[2] == 0x14 &&
                script[23] == 0x88 && script[24] == 0xac)
            {
                script.erase(script.begin(), script.begin()+3);
                script.erase(script.end()-2, script.end());
                u.address = fromXAddr(script);
                return false; // keep
            }

            return true; // remove if script invalid
        }),
        inputs.end()
    );

    return true;
}

//******************************************************************************
//******************************************************************************
template <class CryptoProvider>
bool BtcWalletConnector<CryptoProvider>::getNewAddress(std::string & addr)
{
    if (!rpc::getNewAddress(m_user, m_passwd, m_ip, m_port, addr))
    {
        LOG() << "rpc::getNewAddress failed " << __FUNCTION__;
        return false;
    }

    return true;
}

//******************************************************************************
//******************************************************************************
template <class CryptoProvider>
bool BtcWalletConnector<CryptoProvider>::getTxOut(wallet::UtxoEntry & entry)
{
    if (!rpc::gettxout(m_user, m_passwd, m_ip, m_port, entry))
    {
        return false;
//        LOG() << "gettxout failed, trying call gettransaction " << __FUNCTION__;
//
//        if(!rpc::gettransaction(m_user, m_passwd, m_ip, m_port, entry))
//        {
//            WARN() << "both calls of gettxout and gettransaction failed " << __FUNCTION__;
//            return false;
//        }
    }

    return true;
}

//******************************************************************************
//******************************************************************************
template <class CryptoProvider>
bool BtcWalletConnector<CryptoProvider>::sendRawTransaction(const std::string & rawtx,
                                            std::string & txid,
                                            int32_t & errorCode,
                                            std::string & message)
{
    if (!rpc::sendRawTransaction(m_user, m_passwd, m_ip, m_port,
                                 rawtx, txid, errorCode, message))
    {
        LOG() << "rpc::sendRawTransaction failed, error code: <"
              << errorCode
              << "> message: '"
              << message
              << "' "
              << __FUNCTION__;
        return false;
    }

    return true;
}

//******************************************************************************
//******************************************************************************
template <class CryptoProvider>
bool BtcWalletConnector<CryptoProvider>::signMessage(const std::string & address,
                                     const std::string & message,
                                     std::string & signature)
{
    if (!rpc::signMessage(m_user, m_passwd, m_ip, m_port,
                          address, message, signature))
    {
        LOG() << "rpc::signMessage failed " << __FUNCTION__;
        return false;
    }

    return true;
}

//******************************************************************************
//******************************************************************************
template <class CryptoProvider>
bool BtcWalletConnector<CryptoProvider>::verifyMessage(const std::string & address,
                                       const std::string & message,
                                       const std::string & signature)
{
    if (!rpc::verifyMessage(m_user, m_passwd, m_ip, m_port,
                            address, message, signature))
    {
        LOG() << "rpc::verifyMessage failed " << __FUNCTION__;
        return false;
    }

    return true;
}

//******************************************************************************
//******************************************************************************
template <class CryptoProvider>
bool BtcWalletConnector<CryptoProvider>::getRawMempool(std::vector<std::string> & txids)
{
    if (!rpc::getRawMempool(m_user, m_passwd, m_ip, m_port, txids)) {
        LOG() << "rpc::getRawMempool failed " << __FUNCTION__;
        return false;
    }

    return true;
}

//******************************************************************************
//******************************************************************************
template <class CryptoProvider>
bool BtcWalletConnector<CryptoProvider>::isUTXOSpentInTx(const std::string & txid,
        const std::string & utxoPrevTxId, const uint32_t & utxoVoutN, bool & isSpent)
{
    std::string json;
    if (!rpc::getRawTransaction(m_user, m_passwd, m_ip, m_port, txid, true, json)) {
        LOG() << "rpc::getRawTransaction failed " << __FUNCTION__;
        return false;
    }

    json_spirit::Value txv;
    if (!json_spirit::read_string(json, txv))
    {
        LOG() << "json read error for " << txid << " " << __FUNCTION__;
        return false;
    }

    auto & txo = txv.get_obj();
    auto & vins = json_spirit::find_value(txo, "vin").get_array();
    for (auto & vin : vins) {
        if (vin.type() != json_spirit::obj_type)
            continue;
        auto & vino = vin.get_obj();
        // Check txid
        auto & vin_txid = json_spirit::find_value(vino, "txid");
        if (vin_txid.type() != json_spirit::str_type)
            continue;
        // Check vout
        auto & vin_vout = json_spirit::find_value(vino, "vout");
        if (vin_vout.type() != json_spirit::int_type)
            continue;
        // If match is found, return
        if (vin_txid.get_str() == utxoPrevTxId && vin_vout.get_int() == utxoVoutN) {
            isSpent = true;
            return true;
        }
    }

    return true;
}

//******************************************************************************
//******************************************************************************
template <class CryptoProvider>
bool BtcWalletConnector<CryptoProvider>::getBlock(const std::string & blockHash, std::string & rawBlock)
{
    if (!rpc::getblock(m_user, m_passwd, m_ip, m_port, blockHash, rawBlock)) {
        LOG() << "rpc::getblock failed " << __FUNCTION__;
        return false;
    }
    return true;
}

//******************************************************************************
//******************************************************************************
template <class CryptoProvider>
bool BtcWalletConnector<CryptoProvider>::getBlockHash(const uint32_t & block, std::string & blockHash)
{
    if (!rpc::getblockhash(m_user, m_passwd, m_ip, m_port, block, blockHash)) {
        LOG() << "rpc::getblockhash failed " << __FUNCTION__;
        return false;
    }
    return true;
}

//******************************************************************************
//******************************************************************************
template <class CryptoProvider>
bool BtcWalletConnector<CryptoProvider>::getBlockCount(uint32_t & blockCount) {
    if (!rpc::getblockcount(m_user, m_passwd, m_ip, m_port, blockCount)) {
        LOG() << "rpc::getblockhash failed " << __FUNCTION__;
        return false;
    }
    return true;
}

//******************************************************************************
//******************************************************************************
template <class CryptoProvider>
bool BtcWalletConnector<CryptoProvider>::getTransactionsInBlock(const std::string & blockHash,
                                                                std::vector<std::string> & txids)
{
    std::string json;
    if (!rpc::getblock(m_user, m_passwd, m_ip, m_port, blockHash, json)) {
        LOG() << "rpc::getblock failed " << __FUNCTION__;
        return false;
    }

    json_spirit::Value jblock;
    if (!json_spirit::read_string(json, jblock))
    {
        LOG() << "json read error for " << blockHash << " " << __FUNCTION__;
        return false;
    }
    if (jblock.type() != json_spirit::obj_type)
    {
        LOG() << "json read error for " << blockHash << " " << __FUNCTION__;
        return false;
    }

    txids.clear();

    auto & jblocko = jblock.get_obj();
    auto & txs = json_spirit::find_value(jblocko, "tx").get_array();
    for (auto & tx : txs) {
        auto & txid = tx.get_str();
        txids.push_back(txid);
    }

    return true;
}

//******************************************************************************
//******************************************************************************

/**
 * \brief Checks if specified address has a valid prefix.
 * \param addr Address to check
 * \return returns true if address has a valid prefix, otherwise false.
 *
 * If the specified wallet address has a valid prefix the method returns true, otherwise false.
 */
template <class CryptoProvider>
bool BtcWalletConnector<CryptoProvider>::hasValidAddressPrefix(const std::string & addr) const
{
    std::vector<unsigned char> decoded;
    if (!DecodeBase58Check(addr, decoded))
    {
        return false;
    }

    bool isP2PKH = memcmp(&addrPrefix[0],   &decoded[0], decoded.size()-sizeof(uint160)) == 0;
    bool isP2SH  = memcmp(&scriptPrefix[0], &decoded[0], decoded.size()-sizeof(uint160)) == 0;

    return isP2PKH || isP2SH;
}

//******************************************************************************
//******************************************************************************

/**
 * \brief Checks if specified address is valid.
 * \param addr Address to check
 * \return returns true if address is valid, otherwise false.
 */
template <class CryptoProvider>
bool BtcWalletConnector<CryptoProvider>::isValidAddress(const std::string & addr) const
{
    return hasValidAddressPrefix(addr) && rpc::validateaddress(m_user, m_passwd, m_ip, m_port, addr);
}

//******************************************************************************
//******************************************************************************
template <class CryptoProvider>
bool BtcWalletConnector<CryptoProvider>::isDustAmount(const double & amount) const
{
    // cast to int because amount could be negative
    return static_cast<int256_t>(amount * static_cast<int256_t>(COIN)) < static_cast<int256_t>(dustAmount);
}

//******************************************************************************
//******************************************************************************
template <class CryptoProvider>
bool BtcWalletConnector<CryptoProvider>::newKeyPair(std::vector<unsigned char> & pubkey,
                                    std::vector<unsigned char> & privkey)
{
    m_cp.makeNewKey(privkey);
    return m_cp.getPubKey(privkey, pubkey);
}

//******************************************************************************
//******************************************************************************
template <class CryptoProvider>
std::vector<unsigned char> BtcWalletConnector<CryptoProvider>::getKeyId(const std::vector<unsigned char> & pubkey)
{
    uint160 id = Hash160(&pubkey[0], &pubkey[0] + pubkey.size());
    return std::vector<unsigned char>(id.begin(), id.end());
}

//******************************************************************************
//******************************************************************************
template <class CryptoProvider>
std::vector<unsigned char> BtcWalletConnector<CryptoProvider>::getScriptId(const std::vector<unsigned char> & script)
{
    CScriptID id(CScript(script.begin(), script.end()));
    return std::vector<unsigned char>(id.begin(), id.end());
}

//******************************************************************************
//******************************************************************************
template <class CryptoProvider>
std::string BtcWalletConnector<CryptoProvider>::scriptIdToString(const std::vector<unsigned char> & id) const
{
    xbridge::XBitcoinAddress baddr;
    baddr.Set(CScriptID(uint160(id)), scriptPrefix[0]);
    return baddr.ToString();
}

//******************************************************************************
// calculate tx fee for deposit tx
// output count always 1
//******************************************************************************
template <class CryptoProvider>
double BtcWalletConnector<CryptoProvider>::minTxFee1(const uint32_t inputCount, const uint32_t outputCount) const
{
    int256_t fee = (192*inputCount + 34*outputCount) * feePerByte;
    if (fee < minTxFee)
    {
        fee = minTxFee;
    }
    return (double)fee / COIN;
}

//******************************************************************************
// calculate tx fee for payment/refund tx
// input count always 1
//******************************************************************************
template <class CryptoProvider>
double BtcWalletConnector<CryptoProvider>::minTxFee2(const uint32_t inputCount, const uint32_t outputCount) const
{
    int256_t fee = (192*inputCount + 34*outputCount) * feePerByte;
    if (fee < minTxFee)
    {
        fee = minTxFee;
    }
    return (double)fee / COIN;
}

//******************************************************************************
// return false if deposit tx not found (need wait tx)
// true if tx found and checked
// isGood == true id depost tx is OK
// amount in - for check vout[0].value, out = vout[0].value
//******************************************************************************
template <class CryptoProvider>
bool BtcWalletConnector<CryptoProvider>::checkDepositTransaction(const std::string & depositTxId,
                                                                 const std::string & /*destination*/,
                                                                 double & amount,
                                                                 int256_t & p2shAmount,
                                                                 uint32_t & depositTxVout,
                                                                 const std::string & expectedScript,
                                                                 double & excessAmount,
                                                                 bool & isGood)
{
    isGood  = false;
    excessAmount = 0;

    std::string tx;
    if (!rpc::getRawTransaction(m_user, m_passwd, m_ip, m_port, depositTxId, false, tx))
    {
        LOG() << "no tx found " << depositTxId << " ...waiting " << __FUNCTION__;
        return false;
    }

    std::string rawtx;
    std::string reftxid;
    if (!rpc::decodeRawTransaction(m_user, m_passwd, m_ip, m_port, tx, reftxid, rawtx))
    {
        LOG() << "bad counterparty deposit, decode transaction failed " << depositTxId << " " << tx << " " << __FUNCTION__;
        return true; // done
    }

    // check confirmations
    json_spirit::Value txv;
    if (!json_spirit::read_string(rawtx, txv))
    {
        LOG() << "json read error for " << depositTxId << " " << rawtx << " ...waiting " << __FUNCTION__;
        return false;
    }

    json_spirit::Object txo = txv.get_obj();

    if (requiredConfirmations > 0)
    {
        uint32_t confs{0};

        // Check for confirmations in raw transaction output
        json_spirit::Value txvConfCount = json_spirit::find_value(txo, "confirmations");
        if (txvConfCount.type() != json_spirit::int_type) { // If not found check gettxout for confirmations
            wallet::UtxoEntry utxo;
            utxo.txId = depositTxId;
            utxo.vout = depositTxVout;
            if (rpc::gettxout(m_user, m_passwd, m_ip, m_port, utxo) && utxo.hasConfirmations) {
                confs = utxo.confirmations;
            } else { // confirmation field not found, fail
                LOG() << "confirmations data not found in gettxout for " << depositTxId
                      << " this order may be stuck and may need to be canceled " << __FUNCTION__;
                return false;
            }
        } else {
            confs = static_cast<uint32_t>(txvConfCount.get_int());
        }

        if (confs < requiredConfirmations)
        {
            // wait more
            LOG() << currency << " counterparty deposit " << depositTxId << " confirmations " << confs << " of " << requiredConfirmations << " ...waiting " << __FUNCTION__;
            return false;
        }
    }

    // Ensure p2sh accounts for fees

    // Check vins
    json_spirit::Value vinso = json_spirit::find_value(txo, "vin");
    if (vinso.type() != json_spirit::array_type || vinso.get_array().empty()) {
        LOG() << "tx " << depositTxId << " no vins " << __FUNCTION__;
        return true; // done
    }
    json_spirit::Array vins = vinso.get_array();

    // Check vouts
    json_spirit::Value voutso = json_spirit::find_value(txo, "vout");
    if (voutso.type() != json_spirit::array_type || voutso.get_array().empty()) {
        LOG() << "tx " << depositTxId << " no vouts " << __FUNCTION__;
        return true; // done
    }
    json_spirit::Array vouts = voutso.get_array();

    // Add up all vin amounts (prevouts)
    double totalVinAmount{0};
    for (auto & vin : vins) {
        const json_spirit::Value & txidObj = json_spirit::find_value(vin.get_obj(), "txid");
        if (txidObj.type() != json_spirit::str_type) {
            LOG() << "tx " << depositTxId << " bad vin txid " << __FUNCTION__;
            return true; // done
        }
        const json_spirit::Value & txSequence = json_spirit::find_value(vin.get_obj(), "sequence");
        if (txSequence.type() != json_spirit::null_type) { // if sequence is available, then enforce
            if (txSequence.type() != json_spirit::int_type) {
                LOG() << "tx " << depositTxId << " bad sequence type " << __FUNCTION__;
                return true; // done
            } else if (txSequence.get_int64() != xbridge::SEQUENCE_FINAL) {
                LOG() << "tx " << depositTxId << " sequence " << txSequence.get_int64()
                      << " bad sequence for input, expected " << xbridge::SEQUENCE_FINAL
                      << " " << __FUNCTION__;
                return true; // done
            }
        }
        const json_spirit::Value & txVoutObj = json_spirit::find_value(vin.get_obj(), "vout");
        if (txVoutObj.type() != json_spirit::int_type) {
            LOG() << "tx " << depositTxId << " bad input vout " << __FUNCTION__;
            return true; // done
        }
        // Check prevout amount
        const auto & vinTxId = txidObj.get_str();
        const auto & vinTxVout = txVoutObj.get_int();
        std::string vinTx;
        if (!rpc::getRawTransaction(m_user, m_passwd, m_ip, m_port, vinTxId, true, vinTx)) {
            LOG() << "vin tx not found for deposit " << depositTxId << " vin txid: " << vinTxId << " ...waiting " << __FUNCTION__;
            return false;
        }
        json_spirit::Value vinTxv;
        if (!json_spirit::read_string(vinTx, vinTxv)) {
            LOG() << "vin json read error for deposit " << depositTxId << " vin raw tx: " << vinTx << " ...waiting " << __FUNCTION__;
            return false;
        }
        json_spirit::Object vinTxo = vinTxv.get_obj();
        json_spirit::Array vinOuts = json_spirit::find_value(vinTxo, "vout").get_array();
        if (vinOuts.empty() || vinTxVout >= static_cast<int>(vinOuts.size())) {
            LOG() << "tx " << depositTxId << " bad vin, missing outputs " << __FUNCTION__;
            return true; // done
        }
        bool foundVout{false};
        double vinAmount{0};
        for (const auto & vout : vinOuts) {
            const json_spirit::Value & valObj = json_spirit::find_value(vout.get_obj(), "value");
            const json_spirit::Value & nObj = json_spirit::find_value(vout.get_obj(), "n");
            if (valObj.type() != json_spirit::real_type || nObj.type() != json_spirit::int_type)
                continue;
            if (nObj.get_int() == vinTxVout) {
                vinAmount = valObj.get_real();
                foundVout = true;
                break;
            }
        }
        if (!foundVout) {
            LOG() << "tx " << depositTxId << " bad prevout " << vinTxId << " " << __FUNCTION__;
            return true; // done
        }
        totalVinAmount += vinAmount;
    }

    // Add up all vout amounts
    double totalVoutAmount{0};
    double depositP2SHAmount{0};
    for (auto & vout : vouts) {
        const json_spirit::Value & amountObj = json_spirit::find_value(vout.get_obj(), "value");
        if (amountObj.type() != json_spirit::real_type) {
            LOG() << "tx " << depositTxId << " bad vout amount " << __FUNCTION__;
            return true; // done
        }
        const json_spirit::Value & nObj = json_spirit::find_value(vout.get_obj(), "n");
        if (nObj.type() != json_spirit::int_type) {
            LOG() << "tx " << depositTxId << " bad vout n " << __FUNCTION__;
            return true; // done
        }
        if (amountObj.get_real() < 0) {
            LOG() << "tx " << depositTxId << " bad vout, has negative amount " << __FUNCTION__;
            return true; // done
        }
        totalVoutAmount += amountObj.get_real();

        // Check all vouts for valid deposit
        const json_spirit::Value & scriptPubKey = json_spirit::find_value(vout.get_obj(), "scriptPubKey");
        const json_spirit::Value & hex = json_spirit::find_value(scriptPubKey.get_obj(), "hex");
        if (scriptPubKey.type() == json_spirit::null_type || hex.type() == json_spirit::null_type)
            continue;

        // Check that expected script and amounts match
        if (expectedScript == hex.get_str()) {
            const json_spirit::Value & vamount = json_spirit::find_value(vout.get_obj(), "value");
            const json_spirit::Value & n = json_spirit::find_value(vout.get_obj(), "n");
            if (amount <= vamount.get_real() + std::numeric_limits<double>::epsilon()) {
                depositP2SHAmount = vamount.get_real();
                depositTxVout = static_cast<uint32_t>(n.get_int());
            }
            break; // done searching
        }
    }

    if (depositP2SHAmount == 0) {
        LOG() << "tx " << depositTxId << " no valid p2sh in deposit transaction " << __FUNCTION__;
        return true; // done
    }

    // Check if there's enough to cover fees
    const double counterpartyFees = totalVinAmount - totalVoutAmount;
    const double fee1 = minTxFee1(static_cast<uint32_t>(vins.size()), static_cast<uint32_t>(vouts.size())); // p2sh deposit fee
    const double fee2 = minTxFee2(1, 1); // p2sh redeem fee
    const double ourMinimumFees = fee1 * 0.95; // Allow 5% margin of error in fee amount
    // Check that counterparty provided enough to cover deposit network fee
    if (counterpartyFees < 0 || counterpartyFees < ourMinimumFees) {
        LOG() << "tx " << depositTxId << " not enough inputs to cover p2sh deposit fees: " << ourMinimumFees << " " << __FUNCTION__;
        return true; // done
    }
    // Make sure counterparty provided enough for the redeem fee
    if (depositP2SHAmount < amount + fee2 * 0.95) { // Allow 5% margin of error in fee amount
        LOG() << "tx " << depositTxId << " not enough inputs to cover p2sh redeem fees: " << fee2 * 0.95 << " " << __FUNCTION__;
        return true; // done
    }
    // we should pay ourselves any excess
    if (depositP2SHAmount > amount + fee2)
        excessAmount = depositP2SHAmount - amount - fee2;

    p2shAmount = depositP2SHAmount * COIN;
    isGood = true;
    return true; // done
}

//******************************************************************************
/**
 * Search for the secret in the spent/redeemed transaction.
 * Returns true if the search is complete. Returns false if the search needs
 * more time (due to not finding txid).
 * @tparam CryptoProvider
 * @param paymentTxId Id of the transaction on the blockchain where secret exists.
 * @param depositTxId Prevout txid of the p2sh
 * @param depositTxVOut Prevout n of the p2sh
 * @param hx Hashed secret known by both counterparties
 * @param secret Found secret
 * @param isGood If secret was found and all checks passed.
 * @return
 */
//******************************************************************************
template <class CryptoProvider>
bool BtcWalletConnector<CryptoProvider>::getSecretFromPaymentTransaction(const std::string & paymentTxId,
                                                                         const std::string & depositTxId,
                                                                         const uint32_t & depositTxVOut,
                                                                         const std::vector<unsigned char> & hx,
                                                                         std::vector<unsigned char> & secret,
                                                                         bool & isGood)
{
    isGood = false;

    std::string rawtx;
    if (!rpc::getRawTransaction(m_user, m_passwd, m_ip, m_port, paymentTxId, true, rawtx))
    {
        LOG() << "no tx found " << paymentTxId << " " << __FUNCTION__;
        return false;
    }

    json_spirit::Value txv;
    if (!json_spirit::read_string(rawtx, txv))
    {
        LOG() << "json read error for " << paymentTxId << " " << rawtx << " " << __FUNCTION__;
        return false;
    }

    json_spirit::Object txo = txv.get_obj();

    // extract secret from vins
    json_spirit::Array vins = json_spirit::find_value(txo, "vin").get_array();

    // Check all vins for secret
    for (auto & vin : vins) {
        const json_spirit::Value & depositId = json_spirit::find_value(vin.get_obj(), "txid");
        if (depositId.type() == json_spirit::null_type)
            continue;

        const json_spirit::Value & voutN = json_spirit::find_value(vin.get_obj(), "vout");
        if (voutN.type() == json_spirit::null_type)
            continue;

        if (depositId.get_str() != depositTxId || voutN.get_int() != depositTxVOut)
            continue;

        const json_spirit::Value & scriptPubKey = json_spirit::find_value(vin.get_obj(), "scriptSig");
        if (scriptPubKey.type() == json_spirit::null_type)
            continue;

        const json_spirit::Value & hex = json_spirit::find_value(scriptPubKey.get_obj(), "hex");
        if (hex.type() == json_spirit::null_type)
            continue;

        auto ssig = ParseHex(hex.get_str());
        CScript scriptSig(ssig.begin(), ssig.end());
        std::vector<unsigned char> chk;
        opcodetype op;
        CScript::const_iterator pc = scriptSig.begin();
        while (pc < scriptSig.end()) { // check if hashed secret matches hashed sig data
            if (scriptSig.GetOp(pc, op, chk) && memcmp(&this->getKeyId(chk)[0], &hx[0], hx.size()) == 0) {
                secret = chk;
                isGood = true;
                return true;
            }
        }

        // Done searching if we found an exact vin match
        break;
    }

    LOG() << "tx " << paymentTxId << " secret not found " << __FUNCTION__;

    return true;
}

//******************************************************************************
//******************************************************************************
template <class CryptoProvider>
uint32_t BtcWalletConnector<CryptoProvider>::lockTime(const char role) const
{
    rpc::WalletInfo info;
    if (!rpc::getblockchaininfo(m_user, m_passwd, m_ip, m_port, info))
    {
        LOG() << "getblockchaininfo failed, trying call getinfo " << __FUNCTION__;

        if (!rpc::getinfo(m_user, m_passwd, m_ip, m_port, info))
        {
            WARN() << "both calls of getblockchaininfo and getinfo failed " << __FUNCTION__;
            return 0;
        }
    }

    if (info.blocks == 0)
    {
        LOG() << "block count not defined in blockchain info " << __FUNCTION__;
        return 0;
    }

    // lock time
    uint32_t lt = 0;
    if (role == 'A')
    {
        uint32_t makerTime = XMAKER_LOCKTIME_TARGET_SECONDS;
        uint32_t blocks = makerTime / blockTime;
        if (blocks < XMIN_LOCKTIME_BLOCKS) blocks = XMIN_LOCKTIME_BLOCKS;
        lt = info.blocks + blocks;
    }
    else if (role == 'B')
    {
        uint32_t takerTime = XTAKER_LOCKTIME_TARGET_SECONDS;
        if (blockTime >= XSLOW_BLOCKTIME_SECONDS)
            takerTime = XSLOW_TAKER_LOCKTIME_TARGET_SECONDS; // allow more time for slower chains
        uint32_t blocks = takerTime / blockTime;
        if (blocks < XMIN_LOCKTIME_BLOCKS) blocks = XMIN_LOCKTIME_BLOCKS;
        lt = info.blocks + blocks;
    }

    return lt;
}

//******************************************************************************
//******************************************************************************
template <class CryptoProvider>
bool BtcWalletConnector<CryptoProvider>::acceptableLockTimeDrift(const char role, const uint32_t lckTime) const
{
    auto lt = lockTime(role);
    if (lt == 0 || lt >= LOCKTIME_THRESHOLD || lckTime >= LOCKTIME_THRESHOLD)
        return false;
    const int256_t diff = static_cast<int256_t>(lt) - static_cast<int256_t>(lckTime);
    // Locktime drift is at minimum XLOCKTIME_DRIFT_SECONDS. In cases with slow chains
    // the locktime drift will increase to block time multiplied by the number of
    // blocks representing the maximum allowed locktime drift.
    //
    // If drift determination changes here, update wallet validation checks and unit
    // tests.
    int256_t drift = std::max<int256_t>(XLOCKTIME_DRIFT_SECONDS, XMAX_LOCKTIME_DRIFT_BLOCKS * blockTime);
    return diff * static_cast<int256_t>(blockTime) <= drift;
}

//******************************************************************************
//******************************************************************************
template <class CryptoProvider>
bool BtcWalletConnector<CryptoProvider>::createDepositUnlockScript(const std::vector<unsigned char> & myPubKey,
                                                          const std::vector<unsigned char> & otherPubKey,
                                                          const std::vector<unsigned char> & secretHash,
                                                          const uint32_t lockTime,
                                                          std::vector<unsigned char> & resultSript)
{
    CScript inner;
    inner << OP_IF
                << lockTime << OP_CHECKLOCKTIMEVERIFY << OP_DROP
                << OP_DUP << OP_HASH160 << getKeyId(myPubKey) << OP_EQUALVERIFY << OP_CHECKSIG
          << OP_ELSE
                << OP_DUP << OP_HASH160 << getKeyId(otherPubKey) << OP_EQUALVERIFY << OP_CHECKSIGVERIFY
                << OP_SIZE << 33 << OP_EQUALVERIFY << OP_HASH160 << secretHash << OP_EQUAL
          << OP_ENDIF;

    resultSript = std::vector<unsigned char>(inner.begin(), inner.end());

    return true;
}

//******************************************************************************
//******************************************************************************
template <class CryptoProvider>
bool BtcWalletConnector<CryptoProvider>::createDepositTransaction(const std::vector<XTxIn> & inputs,
                                                                  const std::vector<std::pair<std::string, double> > & outputs,
                                                                  std::string & txId,
                                                                  uint32_t & txVout,
                                                                  std::string & rawTx)
{
    if (!rpc::createRawTransaction(m_user, m_passwd, m_ip, m_port,
                                   inputs, outputs, 0, rawTx, true))
    {
        // cancel transaction
        LOG() << "create transaction error, transaction canceled " << __FUNCTION__;
        return false;
    }

    // sign
    bool complete = false;
    if (!rpc::signRawTransaction(m_user, m_passwd, m_ip, m_port, rawTx, complete))
    {
        // do not sign, cancel
        LOG() << "sign transaction error, transaction canceled " << __FUNCTION__;
        return false;
    }

    if(!complete)
    {
        LOG() << "transaction not fully signed " << __FUNCTION__;
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
    txVout = 0;

    return true;
}

//******************************************************************************
//******************************************************************************
xbridge::CTransactionPtr createTransaction(const bool txWithTimeField)
{
    return xbridge::CTransactionPtr(new xbridge::CTransaction(txWithTimeField));
}

//******************************************************************************
//******************************************************************************
xbridge::CTransactionPtr createTransaction(const WalletConnector & conn,
                                           const std::vector<XTxIn> & inputs,
                                           const std::vector<std::pair<std::string, double> >  & outputs,
                                           const int256_t COIN,
                                           const uint32_t txversion,
                                           const uint32_t lockTime,
                                           const bool txWithTimeField)
{
    xbridge::CTransactionPtr tx(new xbridge::CTransaction(txWithTimeField));
    tx->nVersion  = txversion;
    tx->nLockTime = lockTime;

    for (const XTxIn & in : inputs)
    {
        tx->vin.push_back(CTxIn(COutPoint(uint256S(in.txid), in.n)));
    }

    for (const std::pair<std::string, double> & out : outputs)
    {
        std::vector<unsigned char> id = conn.toXAddr(out.first);

        CScript scr;
        scr << OP_DUP << OP_HASH160 << ToByteVector(id) << OP_EQUALVERIFY << OP_CHECKSIG;

        tx->vout.push_back(CTxOut(out.second * COIN, scr));
    }

    return tx;
}

//******************************************************************************
//******************************************************************************
template <class CryptoProvider>
bool BtcWalletConnector<CryptoProvider>::createRefundTransaction(const std::vector<XTxIn> & inputs,
                                                                 const std::vector<std::pair<std::string, double> > & outputs,
                                                                 const std::vector<unsigned char> & mpubKey,
                                                                 const std::vector<unsigned char> & mprivKey,
                                                                 const std::vector<unsigned char> & innerScript,
                                                                 const uint32_t lockTime,
                                                                 std::string & txId,
                                                                 std::string & rawTx)
{
    xbridge::CTransactionPtr txUnsigned = createTransaction(*this,
                                                            inputs, outputs,
                                                            COIN, txVersion,
                                                            lockTime, txWithTimeField);
    // Correctly set tx input sequence. If lockTime is specified sequence must be 2^32-2, otherwise 2^32-1 (Final)
    uint32_t sequence = lockTime > 0 ? xbridge::SEQUENCE_FINAL-1 : xbridge::SEQUENCE_FINAL;
    txUnsigned->vin[0].nSequence = sequence;

    CScript inner(innerScript.begin(), innerScript.end());

    CScript redeem;
    {
        CScript tmp;
        tmp << ToByteVector(mpubKey) << OP_TRUE << ToByteVector(inner);

        std::vector<unsigned char> signature;
        uint256 hash = SignatureHash(inner, txUnsigned, 0, SIGHASH_ALL);
        if (!m_cp.sign(mprivKey, hash, signature))
        {
            // cancel transaction
            LOG() << "sign transaction error, transaction canceled " << __FUNCTION__;
//                sendCancelTransaction(xtx, crNotSigned);
            return false;
        }

        signature.push_back((unsigned char)SIGHASH_ALL);

        redeem << signature;
        redeem += tmp;
    }

    xbridge::CTransactionPtr tx(createTransaction(txWithTimeField));
    if (!tx)
    {
        ERR() << "transaction not created " << __FUNCTION__;
//            sendCancelTransaction(xtx, crBadSettings);
        return false;
    }
    tx->nVersion  = txUnsigned->nVersion;
    tx->nTime     = txUnsigned->nTime;
    tx->vin.push_back(CTxIn(txUnsigned->vin[0].prevout, redeem, sequence));
    tx->vout      = txUnsigned->vout;
    tx->nLockTime = txUnsigned->nLockTime;

    rawTx = tx->toString();

    std::string json;
    std::string reftxid;
    if (!rpc::decodeRawTransaction(m_user, m_passwd, m_ip, m_port, rawTx, reftxid, json))
    {
        LOG() << "decode signed transaction error, transaction canceled " << __FUNCTION__;
//            sendCancelTransaction(xtx, crRpcError);
            return true;
    }

    txId  = reftxid;

    return true;
}

//******************************************************************************
//******************************************************************************
template <class CryptoProvider>
bool BtcWalletConnector<CryptoProvider>::createPaymentTransaction(const std::vector<XTxIn> & inputs,
                                                                  const std::vector<std::pair<std::string, double> > & outputs,
                                                                  const std::vector<unsigned char> & mpubKey,
                                                                  const std::vector<unsigned char> & mprivKey,
                                                                  const std::vector<unsigned char> & xpubKey,
                                                                  const std::vector<unsigned char> & innerScript,
                                                                  std::string & txId,
                                                                  std::string & rawTx)
{
    xbridge::CTransactionPtr txUnsigned = createTransaction(*this,
                                                            inputs, outputs,
                                                            COIN, txVersion,
                                                            0, txWithTimeField);

    CScript inner(innerScript.begin(), innerScript.end());

    std::vector<unsigned char> signature;
    uint256 hash = SignatureHash(inner, txUnsigned, 0, SIGHASH_ALL);
    if (!m_cp.sign(mprivKey, hash, signature))
    {
        // cancel transaction
        LOG() << "sign transaction error, transaction canceled " << __FUNCTION__;
//                sendCancelTransaction(xtx, crNotSigned);
        return false;
    }

    signature.push_back((unsigned char)SIGHASH_ALL);

    CScript redeem;
    redeem << xpubKey
           << signature << mpubKey
           << OP_FALSE << ToByteVector(inner);

    xbridge::CTransactionPtr tx(createTransaction(txWithTimeField));
    if (!tx)
    {
        ERR() << "transaction not created " << __FUNCTION__;
//                sendCancelTransaction(xtx, crBadSettings);
        return false;
    }
    tx->nVersion  = txUnsigned->nVersion;
    tx->nTime     = txUnsigned->nTime;
    tx->vin.push_back(CTxIn(txUnsigned->vin[0].prevout, redeem));
    tx->vout      = txUnsigned->vout;

    rawTx = tx->toString();

    std::string json;
    std::string paytxid;
    if (!rpc::decodeRawTransaction(m_user, m_passwd, m_ip, m_port, rawTx, paytxid, json))
    {
            LOG() << "decode signed transaction error, transaction canceled " << __FUNCTION__;
//                sendCancelTransaction(xtx, crRpcError);
        return true;
    }

    txId  = paytxid;

    return true;
}

//******************************************************************************
//******************************************************************************
template <class CryptoProvider>
bool BtcWalletConnector<CryptoProvider>::createPartialTransaction(const std::vector<XTxIn> inputs,
                                                                  const std::vector<std::pair<std::string, double> > outputs,
                                                                  std::string & txId, std::string & rawTx)
{
    xbridge::CTransactionPtr tx = createTransaction(*this, inputs, outputs, COIN, txVersion, 0, txWithTimeField);
    rawTx = tx->toString();

    // sign
    bool complete = false;
    if (!rpc::signRawTransaction(m_user, m_passwd, m_ip, m_port, rawTx, complete)) {
        LOG() << "sign transaction error " << __FUNCTION__;
        return false;
    }

    if (!complete) {
        LOG() << "transaction not fully signed " << __FUNCTION__;
        return false;
    }

    std::string txid;
    std::string json;
    if (!rpc::decodeRawTransaction(m_user, m_passwd, m_ip, m_port, rawTx, txid, json)) {
        LOG() << "decode signed transaction error " << __FUNCTION__;
        return false;
    }

    txId = txid;

    return true;
}

//******************************************************************************
//******************************************************************************
template <class CryptoProvider>
bool BtcWalletConnector<CryptoProvider>::splitUtxos(const CAmount splitAmount, const std::string addr,
                                                    const bool includeFees, const std::set<wallet::UtxoEntry> excluded,
                                                    const std::set<COutPoint> utxos, CAmount & totalSplit,
                                                    CAmount & splitIncFees, int & splitCount, std::string & txId,
                                                    std::string & rawTx, std::string & failReason)
{
    const auto hasUserSpecifiedUtxos = !utxos.empty();
    if (isDustAmount(xBridgeValueFromAmount(splitAmount))) {
        failReason = "split amount is dust [" + xBridgeStringValueFromAmount(splitAmount) + "]";
        return false;
    }
    if (!isValidAddress(addr)) {
        failReason = "address is invalid or not in the wallet for token " + currency;
        return false;
    }
    std::vector<wallet::UtxoEntry> unspent;
    if (!getUnspent(unspent, excluded)) {
        failReason = "failed to get unspent transaction outputs for token " + currency;
        return false;
    }
    if (hasUserSpecifiedUtxos) { // Check that user specified utxos are available
        std::vector<wallet::UtxoEntry> newUnspent;
        auto copyUtxos = utxos;
        for (const auto & utxo : unspent) {
            COutPoint vout{uint256S(utxo.txId), utxo.vout};
            if (copyUtxos.count(vout)) {
                copyUtxos.erase(vout);
                newUnspent.push_back(utxo);
            }
        }
        if (!copyUtxos.empty()) {
            failReason = "user specified utxo was not found or is not available: " + copyUtxos.begin()->ToString();
            return false;
        }
        unspent = newUnspent; // only use user specified utxos
    }

    const CAmount fee1 = xBridgeIntFromReal(minTxFee1(1, 3));
    const CAmount fee2 = xBridgeIntFromReal(minTxFee2(1, 1));
    const CAmount feesPerUtxo = fee1 + fee2;
    const CAmount splitSize = splitAmount + (includeFees ? feesPerUtxo : 0);

    if (!hasUserSpecifiedUtxos) {
        // Remove all utxos that already match the expected size or that don't match the specified address
        unspent.erase(std::remove_if(unspent.begin(), unspent.end(),
            [splitSize, addr](const wallet::UtxoEntry & entry) {
                return splitSize - entry.camount() == 0 || entry.address != addr;
            }), unspent.end());
    }

    if (unspent.empty()) {
        failReason = "failed to get unspent transaction outputs for token " + currency;
        return false; // no available utxos
    }

    // Erase any utxos after 100 to prevent issues with creating txs that are too large
    if (unspent.size() > 100)
        unspent.erase(unspent.begin()+100, unspent.begin()+unspent.size());

    CAmount vinsTotal{0};
    std::vector<xbridge::XTxIn> vins;
    for (const auto & vin : unspent) {
        vinsTotal += vin.camount();
        vins.emplace_back(vin.txId, vin.vout, vin.amount);
    }

    auto outputCount = static_cast<int>(vinsTotal / splitSize);
    if (outputCount < 1) {
        failReason = "already split all unused utxos in address [" + addr + "]";
        return false; // not enough coin
    }
    if (outputCount > 100)
        outputCount = 100;

    const CAmount remainder = vinsTotal - (outputCount * splitSize);
    std::vector<std::pair<std::string, CAmount>> vouts;
    for (int i = 0; i < outputCount; ++i)
        vouts.emplace_back(addr, splitSize);

    const CAmount txFees = xBridgeIntFromReal(minTxFee1(vins.size(), vouts.size()));
    const CAmount change = remainder - txFees;
    // add remainder vout if not dust
    if (!isDustAmount(xBridgeValueFromAmount(change)))
        vouts.emplace_back(addr, change);
    else {
        // Remove any utxos consumed by fees
        CAmount feesLeft = txFees;
        while (feesLeft > 0 && !vouts.empty()) {
            auto & vout = vouts[vouts.size()-1];
            const CAmount voutAmount = vout.second;
            const CAmount voutNewAmount = voutAmount - feesLeft;
            // If vout doesn't cover the fee move to the next one (i.e. if new amount is too small or negative)
            if (voutNewAmount <= 0) {
                vouts.erase(vouts.begin() + vouts.size());
                outputCount -= 1;
                feesLeft -= voutAmount; // subtract vout amount from leftover fees
                continue;
            }

            vout.second = voutNewAmount;
            if (isDustAmount(xBridgeValueFromAmount(vout.second)))
                vouts.erase(vouts.begin()+vouts.size()); // remove output if dust
            outputCount -= 1;

            break;
        }
    }

    if (vouts.empty()) {
        failReason = "unable to split further, already split all unused utxos in address [" + addr + "]";
        return false;
    }

    std::vector<std::pair<std::string, double>> dvouts;
    for (auto & vout : vouts)
        dvouts.emplace_back(vout.first, xBridgeValueFromAmount(vout.second));
    xbridge::CTransactionPtr tx = createTransaction(*this, vins, dvouts, COIN, txVersion, 0, txWithTimeField);
    rawTx = tx->toString();

    // sign
    bool complete = false;
    if (!rpc::signRawTransaction(m_user, m_passwd, m_ip, m_port, rawTx, complete) || !complete) {
        failReason = "failed to sign the split transaction " + currency;
        return false;
    }

    std::string txid;
    std::string json;
    if (!rpc::decodeRawTransaction(m_user, m_passwd, m_ip, m_port, rawTx, txid, json)) {
        failReason = "failed to decode the split transaction " + currency;
        return false;
    }

    txId = txid;
    totalSplit = vinsTotal;
    splitIncFees = splitSize;
    splitCount = outputCount;

    return true;
}

// explicit instantiation
BtcWalletConnector<BtcCryptoProvider> variable;

} // namespace xbridge
