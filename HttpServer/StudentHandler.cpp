#include "StudentHandler.h"
#include <mysqlx/xdevapi.h>
#include "MysqlConnectionPool.h"
#include "HttpConnection.h"
#include <json/json.h>
#include <iostream>

void StudentHandler::get_personal_info(std::shared_ptr<HttpConnection> con, uint32_t id)
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
	root["birthdate"]	= row[6].get<std::string>();
	root["email"]		= row[7].get<std::string>();
	root["phone"]		= row[8].get<std::string>();

	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->GetResponse().prepare_payload();
	con->StartWrite();
}

void StudentHandler::update_personal_info(std::shared_ptr<HttpConnection> con, uint32_t id,
	const std::string& birthday, const std::string& email, const std::string& phone, const std::string& password)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	sess.sql("CALL st_update_personal_info(?,?,?,?,?)")
		.bind(id).bind(birthday).bind(email).bind(phone).bind(password)
		.execute();
	sess.close();
	Json::Value root;
	root["result"] = true;
	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->GetResponse().prepare_payload();
	con->StartWrite();
}

void StudentHandler::browse_courses(std::shared_ptr<HttpConnection> con, uint32_t id)
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
		obj["schedule"]		= timeArr;
		obj["credit"]		= row[5].get<double>();
		obj["semester"]		= row[6].get<std::string>();
		obj["startWeek"]	= row[7].get<uint32_t>();
		obj["endWeek"]		= row[8].get<uint32_t>();
		obj["location"]		= row[9].get<std::string>();
		obj["selected"]		= false;
		arr.append(obj);
	}
	root["result"] = true;
	root["courseObj"] = arr;
	
	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->GetResponse().prepare_payload();
	con->StartWrite();
}

void StudentHandler::register_course(std::shared_ptr<HttpConnection> con, uint32_t id, uint32_t section_id)
{
	Json::Value root;

	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	mysqlx::Table sections = sess.getSchema("scut_sims").getTable("sections");
	mysqlx::Row row = sections.select("1").where("section_id = :sid AND `number` < max_capacity")
		.bind("sid", section_id)
		.execute().fetchOne();
	// 有位置
	if (!row.isNull())
	{
		sess.sql("CALL st_register_course(?,?)").bind(id).bind(section_id).execute();
		root["result"] = true;
	}
	else
	{
		root["result"] = false;
		root["reason"] = "Section full";
	}
	sess.close();

	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->GetResponse().prepare_payload();
	con->StartWrite();
}

void StudentHandler::withdraw_course(std::shared_ptr<HttpConnection> con, uint32_t id, uint32_t section_id)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	sess.sql("CALL st_withdraw_course(?,?)").bind(id).bind(section_id).execute();
	sess.close();

	Json::Value root;
	root["result"] = true;

	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->GetResponse().prepare_payload();
	con->StartWrite();
}

void StudentHandler::get_schedule(std::shared_ptr<HttpConnection> con, uint32_t id, const std::string semester)
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
	root["scheduleObj"] = arr;
	
	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->GetResponse().prepare_payload();
	con->StartWrite();
}

void StudentHandler::get_transcript(std::shared_ptr<HttpConnection> con, uint32_t id)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	mysqlx::RowResult res = sess.sql("CALL st_get_personal_info(?)").bind(id).execute();
	sess.close();
	Json::Value root;
	Json::Value arr;
	mysqlx::Row row;

	while (row = res.fetchOne())
	{
		Json::Value obj;
		obj["term"]		= row[0].get<std::string>();
		obj["course"]	= row[1].get<std::string>();
		obj["credit"]	= row[2].get<double>();
		obj["teacher"]	= row[3].get<std::string>();
		obj["score"]	= row[4].get<uint32_t>();
		arr.append(obj);
	}
	root["result"] = true;
	root["scoreObj"] = arr;
	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->GetResponse().prepare_payload();
	con->StartWrite();
}

void StudentHandler::calculate_gpa(std::shared_ptr<HttpConnection> con, uint32_t id)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	mysqlx::RowResult res = sess.sql("CALL st_calculate_gpa(?)").bind(id).execute();
	sess.close();

	Json::Value root;
	root["result"] = true;
	root["myGpa"] = res.fetchOne()[0].get<double>();

	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->GetResponse().prepare_payload();
	con->StartWrite();
}

void StudentHandler::ParseTimeString(std::string_view str_v, Json::Value& timeArr)
{
	do
	{
		Json::Value timeObj; // 一个 day-time 对

		auto c = str_v.find(" ");
		std::string_view day(str_v.data(), c);
		timeObj["day"] = std::string(day);
		str_v = str_v.substr(c + 1);
		c = str_v.find(",");
		if (c != std::string_view::npos) // 后面还有时间
		{
			std::string_view time(str_v.data(), c);
			timeObj["time"] = std::string(time);
			str_v = str_v.substr(c + 1);
		}
		else
		{
			timeObj["time"] = std::string(str_v);
			str_v = str_v.substr(str_v.size());
		}
		timeArr.append(timeObj);
	} while (str_v.size() > 0);
}