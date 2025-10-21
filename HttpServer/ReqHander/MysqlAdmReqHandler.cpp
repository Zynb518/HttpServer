#include "MysqlAdmReqHandler.h"
#include "MysqlConnectionPool.h"
#include "Log.h"

#include "HttpConnection.h"
#include <mysqlx/xdevapi.h>


void MysqlAdmReqHandler::get_students_info(std::shared_ptr<HttpConnection> con)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	mysqlx::RowResult res = sess.sql("CALL adm_get_students_info()").execute();
	sess.close();
	Json::Value root;
	Json::Value arr;
	mysqlx::Row row;
	while (row = res.fetchOne())
	{
		Json::Value obj;
		obj["id"] = std::to_string(row[0].get<uint32_t>());
		obj["user_id"] = std::to_string(row[0].get<uint32_t>());
		obj["name"] = row[1].get <std::string>();
		obj["gender"] = row[2].get<std::string>();
		obj["grade"] = row[3].get<uint32_t>();
		obj["major"] = row[4].get<std::string>();
		obj["college"] = row[5].get<std::string>();
		obj["password"] = row[6].get<std::string>();
		arr.append(std::move(obj));
	}
	root["accountData"] = arr;
	root["result"] = true;
	
	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->StartWrite();
}

void MysqlAdmReqHandler::get_instructors_info(std::shared_ptr<HttpConnection> con)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	mysqlx::RowResult res = sess.sql("CALL adm_get_instructors_info()").execute();
	sess.close();
	Json::Value root;
	Json::Value arr;
	mysqlx::Row row;
	while (row = res.fetchOne())
	{
		Json::Value obj;
		obj["id"] = std::to_string(row[0].get<uint32_t>());
		obj["user_id"] = std::to_string(row[0].get<uint32_t>());
		obj["name"] = row[1].get <std::string>();
		obj["college"] = row[2].get<std::string>();
		obj["password"] = row[3].get<std::string>();
		arr.append(std::move(obj));
	}
	root["accountData"] = arr;
	root["result"] = true;
	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->StartWrite();
}

void MysqlAdmReqHandler::del_someone(std::shared_ptr<HttpConnection> con, const std::vector<uint32_t>& user_id,
	const std::vector<std::string>& role)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	for(size_t i = 0; i < user_id.size(); ++i)
	{
		sess.sql("CALL adm_del_someone(?, ?)")
			.bind(user_id[i])
			.bind(role[i])
			.execute();
	}
	sess.close();
	Json::Value root;
	root["result"] = true;
	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->StartWrite();
}

void MysqlAdmReqHandler::add_student(std::shared_ptr<HttpConnection> con, StringRef name, StringRef gender,
	uint32_t grade, uint32_t major_id, uint32_t college_id)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	mysqlx::RowResult res =  sess.sql("CALL adm_add_student(?,?,?,?,?)")
		.bind(name)
		.bind(gender)
		.bind(grade)
		.bind(major_id)
		.bind(college_id)
		.execute();
	sess.close();
	mysqlx::Row row = res.fetchOne();
	Json::Value root;
	root["result"] = true;
	root["id"] = std::to_string(row[0].get<uint32_t>());
	root["user_id"] = std::to_string(row[0].get<uint32_t>());

	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->StartWrite();
}

void MysqlAdmReqHandler::add_instructor(std::shared_ptr<HttpConnection> con, StringRef name, uint32_t college_id)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	mysqlx::RowResult res = sess.sql("CALL adm_add_instructor(?, ?)")
		.bind(name)
		.bind(college_id)
		.execute();
	sess.close();
	mysqlx::Row row = res.fetchOne();
	Json::Value root;
	root["result"] = true;
	root["id"] = std::to_string(row[0].get<uint32_t>());
	root["user_id"] = std::to_string(row[0].get<uint32_t>());
	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->GetResponse().prepare_payload();
	con->StartWrite();
}

