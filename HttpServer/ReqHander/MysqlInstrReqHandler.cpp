#include "MysqlInstrReqHandler.h"
#include "MysqlConnectionPool.h"
#include "HttpConnection.h"
#include "Tools.h"
#include <boost/beast.hpp>

namespace beast = boost::beast;

void MysqlInstrReqHandler::get_personal_info(std::shared_ptr<HttpConnection> con, uint32_t id)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	mysqlx::RowResult res = sess.sql("CALL ins_get_personal_info(?)").bind(id).execute();
	sess.close();
	mysqlx::Row row = res.fetchOne();

	Json::Value root;
	root["id"]		= std::to_string(row[0].get<uint32_t>());
	root["user_id"] = std::to_string(row[0].get<uint32_t>());
	root["name"]	= row[1].get<std::string>();
	root["college"] = row[2].get<std::string>();
	root["email"]	= row[3].get<std::string>();
	root["phone"]	= row[4].get<std::string>();

	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->StartWrite();
}

void MysqlInstrReqHandler::update_personal_info(std::shared_ptr<HttpConnection> con, uint32_t id,
	const std::string& college, 
	const std::string& email, 
	const std::string& phone, 
	const std::string& password)
{
	Json::Value root;

	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	sess.sql("CALL ins_update_personal_info(:id, :college, :email, :phone, :password)")
		.bind("id", id)
		.bind("college", college)
		.bind("email", email)
		.bind("phone", phone)
		.bind("password", password)
		.execute();
	sess.close();

	
	root["result"] = true;
	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->StartWrite();
}

void MysqlInstrReqHandler::get_teaching_sections(std::shared_ptr<HttpConnection> con, uint32_t id, const std::string& semester)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	mysqlx::RowResult res = sess.sql("CALL ins_get_teaching_sections(:id , :semester)")
		.bind("id", id).bind("semester", semester).execute();
	sess.close();
	
	Json::Value root;
	Json::Value arr;
	for(auto row : res)
	{
		Json::Value obj;
		obj["section_id"] = std::to_string(row[0].get<uint32_t>());
		obj["courseName"] = row[1].get<std::string>();
		obj["instructor"] = row[2].get<std::string>();
		obj["location"] = row[3].get<std::string>();
		Json::Value timeArr;
		auto str = row[4].get<std::string>();
		ParseTimeString(str, timeArr);
		obj["schedule"] = timeArr;
		obj["credit"] = row[5].get<double>();
		obj["startWeek"] = row[6].get<uint32_t>();
		obj["endWeek"] = row[7].get<uint32_t>();
	}
	root["result"] = true;
	root["courseObj"] = arr;
	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->StartWrite();
}

void MysqlInstrReqHandler::get_section_students(std::shared_ptr<HttpConnection> con, uint32_t section_id)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	mysqlx::SqlResult results = sess.sql("CALL ins_get_section_students(?)").bind(section_id).execute();
	sess.close();

	Json::Value root;
	Json::Value arr;
	
	auto row = results.fetchOne();
	root["semester"] = row[0].get<std::string>();
	root["course"] = row[1].get<std::string>();
	root["credit"] = row[2].get<uint32_t>();
	root["teacher"] = row[3].get<std::string>();
	
	if(results.nextResult())
	{
		while (row = results.fetchOne())
		{
			Json::Value obj;
			obj["name"] = row[0].get<std::string>();
			obj["student_id"] = std::to_string(row[1].get<uint32_t>());
			obj["grade"] = row[2].get<int>(); // 内部把null 转成 -1
			arr.append(obj);
		}
	}

	root["result"] = true;
	root["initData"] = arr;
	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->StartWrite();
}

void MysqlInstrReqHandler::post_grades(std::shared_ptr<HttpConnection> con, uint32_t student_id, uint32_t section_id, uint32_t score)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	sess.sql("CALL ins_post_grades(:student_id, :section_id, :score)")
		.bind("student_id", student_id)
		.bind("section_id", section_id)
		.bind("score", score)
		.execute();
	sess.close();
	
	Json::Value root;
	root["result"] = true;
	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->StartWrite();
}
