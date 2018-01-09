#include "s3downloader.h"

using boost::asio::ip::tcp;
namespace ssl = boost::asio::ssl;

std::shared_ptr<S3Downloader> S3Downloader::create(boost::function<void (const std::list<std::string> &, const std::string)> cb,
                                                   const std::string &host,
                                                   const std::string &path)
{
    ssl::context context(ssl::context::tlsv12);
    context.set_default_verify_paths();
    context.set_options(ssl::context::default_workarounds
                        | ssl::context::no_sslv2
                        | ssl::context::no_sslv3
                        | ssl::context::no_tlsv1
//                        | ssl::context::no_tlsv1_1
                        | ssl::context::single_dh_use);

    return std::shared_ptr<S3Downloader>(new S3Downloader(cb, host, path, context));
}

S3Downloader::S3Downloader(boost::function<void (const std::list<std::string> &, const std::string)> cb,
                           const std::string &host,
                           const std::string &path,
                           ssl::context &context)
    : resolver(ioService)
    , socket(ioService, context)
    , deadline(ioService)
    , host(host)
    , path(path)
    , callback(cb)
{
    socket.set_verify_mode(ssl::verify_peer | ssl::verify_fail_if_no_peer_cert);
    SSL_set_tlsext_host_name(socket.native_handle(), host.c_str());
    socket.set_verify_callback(boost::bind(&S3Downloader::verifyCertificate, this, _1, _2));
}

void S3Downloader::downloadList(boost::posix_time::time_duration timeout)
{
    deadline.expires_from_now(timeout);

    tcp::resolver::query query(host, "https");

    std::ostream requestStream(&request);
    requestStream << "GET " << path << " HTTP/1.1\r\n";
    requestStream << "Host: " << host << "\r\n";
    requestStream << "Accept: */*\r\n";
    requestStream << "Cache-Control: no-cache\r\n";
    requestStream << "Connection: close\r\n\r\n";

    resolver.async_resolve(query,
                           boost::bind(&S3Downloader::handleResolve, this,
                                       boost::asio::placeholders::error,
                                       boost::asio::placeholders::iterator));

    checkDeadline();

    ioService.run();
}

void S3Downloader::handleResolve(const boost::system::error_code &error, tcp::resolver::iterator endpointIterator)
{
    if (error)
    {
        callback(std::list<std::string>(), error.message());
        close();
        return;
    }

    boost::asio::async_connect(socket.lowest_layer(), endpointIterator,
                               boost::bind(&S3Downloader::handleConnect, this,
                                           boost::asio::placeholders::error));
}

void S3Downloader::handleConnect(const boost::system::error_code &error)
{
    if (error)
    {
        callback(std::list<std::string>(), error.message());
        close();
        return;
    }

    socket.async_handshake(boost::asio::ssl::stream_base::client,
                           boost::bind(&S3Downloader::handleHandshake, this,
                                       boost::asio::placeholders::error));
}

void S3Downloader::handleHandshake(const boost::system::error_code &error)
{
    if (error)
    {
        callback(std::list<std::string>(), error.message());
        close();
        return;
    }

    //std::cout << "Handshake OK " << "\n";
    //std::cout << "Request: " << "\n";
    const char* header = boost::asio::buffer_cast<const char*>(request.data());
    //std::cout << header << "\n";

    // The handshake was successful. Send the request.
    boost::asio::async_write(socket, request,
                             boost::bind(&S3Downloader::handleWriteRequest, this,
                                         boost::asio::placeholders::error));
}

void S3Downloader::handleWriteRequest(const boost::system::error_code &error)
{
    if (error)
    {
        callback(std::list<std::string>(), error.message());
        close();
        return;
    }

    // Read the response status line. The response streambuf will
    // automatically grow to accommodate the entire line. The growth may be
    // limited by passing a maximum size to the streambuf constructor.
    boost::asio::async_read_until(socket, response, "\r\n",
                                  boost::bind(&S3Downloader::handleReadStatusLine, this,
                                              boost::asio::placeholders::error));
}

void S3Downloader::handleReadStatusLine(const boost::system::error_code &error)
{
    if (error)
    {
        callback(std::list<std::string>(), error.message());
        close();
        return;
    }

    // Check that response is OK.
    std::istream response_stream(&response);
    std::string http_version;
    response_stream >> http_version;
    unsigned int status_code;
    response_stream >> status_code;
    std::string status_message;
    std::getline(response_stream, status_message);
    if (!response_stream || http_version.substr(0, 5) != "HTTP/")
    {
        callback(std::list<std::string>(), "Invalid response");
        close();
        return;
    }
    if (status_code != 200)
    {
        callback(std::list<std::string>(), "Response returned with status code: " + std::to_string(status_code));
        close();
        return;
    }

    // Read the response headers, which are terminated by a blank line.
    boost::asio::async_read_until(socket, response, "\r\n\r\n",
                                  boost::bind(&S3Downloader::handleReadHeaders, this,
                                              boost::asio::placeholders::error));
}

