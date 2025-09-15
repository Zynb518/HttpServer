#pragma once
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include "const.h"
#include "StudentHandler.h"
#include "InstructorHandler.h"
#include "AdminHandler.h"

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

	
private:
	LogicSystem();
	~LogicSystem();
	bool _stop = false;
	std::thread _worker;
	std::condition_variable _conVar;
	std::mutex _mutex;
	std::queue<std::function<void()>> _taskQue;

	StudentHandler studentHandler;
	InstructorHandler instructorHandler;
	AdminHandler adminHandler;
};

template<typename ...Args>
inline void LogicSystem::PushToQue(StudentOp op, Args... args)
{
	auto captured_args = std::make_tuple(&studentHandler, std::forward<Args>(args)...);
	switch (op)
	{
	case StudentOp::GET_PERSONAL_INFO:
	{
		auto task = [this, captured_args = std::move(captured_args)]() {
			std::apply(&StudentHandler::get_personal_info, std::move(captured_args));
			};
		std::lock_guard<std::mutex> lock(_mutex);
		_taskQue.emplace(std::move(task));
		break;
	}

	// 课程相关
	case StudentOp::BROWSE_COURSES:
	{
		auto task = [this, captured_args = std::move(captured_args)]() {
			std::apply(&StudentHandler::browse_courses, std::move(captured_args));
			};
		std::lock_guard<std::mutex> lock(_mutex);
		_taskQue.emplace(std::move(task));
		break;
	}
	case StudentOp::REGISTER_COURSE:
	{
		auto task = [this, captured_args = std::move(captured_args)]() {
			std::apply(&StudentHandler::register_course, std::move(captured_args));
			};
		std::lock_guard<std::mutex> lock(_mutex);
		_taskQue.emplace(std::move(task));
		break;
	}
	case StudentOp::WITHDRAW_COURSE:
	{
		auto task = [this, captured_args = std::move(captured_args)]() {
			std::apply(&StudentHandler::withdraw_course, std::move(captured_args));
			};
		std::lock_guard<std::mutex> lock(_mutex);
		_taskQue.emplace(std::move(task));
		break;
	}
	// 课表与成绩
	case StudentOp::GET_SCHEDULE:
	{
		break;
	}
	case StudentOp::GET_CURRENT_GRADES:
		break;
	case StudentOp::GET_TRANSCRIPT:
	{
		auto task = [this, captured_args = std::move(captured_args)]() {
			std::apply(&StudentHandler::get_transcript, std::move(captured_args));
			};
		std::lock_guard<std::mutex> lock(_mutex);
		_taskQue.emplace(std::move(task));
		break;
	}
	case StudentOp::CALCULATE_GPA:
	{
		auto task = [this, captured_args = std::move(captured_args)]() {
			std::apply(&StudentHandler::calculate_gpa, std::move(captured_args));
			};
		std::lock_guard<std::mutex> lock(_mutex);
		_taskQue.emplace(std::move(task));
		break;
	}
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
