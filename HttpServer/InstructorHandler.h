#pragma once
#include <memory>
#include <string>
#include <json/json.h>
class HttpConnection;

class InstructorHandler
{
public:
	InstructorHandler() = default;
	~InstructorHandler() = default;
	void get_personal_info(std::shared_ptr<HttpConnection> con, uint32_t id);
	void update_personal_info(std::shared_ptr<HttpConnection> con, uint32_t id, 
		const std::string& college, const std::string& email,
		const std::string& phone, const std::string& password);

	void get_teaching_sections(std::shared_ptr<HttpConnection> con, uint32_t id, const std::string& semester);
	void get_section_students(std::shared_ptr<HttpConnection> con, uint32_t section_id);
	void post_grades(std::shared_ptr<HttpConnection> con, uint32_t student_id, uint32_t section_id, uint32_t score);
	void get_section_statistics(std::shared_ptr<HttpConnection> con, uint32_t section_id);
private:
	void ParseTimeString(std::string_view str_v, Json::Value& timeArr);

	Json::StreamWriterBuilder _writer;
};

