#include "HttpConnection.h"
#include "HttpServer.h"
#include "MysqlConnectionPool.h"
#include "LogicSystem.h"
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <iostream>
#include "Log.h"
#include "Tools.h"

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

std::string_view HttpConnection::GetUuid() noexcept
{
	return _uuid;
}
beast::http::response<beast::http::dynamic_body>& HttpConnection::GetResponse() noexcept
{
	return _response;
}

void HttpConnection::StartWrite()
{
	LOG_INFO("After StartWrite response\n" << _response);
	beast::http::async_write(_socket, _response,
		[this, self = shared_from_this()](beast::error_code ec, size_t) {
			if (ec)
			{
				LOG_INFO("HttpConnection async_write error is " << ec.what());
				CloseConnection();
				return;
			}
			StartRead();
		});
}

void HttpConnection::ReadLogin()
{
	LOG_INFO("Start ReadLogin");
	beast::http::async_read(_socket, _buffer, _request,
		[this, self = shared_from_this() ](beast::error_code ec, size_t bytes_transferred) {
			boost::ignore_unused(bytes_transferred);
			if (!ec)
			{
				LOG_INFO("---- HandleLogin ---- \n target is" << _request.target());
				if (!HandleLogin())
				{
					SetBadRequest("Login-Failure");
					ReadLogin();
				}
			}
			else
			{
				LOG_INFO("ReadLogin async_read error is " << ec.message());
				CloseConnection();
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

		// ��½����
		if (_request.method() == beast::http::verb::post &&
			_request.target() == "/api/login/post")
		{

			auto& body = _request.body();
			auto body_str = beast::buffers_to_string(body.data());

			Json::Value recv;
			if (!ParseUserData(body_str, recv))
			{
				// ����ʧ��
				LOG_INFO("ParseUserData Failed");
				return false;
			}

			_user_id = stoi(recv["user_id"].asString());
			_password = recv["password"].asString();
			_role = recv["role"].asString();

			_role = _role == "teacher" ? "instructor" : _role;
			_role = _role == "admin" ? "administer" : _role;

			if (!UserExists(_user_id, _password, _role))
			{
				// �û�������
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
			WriteLoginSuccess();
			return true;
		}
		LOG_INFO("Not Login Request"); // �����ϵ�½�����ʽ
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
		std::cout << "ParseUserData Error is " << err << std::endl;
		return false;
	}
	return true;
}

bool HttpConnection::UserExists(uint32_t user_id, const std::string& password, const std::string& role)
{
	try {
		// ��ȡ users ��
		// mysqlx::Session sess = MySQLConnectionPool::Instance().GetSession();
		mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
		mysqlx::Table users = sess.getSchema("scut_sims").getTable("users");

		// ������ѯ���� - ʹ������(user_id, role)���������ƥ��
		auto result = users.select("user_id")
			.where("user_id = :uid AND password = :pwd AND role = :r")
			.bind("uid", user_id)
			.bind("pwd", password)
			.bind("r", role)  // ֻ��Ҫ����Ƿ���ڣ����Ʒ���1����¼
			.execute();
		sess.close();
		// ����Ƿ��н��
		return result.count() > 0;
	}
	catch (const mysqlx::Error& err) {
		LOG_DEBUG("Database Error: " << err.what());
		return false;
	}
}

void HttpConnection::SetBadRequest() noexcept
{
	// ����״̬��
	_response.result(beast::http::status::bad_request);
	_response.set(beast::http::field::connection, "close"); // �ļ�����
	Json::Value send;
	send["result"] = false;
	beast::ostream(_response.body()) << Json::writeString(_writerBuilder, send);
	_response.prepare_payload();
	WriteBadResponse();
}

void HttpConnection::SetBadRequest(Json::Value& message) noexcept
{
	// ����״̬��: �ֶ�ֵ������ҵ��
	_response.result(beast::http::status::unprocessable_entity);
	// _response.set(beast::http::field::connection, "keep-alive"); 
	beast::ostream(_response.body()) << Json::writeString(_writerBuilder, message);
	_response.prepare_payload();
	WriteBadResponse();
}

void HttpConnection::SetBadRequest(const std::string& reason) noexcept
{
	// ����״̬��: �ֶ�ֵ������ҵ��
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
	// ע��time_wait
	beast::http::async_write(_socket, _response,
		[self = shared_from_this()](beast::error_code ec, size_t) {
			if (ec)
			{
				std::cout << "WriteBadResponse async_write error is "
					<< ec.what();
			}
			// self->CloseConnection();
		}
	);
}

void HttpConnection::WriteLoginSuccess()
{
	_response.prepare_payload();
	// ע��time_wait
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
			boost::ignore_unused(bytes_transferred); // ��ѡ
			if (!ec)
			{
				LOG_INFO("The target is " << self->_request.target());
				self->HandleRead();
			}
			else
			{
				LOG_INFO("HttpConnection Async Read Error is " << ec.what());
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
		// ������Ϣ
		if (_request.target() == "/api/student_infoCheck") 
		{
			LogicSystem::Instance().PushToQue([this, self = shared_from_this()]() {
				_studentHandler.get_personal_info(self, _user_id); });
		}
		// ��ѡ�γ�
		else if (_request.target() == "/api/student_select/get_all")
		{
			LogicSystem::Instance().PushToQue([this, self = shared_from_this()]() {
				_studentHandler.browse_courses(self, _user_id); });
		}
		// �α�
		else if (_request.target().substr(0, sizeof("/api/student_tableCheck/ask?semester=") - 1 ) ==
			"/api/student_tableCheck/ask?semester=")
		{
			constexpr size_t len = sizeof("/api/student_tableCheck/ask?semester=") - 1;
			std::string str(_request.target().substr(len));
			LogicSystem::Instance().PushToQue([this, self = shared_from_this(), str = std::move(str)]() {
				_studentHandler.get_schedule(self, _user_id, str); });
		}
		// �ɼ���
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
			SetBadRequest("StudentRequest Wrong");
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
			// ����ʧ��
			SetBadRequest("ParseUserData error");
			return;
		}

		// ѡ��
		if (_request.target() == "/api/student_select/submit")
		{
			uint32_t section_id = stoi(recv["section_id"].asString());

			Json::Value root;
			// 2.������ͻ
			mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
			mysqlx::Table sections = sess.getSchema("scut_sims").getTable("sections");
			auto row = sections.select("1").where("section_id = :sid AND `number` < max_capacity")
				.bind("sid", section_id)
				.execute().fetchOne();

			if (row.isNull())
			{
				SetBadRequest("Section Full");
				return;
			}

			// 3.ѡ��ʱ���ͻ
			static const std::unordered_map<std::string, int> weekdayMap = {
				{GetUTF8ForDatabase(L"��һ"), 1},
				{GetUTF8ForDatabase(L"�ܶ�"), 2},
				{GetUTF8ForDatabase(L"����"), 3},
				{GetUTF8ForDatabase(L"����"), 4},
				{GetUTF8ForDatabase(L"����"), 5},
				{GetUTF8ForDatabase(L"����"), 6},
				{GetUTF8ForDatabase(L"����"), 7}
			};

			bool timetable[22][8][9];
			memset(timetable, 0, sizeof timetable); // ok

			mysqlx::RowResult result = sess.sql(
				"SELECT s.start_week, s.end_week, s.time_slot "
				"FROM enrollments e "
				"JOIN sections s USING (section_id) "
				"WHERE e.student_id = ? AND e.status = 'Enrolling'"
			).bind(_user_id).execute();

			for (auto row : result)
			{
				ProcessRow(row, timetable, weekdayMap, 1);
			}

			row = sess.sql(
				"SELECT start_week, end_week, time_slot"
				"FROM sections s"
				"WHERE section_id = ?;"
			).bind(section_id).execute().fetchOne();

			bool ok = ProcessRow(row, timetable, weekdayMap, 0);
			if (!ok)
			{
				SetBadRequest("Course Time Conflict");
				return;
			}

			LogicSystem::Instance().PushToQue([this, self = shared_from_this(), section_id]() {
				_studentHandler.register_course(self, _user_id, section_id); });
		}
		// �˿�
		else if (_request.target() == "/api/student_select/cancel")
		{
			uint32_t section_id = stoi(recv["section_id"].asString());
			LogicSystem::Instance().PushToQue([this, self = shared_from_this(), section_id]() {
				_studentHandler.withdraw_course(self, _user_id, section_id); });
		}
		// �޸ĸ�����Ϣ
		else if (_request.target() == "/api/student_infoCheck/modify")
		{
			auto birthday = recv["birthday"].asString();
			auto email = recv["email"].asString();
			auto phone = recv["phone"].asString();
			auto password = recv["password"].asString();

			bool ok = false;
			if (!DataValidator::isValidDate(birthday))
				SetBadRequest("birthday error");
			else if (!DataValidator::isValidEmail(email))
				SetBadRequest("birthday error");
			else if (!DataValidator::isValidPhone(phone))
				SetBadRequest("birthday error");
			else if (!DataValidator::isValidPassword(password))
				SetBadRequest("birthday error");
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
			SetBadRequest("StudentRequest Wrong");
		}

		break;
	}
	default:
		SetBadRequest("StudentRequest Wrong Method");
		break;
	}
}

// 1 ��Ӧ��ѡ��0��Ӧ����ѡ����δѡ
bool HttpConnection::ProcessRow(const mysqlx::Row& row, bool timetable[22][8][9],
	const std::unordered_map<std::string, int>& weekdayMap, size_t choice)
{
	auto start_week = row[0].get<uint32_t>();
	auto end_week = row[1].get<uint32_t>();
	auto time_slot = row[2].get<std::string>();
	LOG_INFO(
		   "start-week: " << start_week << " "
		<< "end-week: " << end_week << " "
		<< "time_slot: " << time_slot << std::endl);

	std::string_view str_v(time_slot);

	do // һ��row���п����ж���ʱ��
	{
		// ����time_slot
		uint32_t Day = 0;
		uint32_t L = 0, R = 0;

		auto t = str_v.find(" "); // �ָ�����ڼ�
		if (t != std::string_view::npos)
		{
			const std::string s(str_v.data(), t);
			auto it = weekdayMap.find(s);
			if (it != weekdayMap.end())
				Day = it->second;
		}
		else
		{
			LOG_ERROR("Format Error");
			break;
		}

		t = str_v.find("-"); // �ҵ�����
		if (t != std::string::npos)
		{
			L = time_slot[t - 1] - '0';
			R = time_slot[t + 1] - '0';
		}
		else
		{
			LOG_ERROR("Format Error");
			break;
		}

		LOG_INFO("day: " << Day << "; time: " << L << "-" << R);
		// �Ѿ��еĿ�
		if (choice)
		{
			for (size_t i = start_week; i <= end_week; ++i)
				for (size_t j = L; j <= R; ++j)
					timetable[i][Day][j] = true;
		}
		else // ����Ҫѡ�Ŀ�
		{
			for (size_t i = start_week; i <= end_week; ++i)
				for (size_t j = L; j <= R; ++j)
					if (timetable[i][Day][j]) return false;
					else timetable[i][Day][j] = true;
		}

		t = str_v.find(",");
		if (t != std::string_view::npos) // ���滹��ʱ��
			str_v = str_v.substr(t + 1);
		else
			break;

	} while (str_v.size() > 0);

	return true;
}

void HttpConnection::InstructorRequest()
{
	switch (_request.method())
	{
	case beast::http::verb::get:
	{
		// ��α�
		if (_request.target().substr(0, sizeof("/api/teacher_tableCheck/ask?semester=") - 1) 
			== "/api/teacher_tableCheck/ask?semester=") //semester=2025��
		{
			constexpr size_t len = sizeof("/api/teacher_tableCheck/ask?semester=") - 1;
			std::string str(_request.target().substr(len));
			LogicSystem::Instance().PushToQue([this, self = shared_from_this(), str = std::move(str)]() {
				_instructorHandler.get_teaching_sections(self, _user_id, str); });
		}
		// ��ѧ������
		else if (_request.target().substr(0, sizeof("/api/teacher_scoreIn/ask?section_id=") - 1) 
			== "/api/teacher_scoreIn/ask?section_id=")
		{
			constexpr size_t len = sizeof("/api/teacher_scoreIn/ask?section_id=") - 1;
			std::string_view strv(_request.target().substr(len));
			uint32_t section_id = stoi(std::string(strv));
			LogicSystem::Instance().PushToQue([this, self = shared_from_this(), section_id]() {
				_instructorHandler.get_section_students(self, section_id); });
		}
		// �������Ϣ
		else if (_request.target() == "/api/teacher_infoCheck/ask")
		{
			LogicSystem::Instance().PushToQue([this, self = shared_from_this()]() {
				_instructorHandler.get_personal_info(self, _user_id); });
		}
		else
		{
			SetBadRequest("InstructorRequest Wrong");
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
			// ����ʧ��
			SetBadRequest("ParseUserData error");
			return;
		}

		if (_request.target() == "/api/teacher_scoreIn/enter")
		{
			uint32_t student_id = stoi(recv["student_id"].asString());
			uint32_t section_id = stoi(recv["section_id"].asString());
			uint32_t score = stoi(recv["score"].asString());
			LogicSystem::Instance().PushToQue([=, this, self = shared_from_this()]() {
				_instructorHandler.post_grades(self, student_id, section_id, score); });
		}
		else if (_request.target() == "/api/teacher_infoCheck/submit")
		{
			auto college = recv["college"].asString();
			auto email = recv["email"].asString();
			auto phone = recv["phone"].asString();
			auto password = recv["password"].asString();

			bool ok = false;
			if (!DataValidator::isValidEmail(email))
				SetBadRequest("Email Error");
			else if (!DataValidator::isValidPhone(phone))
				SetBadRequest("Phone Error");
			else if (!DataValidator::isValidPassword(password))
				SetBadRequest("Password Error");
			else
				ok = true;
			if (!ok)
				return;

			// Ҳ��ѡ�񽻸���ǰ�̴߳������ｻ���߼��̴߳���recv
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
			SetBadRequest("InstructorRequest Wrong");
		}

		break;
	}
	default:
		SetBadRequest("InstructorRequest Wrong Method");
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
		// 1.���ĳ�ֽ�ɫ�������˺���Ϣ
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
						SetBadRequest("role is not student or teacher");

				});
		}
		// 4.��ȡĳѧ�ڵ����пγ�
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
		// 8. ��ȡ�γ̳ɼ��ֲ�
		else if (target.substr(0, sizeof("/api/admin_scoreCheck/course?section_id=") - 1) ==
			"/api/admin_scoreCheck/course?section_id=")
		{
			constexpr size_t len = sizeof("/api/admin_scoreCheck/course?section_id=") - 1;
			std::string str(target.substr(len));
			uint32_t section_id = stoi(str);
			LogicSystem::Instance().PushToQue([this, self = shared_from_this(), section_id]()
				{
					_adminHandler.view_grade_statistics(self, section_id);
				});
		}
		// 9. ��ȡĳѧ��ĳѧ�ڳɼ�
		else if (target.substr(0, sizeof("/api/admin_scoreCheck/student?") - 1) ==
			"/api/admin_scoreCheck/student?")
		{ // /api/admin_scoreCheck/student?student_id=5484&semester=2025��
			auto pos_id = target.find("student_id=");
			auto pos_sem = target.find("&semester=");
			if (pos_id == std::string_view::npos || pos_sem == std::string_view::npos)
			{
				SetBadRequest("student_id= &semester= were not founded");
				return;
			}
			pos_id += sizeof("student_id=") - 1;
			std::string_view strv_id(target.substr(pos_id, pos_sem - pos_id)); // ����pos_sem ָ��&
			pos_sem += sizeof("&semester=") - 1;
			std::string str_sem(target.substr(pos_sem));
			uint32_t user_id = stoi(std::string(strv_id));
			LogicSystem::Instance().PushToQue([this, self = shared_from_this(), user_id, seme = std::move(str_sem)]()
				{
					_adminHandler.get_student_grades(self, user_id, seme);
				});
		}
		// 10.�������̿γ̵�ƽ���ɼ�
		else if (target.substr(0, sizeof("/api/admin_scoreCheck/teacher?") - 1) ==
			"/api/admin_scoreCheck/teacher?")
		{// /api/admin_scoreCheck/teacher?teacher_id=546&semester=2025��
			auto pos_id = target.find("teacher_id=");
			auto pos_sem = target.find("&semester=");
			if (pos_id == std::string_view::npos || pos_sem == std::string_view::npos)
			{
				SetBadRequest("teacher_id= &semester= were not founded");
				return;
			}
			pos_id += sizeof("teacher_id=") - 1;
			std::string_view strv_id(target.substr(pos_id, pos_sem - pos_id)); // ����pos_sem ָ��&
			pos_sem += sizeof("&semester=") - 1;
			std::string str_sem(target.substr(pos_sem));
			uint32_t user_id = stoi(std::string(strv_id));
			LogicSystem::Instance().PushToQue([this, self = shared_from_this(), user_id, seme = std::move(str_sem)]()
				{
					_adminHandler.get_instructor_grades(self, user_id, seme);
				});
		}
		// 11. ��ȡ����ѧԺ
		else if (target == "/api/admin/general/getAllCollege")
		{
			LogicSystem::Instance().PushToQue([this, self = shared_from_this()]()
				{
					_adminHandler.get_colleges(self);
				});
		}
		// 12.��ȡĳ��ѧԺ�����пγ�
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
			SetBadRequest("AdminRequest Wrong");
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
			// ����ʧ��
			SetBadRequest("ParseUserData error");
			return;
		}

		// 2.ɾ��ĳЩ�˺�
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
		// 3. ����ĳ���˺�
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
							stoi(recv["major_id"].asString()),
							recv["college_id"].asUInt());
					}
					else if (role == "teacher")
					{
						_adminHandler.add_instructor(self,
							recv["name"].asString(),
							stoi(recv["college_id"].asString()));
					}
					else
						SetBadRequest("Role was not teacher or student");

				});
		else if (target == "/api/admin_courseManage/newCourse")
		{
			Json::Value courseData = recv["courseData"];
			if (courseData["course_id"].asString().length() == 0) // �¿γ�
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
		else if (target == "/api/admin_courseManage/changeCourse")
		{
			Json::Value courseData = recv["courseData"];
			LogicSystem::Instance().PushToQue([this, self = shared_from_this(), courseData]()
				{
					_adminHandler.modify_section(self,
						courseData["section_id"].asUInt(),
						courseData["teacher_id"].asUInt(),
						courseData["schedule"].asString(),
						courseData["startWeek"].asUInt(),
						courseData["endWeek"].asUInt(),
						courseData["location"].asString());
				});
		}
		else
		{
			SetBadRequest("AdminRequest Wrong");
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
			// ����ʧ��
			SetBadRequest("ParseUserData error");
			return;
		}
		// 5.ɾ��ĳЩ�γ�/api/admin_courseManage/deleteCourse
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
		{
			SetBadRequest("AdminRequest Wrong");
		}
		break;
	}
	default:
		SetBadRequest("AdminRequest Wrong Method");
		break;
	}
}

void HttpConnection::CloseConnection() noexcept
{
	_socket.close();
	_server.ClearConnection(_uuid);
}

StudentHandler HttpConnection::_studentHandler;
InstructorHandler HttpConnection::_instructorHandler;
AdminHandler HttpConnection::_adminHandler;