#include "RequestDispatcher.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "HttpConnection.h"
#include "LogicSystem.h"
#include "MysqlAdmReqHandler.h"
#include "MysqlConnectionPool.h"
#include "MysqlInstrReqHandler.h"
#include "MysqlStReqHandler.h"
#include "DataValidator.h"
#include "Log.h"


inline RequestDispatcher::RequestDispatcher(
    std::shared_ptr<HttpConnection> connection,
    beast::http::request<beast::http::dynamic_body>& request,
    MysqlStReqHandler& studentHandler,
    MysqlInstrReqHandler& instructorHandler,
    MysqlAdmReqHandler& adminHandler) noexcept
    : _connection(std::move(connection))
    , _request(request)
    , _studentHandler(studentHandler)
    , _instructorHandler(instructorHandler)
    , _adminHandler(adminHandler)
{
}

inline RequestDispatcher::DispatchResult RequestDispatcher::Dispatch()
{
    DispatchResult result{};
    auto resolved = Resolve();
    if (!resolved.matched)
    {
        return result;
    }

    result.matched = true;
    if (resolved.task.has_value())
    {
        LogicSystem::Instance().PushToQue(std::move(resolved.task.value()));
        result.queued = true;
    }
    return result;
}

inline std::optional<RequestDispatcher::Task> RequestDispatcher::ResolveTaskOnly()
{
    auto resolved = Resolve();
    if (!resolved.matched)
    {
        return std::nullopt;
    }
    return resolved.task;
}

inline RequestDispatcher::Resolved RequestDispatcher::Resolve()
{
    Resolved resolved{};
    const std::string_view target{ _request.target().data(), _request.target().size() };

    if (StartsWith(target, "/api/login"))
    {
        return resolved;
    }

    const auto& role = _connection->GetRole();
    if (role == "student")
    {
        return ResolveStudent(target);
    }
    if (role == "instructor" || role == "teacher")
    {
        return ResolveInstructor(target);
    }
    if (role == "administer" || role == "administor" || role == "admin")
    {
        return ResolveAdmin(target);
    }

    ReportInvalid("Unknown role");
    resolved.matched = true;
    return resolved;
}

inline RequestDispatcher::Resolved RequestDispatcher::ResolveStudent(std::string_view target)
{
    switch (_request.method())
    {
    case beast::http::verb::get:
        return ResolveStudentGet(target);
    case beast::http::verb::post:
        return ResolveStudentPost(target);
    default:
        ReportInvalid("StudentRequest Wrong Method");
        return Resolved{ true, std::nullopt };
    }
}

inline RequestDispatcher::Resolved RequestDispatcher::ResolveStudentGet(std::string_view target)
{
    const auto userId = _connection->GetUserId();

    if (target == "/api/student_infoCheck")
    {
        return Resolved{ true, MakeTask([handler = &_studentHandler, self = _connection, userId]()
        {
            handler->get_personal_info(self, userId);
        }) };
    }

    if (target == "/api/student_select/get_all")
    {
        return Resolved{ true, MakeTask([handler = &_studentHandler, self = _connection, userId]()
        {
            handler->browse_courses(self, userId);
        }) };
    }

    constexpr std::string_view schedulePrefix = "/api/student_tableCheck/ask?semester=";
    if (StartsWith(target, schedulePrefix))
    {
        std::string semester(target.substr(schedulePrefix.size()));
        if (!_dataValidator.isValidSemester(semester))
        {
            ReportInvalid("Semester Format Wrong!");
            return Resolved{ true, std::nullopt };
        }

        return Resolved{ true, MakeTask([
            handler = &_studentHandler,
            self = _connection,
            userId,
            sem = std::move(semester)
        ]()
        {
            handler->get_schedule(self, userId, sem);
        }) };
    }

    if (target == "/api/student_scoreCheck")
    {
        return Resolved{ true, MakeTask([handler = &_studentHandler, self = _connection, userId]()
        {
            handler->get_transcript(self, userId);
        }) };
    }

    if (target == "/api/student_scoreCheck/ask_gpa")
    {
        return Resolved{ true, MakeTask([handler = &_studentHandler, self = _connection, userId]()
        {
            handler->calculate_gpa(self, userId);
        }) };
    }

    ReportInvalid("StudentRequest Wrong");
    return Resolved{ true, std::nullopt };
}

