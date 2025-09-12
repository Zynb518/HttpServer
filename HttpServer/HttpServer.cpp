#include "HttpServer.h"
#include "IOServicePool.h"
#include "HttpConnection.h"
#include <iostream>
#include <thread>
#include <chrono>
// in_addr_any ¶Ë¿Ú10086
HttpServer::HttpServer(boost::asio::io_context& ioc, uint16_t port)
	:_ioc(ioc),
	_acceptor(ioc, tcp::endpoint(boost::asio::ip::tcp::v4(), port))
{
	std::cout << "Server start success, listen on port :" << port << std::endl;
	_acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
	_acceptor.listen();
	StartAccept();
}

void HttpServer::ClearConnection(std::string_view uuid)
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
				std::cout << "Accept Success" << std::endl;
				std::cout << "now _mapping's size = " << _mapping.size() << std::endl;
				_mapping[connection->GetUuid()] = connection;
				connection->ReadLogin();
			}
			else
			{
				std::cout << "aysnc_accept error is " << ec.what() << std::endl;
			}
			std::this_thread::sleep_for(std::chrono::seconds(10));
			StartAccept();
		});
}



HttpServer::~HttpServer()
{

}
