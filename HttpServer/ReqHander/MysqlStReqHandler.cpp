#include <json/json.h>
#include <iostream>
#include "MysqlStReqHandler.h"
#include <mysqlx/xdevapi.h>
#include "MysqlConnectionPool.h"
#include "HttpConnection.h"
#include "Log.h"
#include "Tools.h"

void MysqlStReqHandler::get_personal_info(std::shared_ptr<HttpConnection> con, uint32_t id)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	mysqlx::RowResult res = sess.sql("CALL st_get_personal_info(?)").bind(id).execute();
	sess.close();
	mysqlx::Row row = res.fetchOne();
	// 有没有可能为空
	Json::Value root;
	root["id"]			= std::to_string(row[0].get<uint32_t>());
	root["user_id"]		= std::to_string(row[0].get<uint32_t>());
	root["name"]		= row[1].get<std::string>();
	root["gender"]		= row[2].get<std::string>();
	root["grade"]		= row[3].get<uint32_t>();
	root["major"]		= row[4].get<std::string>();
	root["college"]		= row[5].get<std::string>();
	root["birthday"]	= row[6].get<std::string>();
	root["email"]		= row[7].get<std::string>();
	root["phone"]		= row[8].get<std::string>();

	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->StartWrite();
}

void MysqlStReqHandler::update_personal_info(std::shared_ptr<HttpConnection> con, uint32_t id,
	StringRef birthday, StringRef email, StringRef phone, StringRef password)
{
	Json::Value root;

	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	sess.sql("CALL st_update_personal_info(?,?,?,?,?)")
		.bind(id).bind(birthday).bind(email).bind(phone).bind(password)
		.execute();
	sess.close();
	
	root["result"] = true;
	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->StartWrite();
}

void MysqlStReqHandler::browse_courses(std::shared_ptr<HttpConnection> con, uint32_t id)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	mysqlx::RowResult res = sess.sql("CALL st_browse_courses(?)")
		.bind(id).execute();
	sess.close();

	Json::Value root;
	Json::Value arr;
	mysqlx::Row row;
	while (row = res.fetchOne())
	{
		Json::Value obj;
		obj["section_id"]	= std::to_string(row[0].get<uint32_t>());
		obj["college"]		= row[1].get<std::string>();
		obj["courseName"]	= row[2].get<std::string>();
		obj["teacher"]		= row[3].get<std::string>();

		auto str = row[4].get<std::string>(); // 时间
		Json::Value timeArr;
		ParseTimeString(str, timeArr);
		LOG_INFO("timeArr-" << timeArr.toStyledString());
		obj["schedule"]		= timeArr;
		obj["credit"]		= row[5].get<double>();
		obj["semester"]		= row[6].get<std::string>();
		obj["startWeek"]	= row[7].get<uint32_t>();
		obj["endWeek"]		= row[8].get<uint32_t>();
		obj["location"]		= row[9].get<std::string>();
		obj["selected"]		= row[10].get<bool>();
		arr.append(obj);
	}
	root["result"] = true;
	root["courseObj"] = arr;
	
	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->StartWrite();
}

void MysqlStReqHandler::register_course(std::shared_ptr<HttpConnection> con, uint32_t id, uint32_t section_id)
{
	Json::Value root;
	// 2.人数冲突
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	mysqlx::Table sections = sess.getSchema("scut_sims").getTable("sections");
	auto row = sections.select("1").where("section_id = :sid AND `number` < max_capacity")
		.bind("sid", section_id)
		.execute().fetchOne();

	if (row.isNull())
	{
		con->SetUnProcessableEntity("Section Full");
		return;
	}

	sess.sql("CALL st_register_course(?,?)").bind(id).bind(section_id).execute();
	root["result"] = true;
	sess.close();

	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->StartWrite();
}



void MysqlStReqHandler::withdraw_course(std::shared_ptr<HttpConnection> con, uint32_t id, uint32_t section_id)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	sess.sql("CALL st_withdraw_course(?,?)").bind(id).bind(section_id).execute();
	sess.close();

	Json::Value root;
	root["result"] = true;

	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->StartWrite();
}

void MysqlStReqHandler::get_schedule(std::shared_ptr<HttpConnection> con, uint32_t id, StringRef semester)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	mysqlx::RowResult res = sess.sql("CALL st_get_schedule(?,?)").bind(id).bind(semester).execute();
	mysqlx::Row row;
	sess.close();
	Json::Value root;
	Json::Value arr;
	
	while (row = res.fetchOne())
	{
		Json::Value obj;
		obj["courseName"]	= row[0].get<std::string>();
		obj["teacher"]		= row[1].get<std::string>();
		obj["location"]		= row[2].get<std::string>();
		Json::Value timeArr;
		auto str = row[3].get<std::string>();
		ParseTimeString(str, timeArr);
		obj["schedule"]		= timeArr;
		obj["credit"]		= row[4].get<double>();
		obj["startWeek"]	= row[5].get<uint32_t>();
		obj["endWeek"]		= row[6].get<uint32_t>();
		arr.append(obj);
	}
	root["result"] = true;
	root["courseObj"] = arr;
	
	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->StartWrite();
}

void MysqlStReqHandler::get_transcript(std::shared_ptr<HttpConnection> con, uint32_t id)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	mysqlx::RowResult res = sess.sql("CALL st_get_transcript(?)").bind(id).execute();
	sess.close();
	Json::Value root;
	Json::Value arr;
	mysqlx::Row row;

	while (row = res.fetchOne())
	{
		Json::Value obj;
		obj["semester"]		= row[0].get<std::string>();
		obj["course"]	= row[1].get<std::string>();
		obj["credit"]	= row[2].get<uint32_t>();
		obj["teacher"]	= row[3].get<std::string>();
		obj["score"]	= row[4].get<double>();
		arr.append(obj);
	}
	root["result"] = true;
	root["scoreObj"] = arr;
	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->StartWrite();
}

void MysqlStReqHandler::calculate_gpa(std::shared_ptr<HttpConnection> con, uint32_t id)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	mysqlx::RowResult res = sess.sql("CALL st_calculate_gpa(?)").bind(id).execute();
	sess.close();

	Json::Value root;
	root["result"] = true;
	root["myGpa"] = res.fetchOne()[0].get<double>();

	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->StartWrite();
}
