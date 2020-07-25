// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//*****************************************************************************
//*****************************************************************************

#ifndef BLOCKNET_XBRIDGE_XBRIDGEWALLETCONNECTORBTC_H
#define BLOCKNET_XBRIDGE_XBRIDGEWALLETCONNECTORBTC_H

#include <xbridge/xbridgewalletconnector.h>

#include <event2/buffer.h>
#include <rpc/protocol.h>
#include <rpc/client.h>
#include <support/events.h>
#include <tinyformat.h>
#include <util/strencodings.h>
#include <util/system.h>
#include <univalue.h>

#include <memory>

#include <json/json_spirit.h>
#include <json/json_spirit_reader_template.h>
#include <json/json_spirit_writer_template.h>

#include <boost/lexical_cast.hpp>

//*****************************************************************************
//*****************************************************************************
namespace xbridge
{
    /** Reply structure for request_done to fill in */
    struct HTTPReply
    {
        HTTPReply(): status(0), error(-1) {}

        int status;
        int error;
        std::string body;
    };

    static const char *http_errorstring(int code)
    {
        switch(code) {
    #if LIBEVENT_VERSION_NUMBER >= 0x02010300
        case EVREQ_HTTP_TIMEOUT:
            return "timeout reached";
        case EVREQ_HTTP_EOF:
            return "EOF reached";
        case EVREQ_HTTP_INVALID_HEADER:
            return "error while reading header, or invalid header";
        case EVREQ_HTTP_BUFFER_ERROR:
            return "error encountered while reading or writing";
        case EVREQ_HTTP_REQUEST_CANCEL:
            return "request was canceled";
        case EVREQ_HTTP_DATA_TOO_LONG:
            return "response body is larger than allowed";
    #endif
        default:
            return "unknown";
        }
    }

