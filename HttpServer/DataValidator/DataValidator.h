#pragma once
#include <regex>
#include <string>
#include <mysqlx/xdevapi.h>
#include <json/json.h>

#include "Tools.h"

class DataValidator {
    using StringRef = const std::string&;
public:
    // 验证方法声明
    bool isValidEmail(StringRef email);
    bool isValidPhone(StringRef phone);
    bool isValidDate(StringRef date);
    bool isValidPassword(StringRef password);
    bool validateAll(StringRef birthday, StringRef email, StringRef phone, StringRef password);

    // 学期格式是否正确
    bool isValidSemester(StringRef semester);
    // 用户是否存在 
    bool isUserExists(uint32_t user_id, StringRef password, StringRef role);

    // 内联函数定义
    inline bool isValidScore(uint32_t score) 
        { return (score >= 0 && score <= 100); }

    inline bool isValidRole(StringRef role) 
        { return role == "teacher" || role == "student"; } 

    inline bool isValidName(StringRef name) 
        { return name.length() > 0 && name.length() <= 50; }

    inline bool isValidGender(StringRef gender) {
        static const std::string man = GetUTF8ForDatabase(L"男");
        static const std::string woman = GetUTF8ForDatabase(L"女");
        return gender == man || gender == woman;
    }

    inline bool isValidGrade(uint32_t grade) {
        return grade >= 2025 && grade <= 2100;
    }

    inline bool isValidCapacity(uint32_t capacity) 
        { return capacity >= 30 && capacity <= 150; }

    inline bool isValidWeek(uint32_t start_week, uint32_t end_week) 
        { return start_week >= 1 && start_week <= 20 && end_week >= 1 && end_week <= 20 && start_week <= end_week;}

    inline bool isValidLocation(StringRef location) 
        { return location.length() >= 4 && location.length() < 100;}

    inline bool isValidCredit(uint32_t credit) 
	    { return credit >= 1 && credit <= 7 ;}

    inline bool isValidType(StringRef type) 
    {
        return type == "GENERAL REQUIRED" || type == "MAJOR REQUIRED" || type == "MAJOR ELECTIVE" ||
            type == "UNIVERSITY ELECTIVE" || type == "PRACTICAL";
	}

    // 带错误处理
    std::string isValidSchedule(const Json::Value& schedule);

    bool isStTimeConflict(uint32_t user_id, uint32_t section_id);

	// section_id为0表示仅检测冲突，不去掉原有课程
	bool isInstrTimeConflict(uint32_t user_id, uint32_t start_week, 
        uint32_t end_week, StringRef schedule, uint32_t section_id = 0);

    bool isValidSectionId(uint32_t section_id);
   

private:
    // 正则表达式模式声明
    static const std::regex EMAIL_REGEX;
    static const std::regex PHONE_REGEX;
    static const std::regex DATE_REGEX;
    static const std::regex PASSWORD_REGEX;

    // 私有辅助方法声明
    static bool isValidDateValues(int year, int month, int day);
    static bool isLeapYear(int year);

    bool ProcessRow(const mysqlx::Row& row, size_t choice);
    // 处理 startWeek, endWeek, schedule的重载, 处理即将要选的课, choice为1检验，choice为0，去1
    bool ProcessRow(uint32_t start_week, uint32_t end_week, StringRef time_slot, uint32_t choice);
};