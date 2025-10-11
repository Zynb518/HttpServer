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

	void ReadLogin(); // �첽��http��½����
private:
	void StartTimer(); // ��ʱ�ͶϿ�����
	void ResetTimer();
	
	void HandleLogin(); // ����request
	bool ParseUserData(const std::string& body, Json::Value& recv); // ������Ϣ�嵽recv_root

	// �û��Ƿ����
	bool UserExists(uint32_t user_id, const std::string& password, const std::string& role);

	void SetBadRequest(); // ���ô�����Ӧ ����WriteBadResponse
	void WriteBadResponse();
	void WriteLoginSuccess(); // �������� ��½�ɹ�

	// �����û�����
	void StartRead();

	void HandleRead(); // ����HTTP request
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
	// �û���½ and 
	beast::http::request<beast::http::dynamic_body> _request;
	beast::http::response<beast::http::dynamic_body> _response;

	// �������� ��������
	Json::StreamWriterBuilder _writerBuilder;
	Json::CharReaderBuilder _readerBuilder;

	uint32_t _user_id;
	std::string _password;
	std::string _role;
};

