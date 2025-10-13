#pragma once
#include <memory>
#include <string>
#include <json/json.h>
class HttpConnection;

class InstrReqHandler
{
public:
	InstrReqHandler() = default;
	virtual ~InstrReqHandler() = default;
	virtual void get_personal_info(std::shared_ptr<HttpConnection> con, uint32_t id) = 0;
	virtual void update_personal_info(std::shared_ptr<HttpConnection> con, uint32_t id,
		const std::string& college, const std::string& email,
		const std::string& phone, const std::string& password) = 0;

	virtual void get_teaching_sections(std::shared_ptr<HttpConnection> con, uint32_t id, const std::string& semester) = 0;
	virtual void get_section_students(std::shared_ptr<HttpConnection> con, uint32_t section_id) = 0;
	virtual void post_grades(std::shared_ptr<HttpConnection> con, uint32_t student_id, uint32_t section_id, uint32_t score) = 0;

protected:
	void ParseTimeString(std::string_view str_v, Json::Value& timeArr);
	Json::StreamWriterBuilder _writer;
};

