#include "HttpConnection.h"
#include "HttpServer.h"
#include "MysqlConnectionPool.h"
#include "LogicSystem.h"
#include "const.h"
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <iostream>


HttpConnection::HttpConnection(boost::asio::io_context& ioc, HttpServer& server)
	:_socket(ioc), _server(server), _user_id(0)
{
	boost::uuids::uuid a_uuid = boost::uuids::random_generator()();
	_uuid = boost::uuids::to_string(a_uuid);
}



boost::asio::ip::tcp::socket& HttpConnection::GetSocket()
{
	return _socket;
}

std::string_view HttpConnection::GetUuid()
{
	return _uuid;
}
beast::http::response<beast::http::dynamic_body>& HttpConnection::GetResponse()
{
	return _response;
}

void HttpConnection::StartWrite()
{
	std::cout << "After StartWrite response\n" <<
		_response << std::endl;

	beast::http::async_write(_socket, _response,
		[self = shared_from_this()](beast::error_code ec, size_t) {
			if (ec)
			{
				std::cout << "StartWrite's async_write error is \n" << ec.what() << std::endl;
				self->CloseConnection();
				return;
			}
			self->StartRead();
		});
}

void HttpConnection::ReadLogin()
{
	std::cout << "ReadLogin\n";
	beast::http::async_read(_socket, _buffer, _request,
		[self = shared_from_this() ](beast::error_code ec, size_t bytes_transferred) {
			boost::ignore_unused(bytes_transferred);
			if (!ec)
			{
				std::cout << "---- HandleLogin ---- \n target is" << self->_request.target() << std::endl;
				self->HandleLogin();
			}
			else
			{
				std::cout << "HttpConnection Async Read Error is " << ec.what() << std::endl;
				self->CloseConnection();
			}
		});

	// StartTimer();
}

void HttpConnection::StartTimer()
{
	_timer.async_wait([self = shared_from_this()](boost::system::error_code ec) {
		if (!ec)
		{
			self->CloseConnection();
		}
		});
}

void HttpConnection::ResetTimer()
{
	_timer.cancel();
	_timer.expires_after(std::chrono::seconds(60));
	StartTimer();
}


void HttpConnection::HandleLogin()
{

	_response.version(_request.version());
	_response.keep_alive(_request.keep_alive());
	_response.set(beast::http::field::content_type, "application/json");
	_response.set(beast::http::field::access_control_allow_origin, "*");
	_response.set(beast::http::field::access_control_allow_methods, "GET, POST, OPTIONS");
	_response.set(beast::http::field::access_control_allow_headers, "Content-Type");


	if (_request.method() == beast::http::verb::options) {
		_response.result(beast::http::status::ok);
		_response.prepare_payload();
		beast::http::write(_socket, _response);
		ReadLogin();
		return;
	}

	std::string target = _request.target();
	if (_request.method() == beast::http::verb::post &&
		target == "/api/login")
	{

		auto& body = _request.body();
		auto body_str = beast::buffers_to_string(body.data());
		if (!ParseUserData(body_str))
		{
			// 解析失败
			std::cout << "ParseUserData error\n";
			SetBadRequest();
			return;
		}

		_user_id = _recv_root["user_id"].asUInt();
		_password = _recv_root["password"].asString();
		_role = _recv_root["role"].asString();

		if (!UserExists(_user_id, _password, _role))
		{
			// 用户不存在
			std::cout << "User does not exist \n";
			SetBadRequest();
			return;
		}
		std::cout << "User exist \n";

		_response.result(beast::http::status::ok);

		_send_root["result"] = "Success";
		beast::ostream(_response.body()) << _send_root.toStyledString();
		WriteLoginSuccess();
		return;
	}
	std::cout << "Bad Login Request\n";
	SetBadRequest(); // 不符合登陆请求格式
}

bool HttpConnection::ParseUserData(std::string& body)
{
	bool t = _reader.parse(body, _recv_root);
	return t;
}

bool HttpConnection::UserExists(uint32_t user_id, const std::string& password, const std::string& role)
{
	try {
		// 获取 users 表
		// mysqlx::Session sess = MySQLConnectionPool::Instance().GetSession();
		mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
		mysqlx::Table users = sess.getSchema("scut_sims").getTable("users");

		// 构建查询条件 - 使用主键(user_id, role)和密码进行匹配
		auto result = users.select("user_id")
			.where("user_id = :uid AND password = :pwd AND role = :r")
			.bind("uid", user_id)
			.bind("pwd", password)
			.bind("r", role)  // 只需要检查是否存在，限制返回1条记录
			.execute();
		sess.close();
		// 检查是否有结果
		return result.count() > 0;
	}
	catch (const mysqlx::Error& err) {
		std::cerr << "数据库查询错误: " << err.what() << std::endl;
		return false;
	}
}

