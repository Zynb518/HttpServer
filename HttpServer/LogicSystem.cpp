#include "LogicSystem.h"
#include "MysqlConnectionPool.h"
#include <json/json.h>
#include <iostream>
#include "Log.h"

LogicSystem& LogicSystem::Instance() noexcept
{
	static LogicSystem instance;
	return instance;
}

void LogicSystem::WorkFunc()
{
	try
	{
		while (!_stop)
		{
			std::unique_lock<std::mutex> lock(_mutex);
			_conVar.wait(lock, [this]() {return !_taskQue.empty() || _stop; });
			if (_stop && _taskQue.empty())
				return;
			auto task = std::move(_taskQue.front());
			_taskQue.pop();
			lock.unlock();
			task();
		}
	}
	catch (const std::exception& e)
	{
		LOG_ERROR("LogicSystem WorkFunc Exception: " << e.what());
		// 考虑是否需要重新启动工作线程
	}
	catch (...)
	{
		LOG_ERROR("LogicSystem WorkFunc Unknown Exception");
	}
	
}

void LogicSystem::Stop() noexcept
{
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_stop = true;
	}
	_conVar.notify_one();  // 确保工作线程能立即响应停止
}

LogicSystem::LogicSystem() noexcept
	:_worker([this]() { this->WorkFunc(); })
{

}

LogicSystem::~LogicSystem() noexcept
{
	Stop();
	if(_worker.joinable())
		_worker.join();
}
