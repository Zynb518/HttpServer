#include "HttpConnection.h"
#include "HttpServer.h"
#include "MysqlConnectionPool.h"
#include "LogicSystem.h"
#include "Log.h"
#include "Tools.h"

#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <unordered_set>

HttpConnection::HttpConnection(boost::asio::io_context& ioc, HttpServer& server) noexcept
	:_socket(ioc), _server(server), _user_id(0)
{
	boost::uuids::uuid a_uuid = boost::uuids::random_generator()();
	_uuid = boost::uuids::to_string(a_uuid);
}


boost::asio::ip::tcp::socket& HttpConnection::GetSocket() noexcept
{
	return _socket;
}

std::string_view HttpConnection::GetUuid() const noexcept
{
	return _uuid;
}
beast::http::response<beast::http::dynamic_body>& HttpConnection::GetResponse() noexcept
{
	return _response;
}

void HttpConnection::ReadLogin()
{
	LOG_INFO("Start ReadLogin");
	beast::http::async_read(_socket, _buffer, _request,
		[this, self = shared_from_this()](beast::error_code ec, size_t bytes_transferred) {
			boost::ignore_unused(bytes_transferred);
			if (!ec)
			{
				LOG_INFO("---- HandleLogin ---- \n target is" << _request.target());
				LOG_INFO("The ThreadID executing HandleLogin is " << std::this_thread::get_id());
				if (!HandleLogin())
				{
					SetUnProcessableEntity("Login-Failure");
					ReadLogin();
				}
			}
			else
			{
				LOG_INFO("ReadLogin async_read error is " << ec.message());
				LOG_INFO("CloseConnection...");
				CloseConnection();
			}
		});

	// StartTimer();
}

void HttpConnection::StartWrite()
{
	LOG_INFO("After StartWrite response\n" << _response);
	_response.prepare_payload();
	beast::http::async_write(_socket, _response,
		[this, self = shared_from_this()](beast::error_code ec, size_t) {
			if (ec)
			{
				LOG_ERROR("HttpConnection async_write error is " << ec.what());
				LOG_ERROR("CloseConnection");
				CloseConnection();
				return;
			}
			StartRead();
		});
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


bool HttpConnection::HandleLogin()
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
			return true;
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
				LOG_INFO("ParseUserData Failed");
				return false;
			}

			_user_id = stoi(recv["user_id"].asString());
			_password = recv["password"].asString();
			_role = recv["role"].asString();

			_role = _role == "teacher" ? "instructor" : _role;
			_role = _role == "admin" ? "administer" : _role;

			if (!_dataValidator.isUserExists(_user_id, _password, _role))
			{
				// 用户不存在
				LOG_INFO("User Not Exist");
				return false;
			}

			LOG_INFO("User Exists, Login Success");
			_response.result(beast::http::status::ok);
			Json::Value send;
			send["result"] = true;
			send["id"] = _user_id;
			send["user_id"] = _user_id;	
			beast::ostream(_response.body()) << Json::writeString(_writerBuilder, send);
			StartWrite();
			return true;
		}
		LOG_INFO("Not Login Request"); // 不符合登陆请求格式
		return false;
	}
	catch(const std::exception& e)
	{
		LOG_INFO("HandleLogin Exception is " << e.what());
		return false;
	}

}

bool HttpConnection::ParseUserData(const std::string& body, Json::Value& recv)
{
	std::unique_ptr<Json::CharReader> jsonReader(_readerBuilder.newCharReader());
	std::string err;
	if (!jsonReader->parse(body.c_str(), body.c_str() + body.length(), &recv, &err)) {
		LOG_ERROR("ParseUserData Error is" << err);
		return false;
	}
	return true;
}

void HttpConnection::SetBadRequest() noexcept
{
	// 设置状态码
	_response.result(beast::http::status::bad_request);
	_response.set(beast::http::field::connection, "close"); // 文件类型
	Json::Value send;
	send["result"] = false;
	beast::ostream(_response.body()) << Json::writeString(_writerBuilder, send);
	_response.prepare_payload();
	WriteBadResponse();
}

void HttpConnection::SetUnProcessableEntity(Json::Value& message) noexcept
{
	// 设置状态码: 字段值不符合业务
	_response.result(beast::http::status::unprocessable_entity);
	// _response.set(beast::http::field::connection, "keep-alive"); 
	beast::ostream(_response.body()) << Json::writeString(_writerBuilder, message);
	_response.prepare_payload();
	WriteBadResponse();
}