inline RequestDispatcher::Resolved RequestDispatcher::ResolveStudentPost(std::string_view target)
{
    Json::Value payload;
    if (!ParseRequestBody(payload))
    {
        ReportInvalid("ParseUserData error");
        return Resolved{ true, std::nullopt };
    }

    const auto userId = _connection->GetUserId();

    if (target == "/api/student_select/submit")
    {
        auto section_id = static_cast<uint32_t>(std::stoul(payload["section_id"].asString()));

        if (!_dataValidator.isStTimeConflict(userId, section_id))
        {
            ReportInvalid("Course Time Conflict");
            return Resolved{ true, std::nullopt };
        }

        return Resolved{ true, MakeTask([=, handler = &_studentHandler, self = _connection]()
        {
            handler->register_course(self, userId, section_id);
        }) };
    }

    if (target == "/api/student_select/cancel")
    {
        auto section_id = static_cast<uint32_t>(std::stoul(payload["section_id"].asString()));
        return Resolved{ true, MakeTask([=, handler = &_studentHandler, self = _connection]()
        {
            handler->withdraw_course(self, userId, section_id);
        }) };
    }

    if (target == "/api/student_infoCheck/modify")
    {
        auto birthday = payload["birthday"].asString();
        auto email = payload["email"].asString();
        auto phone = payload["phone"].asString();
        auto password = payload["password"].asString();

        bool ok = false;
        if (!_dataValidator.isValidDate(birthday))
            ReportInvalid("birthday error");
        else if (!_dataValidator.isValidEmail(email))
            ReportInvalid("email error");
        else if (!_dataValidator.isValidPhone(phone))
            ReportInvalid("phone error");
        else if (!_dataValidator.isValidPassword(password))
            ReportInvalid("password error");
        else
            ok = true;

        if (!ok)
        {
            return Resolved{ true, std::nullopt };
        }

        return Resolved{ true, MakeTask([
            handler = &_studentHandler,
            self = _connection,
            userId,
            birthday = std::move(birthday),
            email = std::move(email),
            phone = std::move(phone),
            password = std::move(password)
        ]() mutable
        {
            handler->update_personal_info(self, userId, birthday, email, phone, password);
        }) };
    }

    ReportInvalid("StudentRequest Wrong");
    return Resolved{ true, std::nullopt };
}

inline RequestDispatcher::Resolved RequestDispatcher::ResolveInstructor(std::string_view target)
{
    switch (_request.method())
    {
    case beast::http::verb::get:
        return ResolveInstructorGet(target);
    case beast::http::verb::post:
        return ResolveInstructorPost(target);
    default:
        ReportInvalid("InstructorRequest Wrong Method");
        return Resolved{ true, std::nullopt };
    }
}

