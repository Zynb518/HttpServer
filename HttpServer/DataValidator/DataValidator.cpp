#include "Log.h"
#include "Tools.h"
#include "MysqlConnectionPool.h"
#include "DataValidator.h"

#include <unordered_map>
#include <unordered_set>

bool timetable[22][8][9];

static const std::unordered_map<std::string, int> weekdayMap = {
                {GetUTF8ForDatabase(L"周一"), 1},
                {GetUTF8ForDatabase(L"周二"), 2},
                {GetUTF8ForDatabase(L"周三"), 3},
                {GetUTF8ForDatabase(L"周四"), 4},
                {GetUTF8ForDatabase(L"周五"), 5},
                {GetUTF8ForDatabase(L"周六"), 6},
                {GetUTF8ForDatabase(L"周日"), 7}
};

// DataValidator 静态成员变量定义
const std::regex DataValidator::EMAIL_REGEX(R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
const std::regex DataValidator::PHONE_REGEX(R"(^1[3-9]\d{9}$)"); // 中国手机号
const std::regex DataValidator::DATE_REGEX(R"(^\d{4}-\d{2}-\d{2}$)");
const std::regex DataValidator::PASSWORD_REGEX(R"(^[a-zA-Z0-9!@#$%^&*()_+\-=\[\]{};':"\\|,.<>\/?]{8,}$)");

// DataValidator 公共方法实现
bool DataValidator::isValidEmail(StringRef email) {
    return std::regex_match(email, EMAIL_REGEX);
}

bool DataValidator::isValidPhone(StringRef phone) {
    return std::regex_match(phone, PHONE_REGEX);
}

bool DataValidator::isValidDate(StringRef date) {
    // 先用正则验证基本格式
    if (!std::regex_match(date, DATE_REGEX)) {
        return false;
    }

    // 使用sscanf安全解析日期
    int year, month, day;
    if (sscanf(date.c_str(), "%d-%d-%d", &year, &month, &day) != 3) {
        return false;
    }

    // 进一步验证日期有效性
    return isValidDateValues(year, month, day);
}

bool DataValidator::isValidPassword(StringRef password) {
    if (password.length() < 8) {
        return false; // 密码长度至少8位
    }
    return std::regex_match(password, PASSWORD_REGEX);
}

bool DataValidator::validateAll(StringRef birthday, StringRef email, StringRef phone, StringRef password) {
    return isValidDate(birthday) &&
        isValidEmail(email) &&
        isValidPhone(phone) &&
        isValidPassword(password);
}

bool DataValidator::isValidSemester(StringRef semester)
{
    // 检查长度
    if (semester.length() != 7)
        return false;

    // 检查前两个字符必须是"20"
    if (semester[0] != '2' || semester[1] != '0')
        return false;

    // 检查第3、4个字符必须是数字
    if (!std::isdigit(semester[2]) || !std::isdigit(semester[3]))
        return false;

    static const std::string spring = GetUTF8ForDatabase(L"春");
    static const std::string autumn = GetUTF8ForDatabase(L"秋");
    // 检查最后一个字符必须是'春'或'秋'
    std::string_view t(semester.substr(4));
    if (t != spring && t != autumn)
        return false;

    return true;
}

bool DataValidator::isUserExists(uint32_t user_id, StringRef password, StringRef role)
{
    try {
        // 获取 users 表
        // mysqlx::Session sess = MySQLConnectionPool::Instance().GetSession();
        auto sess = MysqlConnectionPool::Instance().GetSession();
        auto users = sess.getSchema("scut_sims").getTable("users");

        // 构建查询条件 - 使用主键(user_id, role)和密码进行匹配
        auto result = users.select("user_id")
            .where("user_id = :uid AND password = :pwd AND role = :r")
            .bind("uid", user_id)
            .bind("pwd", password)
            .bind("r", role)  // 只需要检查是否存在，限制返回1条记录
            .execute();
        sess.close();
        // 检查是否有结果
        return result.count() > 0;
    }
    catch (const mysqlx::Error& err) {
        LOG_DEBUG("Database Error: " << err.what());
        return false;
    }
}

bool DataValidator::ProcessRow(const mysqlx::Row& row, size_t choice)
{
    auto start_week = row[0].get<uint32_t>();
    auto end_week = row[1].get<uint32_t>();
    auto time_slot = row[2].get<std::string>();
    LOG_INFO(
        "start-week: " << start_week << " "
        << "end-week: " << end_week << " "
        << "time_slot: " << time_slot << std::endl);

	return ProcessRow(start_week, end_week, time_slot, choice);
}


bool DataValidator::ProcessRow(uint32_t start_week, uint32_t end_week, StringRef time_slot, uint32_t choice)
{
    std::string_view str_v(time_slot);

    do // 一个row中有可能有多条时间
    {
        // 处理time_slot
        uint32_t Day = 0;
        uint32_t L = 0, R = 0;

        auto t = str_v.find(" "); // 分割出星期几
        if (t != std::string_view::npos)
        {
            const std::string s(str_v.data(), t);
            auto it = weekdayMap.find(s);
            if (it != weekdayMap.end())
                Day = it->second;
        }
        else
        {
            LOG_ERROR("Format Error");
            break;
        }

        t = str_v.find("-"); // 找到数字
        if (t != std::string::npos)
        {
            L = time_slot[t - 1] - '0';
            R = time_slot[t + 1] - '0';
        }
        else
        {
            LOG_ERROR("Format Error");
            break;
        }

        LOG_INFO("day: " << Day << "; time: " << L << "-" << R);

        if (choice == 1)
        {
            for (size_t i = start_week; i <= end_week; ++i)
                for (size_t j = L; j <= R; ++j)
                    if (timetable[i][Day][j]) return false;
                    else timetable[i][Day][j] = true;
        }
        else if (choice == 0)
        {
            for (size_t i = start_week; i <= end_week; ++i)
                for (size_t j = L; j <= R; ++j)
                    timetable[i][Day][j] = false;
        }

        t = str_v.find(",");
        if (t != std::string_view::npos) // 后面还有时间
            str_v = str_v.substr(t + 1);
        else
            break;

    } while (str_v.size() > 0);

    return true;
}