void HttpConnection::SetUnProcessableEntity(const std::string& reason) noexcept
{
	// 设置状态码: 字段值不符合业务
	LOG_ERROR(reason);
	_response.result(beast::http::status::unprocessable_entity);
	Json::Value root;
	root["result"] = false;
	root["reason"] = reason;
	beast::ostream(_response.body()) << Json::writeString(_writerBuilder, root);
	_response.prepare_payload();
	WriteBadResponse();
}

void HttpConnection::WriteBadResponse() noexcept
{
	// 注意time_wait
	beast::http::async_write(_socket, _response,
		[self = shared_from_this()](beast::error_code ec, size_t) {
			if (ec)
			{
				LOG_ERROR("WriteBadResponse async_write error is "<< ec.what());
			}
			LOG_INFO("WriteBadResponse()");
		}
	);
}

void HttpConnection::StartRead()
{
	_response.body().clear();

	beast::http::async_read(_socket, _buffer, _request,
		[self = shared_from_this()](beast::error_code ec, size_t bytes_transferred) {
			boost::ignore_unused(bytes_transferred); // 可选
			if (!ec)
			{
				LOG_INFO("The target is " << self->_request.target());
				self->HandleRead();
			}
			else
			{
				LOG_INFO("HttpConnection Async Read Error is " << ec.what());
				LOG_INFO("CloseConnection");
				self->CloseConnection();
			}
		});
}

