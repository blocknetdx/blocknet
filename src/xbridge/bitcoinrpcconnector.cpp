
#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

#include <boost/asio.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/asio/ssl.hpp>
#include <stdio.h>

#include "bitcoinrpcconnector.h"
#include "util/xutil.h"
#include "util/logger.h"
#include "util/txlog.h"

#include "base58.h"
// #include "bignum.h"
#include "rpcserver.h"
#include "rpcprotocol.h"
#include "rpcclient.h"
#include "wallet.h"
#include "init.h"
#include "main.h"
#include "key.h"
#include "sync.h"
#include "core_io.h"
#include "xbridgewallet.h"
#include "xbitcointransaction.h"

#define HTTP_DEBUG

//*****************************************************************************
//*****************************************************************************
namespace xbridge
{

//*****************************************************************************
//*****************************************************************************
namespace rpc
{

using namespace json_spirit;
using namespace std;
using namespace boost;
using namespace boost::asio;


#define PAIRTYPE(t1, t2)    std::pair<t1, t2>

const unsigned int MAX_SIZE = 0x02000000;

static CCriticalSection cs_rpcBlockchainStore;

//******************************************************************************
//******************************************************************************
int readHTTP(std::basic_istream<char>& stream, map<string, string>& mapHeadersRet, string& strMessageRet)
{
    mapHeadersRet.clear();
    strMessageRet = "";

    // Read status
    int nProto = 0;
    int nStatus = ReadHTTPStatus(stream, nProto);

    // Read header
    int nLen = ReadHTTPHeaders(stream, mapHeadersRet);
    if (nLen < 0 || nLen > (int)MAX_SIZE)
        return HTTP_INTERNAL_SERVER_ERROR;

    // Read message
    if (nLen > 0)
    {
        vector<char> vch(nLen);
        stream.read(&vch[0], nLen);
        strMessageRet = string(vch.begin(), vch.end());
    }

    string sConHdr = mapHeadersRet["connection"];

    if ((sConHdr != "close") && (sConHdr != "keep-alive"))
    {
        if (nProto >= 1)
            mapHeadersRet["connection"] = "keep-alive";
        else
            mapHeadersRet["connection"] = "close";
    }

    return nStatus;
}

//******************************************************************************
//******************************************************************************
Object CallRPC(const std::string & rpcuser, const std::string & rpcpasswd,
               const std::string & rpcip, const std::string & rpcport,
               const std::string & strMethod, const Array & params)
{
    boost::asio::ip::tcp::iostream stream;
    stream.expires_from_now(boost::posix_time::seconds(GetArg("-rpcxbridgetimeout", 15)));
    stream.connect(rpcip, rpcport);
    if (stream.error() != boost::system::errc::success) {
        LogPrint("net", "Failed to make rpc connection to %s:%s error %d: %s", rpcip, rpcport, stream.error(), stream.error().message());
        throw runtime_error(strprintf("no response from server %s:%s - %s", rpcip.c_str(), rpcport.c_str(),
                                      stream.error().message().c_str()));
    }

    // HTTP basic authentication
    string strUserPass64 = util::base64_encode(rpcuser + ":" + rpcpasswd);
    map<string, string> mapRequestHeaders;
    mapRequestHeaders["Authorization"] = string("Basic ") + strUserPass64;

    // Send request
    string strRequest = JSONRPCRequest(strMethod, params, 1);

    if(fDebug)
        LOG() << "HTTP: req  " << strMethod << " " << strRequest;

    string strPost = HTTPPost(strRequest, mapRequestHeaders);
    stream << strPost << std::flush;

    // Receive reply
    map<string, string> mapHeaders;
    string strReply;
    int nStatus = readHTTP(stream, mapHeaders, strReply);

    if(fDebug)
        LOG() << "HTTP: resp " << nStatus << " " << strReply;

    if (nStatus == HTTP_UNAUTHORIZED)
        throw runtime_error("incorrect rpcuser or rpcpassword (authorization failed)");
    else if (nStatus >= 400 && nStatus != HTTP_BAD_REQUEST && nStatus != HTTP_NOT_FOUND && nStatus != HTTP_INTERNAL_SERVER_ERROR)
        throw runtime_error(strprintf("server returned HTTP error %d", nStatus));
    else if (strReply.empty())
        throw runtime_error("no response from server");

    // Parse reply
    Value valReply;
    if (!read_string(strReply, valReply))
        throw runtime_error("couldn't parse reply from server");
    const Object& reply = valReply.get_obj();
    if (reply.empty())
        throw runtime_error("expected reply to have result, error and id properties");

    return reply;
}

//*****************************************************************************
//*****************************************************************************
bool createFeeTransaction(const std::vector<unsigned char> & dstScript, const double amount,
        const double feePerByte,
        const std::vector<unsigned char> & data,
        std::vector<xbridge::wallet::UtxoEntry> & availUtxos,
        std::set<xbridge::wallet::UtxoEntry> & feeUtxos,
        std::string & rawTx)
{
    if (availUtxos.empty())
        throw std::runtime_error("Create transaction command finished with error, not enough utxos to cover fee");

    LOCK(cs_rpcBlockchainStore);

    const static std::string createCommand("createrawtransaction");
    const static std::string signCommand("signrawtransaction");

    int         errCode = 0;
    std::string errMessage;

    try
    {
        auto estFee = [feePerByte](const uint32_t inputs, const uint32_t outputs) -> double {
            return (192 * inputs + 34 * outputs) * feePerByte;
        };
        auto feeAmount = [&estFee](const double amt, const uint32_t inputs, const uint32_t outputs) -> double {
            return amt + estFee(inputs, outputs);
        };

        // Fee utxo selector
        auto selectFeeUtxos = [&estFee, &feeAmount](std::vector<xbridge::wallet::UtxoEntry> & a,
                                 std::vector<xbridge::wallet::UtxoEntry> & o,
                                 const double amt) -> void
        {
            bool done{false};
            std::vector<xbridge::wallet::UtxoEntry> gt;
            std::vector<xbridge::wallet::UtxoEntry> lt;

            // Check ideal, find input that is larger than min amount and within range
            double minAmount{feeAmount(amt, 1, 3)};
            for (const auto & utxo : a) {
                if (utxo.amount >= minAmount && utxo.amount < minAmount + estFee(1, 3) * 100) {
                    o.push_back(utxo);
                    done = true;
                    break;
                }
                else if (utxo.amount >= minAmount)
                    gt.push_back(utxo);
                else if (utxo.amount < minAmount)
                    lt.push_back(utxo);
            }

            if (done)
                return;

            // Find the smallest input > min amount
            // - or -
            // Find the biggest inputs smaller than min amount that when added is >= min amount
            // - otherwise fail -

            if (gt.size() == 1)
                o.push_back(gt[0]);
            else if (gt.size() > 1) {
                // sort utxos greater than amount (ascending) and pick first
                sort(gt.begin(), gt.end(),
                     [](const xbridge::wallet::UtxoEntry & a, const xbridge::wallet::UtxoEntry & b) {
                         return a.amount < b.amount;
                     });
                o.push_back(gt[0]);
            } else if (lt.size() < 2)
                return; // fail (not enough inputs)
            else {
                // sort inputs less than amount (descending)
                sort(lt.begin(), lt.end(),
                     [](const xbridge::wallet::UtxoEntry & a, const xbridge::wallet::UtxoEntry & b) {
                         return a.amount > b.amount;
                     });

                std::vector<xbridge::wallet::UtxoEntry> sel; // store all selected inputs
                for (const auto & utxo : lt) {
                    sel.push_back(utxo);

                    // Add amount and incorporate fee calc
                    double runningAmount{0};
                    for (auto & u : sel)
                        runningAmount += u.amount;
                    runningAmount -= estFee(sel.size(), 3); // subtract estimated fees

                    if (runningAmount >= minAmount) {
                        o.insert(o.end(), sel.begin(), sel.end()); // only add utxos if we pass threshold
                        break;
                    }
                }
            }
        };

        // Find inputs
        std::vector<xbridge::wallet::UtxoEntry> utxos(availUtxos.begin(), availUtxos.end());
        std::vector<xbridge::wallet::UtxoEntry> selUtxos;

        // Sort available utxos by amount (descending)
        sort(utxos.begin(), utxos.end(),
             [](const xbridge::wallet::UtxoEntry & a, const xbridge::wallet::UtxoEntry & b) {
                 return a.amount > b.amount;
             });

        selectFeeUtxos(utxos, selUtxos, amount);
        if (selUtxos.empty())
            throw std::runtime_error("Create transaction command finished with error, not enough utxos to cover fee");

        // Fee amount
        double inputAmt{0};
        double feeAmt{estFee(selUtxos.size(), 3)};
        std::string changeAddr{selUtxos[0].address};

        Array inputs;
        for (const auto & utxo : selUtxos)
        {
            Object u;
            u.emplace_back("txid", utxo.txId);
            u.emplace_back("vout", static_cast<int>(utxo.vout));
            inputs.push_back(u);
            feeUtxos.insert(utxo);
            inputAmt += utxo.amount;
        }

        // Total change
        double changeAmt = inputAmt - amount - feeAmt;

        Array outputs;

        if (data.size() > 0)
        {
            Object out;
            std::string strdata = HexStr(data.begin(), data.end());
            out.push_back(Pair("data", strdata));
            outputs.push_back(out);
        }

        {
            Object out;
            out.push_back(Pair("script", HexStr(dstScript)));
            out.push_back(Pair("amount", amount));
            outputs.push_back(out);

            if (changeAmt >= 0.0000546) { // BLOCK dust
                Object change;
                change.push_back(Pair("address", changeAddr));
                change.push_back(Pair("amount", changeAmt));
                outputs.push_back(change);
            }
        }

        Value result;

        {
            Array params;
            params.push_back(inputs);
            params.push_back(outputs);

            // call create
            result = tableRPC.execute(createCommand, params);
            if (result.type() != str_type)
            {
                throw std::runtime_error("Create transaction command finished with error");
            }

            rawTx = result.get_str();
        }

        {
            std::vector<std::string> params;
            params.push_back(rawTx);

            result = tableRPC.execute(signCommand, RPCConvertValues(signCommand, params));
            if (result.type() != obj_type)
            {
                throw std::runtime_error("Sign transaction command finished with error");
            }

            Object obj = result.get_obj();
            const Value  & tx = find_value(obj, "hex");
            const Value & cpl = find_value(obj, "complete");

            if (tx.type() != str_type || cpl.type() != bool_type || !cpl.get_bool())
            {
                throw std::runtime_error("Sign transaction error or not completed");
            }

            rawTx = tx.get_str();
            LOG() << "feetx " << rawTx;
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
        TXERR() << "feetx fundrawtransaction " << rawTx;
        LOG() << "failed to fund the fee transaction " << errCode << " " << errMessage << " " << __FUNCTION__;
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool storeDataIntoBlockchain(const std::string & rawTx, std::string & txid)
{
    LOCK(cs_rpcBlockchainStore);

    const static std::string sendCommand("sendrawtransaction");

    int         errCode = 0;
    std::string errMessage;

    try
    {
        {
            std::vector<std::string> params;
            params.push_back(rawTx);

            Value result = tableRPC.execute(sendCommand, RPCConvertValues(sendCommand, params));
            if (result.type() != str_type)
            {
                throw std::runtime_error("Send transaction command finished with error");
            }

            txid = result.get_str();
        }

        TXLOG() << "feetx sendrawtransaction " << rawTx;
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
        TXERR() << "feetx sendrawtransaction " << rawTx;
        LOG() << "error sending the fee transaction, code " << errCode << " " << errMessage << " " << __FUNCTION__;
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool unspentP2PKH(std::vector<xbridge::wallet::UtxoEntry> & utxos)
{
    vector<COutput> coins;
    pwalletMain->AvailableCoins(coins, false);

    int nMinDepth = 1;
    int nMaxDepth = 9999999;

    std::map<COutPoint, CInPoint> utxoUnusable;
    {
        LOCK(mempool.cs); // protect mempool.mapNextTx
        utxoUnusable = mempool.mapNextTx;
    }

    for (const COutput & out : coins) {
        if (out.nDepth < nMinDepth || out.nDepth > nMaxDepth || !out.fSpendable)
                continue;

        bool bad{false};
        // utxo not already used in mempool
        for (unsigned int i = 0; i < out.tx->vin.size(); ++i) {
            COutPoint outpoint = out.tx->vin[i].prevout;
            if (utxoUnusable.count(outpoint)) {
                bad = true;
                break;
            }
        }
        if (bad)
            continue;

        wallet::UtxoEntry utxo;

        // Only support p2pkh (like 76a91476bba472620ff0ecbfbf93d0d3909c6ca84ac81588ac)
        const CScript & pk = out.tx->vout[out.i].scriptPubKey;
        std::vector<unsigned char> script(pk.begin(), pk.end());
        if (script.size() == 25 &&
            script[0] == 0x76 && script[1] == 0xa9 && script[2] == 0x14 &&
            script[23] == 0x88 && script[24] == 0xac)
        {
            CTxDestination address;
            if (!ExtractDestination(pk, address))
                continue;

            utxo.address = CBitcoinAddress(address).ToString();
            utxo.scriptPubKey = HexStr(pk.begin(), pk.end());

        } else continue; // ignore unsupported addresses (p2sh, p2pk, etc)

        utxo.txId = out.tx->GetHash().GetHex();
        utxo.vout = out.i;
        utxo.amount = static_cast<double>(out.tx->vout[out.i].nValue) / static_cast<double>(COIN);

        utxos.push_back(utxo);
    }

    return !utxos.empty();
}

//*****************************************************************************
//*****************************************************************************
bool unspentP2PKH_rpc(std::vector<xbridge::wallet::UtxoEntry> & utxos)
{
    LOCK(cs_rpcBlockchainStore);

    int         errCode = 0;
    std::string errMessage;
    std::vector<wallet::UtxoEntry> entries;

    try
    {
        const std::string unspentCommand("listunspent");
        Value result = tableRPC.execute(unspentCommand, RPCConvertValues(unspentCommand, std::vector<std::string>()));
        if (result.type() != array_type)
            throw std::runtime_error("feetx listunspent failed");

        Array & arr = result.get_array();
        for (const Value & utxo : arr)
        {
            if (utxo.type() != obj_type)
                continue;

            Object o = utxo.get_obj();

            // Check if spendable
            const Value & spendable = find_value(o, "spendable");
            if (spendable.type() == bool_type && !spendable.get_bool())
                continue;

            // Check if confirmations >= 1
            const Value & confirmations = find_value(o, "confirmations");
            if (confirmations.type() == int_type && confirmations.get_int() < 1)
                continue;

            // store utxo entry
            wallet::UtxoEntry u;
            u.txId = find_value(o, "txid").get_str();
            u.vout = find_value(o, "vout").get_int();
            u.amount = find_value(o, "amount").get_real();
            u.scriptPubKey = find_value(o, "scriptPubKey").get_str();
            u.address = find_value(o, "address").get_str();

            if (!u.txId.empty() && u.amount > 0)
                entries.push_back(u);
        }
    }
    catch (std::runtime_error & e) {
        // specified error
        errCode = -1;
        errMessage = e.what();
    }
    catch (...) {
        errCode = -1;
        errMessage = "unknown error";
    }

    if (errCode != 0) {
        TXERR() << "feetx listunspent failed " << __FUNCTION__;
        LOG() << "feetx listunspent failed " << errCode << " " << errMessage << " " << __FUNCTION__;
        return false;
    }

    // Validate utxos
    for (auto & entry : entries) {
        // Only support p2pkh (like 76a91476bba472620ff0ecbfbf93d0d3909c6ca84ac81588ac)
        std::vector<unsigned char> script = ParseHex(entry.scriptPubKey);
        if (script.size() == 25 &&
            script[0] == 0x76 && script[1] == 0xa9 && script[2] == 0x14 &&
            script[23] == 0x88 && script[24] == 0xac)
        {
            CTxDestination addr;
            if (!ExtractDestination(CScript(script.begin(), script.end()), addr))
                continue;

            entry.address = CBitcoinAddress(addr).ToString();
            utxos.push_back(entry);
        }
    }

    return !utxos.empty();
}

} // namespace rpc
} // namespace xbridge
