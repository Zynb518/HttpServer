#pragma once
#include <iostream>
#include <string>
#include <chrono>
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

constexpr std::string UrlDecode(std::string_view urlEncodedStr) noexcept {
    std::string result;
    result.reserve(urlEncodedStr.length()); // 预分配空间提高效率

    for (size_t i = 0; i < urlEncodedStr.length(); ++i) {
        char c = urlEncodedStr[i];

        if (c == '%' && i + 2 < urlEncodedStr.length()) {
            // 处理 %XX 格式
            char hex1 = urlEncodedStr[i + 1];
            char hex2 = urlEncodedStr[i + 2];

            if (std::isxdigit(hex1) && std::isxdigit(hex2)) {
                // 将十六进制字符转换为字节值
                auto hexCharToValue = [](char h) -> unsigned char {
                    if (h >= '0' && h <= '9') return h - '0';
                    if (h >= 'A' && h <= 'F') return h - 'A' + 10;
                    if (h >= 'a' && h <= 'f') return h - 'a' + 10;
                    return 0;
                    };

                unsigned char byteValue = (hexCharToValue(hex1) << 4) | hexCharToValue(hex2);
                result += static_cast<char>(byteValue);
                i += 2; // 跳过已处理的两位十六进制数字
            }
            else {
                // 无效的十六进制，保持原样
                result += c;
            }
        }
        else if (c == '+') {
            // URL编码中 '+' 表示空格
            result += ' ';
        }
        else {
            // 普通字符直接添加
            result += c;
        }
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