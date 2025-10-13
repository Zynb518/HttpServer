#pragma once
#include "StReqHandler.h"

class MysqlStReqHandler : public StReqHandler
{
	using StringRef = const std::string&;
public:
	MysqlStReqHandler() = default;
	~MysqlStReqHandler() = default;
	void get_personal_info		(std::shared_ptr<HttpConnection> con, uint32_t id) override;
	void update_personal_info	(std::shared_ptr<HttpConnection> con, uint32_t id,
									StringRef birthday, StringRef email, StringRef phone, StringRef password) override;
	void browse_courses			(std::shared_ptr<HttpConnection> con, uint32_t id) override;
	void register_course		(std::shared_ptr<HttpConnection> con, uint32_t id, uint32_t section_id) override;

	void withdraw_course		(std::shared_ptr<HttpConnection> con, uint32_t id, uint32_t section_id) override;
	void get_schedule			(std::shared_ptr<HttpConnection> con, uint32_t id, StringRef semester) override;
	void get_transcript			(std::shared_ptr<HttpConnection> con, uint32_t id) override;
	void calculate_gpa			(std::shared_ptr<HttpConnection> con, uint32_t id) override;
};

