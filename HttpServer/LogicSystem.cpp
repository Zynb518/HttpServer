#include "LogicSystem.h"
#include "MysqlConnectionPool.h"
#include <json/json.h>

LogicSystem& LogicSystem::Instance()
{
	static LogicSystem instance;
	return instance;
}

void LogicSystem::WorkFunc()
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

void LogicSystem::Stop()
{
	_stop = true;
}

LogicSystem::LogicSystem()
	:_worker([this]() {this->WorkFunc(); })
{

}

LogicSystem::~LogicSystem()
{
	if(_worker.joinable())
		_worker.join();
}

