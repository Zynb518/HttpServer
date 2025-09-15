#pragma once
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <json/json.h>
#include <string_view>
#include <string>
#include <memory>
#include <mysqlx/xdevapi.h>


namespace beast = boost::beast;
class HttpServer;

class HttpConnection : public std::enable_shared_from_this<HttpConnection>
{
	using tcp = boost::asio::ip::tcp;
public:
	HttpConnection(boost::asio::io_context& ioc, HttpServer& server);
	tcp::socket& GetSocket();
	std::string_view GetUuid();
	beast::http::response<beast::http::dynamic_body>&
		GetResponse();
	void StartWrite();

	void ReadLogin(); // 异步读http登陆请求
private:
	void StartTimer(); // 超时就断开连接
	void ResetTimer();
	
	void HandleLogin(); // 解析request
	bool ParseUserData(const std::string& body, Json::Value& recv); // 解析消息体到recv_root

	// 用户是否存在
	bool UserExists(uint32_t user_id, const std::string& password, const std::string& role);

	void SetBadRequest(); // 设置错误响应 调用WriteBadResponse
	void WriteBadResponse();
	void WriteLoginSuccess(); // 正常发送 登陆成功

	// 接受用户操作
	void StartRead();

	void HandleRead(); // 解析HTTP request
	void StudentRequest();
	void InstructorRequest();
	void AdminRequest();

	void CloseConnection();
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

	uint32_t _user_id;
	std::string _password;
	std::string _role;
};