    static void http_request_done(struct evhttp_request *req, void *ctx)
    {
        HTTPReply *reply = static_cast<HTTPReply*>(ctx);

        if (req == nullptr) {
            /* If req is nullptr, it means an error occurred while connecting: the
             * error code will have been passed to http_error_cb.
             */
            reply->status = 0;
            return;
        }

        reply->status = evhttp_request_get_response_code(req);

        struct evbuffer *buf = evhttp_request_get_input_buffer(req);
        if (buf)
        {
            size_t size = evbuffer_get_length(buf);
            const char *data = (const char*)evbuffer_pullup(buf, size);
            if (data)
                reply->body = std::string(data, size);
            evbuffer_drain(buf, size);
        }
    }

#if LIBEVENT_VERSION_NUMBER >= 0x02010300
    static void http_error_cb(enum evhttp_request_error err, void *ctx)
    {
        HTTPReply *reply = static_cast<HTTPReply*>(ctx);
        reply->error = err;
    }
#endif

static UniValue XBridgeJSONRPCRequestObj(const std::string& strMethod, const UniValue& params,
        const UniValue& id, const std::string& jsonver="")
{
    UniValue request(UniValue::VOBJ);
    if (!jsonver.empty())
        request.pushKV("jsonrpc", jsonver);
    request.pushKV("method", strMethod);
    request.pushKV("params", params);
    request.pushKV("id", id);
    return request;
}

static json_spirit::Object CallRPC(const std::string & rpcuser, const std::string & rpcpasswd,
                      const std::string & rpcip, const std::string & rpcport,
                      const std::string & strMethod, const json_spirit::Array & params,
                      const std::string & jsonver="", const std::string & contenttype="")
{
    const std::string & host = rpcip;
    const int port = boost::lexical_cast<int>(rpcport);

    // Obtain event base
    raii_event_base base = obtain_event_base();

    // Synchronously look up hostname
    raii_evhttp_connection evcon = obtain_evhttp_connection_base(base.get(), host, port);
    evhttp_connection_set_timeout(evcon.get(), gArgs.GetArg("-rpcxbridgetimeout", 120));

    HTTPReply response;
    raii_evhttp_request req = obtain_evhttp_request(http_request_done, (void*)&response);
    if (req == nullptr)
        throw std::runtime_error("create http request failed");
#if LIBEVENT_VERSION_NUMBER >= 0x02010300
    evhttp_request_set_error_cb(req.get(), http_error_cb);
#endif

    struct evkeyvalq* output_headers = evhttp_request_get_output_headers(req.get());
    assert(output_headers);
    evhttp_add_header(output_headers, "Host", host.c_str());
    evhttp_add_header(output_headers, "Connection", "close");
    // Set content type
    if (!contenttype.empty())
        evhttp_add_header(output_headers, "Content-Type", contenttype.c_str());
    // Set credentials
    if (!rpcuser.empty() || !rpcpasswd.empty()) {
        std::string strRPCUserColonPass = rpcuser + ":" + rpcpasswd;
        evhttp_add_header(output_headers, "Authorization", (std::string("Basic ") + EncodeBase64(strRPCUserColonPass)).c_str());
    }

    // Attach request data
    const auto tostring = json_spirit::write_string(json_spirit::Value(params), json_spirit::none, 8);
    UniValue toval;
    if (!toval.read(tostring))
        throw std::runtime_error(strprintf("failed to decode json_spirit data: %s", tostring));
    const auto reqobj = XBridgeJSONRPCRequestObj(strMethod, toval.get_array(), 1, jsonver);
    std::string strRequest = reqobj.write() + "\n";
    struct evbuffer* output_buffer = evhttp_request_get_output_buffer(req.get());
    assert(output_buffer);
    evbuffer_add(output_buffer, strRequest.data(), strRequest.size());

    // check if we should use a special wallet endpoint
    std::string endpoint = "/";
    int r = evhttp_make_request(evcon.get(), req.get(), EVHTTP_REQ_POST, endpoint.c_str());
    req.release(); // ownership moved to evcon in above call
    if (r != 0) {
        throw std::runtime_error("send http request failed");
    }

    event_base_dispatch(base.get());

    if (response.status == 0) {
        std::string responseErrorMessage;
        if (response.error != -1) {
            responseErrorMessage = strprintf(" (error code %d - \"%s\")", response.error, http_errorstring(response.error));
        }
        throw std::runtime_error(strprintf("Could not connect to the server %s:%d%s\n\nMake sure the blocknetd server is running and that you are connecting to the correct RPC port.", host, port, responseErrorMessage));
    } else if (response.status == HTTP_UNAUTHORIZED) {
        throw std::runtime_error("Authorization failed: Incorrect rpcuser or rpcpassword");
    } else if (response.status >= 400 && response.status != HTTP_BAD_REQUEST && response.status != HTTP_NOT_FOUND && response.status != HTTP_INTERNAL_SERVER_ERROR)
        throw std::runtime_error(strprintf("server returned HTTP error %d", response.status));
    else if (response.body.empty())
        throw std::runtime_error("no response from server");

    // Parse reply
    json_spirit::Value valReply;
    if (!json_spirit::read_string(response.body, valReply))
        throw std::runtime_error("couldn't parse reply from server");
    const json_spirit::Object& reply = valReply.get_obj();
    if (reply.empty())
        throw std::runtime_error("expected reply to have result, error and id properties");

    return reply;
}

//*****************************************************************************
//*****************************************************************************
template <class CryptoProvider>
class BtcWalletConnector : public WalletConnector
{
    class Impl;

public:
    BtcWalletConnector() {}

    bool init();

public:
    // reimplement for currency
    std::string fromXAddr(const std::vector<unsigned char> & xaddr) const;
    std::vector<unsigned char> toXAddr(const std::string & addr) const;

public:
    bool requestAddressBook(std::vector<wallet::AddressBookEntry> & entries);

    bool getInfo(rpc::WalletInfo & info) const;

    bool getUnspent(std::vector<wallet::UtxoEntry> & inputs, const std::set<wallet::UtxoEntry> & excluded) const;

    bool getNewAddress(std::string & addr);

    bool getTxOut(wallet::UtxoEntry & entry);

    bool sendRawTransaction(const std::string & rawtx,
                            std::string & txid,
                            int32_t & errorCode,
                            std::string & message);

    bool signMessage(const std::string & address, const std::string & message, std::string & signature);
    bool verifyMessage(const std::string & address, const std::string & message, const std::string & signature);

