#ifndef S3DOWNLOADER_H
#define S3DOWNLOADER_H

#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <list>
#include <boost/algorithm/string/join.hpp>
#include <boost/asio.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>

//#include "obfuscation.h"
#include "chainparams.h"

class S3Downloader
{

public:
    static std::shared_ptr<S3Downloader> create(boost::function<void (const std::list<std::string>& list, const std::string error)> cb,
                                                const std::string &host = "dxlist.blocknet.co",
                                                const std::string &path = "/dxlist.txt");

    void downloadList(boost::posix_time::time_duration timeout = boost::posix_time::seconds(15));

private:
    S3Downloader(boost::function<void (const std::list<std::string>& list, const std::string error)> cb,
                 const std::string &host,
                 const std::string &path,
                 boost::asio::ssl::context& context);

    S3Downloader(const S3Downloader&) = delete;
    S3Downloader& operator= (const S3Downloader&) = delete;

    void handleResolve(const boost::system::error_code& error, boost::asio::ip::tcp::resolver::iterator endpointIterator);
    void handleConnect(const boost::system::error_code& error);
    void handleHandshake(const boost::system::error_code& error);
    void handleWriteRequest(const boost::system::error_code& error);
    void handleReadStatusLine(const boost::system::error_code& error);
    void handleReadHeaders(const boost::system::error_code& error);
    void handleReadContent(const boost::system::error_code& error);
    bool verifyCertificate(bool preverified, boost::asio::ssl::verify_context& ctx);
    boost::system::error_code close();
//    bool verifySign(const std::string &list, std::vector<unsigned char> &vchSign);

    void checkDeadline();

    boost::asio::io_service ioService;
    boost::asio::ip::tcp::resolver resolver;
    boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket;
    boost::asio::deadline_timer deadline;
    boost::asio::streambuf request;
    boost::asio::streambuf response;
    boost::function<void (const std::list<std::string>& list, std::string error)> callback;
    std::string host;
    std::string path;
};

#endif // S3DOWNLOADER_H
