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
	HttpServer(boost::asio::io_context& ioc, uint16_t port);
	void ClearConnection(std::string_view uuid);
	~HttpServer();
private:
	void StartAccept(); // lambda »Øµ÷ HandleAccept
	
private:
	boost::asio::io_context& _ioc;
	tcp::acceptor _acceptor;
	std::unordered_map<std::string_view, std::shared_ptr<HttpConnection> > _mapping;
	
};

