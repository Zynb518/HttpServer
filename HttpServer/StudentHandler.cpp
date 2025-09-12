#include "StudentHandler.h"
#include <mysqlx/xdevapi.h>
#include "MysqlConnectionPool.h"
#include "HttpConnection.h"
#include <json/json.h>
#include <iostream>

void StudentHandler::get_personal_info(std::shared_ptr<HttpConnection> con, int id)
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
