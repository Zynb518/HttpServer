#include "Log.h"
#include "Tools.h"
#include "MysqlConnectionPool.h"
#include "DataValidator.h"

#include <unordered_map>
#include <unordered_set>

bool timetable[22][8][9];

static const std::unordered_map<std::string, int> weekdayMap = {
                {GetUTF8ForDatabase(L"��һ"), 1},
                {GetUTF8ForDatabase(L"�ܶ�"), 2},
                {GetUTF8ForDatabase(L"����"), 3},
                {GetUTF8ForDatabase(L"����"), 4},
                {GetUTF8ForDatabase(L"����"), 5},
                {GetUTF8ForDatabase(L"����"), 6},
                {GetUTF8ForDatabase(L"����"), 7}
};

// DataValidator ��̬��Ա��������
const std::regex DataValidator::EMAIL_REGEX(R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
const std::regex DataValidator::PHONE_REGEX(R"(^1[3-9]\d{9}$)"); // �й��ֻ���
const std::regex DataValidator::DATE_REGEX(R"(^\d{4}-\d{2}-\d{2}$)");
const std::regex DataValidator::PASSWORD_REGEX(R"(^[a-zA-Z0-9!@#$%^&*()_+\-=\[\]{};':"\\|,.<>\/?]{8,}$)");

// DataValidator ��������ʵ��
bool DataValidator::isValidEmail(StringRef email) {
    return std::regex_match(email, EMAIL_REGEX);
}

bool DataValidator::isValidPhone(StringRef phone) {
    return std::regex_match(phone, PHONE_REGEX);
}

bool DataValidator::isValidDate(StringRef date) {
    // ����������֤������ʽ
    if (!std::regex_match(date, DATE_REGEX)) {
        return false;
    }

    // ʹ��sscanf��ȫ��������
    int year, month, day;
    if (sscanf(date.c_str(), "%d-%d-%d", &year, &month, &day) != 3) {
        return false;
    }

    // ��һ����֤������Ч��
    return isValidDateValues(year, month, day);
}

bool DataValidator::isValidPassword(StringRef password) {
    if (password.length() < 8) {
        return false; // ���볤������8λ
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
    // ��鳤��
    if (semester.length() != 7)
        return false;

    // ���ǰ�����ַ�������"20"
    if (semester[0] != '2' || semester[1] != '0')
        return false;

    // ����3��4���ַ�����������
    if (!std::isdigit(semester[2]) || !std::isdigit(semester[3]))
        return false;

    static const std::string spring = GetUTF8ForDatabase(L"��");
    static const std::string autumn = GetUTF8ForDatabase(L"��");
    // ������һ���ַ�������'��'��'��'
    std::string_view t(semester.substr(4));
    if (t != spring && t != autumn)
        return false;

    return true;
}

bool DataValidator::isUserExists(uint32_t user_id, StringRef password, StringRef role)
{
    try {
        // ��ȡ users ��
        // mysqlx::Session sess = MySQLConnectionPool::Instance().GetSession();
        auto sess = MysqlConnectionPool::Instance().GetSession();
        auto users = sess.getSchema("scut_sims").getTable("users");

        // ������ѯ���� - ʹ������(user_id, role)���������ƥ��
        auto result = users.select("user_id")
            .where("user_id = :uid AND password = :pwd AND role = :r")
            .bind("uid", user_id)
            .bind("pwd", password)
            .bind("r", role)  // ֻ��Ҫ����Ƿ���ڣ����Ʒ���1����¼
            .execute();
        sess.close();
        // ����Ƿ��н��
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

    do // һ��row���п����ж���ʱ��
    {
        // ����time_slot
        uint32_t Day = 0;
        uint32_t L = 0, R = 0;

        auto t = str_v.find(" "); // �ָ�����ڼ�
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

        t = str_v.find("-"); // �ҵ�����
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
        if (t != std::string_view::npos) // ���滹��ʱ��
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
        GetUTF8ForDatabase(L"��һ"), GetUTF8ForDatabase(L"�ܶ�"),
        GetUTF8ForDatabase(L"����"), GetUTF8ForDatabase(L"����"),
        GetUTF8ForDatabase(L"����"), GetUTF8ForDatabase(L"����"),
        GetUTF8ForDatabase(L"����"),
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
                vtime.substr(3) != GetUTF8ForDatabase(L"��"))
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
    // �����Ѿ�ѡ�Ŀγ�
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

    // ������Ҫѡ�Ŀγ�
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
    // ����ʱ���ͻ
    // ��ȡ��ǰѧ�ڵĿα�
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

    // Ҫȥ��ԭ���Ŀγ̰���
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

    // �������еĿγ�
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

// DataValidator ˽�и�������ʵ��
bool DataValidator::isValidDateValues(int year, int month, int day) {
    // ��������Χ
    if (year < 1900 || year > 2100) return false;
    if (month < 1 || month > 12) return false;
    if (day < 1 || day > 31) return false;

    // ���·�����
    int daysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    // ���괦��
    if (month == 2 && isLeapYear(year)) {
        daysInMonth[1] = 29;
    }

    return day <= daysInMonth[month - 1];
}

bool DataValidator::isLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}