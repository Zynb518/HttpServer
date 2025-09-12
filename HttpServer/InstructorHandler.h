#pragma once
#include <memory>
#include <string>
class HttpConnection;

class InstructorHandler
{
public:
	InstructorHandler() = default;
	~InstructorHandler() = default;
	void get_personal_info(std::shared_ptr<HttpConnection> con, int id);
	void update_contact_info(std::shared_ptr<HttpConnection> con, int id, const std::string& phone);
	void update_password(std::shared_ptr<HttpConnection> con, int id, const std::string& password);
	void get_teaching_sections(std::shared_ptr<HttpConnection> con, int id);
	void get_section_students(std::shared_ptr<HttpConnection> con, int section_id);
	void post_grades(std::shared_ptr<HttpConnection> con, int section_id, const std::string& grades_json);
	void update_grades(std::shared_ptr<HttpConnection> con, int section_id, const std::string& grades_json);
	void get_section_statistics(std::shared_ptr<HttpConnection> con, int section_id);
};

