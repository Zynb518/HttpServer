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

