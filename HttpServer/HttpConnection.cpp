#include "HttpConnection.h"
#include "HttpServer.h"
#include "MysqlConnectionPool.h"
#include "LogicSystem.h"
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

	try
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


		// 登陆请求
		if (_request.method() == beast::http::verb::post &&
			_request.target() == "/api/login/post")
		{

			auto& body = _request.body();
			auto body_str = beast::buffers_to_string(body.data());

			Json::Value recv;
			if (!ParseUserData(body_str, recv))
			{
				// 解析失败
				std::cout << "ParseUserData error\n";
				SetBadRequest();
				return;
			}

			_user_id = stoi(recv["user_id"].asString());
			_password = recv["password"].asString();
			_role = recv["role"].asString();

			_role = _role == "teacher" ? "instructor" : _role;
			_role = _role == "admin" ? "administer" : _role;

			if (!UserExists(_user_id, _password, _role))
			{
				// 用户不存在
				std::cout << "User does not exist \n";
				SetBadRequest();
				return;
			}
			std::cout << "User exist \n";
			Json::Value send;
			_response.result(beast::http::status::ok);
			send["id"] = _user_id;
			send["user_id"] = _user_id;
			send["result"] = true;
			beast::ostream(_response.body()) << Json::writeString(_writerBuilder, send);
			WriteLoginSuccess();
			return;
		}
		std::cout << "Bad Login Request\n";
		SetBadRequest(); // 不符合登陆请求格式
	}
	catch(const std::exception& e)
	{
		std::cout << "HandleLogin Exception is " << e.what() << std::endl;
		SetBadRequest();
		return;
	}

}

