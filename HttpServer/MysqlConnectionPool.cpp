#include "MysqlConnectionPool.h"

MysqlConnectionPool::MysqlConnectionPool()
	:ConnectionPool("httpserver:114514ZY@localhost/scut_sims", mysqlx::ClientOption::POOL_MAX_SIZE, 10)
{
	
}

MysqlConnectionPool& MysqlConnectionPool::Instance()
{
	static MysqlConnectionPool instance;
	return instance;
}

mysqlx::Session MysqlConnectionPool::GetSession()
{
	return ConnectionPool.getSession();
}