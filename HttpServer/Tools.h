#pragma once
#include <iostream>
#include <string>
#include <chrono>
#include <sstream>
#include <vector>
#include <cstdio>

// ������ UTF-16 �� UTF-8 ת���������ڻ���������ƽ���ַ���
constexpr std::string GetUTF8ForDatabase(std::wstring_view wideStr) noexcept {
    if (wideStr.empty()) return "";

    std::string result;
    for (wchar_t wc : wideStr) {
        if (wc <= 0x7F) {
            // ASCII �ַ�
            result += static_cast<char>(wc);
        }
        else if (wc <= 0x7FF) {
            // 2�ֽ� UTF-8
            result += static_cast<char>(0xC0 | ((wc >> 6) & 0x1F));
            result += static_cast<char>(0x80 | (wc & 0x3F));
        }
        else if (wc <= 0xFFFF) {
            // 3�ֽ� UTF-8�������ڻ���������ƽ�棩
            result += static_cast<char>(0xE0 | ((wc >> 12) & 0x0F));
            result += static_cast<char>(0x80 | ((wc >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (wc & 0x3F));
        }
        // ���ڴ���ԣ�surrogate pairs����Ҫ�����ӵĴ���
    }
    return result;
}

constexpr std::string UrlDecode(std::string_view urlEncodedStr) noexcept {
    std::string result;
    result.reserve(urlEncodedStr.length()); // Ԥ����ռ����Ч��

    for (size_t i = 0; i < urlEncodedStr.length(); ++i) {
        char c = urlEncodedStr[i];

        if (c == '%' && i + 2 < urlEncodedStr.length()) {
            // ���� %XX ��ʽ
            char hex1 = urlEncodedStr[i + 1];
            char hex2 = urlEncodedStr[i + 2];

            if (std::isxdigit(hex1) && std::isxdigit(hex2)) {
                // ��ʮ�������ַ�ת��Ϊ�ֽ�ֵ
                auto hexCharToValue = [](char h) -> unsigned char {
                    if (h >= '0' && h <= '9') return h - '0';
                    if (h >= 'A' && h <= 'F') return h - 'A' + 10;
                    if (h >= 'a' && h <= 'f') return h - 'a' + 10;
                    return 0;
                    };

                unsigned char byteValue = (hexCharToValue(hex1) << 4) | hexCharToValue(hex2);
                result += static_cast<char>(byteValue);
                i += 2; // �����Ѵ������λʮ����������
            }
            else {
                // ��Ч��ʮ�����ƣ�����ԭ��
                result += c;
            }
        }
        else if (c == '+') {
            // URL������ '+' ��ʾ�ո�
            result += ' ';
        }
        else {
            // ��ͨ�ַ�ֱ�����
            result += c;
        }
    }

    return result;
}

struct Timer
{
    std::chrono::time_point<std::chrono::steady_clock> start, end; //���ﲻ����auto 
    std::chrono::duration<float> duration;

    Timer();
    ~Timer();
};