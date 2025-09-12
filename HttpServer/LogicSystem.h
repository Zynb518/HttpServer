#pragma once
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <mysqlx/xdevapi.h>
#include <boost/beast.hpp>
#include "const.h"
#include "HttpConnection.h"
#include <iostream>

namespace beast = boost::beast;
// 处理数据库业务
class LogicSystem
{
public:
	LogicSystem(const LogicSystem&) = delete;
	LogicSystem& operator=(const LogicSystem&) = delete;
	static LogicSystem& Instance();
	void WorkFunc();
	void Stop();

	template<typename... Args>
	void PushToQue(StudentOp op,  Args... args);

	template<typename... Args>
	void PushToQue(InstructorOp op, Args... args);

	template<typename... Args>
	void PushToQue(AdminOp op, Args... args);

	void st_get_personal_info	(std::shared_ptr<HttpConnection> con, int id);
	void st_update_contact_info	(std::shared_ptr<HttpConnection> con, int id, const std::string& phone);
	void st_update_password		(std::shared_ptr<HttpConnection> con, int id, const std::string& password);
	void st_browse_courses		(std::shared_ptr<HttpConnection> con, int id);
	void st_course_details		(std::shared_ptr<HttpConnection> con, int course_id);
	void st_register_course		(std::shared_ptr<HttpConnection> con, int id, int section_id);
	void st_withdraw_course		(std::shared_ptr<HttpConnection> con, int id, int section_id);
	void st_get_schedule		(std::shared_ptr<HttpConnection> con, int id);
	void st_get_current_grades	(std::shared_ptr<HttpConnection> con, int id, int year); // 根据需求
	void st_get_transcript		(std::shared_ptr<HttpConnection> con, int id);
	void st_calculate_gpa		(std::shared_ptr<HttpConnection> con, int id);
private:
	LogicSystem();
	~LogicSystem();
	bool _stop = false;
	std::thread _worker;
	std::condition_variable _conVar;
	std::mutex _mutex;
	std::queue<std::function<void()>> _taskQue;
};

template<typename ...Args>
inline void LogicSystem::PushToQue(StudentOp op, Args... args)
{
	auto captured_args = std::make_tuple(this, std::forward<Args>(args)...);
	switch (op)
	{
	case StudentOp::GET_PERSONAL_INFO:
	{
		auto task = [this, captured_args = std::move(captured_args)]() {
			// 正确调用成员函数
			std::apply(&LogicSystem::st_get_personal_info, std::move(captured_args));
			};
		std::lock_guard<std::mutex> lock(_mutex);
		_taskQue.emplace(std::move(task));
		break;
	}
		
	case StudentOp::UPDATE_CONTACT_INFO:
		break;
	case StudentOp::UPDATE_PASSWORD:
		break;

	// 课程相关
	case StudentOp::BROWSE_COURSES:
		break;
	case StudentOp::GET_COURSE_DETAILS:
		break;
	case StudentOp::REGISTER_COURSE:
		break;
	case StudentOp::WITHDRAW_COURSE:
		break;
	
	// 课表与成绩
	case StudentOp::GET_SCHEDULE:
		break;
	case StudentOp::GET_CURRENT_GRADES:
		break;
	case StudentOp::GET_TRANSCRIPT:
		break;
	case StudentOp::CALCULATE_GPA:
		break;
	}
	_conVar.notify_one();
}

template<typename ...Args>
inline void LogicSystem::PushToQue(InstructorOp op, Args ...args)
{
}

template<typename ...Args>
inline void LogicSystem::PushToQue(AdminOp op, Args ...args)
{
}