inline RequestDispatcher::Resolved RequestDispatcher::ResolveInstructorGet(std::string_view target)
{
    const auto userId = _connection->GetUserId();

    constexpr std::string_view schedulePrefix = "/api/teacher_tableCheck/ask?semester=";
    if (StartsWith(target, schedulePrefix))
    {
        std::string semester(target.substr(schedulePrefix.size()));
        if (!_dataValidator.isValidSemester(semester))
        {
            ReportInvalid("Semester Format Wrong!");
            return Resolved{ true, std::nullopt };
        }

        return Resolved{ true, MakeTask([
            handler = &_instructorHandler,
            self = _connection,
            userId,
            sem = std::move(semester)
        ]()
        {
            handler->get_teaching_sections(self, userId, sem);
        }) };
    }

    constexpr std::string_view sectionPrefix = "/api/teacher_scoreIn/ask?section_id=";
    if (StartsWith(target, sectionPrefix))
    {
        std::string_view sectionView(target.substr(sectionPrefix.size()));
        uint32_t section_id = static_cast<uint32_t>(std::stoul(std::string(sectionView)));

        return Resolved{ true, MakeTask([
            handler = &_instructorHandler,
            self = _connection,
            section_id
        ]()
        {
            handler->get_section_students(self, section_id);
        }) };
    }

    if (target == "/api/teacher_infoCheck/ask")
    {
        return Resolved{ true, MakeTask([
            handler = &_instructorHandler,
            self = _connection,
            userId
        ]()
        {
            handler->get_personal_info(self, userId);
        }) };
    }

    ReportInvalid("InstructorRequest Wrong");
    return Resolved{ true, std::nullopt };
}

inline RequestDispatcher::Resolved RequestDispatcher::ResolveInstructorPost(std::string_view target)
{
    Json::Value payload;
    if (!ParseRequestBody(payload))
    {
        ReportInvalid("ParseUserData error");
        return Resolved{ true, std::nullopt };
    }

    const auto userId = _connection->GetUserId();

    if (target == "/api/teacher_scoreIn/enter")
    {
        auto student_id = static_cast<uint32_t>(std::stoul(payload["student_id"].asString()));
        auto section_id = static_cast<uint32_t>(std::stoul(payload["section_id"].asString()));
        auto score = payload["score"].asUInt();
        if (_dataValidator.isValidScore(score))
        {
            ReportInvalid("Score Range Error");
            return Resolved{ true, std::nullopt };
        }

        return Resolved{ true, MakeTask([
            =,
            handler = &_instructorHandler,
            self = _connection
        ]()
        {
            handler->post_grades(self, student_id, section_id, score);
        }) };
    }

    if (target == "/api/teacher_infoCheck/submit")
    {
        auto college = payload["college"].asString();
        auto email = payload["email"].asString();
        auto phone = payload["phone"].asString();
        auto password = payload["password"].asString();

        bool ok = false;
        if (!_dataValidator.isValidEmail(email))
            ReportInvalid("Email Error");
        else if (!_dataValidator.isValidPhone(phone))
            ReportInvalid("Phone Error");
        else if (!_dataValidator.isValidPassword(password))
            ReportInvalid("Password Error");
        else
            ok = true;

        if (!ok)
        {
            return Resolved{ true, std::nullopt };
        }

        return Resolved{ true, MakeTask([
            handler = &_instructorHandler,
            self = _connection,
            userId,
            college = std::move(college),
            email = std::move(email),
            phone = std::move(phone),
            password = std::move(password)
        ]() mutable
        {
            handler->update_personal_info(self, userId,
                std::move(college), std::move(email), std::move(phone), std::move(password));
        }) };
    }

    ReportInvalid("InstructorRequest Wrong");
    return Resolved{ true, std::nullopt };
}

inline RequestDispatcher::Resolved RequestDispatcher::ResolveAdmin(std::string_view target)
{
    switch (_request.method())
    {
    case beast::http::verb::get:
        return ResolveAdminGet(target);
    case beast::http::verb::post:
        return ResolveAdminPost(target);
    case beast::http::verb::put:
        return ResolveAdminPut(target);
    case beast::http::verb::delete_:
        return ResolveAdminDelete(target);
    default:
        ReportInvalid("AdminRequest Wrong Method");
        return Resolved{ true, std::nullopt };
    }
}

