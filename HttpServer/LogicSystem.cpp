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

void LogicSystem::st_get_personal_info(std::shared_ptr<HttpConnection> con, int id)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	mysqlx::RowResult res = sess.sql("CALL st_get_personal_info(?)").bind(id).execute();
	mysqlx::Row row = res.fetchOne();
	std::string name = row[1].get<std::string>();
	std::cout << "name is " << name << std::endl;
	Json::Value root;
	root["name"] = name;
	beast::ostream(con->GetResponse().body()) << root.toStyledString();
	con->GetResponse().prepare_payload();
	con->StartWrite();
}
