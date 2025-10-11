#pragma once
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <json/json.h>
#include <string_view>
#include <string>
#include <memory>
#include <mysqlx/xdevapi.h>

#include "StudentHandler.h"
#include "InstructorHandler.h"
#include "AdminHandler.h"

namespace beast = boost::beast;
class HttpServer;

class HttpConnection : public std::enable_shared_from_this<HttpConnection>
{
	using tcp = boost::asio::ip::tcp;
public:
	HttpConnection(boost::asio::io_context& ioc, HttpServer& server) noexcept;
	tcp::socket& GetSocket() noexcept;
	std::string_view GetUuid() noexcept;
	beast::http::response<beast::http::dynamic_body>& GetResponse() noexcept;
	void StartWrite();
	void ReadLogin(); // 异步读http登陆请求
	void SetBadRequest(Json::Value& message) noexcept; // 业务错误,不关闭连接
	void SetBadRequest(const std::string& reason) noexcept; // 业务错误,不关闭连接
private:
	void StartTimer(); // 超时就断开连接
	void ResetTimer();
	
	bool HandleLogin(); // 解析request
	bool ParseUserData(const std::string& body, Json::Value& recv); // 解析消息体到recv_root
	bool UserExists(uint32_t user_id, const std::string& password, const std::string& role); // 用户是否存在
	
	// BadRequest close，再手动CloseConnection
	void SetBadRequest() noexcept; 

	void WriteBadResponse() noexcept;
	// 正常发送 登陆成功
	void WriteLoginSuccess(); 

	// 接受用户操作
	void StartRead();
	// 解析HTTP request
	void HandleRead(); 
	void StudentRequest();
	bool ProcessRow(const mysqlx::Row& row, size_t choice);
	// 处理 startWeek, endWeek, schedule的重载, 处理即将要选的课, choice为1检验，choice为0，去1
	bool ProcessRow(uint32_t start_week, uint32_t end_week, const std::string& time_slot, uint32_t choice);

	void InstructorRequest();
	void AdminRequest();

	// 带错误处理
	std::string ProcessSchedule(const Json::Value& schedule);

	void CloseConnection() noexcept;

private:
	tcp::socket _socket;
	beast::flat_buffer _buffer;
	std::string _uuid;
	HttpServer& _server;
	boost::asio::steady_timer _timer{
		_socket.get_executor(), std::chrono::seconds(60)
	};
	// 用户登陆 and 
	beast::http::request<beast::http::dynamic_body> _request;
	beast::http::response<beast::http::dynamic_body> _response;

	// 解析数据 发送数据
	Json::StreamWriterBuilder _writerBuilder;
	Json::CharReaderBuilder _readerBuilder;

	// 保存用户数据
	uint32_t _user_id;
	std::string _password;
	std::string _role;

	static StudentHandler _studentHandler;
	static InstructorHandler _instructorHandler;
	static AdminHandler _adminHandler;

};