inline RequestDispatcher::Resolved RequestDispatcher::ResolveAdminGet(std::string_view target)
{
    constexpr std::string_view accountRolePrefix = "/api/admin_accountManage/getInfo?role=";
    if (StartsWith(target, accountRolePrefix))
    {
        std::string role(target.substr(accountRolePrefix.size()));
        if (!_dataValidator.isValidRole(role))
        {
            ReportInvalid("role is not student or teacher");
            return Resolved{ true, std::nullopt };
        }

        return Resolved{ true, MakeTask([
            handler = &_adminHandler,
            self = _connection,
            role = std::move(role)
        ]() mutable
        {
            if (role == "student")
                handler->get_students_info(self);
            else if (role == "teacher")
                handler->get_instructors_info(self);
        }) };
    }

    constexpr std::string_view allCoursePrefix = "/api/admin_courseManage/getAllCourse?semester=";
    if (StartsWith(target, allCoursePrefix))
    {
        std::string semester(target.substr(allCoursePrefix.size()));
        if (!_dataValidator.isValidSemester(semester))
        {
            ReportInvalid("Semester Format Wrong!");
            return Resolved{ true, std::nullopt };
        }

        return Resolved{ true, MakeTask([
            handler = &_adminHandler,
            self = _connection,
            sem = std::move(semester)
        ]()
        {
            handler->get_sections(self, sem);
        }) };
    }

    constexpr std::string_view courseStatsPrefix = "/api/admin_scoreCheck/course?section_id=";
    if (StartsWith(target, courseStatsPrefix))
    {
        uint32_t section_id = static_cast<uint32_t>(std::stoul(std::string(target.substr(courseStatsPrefix.size()))));

        return Resolved{ true, MakeTask([
            handler = &_adminHandler,
            self = _connection,
            section_id
        ]()
        {
            handler->view_grade_statistics(self, section_id);
        }) };
    }

    constexpr std::string_view studentGradePrefix = "/api/admin_scoreCheck/student?";
    if (StartsWith(target, studentGradePrefix))
    {
        auto pos_id = target.find("student_id=");
        auto pos_sem = target.find("&semester=");
        if (pos_id == std::string_view::npos || pos_sem == std::string_view::npos)
        {
            ReportInvalid("student_id= &semester= were not founded");
            return Resolved{ true, std::nullopt };
        }

        pos_id += sizeof("student_id=") - 1;
        std::string_view sidView(target.substr(pos_id, pos_sem - pos_id));
        pos_sem += sizeof("&semester=") - 1;
        std::string semester(target.substr(pos_sem));
        uint32_t student_id = static_cast<uint32_t>(std::stoul(std::string(sidView)));

        if (!_dataValidator.isValidSemester(semester))
        {
            ReportInvalid("Semester Format Wrong!");
            return Resolved{ true, std::nullopt };
        }

        return Resolved{ true, MakeTask([
            handler = &_adminHandler,
            self = _connection,
            student_id,
            sem = std::move(semester)
        ]()
        {
            handler->get_student_grades(self, student_id, sem);
        }) };
    }

    constexpr std::string_view teacherGradePrefix = "/api/admin_scoreCheck/teacher?";
    if (StartsWith(target, teacherGradePrefix))
    {
        auto pos_id = target.find("teacher_id=");
        auto pos_sem = target.find("&semester=");
        if (pos_id == std::string_view::npos || pos_sem == std::string_view::npos)
        {
            ReportInvalid("teacher_id= &semester= were not founded");
            return Resolved{ true, std::nullopt };
        }

        pos_id += sizeof("teacher_id=") - 1;
        std::string_view tidView(target.substr(pos_id, pos_sem - pos_id));
        pos_sem += sizeof("&semester=") - 1;
        std::string semester(target.substr(pos_sem));
        uint32_t teacher_id = static_cast<uint32_t>(std::stoul(std::string(tidView)));

        if (!_dataValidator.isValidSemester(semester))
        {
            ReportInvalid("Semester Format Wrong!");
            return Resolved{ true, std::nullopt };
        }

        return Resolved{ true, MakeTask([
            handler = &_adminHandler,
            self = _connection,
            teacher_id,
            sem = std::move(semester)
        ]()
        {
            handler->get_instructor_grades(self, teacher_id, sem);
        }) };
    }

    if (target == "/api/admin/general/getAllCollege")
    {
        return Resolved{ true, MakeTask([
            handler = &_adminHandler,
            self = _connection
        ]()
        {
            handler->get_colleges(self);
        }) };
    }

    constexpr std::string_view collegeCoursePrefix = "/api/admin/general/getCollegeCourse?college_id=";
    if (StartsWith(target, collegeCoursePrefix))
    {
        uint32_t college_id = static_cast<uint32_t>(std::stoul(std::string(target.substr(collegeCoursePrefix.size()))));

        return Resolved{ true, MakeTask([
            handler = &_adminHandler,
            self = _connection,
            college_id
        ]()
        {
            handler->get_college_courses(self, college_id);
        }) };
    }

    constexpr std::string_view collegeTeacherPrefix = "/api/admin/general/getCollegeTeacher?college_id=";
    if (StartsWith(target, collegeTeacherPrefix))
    {
        uint32_t college_id = static_cast<uint32_t>(std::stoul(std::string(target.substr(collegeTeacherPrefix.size()))));

        return Resolved{ true, MakeTask([
            handler = &_adminHandler,
            self = _connection,
            college_id
        ]()
        {
            handler->get_college_instructors(self, college_id);
        }) };
    }

    ReportInvalid("AdminRequest Wrong");
    return Resolved{ true, std::nullopt };
}