void MysqlAdmReqHandler::get_sections(std::shared_ptr<HttpConnection> con, StringRef semester)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	mysqlx::RowResult res = sess.sql("CALL adm_get_sections(?)")
		.bind(semester)
		.execute();
	sess.close();
	mysqlx::Row row;
	Json::Value root;
	Json::Value arr;

	while(row = res.fetchOne())
	{
		Json::Value obj;
		obj["section_id"] = std::to_string(row[0].get<uint32_t>());
		obj["course_id"] = std::to_string(row[1].get<uint32_t>());
		obj["course"] = row[2].get<std::string>();
		obj["credit"] = row[3].get<double>();
		obj["college_id"] = std::to_string(row[4].get<uint32_t>());
		obj["college"] = row[5].get<std::string>();
		obj["type"] = row[6].get<std::string>();
		obj["teacher"] = row[7].get<std::string>();
		Json::Value timeArr;
		auto str = row[8].get<std::string>();
		ParseTimeString(str, timeArr);
		obj["schedule"] = timeArr;
		obj["choose_num"] = row[9].get<uint32_t>();
		obj["max_capacity"] = row[10].get<uint32_t>();
		obj["startWeek"] = row[11].get<uint32_t>();
		obj["endWeek"] = row[12].get<uint32_t>();
		obj["location"] = row[13].get<std::string>();
		arr.append(std::move(obj));
	}
	root["result"] = true;
	root["courseData"] = arr;

	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->StartWrite();
}

void MysqlAdmReqHandler::del_section(std::shared_ptr<HttpConnection> con, const std::vector<uint32_t>& section_id)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	for(auto id : section_id)
	{
		sess.sql("CALL adm_del_section(?)")
			.bind(id)
			.execute();
	}
	sess.close();
	Json::Value root;
	root["result"] = true;
	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->StartWrite();
}

void MysqlAdmReqHandler::add_section_new(std::shared_ptr<HttpConnection> con, uint32_t college_id, StringRef course_name, uint32_t credit, StringRef type, StringRef semester, uint32_t instructor_id, StringRef time_slot, uint32_t max_capacity, uint32_t start_week, uint32_t end_week, StringRef location)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	mysqlx::RowResult res = sess.sql("CALL adm_add_section_new(?,?,?,?,?,?,?,?,?,?,?)")
		.bind(college_id)
		.bind(course_name)
		.bind(credit)
		.bind(type)
		.bind(semester)
		.bind(instructor_id)
		.bind(time_slot)
		.bind(max_capacity)
		.bind(start_week)
		.bind(end_week)
		.bind(location)
		.execute();
	sess.close();
	mysqlx::Row row = res.fetchOne();
	Json::Value root;
	root["result"] = true;
	root["course_id"] = std::to_string(row[0].get<uint32_t>());
	root["section_id"] = std::to_string(row[1].get<uint32_t>());
	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->StartWrite();
}

void MysqlAdmReqHandler::add_section_old(std::shared_ptr<HttpConnection> con, uint32_t course_id, StringRef semester, uint32_t instructor_id, StringRef time_slot, uint32_t max_capacity, uint32_t start_week, uint32_t end_week, StringRef location)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	mysqlx::RowResult res = sess.sql("CALL adm_add_section_old(?,?,?,?,?,?,?,?)")
		.bind(course_id)
		.bind(semester)
		.bind(instructor_id)
		.bind(time_slot)
		.bind(max_capacity)
		.bind(start_week)
		.bind(end_week)
		.bind(location)
		.execute();
	sess.close();
	mysqlx::Row row = res.fetchOne();
	Json::Value root;
	root["result"] = true;
	root["section_id"] = std::to_string(row[0].get<uint32_t>());
	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->StartWrite();
}

void MysqlAdmReqHandler::modify_section(std::shared_ptr<HttpConnection> con, uint32_t section_id, uint32_t instructor_id, StringRef time_slot, uint32_t start_week, uint32_t end_week, StringRef location)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	sess.sql("CALL adm_modify_section(?,?,?,?,?,?)")
		.bind(section_id)
		.bind(instructor_id)
		.bind(time_slot)
		.bind(start_week)
		.bind(end_week)
		.bind(location)
		.execute();
	sess.close();
	Json::Value root;
	root["result"] = true;
	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->StartWrite();
}

void MysqlAdmReqHandler::view_grade_statistics(std::shared_ptr<HttpConnection> con, uint32_t section_id)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	mysqlx::RowResult res = sess.sql("CALL adm_view_grade_statistics(?)")
		.bind(section_id)
		.execute();
	sess.close();
	Json::Value root;
	root["result"] = true;
	Json::Value statistics;
	LOG_INFO("BEFORE FOR");
	for (auto row : res)
	{
		statistics.append(row[1].get<size_t>());
	}
		
	root["statistics"] = statistics;

	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->StartWrite();
}

