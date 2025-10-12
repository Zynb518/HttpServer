#include "IOServicePool.h"
#include <iostream>
#include "Log.h"

IOServicePool::~IOServicePool()
{
    LOG_INFO("IOServicePool Destruct");
    // 1. ���� work guards (���� io_context �˳� run() ѭ��)
    for (auto& guard : _workGuards) {
        guard.reset();  // ����ͷŹ�������
    }
    // 2. ֹͣ���� I/O ����
    for (auto& service : _ioServices) {
        service.stop();
    }
    // 3. �ȴ������߳��˳�
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
    : _ioServices(size), _workGuards(size), _nextIOService(0)   // ʹ�� workGuards ��� works
{
    for (std::size_t i = 0; i < size; ++i) {
        // ʹ�� executor_work_guard ��� work
        _workGuards[i] = std::make_unique<WorkGuard>(
            boost::asio::make_work_guard(_ioServices[i])
        );
    }

    // ��ֹû���첽����ֱ�ӷ���
    for (std::size_t i = 0; i < _ioServices.size(); ++i) {
        _threads.emplace_back([this, i]() {
            _ioServices[i].run();
            });
    }
}