std::string DataValidator::isValidSchedule(const Json::Value& schedule)
{
    if (!schedule.isArray()) {
        return "";
    }

    std::ostringstream oss;
    bool first = true;

    static std::unordered_set<std::string> validDays = {
        GetUTF8ForDatabase(L"周一"), GetUTF8ForDatabase(L"周二"),
        GetUTF8ForDatabase(L"周三"), GetUTF8ForDatabase(L"周四"),
        GetUTF8ForDatabase(L"周五"), GetUTF8ForDatabase(L"周六"),
        GetUTF8ForDatabase(L"周日"),
    };

    for (auto& row : schedule)
    {
        if (row.isObject() &&
            row.isMember("day") && row["day"].isString() &&
            row.isMember("time") && row["time"].isString())
        {
            auto day = row["day"].asString();
            auto time = row["time"].asString();
            if (validDays.find(day) == validDays.end())
                return "";

            std::string_view vtime = time;
            if (vtime.length() != 7 || vtime[1] != '-' || !std::isdigit(vtime[0]) || !std::isdigit(vtime[2]) ||
                vtime.substr(3) != GetUTF8ForDatabase(L"节"))
                return "";

            if (!first)
                oss << ",";

            oss << row["day"].asString() << " " << row["time"].asString();
            first = false;
        }
        else
            return "";
    }
    return oss.str();
}

bool DataValidator::isStTimeConflict(uint32_t user_id, uint32_t section_id)
{
    memset(timetable, 0, sizeof timetable);
    mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
    sess.getSchema("scut_sims");
    // 处理已经选的课程
    mysqlx::RowResult result = sess.sql(
        "SELECT s.start_week, s.end_week, s.time_slot "
        "FROM enrollments e "
        "JOIN sections s USING (section_id) "
        "WHERE e.student_id = ? AND e.status = 'Enrolling'"
    ).bind(user_id).execute();

    for (auto row : result)
    {
        ProcessRow(row, 1);
    }

    // 处理即将要选的课程
    auto row = sess.sql(
        "SELECT start_week, end_week, time_slot"
        "FROM sections s"
        "WHERE section_id = ?;"
    ).bind(section_id).execute().fetchOne();

    bool ok = ProcessRow(row, 0);
    return false;
}

bool DataValidator::isInstrTimeConflict(uint32_t user_id, uint32_t start_week,
    uint32_t end_week, StringRef schedule, uint32_t section_id)
{
    memset(timetable, 0, sizeof timetable);
    mysqlx::Session sess = MysqlConnectionPool::Instance().GetSession();
    sess.getSchema("scut_sims");
    // 教授时间冲突
    // 获取当前学期的课表
    mysqlx::RowResult result = sess.sql(
        "SELECT start_week, end_week, time_slot"
        "FROM instructors i"
        "JOIN sections s USING(instructor_id)"
        "JOIN semesters sm USING(semester_id)"
        "WHERE instructor_id = 1 AND"
        "sm.`year` = YEAR(NOW()) AND"
        "DATE(NOW()) >= DATE_SUB(sm.start_date, INTERVAL 6 MONTH)"
        "AND DATE(NOW()) <= sm.end_date;"
    ).bind(user_id).execute();

    // 要去掉原本的课程安排
	mysqlx::Row row;
    if (section_id != 0)
    {
        row = sess.sql(
            "SELECT start_week, end_week, time_slot"
            "FROM sections "
            "WHERE section_id = ?;"
        ).bind(section_id).execute().fetchOne();
    }
    sess.close();

    // 处理已有的课程
    for (auto row : result)
    {
        ProcessRow(row, 1);
    }

    if (section_id != 0)
    {
        auto m_startWeek = row[0].get<uint32_t>();
        auto m_endWeek = row[1].get<uint32_t>();
        auto m_timeSlot = row[2].get<std::string>();
        ProcessRow(m_startWeek, m_endWeek, m_timeSlot, 0);
    }

    return ProcessRow(start_week, end_week, schedule, 1);
}

bool DataValidator::isValidSectionId(uint32_t section_id)
{
    try {
        auto sess = MysqlConnectionPool::Instance().GetSession();
        auto sections = sess.getSchema("scut_sims");
        auto row = sess.sql(
            "SELECT 1 FROM sections sc "
            "JOIN semesters sm USING (semester_id)"
            "WHERE sc.section_id = ? AND "
            "sm.`year` = YEAR(NOW()) AND "
            "MONTH(sm.start_date) >= IF(MONTH(NOW()) >= 7, 7, 0);")
            .bind(section_id).execute().fetchOne();
        sess.close();
        return !row.isNull();
    }
    catch (const mysqlx::Error& err) {
        LOG_DEBUG("Database Error: " << err.what());
        return false;
    }
}

// DataValidator 私有辅助方法实现
bool DataValidator::isValidDateValues(int year, int month, int day) {
    // 检查基本范围
    if (year < 1900 || year > 2100) return false;
    if (month < 1 || month > 12) return false;
    if (day < 1 || day > 31) return false;

    // 各月份天数
    int daysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    // 闰年处理
    if (month == 2 && isLeapYear(year)) {
        daysInMonth[1] = 29;
    }

    return day <= daysInMonth[month - 1];
}

bool DataValidator::isLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}