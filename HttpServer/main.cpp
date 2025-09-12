// HttpServer.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。

#include <iostream>
#include "HttpServer.h"
#include <json/json.h>
#include <mysqlx/xdevapi.h>

int main()
{
    try{
#ifdef _WIN32
        system("chcp 65001 > nul");
#endif
        // MySQLConnectionPool::Instance().Initialize();
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