void MysqlAdmReqHandler::get_student_grades(std::shared_ptr<HttpConnection> con, uint32_t student_id, StringRef semester)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	mysqlx::RowResult res = sess.sql("CALL adm_get_student_grades(?,?)")
		.bind(student_id)
		.bind(semester)
		.execute();
	sess.close();
	Json::Value root;
	Json::Value arr;
	mysqlx::Row row;
	while(row = res.fetchOne())
	{
		Json::Value obj;
		obj["course_name"] = row[0].get<std::string>();
		obj["score"] = row[1].get<double>();
		arr.append(std::move(obj));
	}
	root["result"] = true;
	root["scoreData"] = arr;
	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->StartWrite();
}

void MysqlAdmReqHandler::get_instructor_grades(std::shared_ptr<HttpConnection> con, uint32_t instructor_id, StringRef semester)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	mysqlx::RowResult res = sess.sql("CALL adm_get_instructor_grades(?,?)")
		.bind(instructor_id)
		.bind(semester)
		.execute();
	sess.close();
	Json::Value root;
	root["result"] = true;
	Json::Value arr;
	mysqlx::Row row;
	while(row = res.fetchOne())
	{
		Json::Value obj;
		obj["course_name"] = row[1].get<std::string>();
		obj["average_score"] = row[2].get<double>();
		arr.append(std::move(obj));
	}
	root["scoreData"] = arr;
	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->StartWrite();
}

void MysqlAdmReqHandler::get_colleges(std::shared_ptr<HttpConnection> con)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	mysqlx::RowResult res = sess.sql("CALL adm_get_colleges()").execute();
	sess.close();
	Json::Value root;
	Json::Value arr;
	mysqlx::Row row;
	while(row = res.fetchOne())
	{
		Json::Value obj;
		obj["college_id"] = std::to_string(row[0].get<uint32_t>());
		obj["college_name"] = row[1].get<std::string>();
		arr.append(std::move(obj));
	}
	root["result"] = true;
	root["collegeInfo"] = arr;
	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->StartWrite();
}

void MysqlAdmReqHandler::get_college_courses(std::shared_ptr<HttpConnection> con, uint32_t college_id)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	mysqlx::RowResult res = sess.sql("CALL adm_get_college_courses(?)")
		.bind(college_id)
		.execute();
	sess.close();
	Json::Value root;
	Json::Value arr;
	mysqlx::Row row;
	while(row = res.fetchOne())
	{
		Json::Value obj;
		obj["course_id"] = std::to_string(row[0].get<uint32_t>());
		obj["course_name"] = row[1].get<std::string>();
		obj["course_credits"] = row[2].get<double>();
		obj["type"] = row[3].get<std::string>();
		arr.append(std::move(obj));
	}
	root["result"] = true;
	root["courseInfo"] = arr;
	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->StartWrite();
}

void MysqlAdmReqHandler::get_college_instructors(std::shared_ptr<HttpConnection> con, uint32_t college_id)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	mysqlx::RowResult res = sess.sql("CALL adm_get_college_instructors(?)")
		.bind(college_id)
		.execute();
	sess.close();
	Json::Value root;
	Json::Value arr;
	mysqlx::Row row;
	while(row = res.fetchOne())
	{
		Json::Value obj;
		obj["teacher_id"] = std::to_string(row[0].get<uint32_t>());
		obj["teacher_name"] = row[1].get<std::string>();
		arr.append(std::move(obj));
	}
	root["result"] = true;
	root["teacherInfo"] = arr;
	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->StartWrite();
}

void MysqlAdmReqHandler::get_college_majors(std::shared_ptr<HttpConnection> con, uint32_t college_id)
{
	mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
	mysqlx::RowResult res = sess.sql("CALL adm_get_college_majors(?)")
		.bind(college_id)
		.execute();

	Json::Value root;
	Json::Value arr;
	mysqlx::Row row;
	while (row = res.fetchOne())
	{
		Json::Value obj;
		obj["major_id"] = std::to_string(row[0].get<uint32_t>());
		obj["major_name"] = row[1].get<std::string>();
		arr.append(std::move(obj));
	}
	root["result"] = true;
	root["majorInfo"] = arr;
	beast::ostream(con->GetResponse().body()) << Json::writeString(_writer, root);
	con->StartWrite();
}
