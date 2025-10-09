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
		LOG_ERROR("LogicSystem WorkFunc Exception is " << e.what());
	}
	
}

void LogicSystem::Stop() noexcept
{
	_stop = true;
}

LogicSystem::LogicSystem() noexcept
	:_worker([this]() { this->WorkFunc(); })
{

}

LogicSystem::~LogicSystem() noexcept
{
	if(_worker.joinable())
		_worker.join();
}

