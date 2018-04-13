
#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

#include <boost/asio.hpp>
//#include <boost/asio/ip/v6_only.hpp>
//#include <boost/bind.hpp>
//#include <boost/filesystem.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/algorithm/string.hpp>
//#include <boost/lexical_cast.hpp>
#include <boost/asio/ssl.hpp>
//#include <boost/filesystem/fstream.hpp>
//#include <boost/shared_ptr.hpp>
//#include <list>
#include <stdio.h>

#include "bitcoinrpc.h"
#include "bignum.h"
#include "util/util.h"
#include "util/logger.h"
#include "httpstatuscode.h"
#include "xbridge/xbridgerpc.h"
#define HTTP_DEBUG

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

static const char* pszBase58 = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

//******************************************************************************
//******************************************************************************
Object CallRPC(const std::string auth, const std::string & rpcip, const std::string & rpcport,
               const std::string & strMethod, const Array & params)
{
//    if (mapArgs["-rpcuser"] == "" && mapArgs["-rpcpassword"] == "")
//        throw runtime_error(strprintf(
//            _("You must set rpcpassword=<password> in the configuration file:\n%s\n"
//              "If the file does not exist, create it with owner-readable-only file permissions."),
//                GetConfigFile().string().c_str()));

    // Connect to localhost
    bool fUseSSL = false;//GetBoolArg("-rpcssl");
    asio::io_service io_service;
    ssl::context context(io_service, ssl::context::sslv23);
    context.set_options(ssl::context::no_sslv2);
    asio::ssl::stream<asio::ip::tcp::socket> sslStream(io_service, context);
    SSLIOStreamDevice<asio::ip::tcp> d(sslStream, fUseSSL);
    iostreams::stream< SSLIOStreamDevice<asio::ip::tcp> > stream(d);
    if (!d.connect(rpcip, rpcport))
        throw runtime_error("couldn't connect to server");

    // HTTP basic authentication
    string strUserPass64 = util::base64_encode(rpcuser + ":" + rpcpasswd);
    map<string, string> mapRequestHeaders;
    mapRequestHeaders["Authorization"] = string("Basic ") + auth;

    // Send request
    string strRequest = JSONRPCRequest(strMethod, params, 1);

#ifdef HTTP_DEBUG
    LOG() << "HTTP: req  " << strMethod << " " << strRequest;
#endif

    string strPost = HTTPPost(strRequest, mapRequestHeaders);
    stream << strPost << std::flush;

    // Receive reply
    map<string, string> mapHeaders;
    string strReply;
    int nStatus = readHTTP(stream, mapHeaders, strReply);

#ifdef HTTP_DEBUG
    LOG() << "HTTP: resp " << nStatus << " " << strReply;
#endif

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

} // namespace rpc