void HttpConnection::SetBadRequest()
{
	// 设置状态码
	_response.result(beast::http::status::bad_request);
	_response.set(beast::http::field::connection, "close"); // 文件类型

	_send_root["result"] = "Failure";
	beast::ostream(_response.body()) << _send_root.toStyledString();
	WriteBadResponse();
}

void HttpConnection::WriteBadResponse()
{
	_response.prepare_payload();
	// 注意time_wait
	beast::http::async_write(_socket, _response,
		[self = shared_from_this()](beast::error_code ec, size_t) {
			if (ec)
			{
				std::cout << "WriteBadResponse async_write error is "
					<< ec.what();
			}
			self->CloseConnection();
		}
	);

}

void HttpConnection::WriteLoginSuccess()
{
	_response.content_length(_response.body().size());
	// 注意time_wait
	beast::http::write(_socket, _response);
	
#if 0
	CloseConnection();
#else
	StartRead();
#endif 
}

void HttpConnection::StartRead()
{
	_recv_root.clear();
	_send_root.clear();
	_response.body().clear();

	beast::http::async_read(_socket, _buffer, _request,
		[self = shared_from_this()](beast::error_code ec, size_t bytes_transferred) {
			boost::ignore_unused(bytes_transferred);
			if (!ec)
			{
				self->HandleRead();
				std::cout << "After Login, the target is " << self->_request.target() << std::endl;
			}
			else
			{
				std::cout << "HttpConnection Async Read Error is " << ec.what() << std::endl;
				self->CloseConnection();
			}
		});
}

void HttpConnection::HandleRead()
{
	
	_response.version(_request.version());
	_response.keep_alive(_request.keep_alive());
	if (_request.method() == beast::http::verb::options) {
		_response.result(beast::http::status::ok);
		_response.prepare_payload();
		beast::http::write(_socket, _response);
		StartRead();
		return;
	}
	if (_role == "student")
	{
		StudentRequest();
	}
	else if (_role == "instructor")
	{
		InstructorRequest();
	}
	else if (_role == "administor")
	{
		AdminRequest();
	}
	else
	{
		std::cout << "role error" << std::endl;
	}
}

void HttpConnection::StudentRequest()
{
	// Post To Queue
	switch (_request.method())
	{
	case beast::http::verb::get:
		if (_request.target() == "/api/get_personal_info")
		{
			std::cout << "Post to Que\n";
			LogicSystem::Instance().PushToQue(StudentOp::GET_PERSONAL_INFO, shared_from_this(), _user_id);
		}
		break;
	case beast::http::verb::post:
		break;
	case beast::http::verb::put:

		break;
	case beast::http::verb::patch:

		break;
	case beast::http::verb::delete_:

		break;
	default:
		std::cout << "StudentRequest Wrong, SetBadRequest\n";
		SetBadRequest();

		break;
	}
}

void HttpConnection::InstructorRequest()
{
	switch (_request.method())
	{
	case beast::http::verb::get:
		_response.result(beast::http::status::ok);
		_response.set(beast::http::field::server, "Beast");
		// create_response();
		break;
	case beast::http::verb::post:
		_response.result(beast::http::status::ok);
		_response.set(beast::http::field::server, "Beast");
		// create_post_response();
		break;
	case beast::http::verb::put:

		break;
	case beast::http::verb::patch:

		break;
	default:
		// 设置状态码
		SetBadRequest();

		break;
	}
}

void HttpConnection::AdminRequest()
{
	switch (_request.method())
	{
	case beast::http::verb::get:
		_response.result(beast::http::status::ok);
		_response.set(beast::http::field::server, "Beast");
		// create_response();
		break;
	case beast::http::verb::post:
		_response.result(beast::http::status::ok);
		_response.set(beast::http::field::server, "Beast");
		// create_post_response();
		break;
	case beast::http::verb::put:

		break;
	case beast::http::verb::patch:

		break;
	case beast::http::verb::delete_:

		break;
	default:
		SetBadRequest();
		break;
	}
}

//void HttpConnection::StartWrite()
//{
//	// 不是get请求，可以直接回复
//	_response.content_length(_response.body().size());
//	// 注意time_wait
//	beast::http::async_write(_socket, _response,
//		[self = shared_from_this()](beast::error_code ec, size_t) {
//			if (ec)
//			{
//				std::cout << "HttpConnection async_write error is " << ec.what() << std::endl;
//				self->CloseConnection();
//			}
//		}
//	);
//}

void HttpConnection::CloseConnection()
{
	_socket.close();
	_server.ClearConnection(_uuid);
}