void HttpConnection::HandleRead()
{
	
	_response.version(_request.version());
	_response.keep_alive(_request.keep_alive());
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
			LOG_INFO("Student Request");
			StudentRequest();
		}
		else if (_role == "instructor")
		{
			LOG_INFO("Instructor Request");
			InstructorRequest();
		}
		else if (_role == "administor")
		{
			LOG_INFO("Administor Request");
			AdminRequest();
		}
		else
		{
			LOG_INFO(" Wrong Role, But It's Impossible");
		}
	}
	catch (std::exception& e)
	{
		LOG_INFO("HandleRead Exception is " << e.what());
		SetBadRequest();
		CloseConnection();
		return;
	}
	StartRead();
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
		else if (_request.target().substr(0, sizeof("/api/student_tableCheck/ask?semester=") - 1 ) ==
			"/api/student_tableCheck/ask?semester=")
		{
			constexpr size_t len = sizeof("/api/student_tableCheck/ask?semester=") - 1;
			std::string str(_request.target().substr(len));

			if (!_dataValidator.isValidSemester(str))
			{
				SetUnProcessableEntity("Semester Format Wrong!");
				return;
			}

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
		else
		{
			LOG_ERROR("StudentRequest Wrong");
			SetUnProcessableEntity("StudentRequest Wrong");
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
			SetUnProcessableEntity("ParseUserData error");
			return;
		}

		// 选课
		if (_request.target() == "/api/student_select/submit")
		{
			uint32_t section_id = stoi(recv["section_id"].asString());

			Json::Value root;
			// 选课时间冲突

			bool ok = _dataValidator.isStTimeConflict(_user_id, section_id);
			if (!ok)
			{
				SetUnProcessableEntity("Course Time Conflict");
				return;
			}

			LogicSystem::Instance().PushToQue([this, self = shared_from_this(), section_id]() {
				// 2.人数冲突 必须放在队列里进行判断
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
			auto birthday = recv["birthday"].asString();
			auto email = recv["email"].asString();
			auto phone = recv["phone"].asString();
			auto password = recv["password"].asString();

			bool ok = false;
			if (!_dataValidator.isValidDate(birthday))
				SetUnProcessableEntity("birthday error");
			else if (!_dataValidator.isValidEmail(email))
				SetUnProcessableEntity("email error");
			else if (!_dataValidator.isValidPhone(phone))
				SetUnProcessableEntity("phone error");
			else if (!_dataValidator.isValidPassword(password))
				SetUnProcessableEntity("password error");
			else
				ok = true;

			if (!ok) return;
			LogicSystem::Instance().PushToQue(
				[
					birthday = std::move(birthday),
					email = std::move(email),
					phone = std::move(phone),
					password = std::move(password),
					this, self = shared_from_this()
				]() {
					_studentHandler.update_personal_info(self, _user_id, birthday, email, phone, password);
				});
		}
		else
		{
			SetUnProcessableEntity("StudentRequest Wrong");
		}

		break;
	}
	default:
		SetUnProcessableEntity("StudentRequest Wrong Method");
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
		if (_request.target().substr(0, sizeof("/api/teacher_tableCheck/ask?semester=") - 1) 
			== "/api/teacher_tableCheck/ask?semester=") //semester=2025春
		{
			constexpr size_t len = sizeof("/api/teacher_tableCheck/ask?semester=") - 1;
			std::string str(_request.target().substr(len));

			if (!_dataValidator.isValidSemester(str))
			{
				SetUnProcessableEntity("Semester Format Wrong!");
				return;
			}
			LogicSystem::Instance().PushToQue([this, self = shared_from_this(), str = std::move(str)]() {
				_instructorHandler.get_teaching_sections(self, _user_id, str); });
		}
		// 查学生名单
		else if (_request.target().substr(0, sizeof("/api/teacher_scoreIn/ask?section_id=") - 1) 
			== "/api/teacher_scoreIn/ask?section_id=")
		{
			constexpr size_t len = sizeof("/api/teacher_scoreIn/ask?section_id=") - 1;
			std::string_view strv(_request.target().substr(len));
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
		else
		{
			SetUnProcessableEntity("InstructorRequest Wrong");
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
			SetUnProcessableEntity("ParseUserData error");
			return;
		}
		// 录入成绩
		if (_request.target() == "/api/teacher_scoreIn/enter")
		{
			auto student_id = stoi(recv["student_id"].asString());
			auto section_id = stoi(recv["section_id"].asString());
			auto score = recv["score"].asUInt();
			if(_dataValidator.isValidScore(score))
			{
				SetUnProcessableEntity("Score Range Error");
				return;
			}

			LogicSystem::Instance().PushToQue([=, this, self = shared_from_this()]() {
				_instructorHandler.post_grades(self, student_id, section_id, score); });
		}
		// 个人信息修改
		else if (_request.target() == "/api/teacher_infoCheck/submit")
		{
			auto college = recv["college"].asString();
			auto email = recv["email"].asString();
			auto phone = recv["phone"].asString();
			auto password = recv["password"].asString();

			bool ok = false;
			if (!_dataValidator.isValidEmail(email))
				SetUnProcessableEntity("Email Error");
			else if (!_dataValidator.isValidPhone(phone))
				SetUnProcessableEntity("Phone Error");
			else if (!_dataValidator.isValidPassword(password))
				SetUnProcessableEntity("Password Error");
			else
				ok = true;
			if (!ok)
				return;

			// 交给当前线程处理
			LogicSystem::Instance().PushToQue(
				[
					college = std::move(college),
					email = std::move(email),
					phone = std::move(phone),
					password = std::move(password),
					this, self = shared_from_this()
				]() {
					_instructorHandler.update_personal_info(self, _user_id, college, email, phone, password);
				});
		}
		else
		{
			SetUnProcessableEntity("InstructorRequest Wrong");
		}

		break;
	}
	default:
		SetUnProcessableEntity("InstructorRequest Wrong Method");
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
			constexpr size_t len = sizeof("/api/admin_accountManage/getInfo?role=") - 1;
			std::string str(_request.target().substr(len));
			if (!_dataValidator.isValidRole(str))
				SetUnProcessableEntity("role is not student or teacher");

			LogicSystem::Instance().PushToQue([this, self = shared_from_this(), str = std::move(str)]()
				{
					if (str == "student")
						_adminHandler.get_students_info(self);
					else if (str == "teacher")
						_adminHandler.get_instructors_info(self);
				});
		}
		// 4.获取某学期的所有课程
		else if(target.substr(0, sizeof("/api/admin_courseManage/getAllCourse?semester=") - 1) == 
			"/api/admin_courseManage/getAllCourse?semester=")
		{
			constexpr size_t len = sizeof("/api/admin_courseManage/getAllCourse?semester=") - 1;
			std::string semester(target.substr(len));
			if (!_dataValidator.isValidSemester(semester))
			{
				SetUnProcessableEntity("Semester Format Wrong!");
				return;
			}

			LogicSystem::Instance().PushToQue([this, self = shared_from_this(), seme = std::move(semester)]()
				{
					_adminHandler.get_sections(self, seme);
				});
		}
		// 8. 获取课程成绩分布
		else if (target.substr(0, sizeof("/api/admin_scoreCheck/course?section_id=") - 1) ==
			"/api/admin_scoreCheck/course?section_id=")
		{
			constexpr size_t len = sizeof("/api/admin_scoreCheck/course?section_id=") - 1;
			uint32_t section_id = stoi( std::string(target.substr(len)) );
			LogicSystem::Instance().PushToQue([this, self = shared_from_this(), section_id]()
				{
					_adminHandler.view_grade_statistics(self, section_id);
				});
		}
		// 9. 获取某学生某学期成绩
		else if (target.substr(0, sizeof("/api/admin_scoreCheck/student?") - 1) ==
			"/api/admin_scoreCheck/student?")
		{ // /api/admin_scoreCheck/student?student_id=5484&semester=2025春
			auto pos_id = target.find("student_id=");
			auto pos_sem = target.find("&semester=");
			if (pos_id == std::string_view::npos || pos_sem == std::string_view::npos)
			{
				SetUnProcessableEntity("student_id= &semester= were not founded");
				return;
			}
			pos_id += sizeof("student_id=") - 1;
			std::string_view strv_id(target.substr(pos_id, pos_sem - pos_id)); // 先让pos_sem 指向&
			pos_sem += sizeof("&semester=") - 1;
			std::string str_sem(target.substr(pos_sem));
			uint32_t user_id = stoi(std::string(strv_id));

			if (!_dataValidator.isValidSemester(str_sem))
			{
				SetUnProcessableEntity("Semester Format Wrong!");
				return;
			}

			LogicSystem::Instance().PushToQue([this, self = shared_from_this(), user_id, seme = std::move(str_sem)]()
				{
					_adminHandler.get_student_grades(self, user_id, seme);
				});
		}
		// 10.教授所教课程的平均成绩
		else if (target.substr(0, sizeof("/api/admin_scoreCheck/teacher?") - 1) ==
			"/api/admin_scoreCheck/teacher?")
		{// /api/admin_scoreCheck/teacher?teacher_id=546&semester=2025春
			auto pos_id = target.find("teacher_id=");
			auto pos_sem = target.find("&semester=");
			if (pos_id == std::string_view::npos || pos_sem == std::string_view::npos)
			{
				SetUnProcessableEntity("teacher_id= &semester= were not founded");
				return;
			}
			pos_id += sizeof("teacher_id=") - 1;
			std::string_view strv_id(target.substr(pos_id, pos_sem - pos_id)); // 先让pos_sem 指向&
			pos_sem += sizeof("&semester=") - 1;
			std::string str_sem(target.substr(pos_sem));
			uint32_t user_id = stoi(std::string(strv_id));

			if (!_dataValidator.isValidSemester(str_sem))
			{
				SetUnProcessableEntity("Semester Format Wrong!");
				return;
			}

			LogicSystem::Instance().PushToQue([this, self = shared_from_this(), user_id, seme = std::move(str_sem)]()
				{
					_adminHandler.get_instructor_grades(self, user_id, seme);
				});
		}
		// 11. 获取所有学院
		else if (target == "/api/admin/general/getAllCollege")
		{
			LogicSystem::Instance().PushToQue([this, self = shared_from_this()]()
				{
					_adminHandler.get_colleges(self);
				});
		}
		// 12.获取某个学院的所有课程
		else if (target.substr(0, sizeof("/api/admin/general/getCollegeCourse?college_id=") - 1) ==
			"/api/admin/general/getCollegeCourse?college_id=")
		{
			constexpr size_t len = sizeof("/api/admin/general/getCollegeCourse?college_id=") - 1;
			std::string str(target.substr(len));
			uint32_t college_id = stoi(str);
			LogicSystem::Instance().PushToQue([this, self = shared_from_this(), college_id]()
				{
					_adminHandler.get_college_courses(self, college_id);
				});
		}
		// 获取所有老师
		else if(target.substr(0, sizeof("/api/admin/general/getCollegeTeacher?college_id=") - 1) ==
			"/api/admin/general/getCollegeTeacher?college_id=")
		{
			constexpr size_t len = sizeof("/api/admin/general/getCollegeInstructor?college_id=") - 1;
			std::string str(target.substr(len));
			uint32_t college_id = stoi(str);
			LogicSystem::Instance().PushToQue([this, self = shared_from_this(), college_id]()
				{
					_adminHandler.get_college_instructors(self, college_id);
				});
		}
		else
		{
			SetUnProcessableEntity("AdminRequest Wrong");
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
			SetUnProcessableEntity("ParseUserData error");
			return;
		}

		// 2.删除某些账号
		if (target == "/api/admin_accountManage/deleteInfo")
		{
			std::vector<uint32_t> user_ids;
			std::vector<std::string> roles;
			auto& deleteInfo = recv["deleteInfo"];
			for (const auto& item : deleteInfo)
			{
				user_ids.push_back(stoi(item["user_id"].asString()));
				roles.push_back(item["role"].asString());
			}
			for (const auto& item : roles)
			{
				if (!_dataValidator.isValidRole(item))
				{
					SetUnProcessableEntity("delete Account Role error");
					return;
				}
			}

			LogicSystem::Instance().PushToQue(
				[
					user_ids = std::move(user_ids),
					roles = std::move(roles),
					this, self = shared_from_this()
				]()
				{
					
					_adminHandler.del_someone(self, user_ids, roles);
				});
		}
		// 3. 增加某个账号
		else if (target == "/api/admin_accountManage/new")
		{
			auto role = recv["role"].asString();

			auto name = recv["name"].asString();
			if (_dataValidator.isValidName(name))
			{
				SetUnProcessableEntity("Name too long or Zero");
				return;
			}

			if (role == "student")
			{
				auto gender = recv["gender"].asString();
				auto grade = recv["grade"].asUInt();
				auto major_id = stoi(recv["major_id"].asString());
				auto college_id = recv["college_id"].asUInt();

				bool ok = false;
				if (_dataValidator.isValidGender(gender))
					SetUnProcessableEntity("Gender Error");
				else if(_dataValidator.isValidGrade(grade))
					SetUnProcessableEntity("Grade Error");
				else
					ok = true;

				if (!ok) return;

				LogicSystem::Instance().PushToQue(
					[
						=, name = std::move(name),
						gender = std::move(gender),
						this, self = shared_from_this()
					]()
					{
						_adminHandler.add_student(self,
							name, gender, grade, major_id, college_id);
							
					});
				
			}
			else if (role == "teacher")
			{
				auto college_id = stoi(recv["college_id"].asString());
				LogicSystem::Instance().PushToQue(
					[ =, name = std::move(name),
					  this, self = shared_from_this() ]()
					{
						_adminHandler.add_instructor(self,
							name, college_id);
					});

			}
			else
				SetUnProcessableEntity("Role was not teacher or student");
		}
		// 增加某些课程
		else if (target == "/api/admin_courseManage/newCourse")
		{
			auto& courseData = recv["courseData"];
			// 提取公共部分
			auto semester		= courseData["semester"].asString();
			auto teacher_id		= courseData["teacher_id"].asUInt();
			auto& schedule		= courseData["schedule"]; // Json::Value
			auto max_capacity	= courseData["max_capacity"].asUInt();
			auto startWeek		= courseData["startWeek"].asUInt();
			auto endWeek		= courseData["endWeek"].asUInt();
			auto location		= courseData["location"].asString();

			std::string schedule_str = _dataValidator.isValidSchedule(schedule);
			// 检验公共部分
			bool ok = false;
			if (!_dataValidator.isValidSemester(semester))
				SetUnProcessableEntity("Semester Format Error");
			else if (schedule_str.length() == 0)
				SetUnProcessableEntity("Schedule Format Error");
			else if (max_capacity < 30 || max_capacity > 150)
				SetUnProcessableEntity("max_capacity < 30 || max_capacity > 150");
			else if (startWeek == 0 || startWeek > 20 || endWeek == 0 || endWeek > 20 || startWeek > endWeek)
				SetUnProcessableEntity("startWeek or endWeek Error");
			else if (location.length() <= 4 || location.length() > 100)
				SetUnProcessableEntity("location.length() <= 4 || location.length() > 100");
			else
				ok = true;
			if (!ok) return;

			// 新增的课程 不需要去掉原有的时间 section_id 默认参数0
			ok = _dataValidator.isInstrTimeConflict(teacher_id, startWeek, endWeek, schedule_str);
			if (!ok)
			{
				SetUnProcessableEntity("Course Time Conflict");
				return;
			}

			if (courseData["course_id"].asString().length() == 0) // 新课程
			{
				auto college_id		= courseData["college_id"].asUInt();
				auto course_name	= courseData["course_name"].asString();
				auto credit			= courseData["credit"].asUInt();
				auto type			= courseData["type"].asString();

				std::transform(type.begin(), type.end(), type.begin(), ::toupper);

				if (_dataValidator.isValidName(course_name))
					SetUnProcessableEntity("Course Name Too Long");
				else if (_dataValidator.isValidCredit(credit))
					SetUnProcessableEntity("Credit == 0 Or Credit > 7");
				else if (_dataValidator.isValidType(type))
					SetUnProcessableEntity("Type Error");
				else 
					ok = true;

				if (!ok) return;

				LogicSystem::Instance().PushToQue(
					[
						=, 
						course_name = std::move(course_name), type = std::move(type), 
						semester = std::move(semester), schedule = std::move(schedule_str), 
						location = std::move(location),
						this, self = shared_from_this()
					]()
					{
						_adminHandler.add_section_new(self, college_id, course_name, credit,
							type, semester, teacher_id, schedule, max_capacity,
							startWeek, endWeek, location);
							
					});
			}
			else
			{
				auto course_id = courseData["course_id"].asUInt();
				LogicSystem::Instance().PushToQue(
					[
						=,
						semester = std::move(semester), schedule = std::move(schedule_str), 
						location = std::move(location),
						this, self = shared_from_this()
					]()
					{
						_adminHandler.add_section_old(self,
							course_id, semester,
							teacher_id, schedule,
							max_capacity, startWeek,
							endWeek, location);
					});
			}
		}
		// 修改课程信息
		else if (target == "/api/admin_courseManage/changeCourse")
		{
			auto& courseData = recv["courseData"];
			auto section_id = courseData["section_id"].asUInt();
			auto teacher_id = courseData["teacher_id"].asUInt();
			auto& schedule = courseData["schedule"];
			auto startWeek = courseData["startWeek"].asUInt();
			auto endWeek = courseData["endWeek"].asUInt();
			auto location = courseData["location"].asString();

			auto schedule_str = _dataValidator.isValidSchedule(schedule);

			bool ok = false;
			if (schedule_str.length() == 0)
				SetUnProcessableEntity("Schedule Format Error");
			else if (_dataValidator.isValidWeek(startWeek, endWeek) )
				SetUnProcessableEntity("startWeek or endWeek Error");
			else if (_dataValidator.isValidLocation(location))
				SetUnProcessableEntity("location.length() <= 4 || location.length() > 100");
			else
				ok = true;

			if (!ok) return;

			// 修改课程时间 检查时间冲突 需要去掉当前课程的时间
			ok = _dataValidator.isInstrTimeConflict(teacher_id, startWeek, endWeek, schedule_str, section_id);

			if (!ok)
			{
				SetUnProcessableEntity("Course Time Conflict");
				return;
			}

			LogicSystem::Instance().PushToQue(
				[
					=,
					schedule = std::move(schedule_str),
					location = std::move(location),
					this, self = shared_from_this()
				]()
				{
					_adminHandler.modify_section(self,
						section_id, teacher_id,
						schedule, startWeek,
						endWeek, location);
				});
		}
		else
		{
			SetUnProcessableEntity("AdminRequest Wrong");
		}

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
			SetUnProcessableEntity("ParseUserData error");
			return;
		}
		// 5.删除某些课程/api/admin_courseManage/deleteCourse
		if (target == "/api/admin_courseManage/deleteCourse")
		{
			std::vector<uint32_t> section_ids;
			auto& section = recv["section_id"];
			mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();

			// 检查section_id 是否合法
			for (const auto& item : section)
			{
				uint32_t id = stoull(item["section_id"].asString());
				if (_dataValidator.isValidSectionId(id))
				{
					SetUnProcessableEntity("section_id error");
					return;
				}
				else
					section_ids.push_back(id);
			}

			LogicSystem::Instance().PushToQue([this, self = shared_from_this(), section_ids = std::move(section_ids)]()
				{
					_adminHandler.del_section(self, section_ids);
				});
		}
		else
		{
			SetUnProcessableEntity("AdminRequest Wrong");
		}
		break;
	}
	default:
		SetUnProcessableEntity("AdminRequest Wrong Method");
		break;
	}
}

void HttpConnection::CloseConnection() noexcept
{
	_socket.close();
	_server.ClearConnection(_uuid);
}

DataValidator HttpConnection::_dataValidator;
// MysqlStReqHandler HttpConnection::_studentHandler;
// MysqlInstrReqHandler HttpConnection::_instructorHandler;
// MysqlAdmReqHandler HttpConnection::_adminHandler;