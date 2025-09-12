#pragma once
#include <memory>
#include <string>

class HttpConnection;

class StudentHandler
{
public:
	StudentHandler() = default;
	~StudentHandler() = default;

	void get_personal_info	(std::shared_ptr<HttpConnection> con, int id);
	void update_contact_info(std::shared_ptr<HttpConnection> con, int id, const std::string& phone);
	void update_password	(std::shared_ptr<HttpConnection> con, int id, const std::string& password);
	void browse_courses		(std::shared_ptr<HttpConnection> con, int id);
	void course_details		(std::shared_ptr<HttpConnection> con, int section_id);
	void register_course	(std::shared_ptr<HttpConnection> con, int id, int section_id);
	void withdraw_course	(std::shared_ptr<HttpConnection> con, int id, int section_id);
	void get_schedule		(std::shared_ptr<HttpConnection> con, int id);
	void get_current_grades	(std::shared_ptr<HttpConnection> con, int id, int year); // 根据需求
	void get_transcript		(std::shared_ptr<HttpConnection> con, int id);
	void calculate_gpa		(std::shared_ptr<HttpConnection> con, int id);
};

