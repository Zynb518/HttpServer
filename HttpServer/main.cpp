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
        std::cout << std::isdigit('8') << std::endl;

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

