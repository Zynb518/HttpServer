#pragma once
#include <vector>
#include <boost/asio.hpp>

// 单例类

class IOServicePool
{
    using IOService = boost::asio::io_context;
    // 使用 executor_work_guard 替代 io_context::work
    using WorkGuard = boost::asio::executor_work_guard<IOService::executor_type>;
    using WorkGuardPtr = std::unique_ptr<WorkGuard>;
public:
    ~IOServicePool();
    IOServicePool(const IOServicePool&) = delete;
    IOServicePool& operator=(const IOServicePool&) = delete;
    
    static IOServicePool& Instance();
    // 使用 round-robin 的方式返回一个 io_service
    boost::asio::io_context& GetIOService();

private:
    IOServicePool(std::size_t size = std::thread::hardware_concurrency());

    std::vector<IOService> _ioServices;
    std::vector<WorkGuardPtr> _workGuards;  // 重命名为 workGuards
    std::vector<std::thread> _threads;
    std::size_t _nextIOService;
};

