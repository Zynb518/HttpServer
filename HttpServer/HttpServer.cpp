#include "HttpServer.h"
#include "IOServicePool.h"
#include "HttpConnection.h"
#include <iostream>
#include <thread>
#include <chrono>
#include "Log.h"

// in_addr_any ¶Ë¿Ú10086
HttpServer::HttpServer(boost::asio::io_context& ioc, uint16_t port) noexcept
	:_ioc(ioc),
	_acceptor(ioc, tcp::endpoint(boost::asio::ip::tcp::v4(), port))
{
	// boost::asio::ip::make_address("");
	LOG_INFO("HttpServer start listen on port " << port);
	_acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
	_acceptor.listen();
	StartAccept();
}

void HttpServer::ClearConnection(std::string_view uuid) noexcept
{
	_mapping.erase(uuid);
}

void HttpServer::StartAccept()
{
	auto& ioc = IOServicePool::Instance().GetIOService();
	auto connection = std::make_shared<HttpConnection>(ioc, *this);

	_acceptor.async_accept(connection->GetSocket(),
		[this, connection](boost::system::error_code ec) {
			if (!ec)
			{
				LOG_INFO("Accept Success !!! \nnow _mapping's size = " << _mapping.size());
				LOG_INFO("The thread ID executing ReadLogin is " << std::this_thread::get_id());
				_mapping[connection->GetUuid()] = connection;
				connection->ReadLogin();
			}
			else
			{
				LOG_INFO("aysnc_accept error is " << ec.what());
			}
			std::this_thread::sleep_for(std::chrono::seconds(10));
			StartAccept();
		});
}



HttpServer::~HttpServer()
{

}
