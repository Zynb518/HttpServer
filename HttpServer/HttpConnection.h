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
	void ReadLogin(); // �첽��http��½����
	void SetBadRequest(Json::Value& message) noexcept; // ҵ�����,���ر�����
	void SetBadRequest(const std::string& reason) noexcept; // ҵ�����,���ر�����
private:
	void StartTimer(); // ��ʱ�ͶϿ�����
	void ResetTimer();
	
	bool HandleLogin(); // ����request
	bool ParseUserData(const std::string& body, Json::Value& recv); // ������Ϣ�嵽recv_root
	bool UserExists(uint32_t user_id, const std::string& password, const std::string& role); // �û��Ƿ����
	
	// BadRequest close�����ֶ�CloseConnection
	void SetBadRequest() noexcept; 

	void WriteBadResponse() noexcept;
	// �������� ��½�ɹ�
	void WriteLoginSuccess(); 

	// �����û�����
	void StartRead();
	// ����HTTP request
	void HandleRead(); 
	void StudentRequest();
	bool ProcessRow(const mysqlx::Row& row, size_t choice);
	// ���� startWeek, endWeek, schedule������, ������Ҫѡ�Ŀ�, choiceΪ1���飬choiceΪ0��ȥ1
	bool ProcessRow(uint32_t start_week, uint32_t end_week, const std::string& time_slot, uint32_t choice);

	void InstructorRequest();
	void AdminRequest();

	// ��������
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
	// �û���½ and 
	beast::http::request<beast::http::dynamic_body> _request;
	beast::http::response<beast::http::dynamic_body> _response;

	// �������� ��������
	Json::StreamWriterBuilder _writerBuilder;
	Json::CharReaderBuilder _readerBuilder;

	// �����û�����
	uint32_t _user_id;
	std::string _password;
	std::string _role;

	static StudentHandler _studentHandler;
	static InstructorHandler _instructorHandler;
	static AdminHandler _adminHandler;

};