inline RequestDispatcher::Resolved RequestDispatcher::ResolveAdminPost(std::string_view target)
{
    Json::Value payload;
    if (!ParseRequestBody(payload))
    {
        ReportInvalid("ParseUserData error");
        return Resolved{ true, std::nullopt };
    }

    if (target == "/api/admin_accountManage/deleteInfo")
    {
        std::vector<uint32_t> user_ids;
        std::vector<std::string> roles;
        auto& deleteInfo = payload["deleteInfo"];
        for (const auto& item : deleteInfo)
        {
            user_ids.push_back(static_cast<uint32_t>(std::stoul(item["user_id"].asString())));
            roles.push_back(item["role"].asString());
        }

        for (const auto& role : roles)
        {
            if (!_dataValidator.isValidRole(role))
            {
                ReportInvalid("delete Account Role error");
                return Resolved{ true, std::nullopt };
            }
        }

        return Resolved{ true, MakeTask([
            handler = &_adminHandler,
            self = _connection,
            user_ids = std::move(user_ids),
            roles = std::move(roles)
        ]() mutable
        {
            handler->del_someone(self, user_ids, roles);
        }) };
    }

    if (target == "/api/admin_accountManage/new")
    {
        auto role = payload["role"].asString();
        auto name = payload["name"].asString();
        if (_dataValidator.isValidName(name))
        {
            ReportInvalid("Name too long or Zero");
            return Resolved{ true, std::nullopt };
        }

        if (role == "student")
        {
            auto gender = payload["gender"].asString();
            auto grade = payload["grade"].asUInt();
            auto major_id = static_cast<uint32_t>(std::stoul(payload["major_id"].asString()));
            auto college_id = payload["college_id"].asUInt();

            bool ok = false;
            if (_dataValidator.isValidGender(gender))
                ReportInvalid("Gender Error");
            else if (_dataValidator.isValidGrade(grade))
                ReportInvalid("Grade Error");
            else
                ok = true;

            if (!ok)
            {
                return Resolved{ true, std::nullopt };
            }

            return Resolved{ true, MakeTask([
                =,
                handler = &_adminHandler,
                self = _connection,
                name = std::move(name),
                gender = std::move(gender)
            ]() mutable
            {
                handler->add_student(self, name, gender, grade, major_id, college_id);
            }) };
        }
        else if (role == "teacher")
        {
            auto college_id = static_cast<uint32_t>(std::stoul(payload["college_id"].asString()));
            return Resolved{ true, MakeTask([
                handler = &_adminHandler,
                self = _connection,
                name = std::move(name),
                college_id
            ]() mutable
            {
                handler->add_instructor(self, name, college_id);
            }) };
        }

        ReportInvalid("Role was not teacher or student");
        return Resolved{ true, std::nullopt };
    }

    if (target == "/api/admin_courseManage/newCourse")
    {
        auto& courseData = payload["courseData"];

        auto semester = courseData["semester"].asString();
        auto teacher_id = courseData["teacher_id"].asUInt();
        auto& schedule = courseData["schedule"];
        auto max_capacity = courseData["max_capacity"].asUInt();
        auto startWeek = courseData["startWeek"].asUInt();
        auto endWeek = courseData["endWeek"].asUInt();
        auto location = courseData["location"].asString();

        std::string schedule_str = _dataValidator.isValidSchedule(schedule);
        bool ok = false;
        if (!_dataValidator.isValidSemester(semester))
            ReportInvalid("Semester Format Error");
        else if (schedule_str.length() == 0)
            ReportInvalid("Schedule Format Error");
        else if (max_capacity < 30 || max_capacity > 150)
            ReportInvalid("max_capacity < 30 || max_capacity > 150");
        else if (startWeek == 0 || startWeek > 20 || endWeek == 0 || endWeek > 20 || startWeek > endWeek)
            ReportInvalid("startWeek or endWeek Error");
        else if (location.length() <= 4 || location.length() > 100)
            ReportInvalid("location.length() <= 4 || location.length() > 100");
        else
            ok = true;

        if (!ok)
        {
            return Resolved{ true, std::nullopt };
        }

        ok = _dataValidator.isInstrTimeConflict(teacher_id, startWeek, endWeek, schedule_str);
        if (!ok)
        {
            ReportInvalid("Course Time Conflict");
            return Resolved{ true, std::nullopt };
        }

        if (courseData["course_id"].asString().empty())
        {
            auto college_id = courseData["college_id"].asUInt();
            auto course_name = courseData["course_name"].asString();
            auto credit = courseData["credit"].asUInt();
            auto type = courseData["type"].asString();

            std::transform(type.begin(), type.end(), type.begin(), ::toupper);

            if (_dataValidator.isValidName(course_name))
                ReportInvalid("Course Name Too Long");
            else if (_dataValidator.isValidCredit(credit))
                ReportInvalid("Credit == 0 Or Credit > 7");
            else if (_dataValidator.isValidType(type))
                ReportInvalid("Type Error");
            else
                ok = true;

            if (!ok)
            {
                return Resolved{ true, std::nullopt };
            }

            return Resolved{ true, MakeTask([
                =,
                handler = &_adminHandler,
                self = _connection,
                course_name = std::move(course_name),
                type = std::move(type),
                semester = std::move(semester),
                schedule = std::move(schedule_str),
                location = std::move(location)
            ]() mutable
            {
                handler->add_section_new(self, college_id, course_name, credit, type,
                    semester, teacher_id, schedule, max_capacity, startWeek, endWeek, location);
            }) };
        }
        else
        {
            auto course_id = courseData["course_id"].asUInt();

            return Resolved{ true, MakeTask([
                =,
                handler = &_adminHandler,
                self = _connection,
                semester = std::move(semester),
                schedule = std::move(schedule_str),
                location = std::move(location)
            ]() mutable
            {
                handler->add_section_old(self, course_id, semester, teacher_id,
                    schedule, max_capacity, startWeek, endWeek, location);
            }) };
        }
    }

    if (target == "/api/admin_courseManage/changeCourse")
    {
        auto& courseData = payload["courseData"];
        auto section_id = courseData["section_id"].asUInt();
        auto teacher_id = courseData["teacher_id"].asUInt();
        auto& schedule = courseData["schedule"];
        auto startWeek = courseData["startWeek"].asUInt();
        auto endWeek = courseData["endWeek"].asUInt();
        auto location = courseData["location"].asString();

        auto schedule_str = _dataValidator.isValidSchedule(schedule);

        bool ok = false;
        if (schedule_str.length() == 0)
            ReportInvalid("Schedule Format Error");
        else if (_dataValidator.isValidWeek(startWeek, endWeek))
            ReportInvalid("startWeek or endWeek Error");
        else if (_dataValidator.isValidLocation(location))
            ReportInvalid("location.length() <= 4 || location.length() > 100");
        else
            ok = true;

        if (!ok)
        {
            return Resolved{ true, std::nullopt };
        }

        ok = _dataValidator.isInstrTimeConflict(teacher_id, startWeek, endWeek, schedule_str, section_id);
        if (!ok)
        {
            ReportInvalid("Course Time Conflict");
            return Resolved{ true, std::nullopt };
        }

        return Resolved{ true, MakeTask([
            =,
            handler = &_adminHandler,
            self = _connection,
            schedule = std::move(schedule_str),
            location = std::move(location)
        ]() mutable
        {
            handler->modify_section(self, section_id, teacher_id, schedule, startWeek, endWeek, location);
        }) };
    }

    ReportInvalid("AdminRequest Wrong");
    return Resolved{ true, std::nullopt };
}

