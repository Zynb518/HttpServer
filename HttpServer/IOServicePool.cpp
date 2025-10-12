#include "IOServicePool.h"
#include <iostream>
#include "Log.h"

IOServicePool::~IOServicePool()
{
    LOG_INFO("IOServicePool Destruct");
    // 1. 重置 work guards (允许 io_context 退出 run() 循环)
    for (auto& guard : _workGuards) {
        guard.reset();  // 这会释放工作保护
    }
    // 2. 停止所有 I/O 服务
    for (auto& service : _ioServices) {
        service.stop();
    }
    // 3. 等待所有线程退出
    for (auto& t : _threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}

IOServicePool& IOServicePool::Instance()
{
    static IOServicePool pool;
    return pool;
}

boost::asio::io_context& IOServicePool::GetIOService()
{
    auto& service = _ioServices[_nextIOService];
    _nextIOService = (_nextIOService + 1) % _ioServices.size();
    return service;
}


IOServicePool::IOServicePool(std::size_t size)
    : _ioServices(size), _workGuards(size), _nextIOService(0)   // 使用 workGuards 替代 works
{
    for (std::size_t i = 0; i < size; ++i) {
        // 使用 executor_work_guard 替代 work
        _workGuards[i] = std::make_unique<WorkGuard>(
            boost::asio::make_work_guard(_ioServices[i])
        );
    }

    // 防止没有异步任务直接返回
    for (std::size_t i = 0; i < _ioServices.size(); ++i) {
        _threads.emplace_back([this, i]() {
            _ioServices[i].run();
            });
    }
}