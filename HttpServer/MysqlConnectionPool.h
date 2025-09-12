#pragma once
#include <mysqlx/xdevapi.h>

class MysqlConnectionPool
{
public:
	MysqlConnectionPool(const MysqlConnectionPool&) = delete;
	MysqlConnectionPool& operator=(const MysqlConnectionPool&) = delete;
	static MysqlConnectionPool& Instance();
	mysqlx::Session GetSession();
private:
	MysqlConnectionPool();
	mysqlx::Client ConnectionPool;
};

