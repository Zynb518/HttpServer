#include "HttpServer.h"
#include "IOServicePool.h"
#include "HttpConnection.h"
#include "SessionManager.h"
#include <iostream>
#include <thread>
#include <chrono>
#include "Log.h"

// in_addr_any ¶Ë¿Ú10086
HttpServer::HttpServer(boost::asio::io_context& ioc, uint16_t port) noexcept
	:_ioc(ioc),
	 _timer(ioc, boost::asio::chrono::minutes(1)),
	_acceptor(ioc, tcp::endpoint(boost::asio::ip::make_address("192.168.160.65"), port))
{
	// boost::asio::ip::make_address("10.195.144.254");
	LOG_INFO("HttpServer start listen on port " << port);
	_acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
	_acceptor.listen();
	StartTimer();
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
				LOG_INFO("Accept Success !!! now _mapping's size = " << _mapping.size());
				LOG_INFO("The thread ID executing StartRead is " << std::this_thread::get_id());
				_mapping[connection->GetUuid()] = connection;
				connection->StartRead();
			}
			else
			{
				LOG_INFO("aysnc_accept error is " << ec.what());
			}
			// std::this_thread::sleep_for(std::chrono::seconds(10));
			StartAccept();
		});
}

void HttpServer::StartTimer()
{
	static size_t count = 0;
	_timer.async_wait([this](boost::system::error_code ec) {
		LOG_INFO("Timer Have Worked " << ++count );
		SessionManager::Instance().RemoveExpiredSessions();
		_timer.expires_after(boost::asio::chrono::minutes(1));
		StartTimer();
		});
}

HttpServer::~HttpServer()
{

}
