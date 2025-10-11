#pragma once
#include <iostream>
#include <string>
#include <chrono>
#include <regex>
#include <sstream>
#include <vector>
#include <cstdio>

// 编译期 UTF-16 到 UTF-8 转换（适用于基本多语言平面字符）
constexpr std::string GetUTF8ForDatabase(std::wstring_view wideStr) noexcept {
    if (wideStr.empty()) return "";

    std::string result;
    for (wchar_t wc : wideStr) {
        if (wc <= 0x7F) {
            // ASCII 字符
            result += static_cast<char>(wc);
        }
        else if (wc <= 0x7FF) {
            // 2字节 UTF-8
            result += static_cast<char>(0xC0 | ((wc >> 6) & 0x1F));
            result += static_cast<char>(0x80 | (wc & 0x3F));
        }
        else if (wc <= 0xFFFF) {
            // 3字节 UTF-8（适用于基本多语言平面）
            result += static_cast<char>(0xE0 | ((wc >> 12) & 0x0F));
            result += static_cast<char>(0x80 | ((wc >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (wc & 0x3F));
        }
        // 对于代理对（surrogate pairs）需要更复杂的处理
    }
    return result;
}

struct Timer
{
    std::chrono::time_point<std::chrono::steady_clock> start, end; //这里不能用auto 
    std::chrono::duration<float> duration;

    Timer();
    ~Timer();
};

class DataValidator {
private:
    // 正则表达式模式声明
    static const std::regex EMAIL_REGEX;
    static const std::regex PHONE_REGEX;
    static const std::regex DATE_REGEX;
    static const std::regex PASSWORD_REGEX;

    // 私有辅助方法声明
    static bool isValidDateValues(int year, int month, int day);
    static bool isLeapYear(int year);

public:
    // 验证方法声明
    static bool isValidEmail(const std::string& email);
    static bool isValidPhone(const std::string& phone);
    static bool isValidDate(const std::string& date);
    static bool isValidPassword(const std::string& password);
    static bool validateAll(const std::string& birthday, const std::string& email,
        const std::string& phone, const std::string& password);

    static bool isValidSemester(const std::string& semester);
};