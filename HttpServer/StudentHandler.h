#pragma once
#include <memory>
#include <string>
#include <json/json.h>

class HttpConnection;

class StudentHandler
{
public:
	StudentHandler() = default;
	~StudentHandler() = default;
	void get_personal_info	 (std::shared_ptr<HttpConnection> con, uint32_t id);
	void update_personal_info(std::shared_ptr<HttpConnection> con, uint32_t id,
		const std::string& birthday, const std::string& email, const std::string& phone, const std::string& password);
	void browse_courses		 (std::shared_ptr<HttpConnection> con, uint32_t id);
	void register_course	 (std::shared_ptr<HttpConnection> con, uint32_t id, uint32_t section_id);
	void withdraw_course	 (std::shared_ptr<HttpConnection> con, uint32_t id, uint32_t section_id);
	void get_schedule		 (std::shared_ptr<HttpConnection> con, uint32_t id, const std::string& semester);
	void get_transcript		 (std::shared_ptr<HttpConnection> con, uint32_t id);
	void calculate_gpa		 (std::shared_ptr<HttpConnection> con, uint32_t id);
private:
	Json::StreamWriterBuilder _writer;
};

