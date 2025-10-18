
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
#include <string_view>

#include <json/json.h>
namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

// 编译期 UTF-16 到 UTF-8 转换（适用于基本多语言平面字符）
std::string GetUTF8ForDatabase(std::wstring_view wideStr) noexcept {
    if (wideStr.empty()) return "";

    std::string result;
    for (wchar_t wc : wideStr) {
        if (wc <= 0x7F) {
            // ASCII 字符
            result += static_cast<char>(wc);
        }
        else if (wc <= 0x7FF) {
            // 2字节 UTF-8
            result += static_cast<char>(0xC0 | ((wc >> 6) & 0x1F));
            result += static_cast<char>(0x80 | (wc & 0x3F));
        }
        else if (wc <= 0xFFFF) {
            // 3字节 UTF-8（适用于基本多语言平面）
            result += static_cast<char>(0xE0 | ((wc >> 12) & 0x0F));
            result += static_cast<char>(0x80 | ((wc >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (wc & 0x3F));
        }
        // 对于代理对（surrogate pairs）需要更复杂的处理
    }
    return result;
}

#if 1
// Performs an HTTP POST with JSON data and prints the response
int main()
{
    try
    {
#ifdef _WIN32
        system("chcp 65001 > nul");
#endif
        // Fixed connection parameters
        const std::string host = "127.0.0.1";
        const unsigned short port = 10086;
        const int version = 11; // HTTP/1.1

        // The io_context is required for all I/O
        net::io_context ioc;

        // Create TCP stream
        beast::tcp_stream stream(ioc);

        // Create endpoint directly from IP and port
        auto const endpoint = tcp::endpoint(net::ip::make_address(host), port);

        // Make the connection directly to the endpoint
        stream.connect(endpoint);

        Json::Value loginData;
		loginData["user_id"] = 1;
		loginData["password"] = "admin";
		loginData["role"] = "admin";
        // Set up the JSON payload

        // Set up an HTTP POST request message
        http::request<http::dynamic_body> req{ http::verb::post, "/api/login/post" , version };
        req.set(http::field::host, host);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        req.set(http::field::content_type, "application/json");
        req.set(http::field::accept, "application/json");
        req.set(http::field::connection, "keep-alive");
        beast::ostream(req.body()) << loginData.toStyledString();
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
        std::string s = GetUTF8ForDatabase(L"/api/admin/general/getMajorId?college_id=1");
		std::cout << s << std::endl;
        http::request<http::dynamic_body> req2{ http::verb::get, s, version };
        req2.set(http::field::host, host);
        req2.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        req2.set(http::field::content_type, "application/json");
        req2.set(http::field::accept, "application/json");
        req2.set(http::field::connection, "keep-alive");
        Json::Value postData;

        beast::ostream(req2.body()) << postData.toStyledString();
        req2.prepare_payload();
        http::write(stream, req2);

        // Receive the HTTP response
        res.body().clear();
        http::read(stream, buffer, res);
        std::cout << "\n";
		std::string body = beast::buffers_to_string(res.body().data());
        // Write the message to standard out
        std::cout << body << std::endl;

        

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