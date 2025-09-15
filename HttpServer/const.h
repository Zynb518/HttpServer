#pragma once

enum class StudentOp {
    // 个人信息管理
    GET_PERSONAL_INFO,          // 个人信息
	UPDATE_PERSONAL_INFO,       // 修改个人信息
    // 课程相关
    BROWSE_COURSES,             // 可选课程
    REGISTER_COURSE,            // 注册课程
    WITHDRAW_COURSE,            // 退选课程

    // 课表与成绩
    GET_SCHEDULE,               // 看课表
    GET_TRANSCRIPT,             // 查询所有成绩
	CALCULATE_GPA               // 计算GPA
};

enum class InstructorOp {
    // 个人信息管理
    GET_PERSONAL_INFO,          // GET /api/instructors/me
    UPDATE_CONTACT_INFO,        // PATCH /api/instructors/me/contact
    UPDATE_PASSWORD,            // PUT /api/instructors/me/password

    // 教学相关
    GET_TEACHING_SECTIONS,      // GET /api/instructors/me/sections
    GET_SECTION_STUDENTS,       // GET /api/sections/{id}/students
    POST_GRADES,                // POST /api/sections/{id}/grades
    UPDATE_GRADES,              // PUT /api/sections/{id}/grades
    GET_SECTION_STATISTICS      // GET /api/sections/{id}/statistics
};

enum class AdminOp {
    // 用户管理
    GET_PENDING_INSTRUCTORS,       // GET /api/admin/pending-instructors
    APPROVE_INSTRUCTOR_ACCOUNT,    // POST /api/admin/instructors/{id}/approve
    GET_ALL_USERS,                 // GET /api/admin/users
    RESET_USER_PASSWORD,           // PUT /api/admin/users/{id}/password
    TOGGLE_USER_STATUS,            // PUT /api/admin/users/{id}/status
    SET_USER_ROLE,                 // PUT /api/admin/users/{id}/role

    // 课程管理
    GET_ALL_COURSES,               // GET /api/admin/courses
    CREATE_COURSE,                 // POST /api/admin/courses
    UPDATE_COURSE,                 // PUT /api/admin/courses/{id}
    DELETE_COURSE,                 // DELETE /api/admin/courses/{id}
    SET_COURSE_PREREQUISITES,      // PUT /api/admin/courses/{id}/prerequisites
    CREATE_SEMESTER,               // POST /api/admin/semesters
    UPDATE_SEMESTER,               // PUT /api/admin/semesters/{id}

    // 班次管理
    CREATE_SECTION,                // POST /api/admin/sections
    UPDATE_SECTION,                // PUT /api/admin/sections/{id}
    DELETE_SECTION,                // DELETE /api/admin/sections/{id}

    // 强制操作
    FORCE_ENROLL_STUDENT,          // POST /api/admin/students/{id}/courses
    FORCE_WITHDRAW_STUDENT,        // DELETE /api/admin/students/{id}/courses/{course_id}

    // 报表
    GENERATE_ENROLLMENT_REPORT     // GET /api/admin/reports/enrollment
};