void S3Downloader::handleReadHeaders(const boost::system::error_code &error)
{
    if (error)
    {
        callback(std::list<std::string>(), error.message());
        close();
        return;
    }

    // Process the response headers.
    std::istream response_stream(&response);
    std::string header;
    while (std::getline(response_stream, header) && header != "\r")
        /*std::cout << header << "\n";*/true;
//    std::cout << "\n";

    // Start reading remaining data until EOF.
    boost::asio::async_read(socket, response,
                            boost::asio::transfer_at_least(1),
                            boost::bind(&S3Downloader::handleReadContent, this,
                                        boost::asio::placeholders::error));
}

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    static_assert(false, "fix for openssl 1.1");
#endif

void S3Downloader::handleReadContent(const boost::system::error_code &error)
{
    if (error
            && error != boost::asio::error::eof
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
#else
            && error != boost::system::error_code(ERR_PACK(ERR_LIB_SSL, 0, SSL_R_SHORT_READ), boost::asio::error::get_ssl_category())
#endif
       )
    {
        callback(std::list<std::string>(), error.message());
        close();
        return;
    }
    else if (!error)
    {
        // Continue reading remaining data until EOF.
        boost::asio::async_read(socket, response,
                                boost::asio::transfer_at_least(1),
                                boost::bind(&S3Downloader::handleReadContent, this,
                                            boost::asio::placeholders::error));
    }
    else
    {
        std::string line;
        std::list<std::string> list;
        std::istream is(&response);
        while (std::getline(is, line))
        {
            if(line == "\n" || line == "signature")
                continue;

            list.push_back(line);
        }

//        std::string signatureString = list.back();
//        list.pop_back();
//        std::vector<unsigned char> signature(signatureString.begin(), signatureString.end());
//        std::string joinedList = boost::algorithm::join(list, "\n");
//
//        if(!verifySign(joinedList, signature))
//        {
//            callback(std::list<std::string>(), "Signature verifying fail");
//            close();
//            return;
//        }

        callback(list, "");
        close();
    }
}

bool S3Downloader::verifyCertificate(bool preverified, ssl::verify_context &ctx)
{
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    static_assert(false, "fix for openssl 1.1");
#else

    int8_t subject_name[256];
    X509_STORE_CTX *cts = ctx.native_handle();

    X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
    if (cts->error)
        std::cout << "CTX ERROR: " << cts->error << std::endl;

    int32_t depth = X509_STORE_CTX_get_error_depth(cts);
    if (cts->error)
        std::cout << "CTX DEPTH: " << depth << std::endl;

    bool rfc2818check = ssl::rfc2818_verification(host)(preverified, ctx);
    if (cts->error)
        std::cout << "RFC 2818 CHECK: " << rfc2818check << std::endl;

    const int32_t name_length = 256;
    X509_NAME_oneline(X509_get_subject_name(cert), reinterpret_cast<char*>(subject_name), name_length);
#endif
    return true;
}

//bool S3Downloader::verifySign(const std::string &list, std::vector<unsigned char> &vchSign)
//{
//    //CPubKey publicKey(ParseHex(Params().SporkKey()));
//    //TODO: delete after adding correct privkey/pubkey pair
//    CPubKey publicKey(ParseHex("04fb6d1940ce071c0f43d33db85b381ad2a761cb7e23be979153fabdcfe8d991a129df68801e84fd559ed3a675a94d6c31aad1bcb64a2d72d11d4b806582a482b5"));
//
//    if(!publicKey.IsValid())
//        return false;
//
//    std::string errorMessage = "";
//    if (!obfuScationSigner.VerifyMessage(publicKey, vchSign, list, errorMessage))
//        return false;
//
//    return true;
//}

void S3Downloader::checkDeadline()
{
    if (deadline.expires_at() <= boost::asio::deadline_timer::traits_type::now())
    {
        // The deadline has passed. The socket is closed so that any outstanding
        // asynchronous operations are cancelled.
        callback(std::list<std::string>(), "Downloader: Timeout exceeded, connection closed");
        close();
        return;
    }

    deadline.async_wait(boost::bind(&S3Downloader::checkDeadline, this));
}

boost::system::error_code S3Downloader::close() {
    boost::system::error_code error;
    if (!ioService.stopped()) {
        socket.lowest_layer().close(error);
        ioService.stop();
    }
    return error;
}
