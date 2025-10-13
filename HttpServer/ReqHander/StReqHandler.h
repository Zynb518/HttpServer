#pragma once
#include <memory>
#include <string>
#include <json/json.h>
#include <mysqlx/xdevapi.h>

class HttpConnection;

class StReqHandler
{
	using StringRef = const std::string&;
public:
	StReqHandler() = default;
	virtual ~StReqHandler() = default;
	virtual void get_personal_info(std::shared_ptr<HttpConnection> con, uint32_t id) = 0;
	virtual void update_personal_info(std::shared_ptr<HttpConnection> con, uint32_t id,
		StringRef birthday, StringRef email, StringRef phone, StringRef password) = 0;
	virtual void browse_courses(std::shared_ptr<HttpConnection> con, uint32_t id) = 0;
	virtual void register_course(std::shared_ptr<HttpConnection> con, uint32_t id, uint32_t section_id) = 0;

	virtual void withdraw_course(std::shared_ptr<HttpConnection> con, uint32_t id, uint32_t section_id) = 0;
	virtual void get_schedule(std::shared_ptr<HttpConnection> con, uint32_t id, StringRef semester) = 0;
	virtual void get_transcript(std::shared_ptr<HttpConnection> con, uint32_t id) = 0;
	virtual void calculate_gpa(std::shared_ptr<HttpConnection> con, uint32_t id) = 0;
protected:
	void ParseTimeString(std::string_view str_v, Json::Value& timeArr);
	Json::StreamWriterBuilder _writer;
};

