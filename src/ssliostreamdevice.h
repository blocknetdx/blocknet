// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2018 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SSLIOSTREAMDEVICE_H
#define SSLIOSTREAMDEVICE_H

#include "util.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/bind.hpp>
#include <boost/current_function.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>

#include <chrono>
#include <string>

/**
 * IOStream device that speaks SSL but can also speak non-SSL
 */
template <typename Protocol>
class SSLIOStreamDevice : public boost::iostreams::device<boost::iostreams::bidirectional>
{
public:
    SSLIOStreamDevice(boost::asio::ssl::stream<typename Protocol::socket>& streamIn, bool fUseSSLIn)
      : stream(streamIn)
    {
        fUseSSL = fUseSSLIn;
        fNeedHandshake = fUseSSLIn;
    }

    void handshake(boost::asio::ssl::stream_base::handshake_type role)
    {
        if (!fNeedHandshake) return;
        fNeedHandshake = false;
        stream.handshake(role);
    }
    std::streamsize read(char* s, std::streamsize n)
    {
        handshake(boost::asio::ssl::stream_base::server); // HTTPS servers read first
        if (fUseSSL) return stream.read_some(boost::asio::buffer(s, n));
        return stream.next_layer().read_some(boost::asio::buffer(s, n));
    }
    std::streamsize write(const char* s, std::streamsize n)
    {
        handshake(boost::asio::ssl::stream_base::client); // HTTPS clients write first
        if (fUseSSL) return boost::asio::write(stream, boost::asio::buffer(s, n));
        return boost::asio::write(stream.next_layer(), boost::asio::buffer(s, n));
    }

    bool connect(const std::string& server, const std::string& port,
        boost::asio::steady_timer::duration timeout = std::chrono::milliseconds(nConnectTimeout))
    {
        using namespace boost::asio::ip;
        if (timeout <= std::chrono::milliseconds(0))
            timeout = std::chrono::milliseconds(nConnectTimeout);
        auto & ios = stream.get_io_service();
        tcp::resolver resolver(ios);
        tcp::resolver::iterator endpoint_iterator;
        connect_ec = boost::asio::error::host_not_found;
        try {
            // The default query (flags address_configured) tries IPv6 if
            // non-localhost IPv6 configured, and IPv4 if non-localhost IPv4
            // configured.
            tcp::resolver::query query(server.c_str(), port.c_str());
            endpoint_iterator = resolver.resolve(query);
        } catch (boost::system::system_error& e) {
            LogPrint("net", "SSLIOStreamDevice: exception resolving %s:%s: %s\n", server, port, e.what());
            // If we at first don't succeed, try blanket lookup (IPv4+IPv6 independent of configured interfaces)
            try {
                tcp::resolver::query query(server.c_str(), port.c_str(), resolver_query_base::flags());
                endpoint_iterator = resolver.resolve(query);
                connect_ec = boost::asio::error::host_not_found;
            } catch (boost::system::system_error& e) {
                LogPrint("net", "SSLIOStreamDevice: exception resolving %s:%s: %s\n", server, port, e.what());
                connect_ec = e.code();
                return false;
            }
        }
        try {
          tcp::resolver::iterator end;
          auto & socket = stream.lowest_layer();
          while (connect_ec && endpoint_iterator != end) {
            socket.close(); // Ensure it is closed before attempting new connection
            //
            // The default timeout for the kernel 'connect()' system call is O/S specific
            // and can range from 20 to 75 seconds
            //     http://man7.org/linux/man-pages/man2/connect.2.html
            //
            // Use a deadline timer to close the socket and force a cancel of the 'connect()' system call.
            // Boost's synchronous/blocking 'connect()' is not cancelled by closing the socket,
            // the connection must be attempted with an asynchronous 'async_connect()' in order to
            // be able to cancel with a socket close()
            //
            boost::asio::steady_timer deadline(ios);
            deadline.expires_from_now(timeout);
            deadline.async_wait([&](const boost::system::error_code& ec) {
                // Timer expired or was cancelled
                LogPrint("net", "SSLIOStreamDevice: async_wait(): deadline=%fsec %s %s:%s\n",
                    std::chrono::duration<double>(timeout).count(),
                    ec ? ec.message() : "timer expired", server, port);
                if (!connect_ec) return; // async_connect was successful
                // Note: race condition here: connection might succeed before socket is closed,
                //       not worth using a mutex, but must set error code after closing
                socket.close();                             // cancel async_connect
                connect_ec = boost::asio::error::timed_out; // ... then set the error code
              });
            socket.async_connect(*endpoint_iterator++, [&](const boost::system::error_code& ec) {
                // Connection completed or was cancelled
                connect_ec = ec;
                deadline.expires_from_now(timeout); // cancel async_wait
                LogPrint("net", "SSLIOStreamDevice: async_connect(): %s %s:%s\n", ec ? ec.message() : "connected to", server, port);
              });
            // concurrently run async_wait and async_connect
            ios.run(); // blocks until both handlers have run
          }
        } catch (boost::system::system_error& e) {
            LogPrint("net", "SSLIOStreamDevice: connect exception: %s\n", e.what());
        }
        return connect_ec == boost::system::errc::success;
    }

    boost::system::error_code connect_error_code() const { return connect_ec; }

 private:
    bool fNeedHandshake;
    bool fUseSSL;
    boost::system::error_code connect_ec;
    boost::asio::ssl::stream<typename Protocol::socket>& stream;
};

#endif // SSLIOSTREAMDEVICE_H
