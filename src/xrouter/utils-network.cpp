// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <xrouter/xrouterutils.h>

#include <xrouter/xrouterdef.h>

#include <event2/buffer.h>
#include <rpc/protocol.h>
#include <support/events.h>
#include <tinyformat.h>
#include <util/strencodings.h>
#include <util/system.h>
#include <univalue.h>

#include <array>
#include <stdio.h>

#include <json/json_spirit_writer_template.h>

//*****************************************************************************
//*****************************************************************************
namespace xrouter
{

/** Reply structure for request_done to fill in */
struct HTTPReply
{
    HTTPReply(): status(0), error(-1) {}

    int status;
    int error;
    CPubKey hdrpubkey;
    std::vector<unsigned char> hdrsignature;
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

    auto headers = evhttp_request_get_input_headers(req);
    if (headers) {
        std::string hdrPubKey = "XR-Pubkey";
        std::string hdrSignature = "XR-Signature";
        auto hdrPubKeyVal = evhttp_find_header(headers, hdrPubKey.c_str());
        if (hdrPubKeyVal)
            reply->hdrpubkey = CPubKey(ParseHex(hdrPubKeyVal));
        auto hdrSignatureVal = evhttp_find_header(headers, hdrSignature.c_str());
        if (hdrSignatureVal)
            reply->hdrsignature = ParseHex(hdrSignatureVal);
    }
}

#if LIBEVENT_VERSION_NUMBER >= 0x02010300
static void http_error_cb(enum evhttp_request_error err, void *ctx)
{
    HTTPReply *reply = static_cast<HTTPReply*>(ctx);
    reply->error = err;
}
#endif

UniValue XRouterJSONRPCRequestObj(const std::string& strMethod, const UniValue& params,
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

std::string CallRPC(const std::string & rpcip, const std::string & rpcport, const std::string & strMethod,
                    const Array & params, const std::string & jsonver)
{
    return std::move(CallRPC("", "", rpcip, rpcport, strMethod, params, jsonver));
}

std::string CallRPC(const std::string & rpcuser, const std::string & rpcpasswd,
                      const std::string & rpcip, const std::string & rpcport,
                      const std::string & strMethod, const json_spirit::Array & params,
                      const std::string & jsonver)
{
    const std::string & host = rpcip;
    const int port = stoi(rpcport);

    // Obtain event base
    raii_event_base base = obtain_event_base();

    // Synchronously look up hostname
    raii_evhttp_connection evcon = obtain_evhttp_connection_base(base.get(), host, port);
    evhttp_connection_set_timeout(evcon.get(), gArgs.GetArg("-rpcxroutertimeout", 60));

    HTTPReply response;
    raii_evhttp_request req = obtain_evhttp_request(http_request_done, (void*)&response);
    if (req == nullptr)
        throw std::runtime_error("create http request failed");
#if LIBEVENT_VERSION_NUMBER >= 0x02010300
    evhttp_request_set_error_cb(req.get(), http_error_cb);
#endif

    // Get credentials
    std::string strRPCUserColonPass = rpcuser + ":" + rpcpasswd;

    struct evkeyvalq* output_headers = evhttp_request_get_output_headers(req.get());
    assert(output_headers);
    evhttp_add_header(output_headers, "Host", host.c_str());
    evhttp_add_header(output_headers, "Connection", "close");
    evhttp_add_header(output_headers, "Authorization", (std::string("Basic ") + EncodeBase64(strRPCUserColonPass)).c_str());

    // Attach request data
    const auto tostring = json_spirit::write_string(json_spirit::Value(params), json_spirit::none, 8);
    UniValue toval;
    if (!toval.read(tostring))
        throw std::runtime_error(strprintf("failed to decode json_spirit data: %s", tostring));
    const auto reqobj = XRouterJSONRPCRequestObj(strMethod, toval.get_array(), 1, jsonver);
    std::string strRequest = reqobj.write() + "\n";
    struct evbuffer* output_buffer = evhttp_request_get_output_buffer(req.get());
    assert(output_buffer);
    evbuffer_add(output_buffer, strRequest.data(), strRequest.size());

    int r = evhttp_make_request(evcon.get(), req.get(), EVHTTP_REQ_POST, "/");
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

    return response.body;
}

XRouterReply CallXRouterUrl(const std::string & host, const int & port, const std::string & url, const std::string & data,
                    const int & timeout, const CKey & signingkey, const CPubKey & serverkey, const std::string & paymentrawtx)
{
    // Obtain event base
    raii_event_base base = obtain_event_base();

    // Synchronously look up hostname
    raii_evhttp_connection evcon = obtain_evhttp_connection_base(base.get(), host, port);
    evhttp_connection_set_timeout(evcon.get(), timeout > 0 ? timeout
                                                           : static_cast<int>(gArgs.GetArg("-rpcxroutertimeout", 60)));

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
    evhttp_add_header(output_headers, "XR-Pubkey", HexStr(signingkey.GetPubKey()).c_str());

    CHashWriter hw(SER_GETHASH, 0);
    hw << data;
    std::vector<unsigned char> signature;
    if (!signingkey.SignCompact(hw.GetHash(), signature))
        throw std::runtime_error("failed to produce signature on payload");
    evhttp_add_header(output_headers, "XR-Signature", HexStr(signature).c_str());
    evhttp_add_header(output_headers, "XR-Payment", paymentrawtx.c_str());

    // Attach request data
    std::string strRequest = data + "\n";
    struct evbuffer *output_buffer = evhttp_request_get_output_buffer(req.get());
    if (!output_buffer)
        throw std::runtime_error(strprintf("Internal error in connection to server %s:%d failed to set headers\n", host, port));
    evbuffer_add(output_buffer, strRequest.data(), strRequest.size());

    int r = evhttp_make_request(evcon.get(), req.get(), EVHTTP_REQ_POST, url.c_str());
    req.release(); // ownership moved to evcon in above call
    if (r != 0)
        throw std::runtime_error("send http request failed");

    event_base_dispatch(base.get());

    if (response.status == 0) {
        std::string responseErrorMessage;
        if (response.error != -1) {
            responseErrorMessage = strprintf(" (error code %d - \"%s\")", response.error, http_errorstring(response.error));
        }
        throw std::runtime_error(strprintf("Could not connect to the server %s:%d %s\n", host, port, responseErrorMessage));
    } else if (response.status == HTTP_UNAUTHORIZED) {
        throw std::runtime_error("Authorization failed");
    } else if (response.status >= 400 && response.status != HTTP_BAD_REQUEST && response.status != HTTP_NOT_FOUND && response.status != HTTP_INTERNAL_SERVER_ERROR) {
        throw std::runtime_error(strprintf("server returned HTTP error %d", response.status));
    } else if (response.body.empty()) {
        throw std::runtime_error("no response from server");
    }

    XRouterReply reply;
    reply.status = response.status;
    reply.error = response.error;
    reply.result = response.body;
    reply.hdrpubkey = response.hdrpubkey;
    reply.hdrsignature = response.hdrsignature;
    return std::move(reply);
}

std::string CallCMD(const std::string & cmd, int & exit) {
    std::array<char, 128> buffer{};
    FILE *pipe = popen(std::string(cmd + " 2>&1").c_str(), "r");
    if (!pipe)
        throw std::runtime_error("popen() failed!");

    std::string result;
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    auto n = pclose(pipe);

#ifdef WIN32
    exit = n & 0xff;
#else
    exit = WEXITSTATUS(n);
#endif

    return result;
}

} // namespace xrouter
