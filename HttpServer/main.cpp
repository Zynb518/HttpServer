// HttpServer.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。

#include <iostream>
#include <string>
#include <sstream>
#include <json/json.h>
#include <mysqlx/xdevapi.h>
#include <unordered_map>
#include <functional>
#include <string_view>

#include "HttpServer.h"
#include "MysqlConnectionPool.h"
#include "Tools.h"
#include "Log.h"

#ifdef _WIN32
#include <windows.h>
#endif
int main()
{
    try{
#ifdef _WIN32
        system("chcp 65001 > nul");
#endif
        std::string s = GetUTF8ForDatabase(L"2025春");
        std::cout << s.size();
        // 定义一个 std::unordered_map，以 string_view 为键，std::function 为值
        std::unordered_map<std::string_view, std::function<void()>> map;

        // 定义一个简单的函数
        auto func = []() { std::cout << "Hello from the function!" << std::endl; };

        // 向 map 中插入一个键值对，使用 std::string 作为键
        std::string key = "hello";
        map[key] = func;

        // 使用 std::string_view 作为键进行查找
        std::string_view key_view = "hello";
        auto it = map.find(key_view);

        if (it != map.end()) {
            // 找到对应的函数并调用
            it->second();
        }
        else {
            std::cout << "Key not found!" << std::endl;
        }


        boost::asio::io_context ioc;
        HttpServer server(ioc, 10086);
        ioc.run();
    }
    catch (const mysqlx::Error& err) {
        std::cerr << "数据库查询错误: " << err.what() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "标准异常: " << e.what() << std::endl;
    }
    
}