bool HttpConnection::ParseUserData(const std::string& body, Json::Value& recv)
{
	std::unique_ptr<Json::CharReader> jsonReader(_readerBuilder.newCharReader());
	std::string err;
	if (!jsonReader->parse(body.c_str(), body.c_str() + body.length(), &recv, &err)) {
		std::cout << "ParseUserData Error is " << err << std::endl;
		return false;
	}
	return true;
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
	Json::Value send;
	send["result"] = false;
	beast::ostream(_response.body()) << Json::writeString(_writerBuilder, send);
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
	_response.prepare_payload();
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
	std::cout << "target is " << _request.target() << std::endl;
	try
	{
		if (_request.method() == beast::http::verb::options) {
			_response.result(beast::http::status::ok);
			_response.prepare_payload();
			beast::http::write(_socket, _response);
			StartRead();
			return;
		}
		if (_role == "student")
		{
			std::cout << "Student Request\n";
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
	catch (std::exception& e)
	{
		std::cout << "HandleRead Exception is " << e.what() << std::endl;
		SetBadRequest();
		return;
	}
	
}

void HttpConnection::StudentRequest()
{
	// Post To Queue
	switch (_request.method())
	{
	case beast::http::verb::get:
	{
		// 个人信息
		if (_request.target() == "/api/student_infoCheck") 
		{
			LogicSystem::Instance().PushToQue([this, self = shared_from_this()]() {
				_studentHandler.get_personal_info(self, _user_id); });
		}
		// 可选课程
		else if (_request.target() == "/api/student_select/get_all")
		{
			LogicSystem::Instance().PushToQue([this, self = shared_from_this()]() {
				_studentHandler.browse_courses(self, _user_id); });
		}
		// 课表
		else if (_request.target().substr(0, sizeof("/api/student_tableCheck") - 1 ) == "/api/student_tableCheck")
		{
			// semester=2025春
			auto pos = _request.target().find("semester=");
			if (pos == std::string_view::npos)
			{
				SetBadRequest();
				return;
			}
			pos += sizeof("semester=") - 1;
			std::string str(_request.target().substr(pos));
			LogicSystem::Instance().PushToQue([this, self = shared_from_this(), str = std::move(str)]() {
				_studentHandler.get_schedule(self, _user_id, str); });
		}
		// 成绩单
		else if (_request.target() == "/api/student_scoreCheck")
		{
			LogicSystem::Instance().PushToQue([this, self = shared_from_this()]() {
				_studentHandler.get_transcript(self, _user_id); });
		}
		// GPA
		else if (_request.target() == "/api/student_scoreCheck/ask_gpa")
		{
			LogicSystem::Instance().PushToQue([this, self = shared_from_this()]() {
				_studentHandler.calculate_gpa(self, _user_id); });
		}
		break;
	}
	case beast::http::verb::post:
	{
		Json::Value recv;
		auto& body = _request.body();
		auto body_str = beast::buffers_to_string(body.data());
		if (!ParseUserData(body_str, recv))
		{
			// 解析失败
			std::cout << "ParseUserData error\n";
			SetBadRequest();
			return;
		}

		// 选课
		if (_request.target() == "/api/student_select/submit")
		{
			uint32_t section_id = stoi(recv["section_id"].asString());
			LogicSystem::Instance().PushToQue([this, self = shared_from_this(), section_id]() {
				_studentHandler.register_course(self, _user_id, section_id); });
		}
		// 退课
		else if (_request.target() == "/api/student_select/cancel")
		{
			uint32_t section_id = stoi(recv["section_id"].asString());
			LogicSystem::Instance().PushToQue([this, self = shared_from_this(), section_id]() {
				_studentHandler.withdraw_course(self, _user_id, section_id); });
		}
		// 修改个人信息
		else if (_request.target() == "/api/student_infoCheck/modify")
		{
			LogicSystem::Instance().PushToQue(
				[this, self = shared_from_this(), recv]() {
					auto birthday = recv["birthday"].asString();
					auto email = recv["email"].asString();
					auto phone = recv["phone"].asString();
					auto password = recv["password"].asString();
					_studentHandler.update_personal_info(self, _user_id, birthday, email, phone, password); 
				});
		}

		break;
	}
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
	{
		// 查课表
		if (_request.target().substr(0, sizeof("/api/teacher_tableCheck/ask?") - 1) 
			== "/api/teacher_tableCheck/ask?") //semester=2025春
		{
			auto pos = _request.target().find("semester=");
			if (pos == std::string_view::npos)
			{
				SetBadRequest();
				return;
			}
			pos += sizeof("semester=") - 1;
			std::string str(_request.target().substr(pos));
			LogicSystem::Instance().PushToQue([this, self = shared_from_this(), str = std::move(str)]() {
				_instructorHandler.get_teaching_sections(self, _user_id, str); });
		}
		// 查学生名单
		else if (_request.target().substr(0, sizeof("/api/teacher_scoreIn/ask?") - 1) 
			== "/api/teacher_scoreIn/ask?")
		{
			auto pos = _request.target().find("section_id=");
			if (pos == std::string_view::npos)
			{
				SetBadRequest();
				return;
			}
			pos += sizeof("section_id=") - 1;
			std::string_view strv(_request.target().substr(pos));
			uint32_t section_id = stoi(std::string(strv));
			LogicSystem::Instance().PushToQue([this, self = shared_from_this(), section_id]() {
				_instructorHandler.get_section_students(self, section_id); });
		}
		// 查个人信息
		else if (_request.target() == "/api/teacher_infoCheck/ask")
		{
			LogicSystem::Instance().PushToQue([this, self = shared_from_this()]() {
				_instructorHandler.get_personal_info(self, _user_id); });
		}
		break;
	}
	case beast::http::verb::post:
	{
		Json::Value recv;
		auto& body = _request.body();
		auto body_str = beast::buffers_to_string(body.data());
		if (!ParseUserData(body_str, recv))
		{
			// 解析失败
			std::cout << "ParseUserData error\n";
			SetBadRequest();
			return;
		}

		if(_request.target() == "/api/teacher_scoreIn/enter")
		{
			uint32_t student_id = stoi(recv["student_id"].asString());
			uint32_t section_id = stoi(recv["section_id"].asString());
			uint32_t score = stoi(recv["score"].asString());
			LogicSystem::Instance().PushToQue([ = , this, self = shared_from_this()]() {
				_instructorHandler.post_grades(self, student_id, section_id, score); });
		} 
		else if (_request.target() == "/api/teacher_infoCheck/submit")
		{
			LogicSystem::Instance().PushToQue(
				[this, self = shared_from_this(), recv]() {
					auto college	= recv["college"].asString();
					auto email		= recv["email"].asString();
					auto phone		= recv["phone"].asString();
					auto password	= recv["password"].asString();
					_instructorHandler.update_personal_info(self, _user_id, college, email, phone, password);
				});
		}
		break;
	}
	default:
		// 设置状态码
		SetBadRequest();
		break;
	}
}

void HttpConnection::AdminRequest()
{
	auto target = _request.target();

	switch (_request.method())
	{
	case beast::http::verb::get:
	{
		// 1.获得某种角色的所有账号信息
		if (target.substr(0, sizeof("/api/admin_accountManage/getInfo?role=") - 1) ==
				"/api/admin_accountManage/getInfo?role=")
		{
			LogicSystem::Instance().PushToQue([this, self = shared_from_this()]()
				{
					constexpr size_t len = sizeof("/api/admin_accountManage/getInfo?role=") - 1;
					std::string str(_request.target().substr(len));
					if (str == "student")
						_adminHandler.get_students_info(self);
					else if (str == "teacher")
						_adminHandler.get_instructors_info(self);
					else
						SetBadRequest();
				});
		}
		// 4.获取某学期的所有课程
		else if(target.substr(0, sizeof("/api/admin_courseManage/getAllCourse?semester=") - 1) == 
			"/api/admin_courseManage/getAllCourse?semester=")
		{
			constexpr size_t len = sizeof("/api/admin_courseManage/getAllCourse?semester=") - 1;
			std::string semester(target.substr(len));
			LogicSystem::Instance().PushToQue([this, self = shared_from_this(), seme = std::move(semester)]()
				{
					_adminHandler.get_sections(self, seme);
				});
		}

		else 
			SetBadRequest();

		break;
	}
	case beast::http::verb::post:
	{
		Json::Value recv;
		auto& body = _request.body();
		auto body_str = beast::buffers_to_string(body.data());
		if (!ParseUserData(body_str, recv))
		{
			// 解析失败
			std::cout << "ParseUserData error\n";
			SetBadRequest();
			return;
		}

		// 2.删除某些账号
		if (target == "/api/admin_accountManage/deleteInfo")
			LogicSystem::Instance().PushToQue([this, self = shared_from_this(), recv]()
				{

					std::vector<uint32_t> user_id;
					std::vector<std::string> role;
					Json::Value deleteInfo = recv["deleteInfo"];
					for (const auto& item : deleteInfo)
					{
						user_id.push_back(stoi(item["user_id"].asString()));
						role.push_back(item["role"].asString());
					}
					_adminHandler.del_someone(self, user_id, role);
				});
		// 3. 增加某个账号
		else if (target == "/api/admin_accountManage/new")
			LogicSystem::Instance().PushToQue([this, self = shared_from_this(), recv]()
				{
					auto role = recv["role"].asString();
					if (role == "student")
					{
						_adminHandler.add_student(self,
							recv["name"].asString(),
							recv["gender"].asString(),
							recv["grade"].asUInt(),
							recv["major"].asString(),
							recv["college_id"].asUInt());
					}
					else if (role == "teacher")
					{
						_adminHandler.add_instructor(self,
							recv["name"].asString(),
							stoi(recv["college_id"].asString()));
					}
					else
						SetBadRequest();
				});
		else if (target == "/api/admin_courseManage/newCourse")
		{
			Json::Value courseData = recv["courseData"];
			if (courseData["course_id"].asString().length() == 0) // 新课程
			{
				LogicSystem::Instance().PushToQue([this, self = shared_from_this(), courseData]()
					{
						_adminHandler.add_section_new(self,
							courseData["college_id"].asUInt(),
							courseData["course_name"].asString(),
							courseData["credit"].asUInt(),
							courseData["type"].asString(),
							courseData["semester"].asString(),
							courseData["teacher_id"].asUInt(),
							courseData["schedule"].asString(),
							courseData["max_capacity"].asUInt(),
							courseData["startWeek"].asUInt(),
							courseData["endWeek"].asUInt(),
							courseData["location"].asString());
					});
			}
			else
			{
				LogicSystem::Instance().PushToQue([this, self = shared_from_this(), courseData]()
					{
						_adminHandler.add_section_old(self,
							courseData["course_id"].asUInt(),
							courseData["semester"].asString(),
							courseData["teacher_id"].asUInt(),
							courseData["schedule"].asString(),
							courseData["max_capacity"].asUInt(),
							courseData["startWeek"].asUInt(),
							courseData["endWeek"].asUInt(),
							courseData["location"].asString());
					});
			}
		}
		else
			SetBadRequest();

		break;
	}
	case beast::http::verb::delete_:
	{
		Json::Value recv;
		auto& body = _request.body();
		auto body_str = beast::buffers_to_string(body.data());
		if (!ParseUserData(body_str, recv))
		{
			// 解析失败
			std::cout << "ParseUserData error\n";
			SetBadRequest();
			return;
		}
		// 5.删除某些课程/api/admin_courseManage/deleteCourse
		if (target == "/api/admin_courseManage/deleteCourse")
		{
			LogicSystem::Instance().PushToQue([this, self = shared_from_this(), recv]()
				{
					std::vector<uint32_t> section_id;
					Json::Value section = recv["section_id"];
					for (const auto& item : section)
					{
						section_id.push_back(stoi(item["section_id"].asString()));
					}
					_adminHandler.del_section(self, section_id);
				});
		}
		else
			SetBadRequest();
		break;
	}
	default:
		SetBadRequest();
		break;
	}
}

void HttpConnection::CloseConnection()
{
	_socket.close();
	_server.ClearConnection(_uuid);
}