    bool getRawMempool(std::vector<std::string> & txids);

    bool getBlock(const std::string & blockHash, std::string & rawBlock);

    bool getBlockHash(const uint32_t & block, std::string & blockHash);

    bool getBlockCount(uint32_t & blockCount);

public:
    bool hasValidAddressPrefix(const std::string & addr) const;
    bool isValidAddress(const std::string & addr) const;

    bool isDustAmount(const double & amount) const;

    bool newKeyPair(std::vector<unsigned char> & pubkey, std::vector<unsigned char> & privkey);

    std::vector<unsigned char> getKeyId(const std::vector<unsigned char> & pubkey);
    std::vector<unsigned char> getScriptId(const std::vector<unsigned char> & script);
    std::string scriptIdToString(const std::vector<unsigned char> & id) const;

    double minTxFee1(const uint32_t inputCount, const uint32_t outputCount) const;
    double minTxFee2(const uint32_t inputCount, const uint32_t outputCount) const;

    bool checkDepositTransaction(const std::string & depositTxId,
                                 const std::string & /*destination*/,
                                 double & amount,
                                 uint64_t & p2shAmount,
                                 uint32_t & depositTxVout,
                                 const std::string & expectedScript,
                                 double & excessAmount,
                                 bool & isGood);

    bool getSecretFromPaymentTransaction(const std::string & paymentTxId,
                                         const std::string & depositTxId,
                                         const uint32_t & depositTxVOut,
                                         const std::vector<unsigned char> & hx,
                                         std::vector<unsigned char> & secret,
                                         bool & isGood);

    uint32_t lockTime(const char role) const;

    bool acceptableLockTimeDrift(const char role, const uint32_t lckTime) const;

    bool createDepositUnlockScript(const std::vector<unsigned char> & myPubKey,
                                   const std::vector<unsigned char> & otherPubKey,
                                   const std::vector<unsigned char> & secretHash,
                                   const uint32_t lockTime,
                                   std::vector<unsigned char> & resultSript);

    bool createDepositTransaction(const std::vector<XTxIn> & inputs,
                                  const std::vector<std::pair<std::string, double> > & outputs,
                                  std::string & txId,
                                  uint32_t & txVout,
                                  std::string & rawTx);

    bool createRefundTransaction(const std::vector<XTxIn> & inputs,
                                 const std::vector<std::pair<std::string, double> > & outputs,
                                 const std::vector<unsigned char> & mpubKey,
                                 const std::vector<unsigned char> & mprivKey,
                                 const std::vector<unsigned char> & innerScript,
                                 const uint32_t lockTime,
                                 std::string & txId,
                                 std::string & rawTx);

    bool createPaymentTransaction(const std::vector<XTxIn> & inputs,
                                  const std::vector<std::pair<std::string, double> > & outputs,
                                  const std::vector<unsigned char> & mpubKey,
                                  const std::vector<unsigned char> & mprivKey,
                                  const std::vector<unsigned char> & xpubKey,
                                  const std::vector<unsigned char> & innerScript,
                                  std::string & txId,
                                  std::string & rawTx);

    bool createPartialTransaction(const std::vector<XTxIn> inputs,
                              const std::vector<std::pair<std::string, double> > outputs,
                              std::string & txId,
                              std::string & rawTx) override;

    bool splitUtxos(CAmount splitAmount, std::string addr, bool includeFees, std::set<wallet::UtxoEntry> excluded,
                    std::set<COutPoint> utxos, CAmount & totalSplit, CAmount & splitIncFees, int & splitCount,
                    std::string & txId, std::string & rawTx, std::string & failReason) override;

    bool isUTXOSpentInTx(const std::string & txid, const std::string & utxoPrevTxId,
                         const uint32_t & utxoVoutN, bool & isSpent);

    bool getTransactionsInBlock(const std::string & blockHash, std::vector<std::string> & txids);

protected:
    CryptoProvider m_cp;
};

} // namespace xbridge

#endif // BLOCKNET_XBRIDGE_XBRIDGEWALLETCONNECTORBTC_H