inline RequestDispatcher::Resolved RequestDispatcher::ResolveAdminPut(std::string_view)
{
    ReportInvalid("AdminRequest Wrong Method");
    return Resolved{ true, std::nullopt };
}

inline RequestDispatcher::Resolved RequestDispatcher::ResolveAdminDelete(std::string_view target)
{
    Json::Value payload;
    if (!ParseRequestBody(payload))
    {
        ReportInvalid("ParseUserData error");
        return Resolved{ true, std::nullopt };
    }

    if (target == "/api/admin_courseManage/deleteCourse")
    {
        std::vector<uint32_t> section_ids;
        auto& section = payload["section_id"];
        mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
        (void)sess;

        for (const auto& item : section)
        {
            uint32_t id = static_cast<uint32_t>(std::stoull(item["section_id"].asString()));
            if (_dataValidator.isValidSectionId(id))
            {
                ReportInvalid("section_id error");
                return Resolved{ true, std::nullopt };
            }
            section_ids.push_back(id);
        }

        return Resolved{ true, MakeTask([
            handler = &_adminHandler,
            self = _connection,
            section_ids = std::move(section_ids)
        ]() mutable
        {
            handler->del_section(self, section_ids);
        }) };
    }

    ReportInvalid("AdminRequest Wrong");
    return Resolved{ true, std::nullopt };
}

template<typename Functor>
inline std::optional<RequestDispatcher::Task> RequestDispatcher::MakeTask(Functor&& functor) noexcept
{
    return std::optional<Task>{ Task(std::forward<Functor>(functor)) };
}

inline void RequestDispatcher::ReportInvalid(std::string_view reason)
{
    _connection->SetUnProcessableEntity(std::string(reason));
}

inline bool RequestDispatcher::ParseRequestBody(Json::Value& out)
{
    auto& body = _request.body();
    auto raw = beast::buffers_to_string(body.data());
    return ParseUserData(raw, out);
}

inline bool RequestDispatcher::StartsWith(std::string_view text, std::string_view prefix) noexcept
{
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

inline bool RequestDispatcher::ParseUserData(const std::string& body, Json::Value& out)
{
    std::unique_ptr<Json::CharReader> jsonReader(_readerBuilder.newCharReader());
    std::string err;
    if (!jsonReader->parse(body.c_str(), body.c_str() + body.length(), &out, &err))
    {
        LOG_ERROR("ParseUserData Error: " << err);
        return false;
    }
    return true;
}

inline DataValidator RequestDispatcher::_dataValidator{};
inline Json::CharReaderBuilder RequestDispatcher::_readerBuilder{};
