#pragma once
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

// 处理数据库业务
class LogicSystem
{
public:
	LogicSystem(const LogicSystem&) = delete;
	LogicSystem& operator=(const LogicSystem&) = delete;
	static LogicSystem& Instance() noexcept;
	void WorkFunc();
	void Stop() noexcept;

	// 通用任务入队函数
	template<typename Func, typename... Args>
	inline void PushToQue(Func&& func, Args&&... args);

private:
	LogicSystem() noexcept;
	~LogicSystem() noexcept;
	bool _stop = false;
	std::thread _worker;
	std::condition_variable _conVar;
	std::mutex _mutex;
	std::queue<std::function<void()>> _taskQue;

};

template<typename Func, typename ...Args>
inline void LogicSystem::PushToQue(Func&& func, Args&& ...args)
{
	auto task = [func = std::forward<Func>(func),
		args_tuple = std::make_tuple(std::forward<Args>(args)...)]() mutable {
		std::apply(func, std::move(args_tuple));
		};

	std::lock_guard<std::mutex> lock(_mutex);
	_taskQue.emplace(std::move(task));
	_conVar.notify_one();
}
