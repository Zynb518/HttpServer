#pragma once
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <unordered_map>
#include <string_view>
#include <memory>
#include <mysqlx/xdevapi.h>

namespace beast = boost::beast;

class HttpConnection;

class HttpServer
{
	using tcp = boost::asio::ip::tcp;
public:
	HttpServer(boost::asio::io_context& ioc, uint16_t port) noexcept;
	void ClearConnection(std::string_view uuid) noexcept;
	~HttpServer();
private:
	void StartAccept(); // lambda »Øµ÷ HandleAccept
	void StartTimer();
private:
	boost::asio::io_context& _ioc;
	boost::asio::steady_timer _timer;
	tcp::acceptor _acceptor;
	std::unordered_map<std::string_view, std::shared_ptr<HttpConnection> > _mapping;
	
};

