#pragma once
#include <memory>
#include <string>
#include <Json/json.h>
#include <vector>
class HttpConnection;

class AdmReqHandler
{
	using StringRef = const std::string&;
public:
	AdmReqHandler() = default;
	virtual ~AdmReqHandler() = default;

	virtual void get_students_info(std::shared_ptr<HttpConnection> con) = 0;

	virtual void get_instructors_info(std::shared_ptr<HttpConnection> con) = 0;

	virtual void del_someone(std::shared_ptr<HttpConnection> con, const std::vector<uint32_t>& user_id,
		const std::vector<std::string>& role) = 0;

	virtual void add_student(std::shared_ptr<HttpConnection> con, StringRef name,
		StringRef gender, uint32_t grade, uint32_t major_id, uint32_t college_id) = 0;

	virtual void add_instructor(std::shared_ptr<HttpConnection> con, StringRef name,
		uint32_t college_id) = 0;

	virtual void get_sections(std::shared_ptr<HttpConnection> con, StringRef semester) = 0;

	virtual void del_section(std::shared_ptr<HttpConnection> con, const std::vector<uint32_t>& section_id) = 0;

	virtual void add_section_new(std::shared_ptr<HttpConnection> con, uint32_t college_id,
		StringRef course_name, uint32_t credit, StringRef type, StringRef semester,
		uint32_t instructor_id, StringRef time_slot, uint32_t max_capacity,
		uint32_t start_week, uint32_t end_week, StringRef location) = 0;

	virtual void add_section_old(std::shared_ptr<HttpConnection> con, uint32_t course_id,
		StringRef semester, uint32_t instructor_id, StringRef time_slot,
		uint32_t max_capacity, uint32_t start_week, uint32_t end_week,
		StringRef location) = 0;

	virtual void modify_section(std::shared_ptr<HttpConnection> con, uint32_t section_id,
		uint32_t instructor_id, StringRef time_slot,
		uint32_t start_week, uint32_t end_week, StringRef location) = 0;

	virtual void view_grade_statistics(std::shared_ptr<HttpConnection> con, uint32_t section_id) = 0;

	virtual void get_student_grades(std::shared_ptr<HttpConnection> con, uint32_t student_id, StringRef semester) = 0;

	virtual void get_instructor_grades(std::shared_ptr<HttpConnection> con, uint32_t instructor_id, StringRef semester) = 0;

	virtual void get_colleges(std::shared_ptr<HttpConnection> con) = 0;

	virtual void get_college_courses(std::shared_ptr<HttpConnection> con, uint32_t college_id) = 0;

	virtual void get_college_instructors(std::shared_ptr<HttpConnection> con, uint32_t college_id) = 0;

protected:
	void ParseTimeString(std::string_view str_v, Json::Value& timeArr);
	Json::StreamWriterBuilder _writer;
};

