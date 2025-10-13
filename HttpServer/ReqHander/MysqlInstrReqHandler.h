#pragma once
#include "InstrReqHandler.h"

class MysqlInstrReqHandler : public InstrReqHandler
{
public:
	MysqlInstrReqHandler() = default;
	~MysqlInstrReqHandler() = default;
	void get_personal_info(std::shared_ptr<HttpConnection> con, uint32_t id) override;
	void update_personal_info(std::shared_ptr<HttpConnection> con, uint32_t id, 
		const std::string& college, const std::string& email,
		const std::string& phone, const std::string& password) override;

	void get_teaching_sections(std::shared_ptr<HttpConnection> con, uint32_t id, const std::string& semester) override;
	void get_section_students(std::shared_ptr<HttpConnection> con, uint32_t section_id) override;
	void post_grades(std::shared_ptr<HttpConnection> con, uint32_t student_id, uint32_t section_id, uint32_t score) override;
};

