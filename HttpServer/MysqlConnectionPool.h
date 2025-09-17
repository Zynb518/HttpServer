#pragma once
#include <mysqlx/xdevapi.h>
#include <mutex>
class MysqlConnectionPool
{
public:
	MysqlConnectionPool(const MysqlConnectionPool&) = delete;
	MysqlConnectionPool& operator=(const MysqlConnectionPool&) = delete;
	static MysqlConnectionPool& Instance();
	mysqlx::Session GetSession();
private:
	MysqlConnectionPool();
	std::mutex _mutex;
	mysqlx::Client ConnectionPool;
};

