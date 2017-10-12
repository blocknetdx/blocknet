#include "s3downloader.h"

bool downloadBlackList(std::list<std::string> blackList, const std::string &host, const std::string &get)
{
    using boost::asio::ip::tcp;
    namespace ssl = boost::asio::ssl;

    auto cb = [&host](bool preverified, ssl::verify_context& ctx) {
        int8_t subject_name[256];
        X509_STORE_CTX *cts = ctx.native_handle();

        X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
        std::cout << "CTX ERROR: " << cts->error << std::endl;

        int32_t depth = X509_STORE_CTX_get_error_depth(cts);
        std::cout << "CTX DEPTH: " << depth << std::endl;

        bool rfc2818check = ssl::rfc2818_verification(host)(preverified, ctx);
        std::cout << "RFC 2818 CHECK: " << rfc2818check << std::endl;

        const int32_t name_length = 256;
        X509_NAME_oneline(X509_get_subject_name(cert), reinterpret_cast<char*>(subject_name), name_length);

        return true;
    };

    try {
        boost::asio::io_service ioService;
        // Get a list of endpoints corresponding to the server name.
        tcp::resolver resolver(ioService);
        tcp::resolver::query query(host, "https");
        tcp::resolver::iterator endpointIterator = resolver.resolve(query);

        boost::asio::ssl::context context(ssl::context::tlsv12);
        context.set_default_verify_paths();
        context.set_options(ssl::context::default_workarounds
                            | ssl::context::no_sslv2
                            | ssl::context::no_sslv3
                            | ssl::context::no_tlsv1
                            | ssl::context::no_tlsv1_1
                            | ssl::context::single_dh_use);

        // Try each endpoint until we successfully establish a connection.
        boost::asio::ssl::stream<tcp::socket> socket(ioService, context);

        boost::system::error_code error = boost::asio::error::host_not_found;
        boost::asio::connect(socket.lowest_layer(), endpointIterator, error);

        // Perform SSL handshake and verify the remote host's certificate.
        socket.set_verify_mode(ssl::verify_peer | ssl::verify_fail_if_no_peer_cert);
        SSL_set_tlsext_host_name(socket.native_handle(), host.c_str());
        socket.set_verify_callback(cb);
        socket.handshake(ssl::stream_base::client, error);

        if (error) {
            throw boost::system::system_error(error);
        }

        // Form the request. We specify the "Connection: close" header so that the
        // server will close the socket after transmitting the response. This will
        // allow us to treat all data up until the EOF as the content.
        boost::asio::streambuf request;
        std::ostream requestStream(&request);
        requestStream << "GET " << get << " HTTP/1.0\r\n";
        requestStream << "Host: " << host << "\r\n";
        requestStream << "Accept: */*\r\n";
        requestStream << "Cache-Control: no-cache\r\n";
        requestStream << "Connection: close\r\n\r\n";

        // Send the request.
        boost::asio::write(socket, request);

        // Read the response status line. The response streambuf will automatically
        // grow to accommodate the entire line. The growth may be limited by passing
        // a maximum size to the streambuf constructor.
        boost::asio::streambuf response;
        boost::asio::read_until(socket, response, "\r\n");

        // Check that response is OK.
        std::istream responseStream(&response);
        std::string httpVersion;
        responseStream >> httpVersion;
        unsigned int statusCode;
        responseStream >> statusCode;
        std::string statusMessage;
        std::getline(responseStream, statusMessage);

        if (!responseStream || httpVersion.substr(0, 5) != "HTTP/"){
            std::cout << "Invalid response\n";
            return false;
        }

        if (statusCode != 200){
            std::cout << "Response returned with status code " << statusCode << "\n";
            return false;
        }

        // Read the response headers, which are terminated by a blank line.
        boost::asio::read_until(socket, response, "\r\n\r\n");
        // Read until EOF, writing data to output as we go.
        std::string line;

        while (boost::asio::read(socket, response, boost::asio::transfer_at_least(1), error)){
            std::istream is(&response);
            bool isHeader = true;
            while (std::getline(is, line)) {
                if(!isHeader)
                    blackList.push_back(line);

                if(line == "\r")
                    isHeader = false;
            }
        }
    }
    catch (std::exception& e){
        std::cout << "Exception: " << e.what() << "\n";
    }

    return true;
}
