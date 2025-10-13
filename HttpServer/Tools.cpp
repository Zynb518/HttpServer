#include "Tools.h"
#include "Log.h"
#include "MysqlConnectionPool.h"

// Timer ���ʵ��
Timer::Timer()
{
    start = std::chrono::high_resolution_clock::now();
}

Timer::~Timer()
{
    end = std::chrono::high_resolution_clock::now();
    duration = end - start;

    float ms = duration.count() * 1000.0f;
    std::cout << "Timer took" << ms << "ms " << std::endl;
}

// DataValidator ��̬��Ա��������
const std::regex DataValidator::EMAIL_REGEX(R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
const std::regex DataValidator::PHONE_REGEX(R"(^1[3-9]\d{9}$)"); // �й��ֻ���
const std::regex DataValidator::DATE_REGEX(R"(^\d{4}-\d{2}-\d{2}$)");
const std::regex DataValidator::PASSWORD_REGEX(R"(^[a-zA-Z0-9!@#$%^&*()_+\-=\[\]{};':"\\|,.<>\/?]{8,}$)");

// DataValidator ��������ʵ��
bool DataValidator::isValidEmail(const std::string& email) {
    return std::regex_match(email, EMAIL_REGEX);
}

bool DataValidator::isValidPhone(const std::string& phone) {
    return std::regex_match(phone, PHONE_REGEX);
}

bool DataValidator::isValidDate(const std::string& date) {
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

bool DataValidator::isValidPassword(const std::string& password) {
    if (password.length() < 8) {
        return false; // ���볤������8λ
    }
    return std::regex_match(password, PASSWORD_REGEX);
}

bool DataValidator::validateAll(const std::string& birthday, const std::string& email,
    const std::string& phone, const std::string& password) {
    return isValidDate(birthday) &&
        isValidEmail(email) &&
        isValidPhone(phone) &&
        isValidPassword(password);
}

bool DataValidator::isValidSemester(const std::string& semester)
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

bool DataValidator::isUserExists(uint32_t user_id, const std::string& password, const std::string& role)
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