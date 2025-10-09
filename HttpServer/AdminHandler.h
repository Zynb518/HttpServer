#pragma once
#include <memory>
#include <string>
#include <Json/json.h>
#include <vector>
class HttpConnection;

class AdminHandler
{
	using StringRef = const std::string&;
public:
	AdminHandler() = default;
	~AdminHandler() = default;

	void get_students_info(std::shared_ptr<HttpConnection> con);

	void get_instructors_info(std::shared_ptr<HttpConnection> con);

	void del_someone(std::shared_ptr<HttpConnection> con, const std::vector<uint32_t>& user_id,
		const std::vector<std::string>& role);

	void add_student(std::shared_ptr<HttpConnection> con, StringRef name,
		StringRef gender, uint32_t grade, uint32_t major_id, uint32_t college_id);

	void add_instructor(std::shared_ptr<HttpConnection> con, StringRef name,
		uint32_t college_id);

	void get_sections(std::shared_ptr<HttpConnection> con, StringRef semester);

	void del_section(std::shared_ptr<HttpConnection> con, const std::vector<uint32_t>& section_id);

	void add_section_new(std::shared_ptr<HttpConnection> con, uint32_t college_id,
		StringRef course_name, uint32_t credit, StringRef type, StringRef semester,
		uint32_t instructor_id, StringRef time_slot, uint32_t max_capacity,
		uint32_t start_week, uint32_t end_week, StringRef location);

	void add_section_old(std::shared_ptr<HttpConnection> con, uint32_t course_id,
		StringRef semester, uint32_t instructor_id, StringRef time_slot,
		uint32_t max_capacity, uint32_t start_week, uint32_t end_week,
		StringRef location);

	void modify_section(std::shared_ptr<HttpConnection> con, uint32_t section_id, 
		uint32_t instructor_id, StringRef time_slot, 
		uint32_t start_week, uint32_t end_week, StringRef location);

	void view_grade_statistics(std::shared_ptr<HttpConnection> con, uint32_t section_id);

	void get_student_grades(std::shared_ptr<HttpConnection> con, uint32_t student_id, StringRef semester);

	void get_instructor_grades(std::shared_ptr<HttpConnection> con, uint32_t instructor_id, StringRef semester);

	void get_colleges(std::shared_ptr<HttpConnection> con);

	void get_college_courses(std::shared_ptr<HttpConnection> con, uint32_t college_id);

	void get_college_instructors(std::shared_ptr<HttpConnection> con, uint32_t college_id);

private:
	void ParseTimeString(std::string_view str_v, Json::Value& timeArr);
private:
	Json::StreamWriterBuilder _writer;
};

