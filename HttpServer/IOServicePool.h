#pragma once
#include <vector>
#include <boost/asio.hpp>

// ������

class IOServicePool
{
    using IOService = boost::asio::io_context;
    // ʹ�� executor_work_guard ��� io_context::work
    using WorkGuard = boost::asio::executor_work_guard<IOService::executor_type>;
    using WorkGuardPtr = std::unique_ptr<WorkGuard>;
public:
    ~IOServicePool();
    IOServicePool(const IOServicePool&) = delete;
    IOServicePool& operator=(const IOServicePool&) = delete;
    
    static IOServicePool& Instance();
    // ʹ�� round-robin �ķ�ʽ����һ�� io_service
    boost::asio::io_context& GetIOService();

private:
    IOServicePool(std::size_t size = std::thread::hardware_concurrency());

    std::vector<IOService> _ioServices;
    std::vector<WorkGuardPtr> _workGuards;  // ������Ϊ workGuards
    std::vector<std::thread> _threads;
    std::size_t _nextIOService;
};

