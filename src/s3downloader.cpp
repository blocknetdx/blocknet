#include "s3downloader.h"

bool downloadBlackList(std::list<std::string> blackList, const std::string &host, const std::string &get)
{
    using boost::asio::ip::tcp;

    try {
        boost::asio::io_service ioService;
        // Get a list of endpoints corresponding to the server name.
        tcp::resolver resolver(ioService);
        tcp::resolver::query query(host, "http");
        tcp::resolver::iterator endpointIterator = resolver.resolve(query);
        tcp::resolver::iterator end;

        // Try each endpoint until we successfully establish a connection.
        tcp::socket socket(ioService);
        boost::system::error_code error = boost::asio::error::host_not_found;
        while (error && endpointIterator != end){
            socket.close();
            socket.connect(*endpointIterator++, error);
        }

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
