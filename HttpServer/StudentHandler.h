#pragma once
#include <memory>
#include <string>
#include <json/json.h>
#include <mysqlx/xdevapi.h>

class HttpConnection;

class StudentHandler
{
	using StringRef = const std::string&;
public:
	StudentHandler() = default;
	~StudentHandler() = default;
	void get_personal_info		(std::shared_ptr<HttpConnection> con, uint32_t id);
	void update_personal_info	(std::shared_ptr<HttpConnection> con, uint32_t id,
									StringRef birthday, StringRef email, StringRef phone, StringRef password);
	void browse_courses			(std::shared_ptr<HttpConnection> con, uint32_t id);
	void register_course		(std::shared_ptr<HttpConnection> con, uint32_t id, uint32_t section_id);

	void withdraw_course		(std::shared_ptr<HttpConnection> con, uint32_t id, uint32_t section_id);
	void get_schedule			(std::shared_ptr<HttpConnection> con, uint32_t id, StringRef semester);
	void get_transcript			(std::shared_ptr<HttpConnection> con, uint32_t id);
	void calculate_gpa			(std::shared_ptr<HttpConnection> con, uint32_t id);
private:
	void ParseTimeString		(std::string_view str_v, Json::Value& timeArr);

	Json::StreamWriterBuilder _writer;
};

