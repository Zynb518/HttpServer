//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// Example: HTTP server, asynchronous
//
//------------------------------------------------------------------------------

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/config.hpp>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

#if 1
// Performs an HTTP POST with JSON data and prints the response
int main()
{
    try
    {
        // Fixed connection parameters
        const std::string host = "10.195.153.16";
        const unsigned short port = 10086;
        const std::string target = "/api/login";
        const int version = 11; // HTTP/1.1

        // The io_context is required for all I/O
        net::io_context ioc;

        // Create TCP stream
        beast::tcp_stream stream(ioc);

        // Create endpoint directly from IP and port
        auto const endpoint = tcp::endpoint(net::ip::make_address(host), port);

        // Make the connection directly to the endpoint
        stream.connect(endpoint);

        // Set up the JSON payload
        std::string const json_body = "{\"user_id\":1,\"password\":\"student1\",\"role\":\"student\"}";

        // Set up an HTTP POST request message
        http::request<http::string_body> req{ http::verb::post, target, version };
        req.set(http::field::host, host);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        req.set(http::field::content_type, "application/json");
        req.set(http::field::accept, "application/json");
        req.set(http::field::connection, "keep-alive");
        req.body() = json_body;
        req.prepare_payload();  // This will set Content-Length automatically

        // Send the HTTP request to the remote host
        http::write(stream, req);

        // This buffer is used for reading and must be persisted
        beast::flat_buffer buffer;

        // Declare a container to hold the response
        http::response<http::dynamic_body> res;

        // Receive the HTTP response
        http::read(stream, buffer, res);

        // Write the message to standard out
        std::cout << res << std::endl;

        // -------------------------------------------TEST2-------------------------------------------
        http::request<http::string_body> req2{ http::verb::get, "/api/get_personal_info", version };
        req2.set(http::field::host, host);
        req2.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        req2.set(http::field::content_type, "application/json");
        req2.set(http::field::accept, "application/json");
        req2.set(http::field::connection, "keep-alive");
        req2.body() = json_body;
        req2.prepare_payload();
        http::write(stream, req2);

        std::cout << "write ok\n";
        // Gracefully close the socket
        // Receive the HTTP response
        http::response<http::dynamic_body> res2;
        http::read(stream, buffer, res2);
        std::cout << "read ok\n";
        // Write the message to standard out
        std::cout << res2 << std::endl;


        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);

        std::this_thread::sleep_for(std::chrono::minutes(2));
        // not_connected happens sometimes
        // so don't bother reporting it.
        //
        if (ec && ec != beast::errc::not_connected)
            throw beast::system_error{ ec };

        // If we get here then the connection is closed gracefully
    }
    catch (std::exception const& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return 0;
}

#endif