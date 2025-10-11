#include "Tools.h"

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