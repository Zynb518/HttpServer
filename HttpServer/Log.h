#pragma once
#include <iostream>
#include <string>

// ��־������
#define LOG_LEVEL_DEBUG    0
#define LOG_LEVEL_INFO     1
#define LOG_LEVEL_WARNING  2
#define LOG_LEVEL_ERROR    3

// ��ǰ��־���𣨿����޸����ֵ�����������
#define CURRENT_LOG_LEVEL LOG_LEVEL_DEBUG

// ����ʹ�ô�ͳ�ַ�������
inline const char* get_filename(const char* path) {
    const char* filename = strrchr(path, '/');
    if (filename) return filename + 1;

    filename = strrchr(path, '\\');
    if (filename) return filename + 1;

    return path;
}

// ������־��
#define LOG(level, level_str, ...) \
    do { \
        if (level >= CURRENT_LOG_LEVEL) { \
            std::cout << "[" << level_str << "] " << get_filename(__FILE__) << ":" << __LINE__ << " - "; \
            std::cout << __VA_ARGS__ << std::endl; \
        } \
    } while(0)

// ���弶�����־��
#define LOG_DEBUG(...)   LOG(LOG_LEVEL_DEBUG, "DEBUG", __VA_ARGS__)
#define LOG_INFO(...)    LOG(LOG_LEVEL_INFO, "INFO", __VA_ARGS__)
#define LOG_WARNING(...) LOG(LOG_LEVEL_WARNING, "WARNING", __VA_ARGS__)
#define LOG_ERROR(...)   LOG(LOG_LEVEL_ERROR, "ERROR", __VA_ARGS__)

// ������־��
#define LOG_IF(condition, ...) \
    do { \
        if (condition) { \
            LOG_INFO(__VA_ARGS__); \
        } \
    } while(0)

// ��ȫ�ر�������־�ĺ�
// #define LOG_DEBUG(...)
// #define LOG_INFO(...)
// #define LOG_WARNING(...)
// #define LOG_ERROR(...)