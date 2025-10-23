#include "SessionManager.h"

#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

SessionManager& SessionManager::Instance() noexcept
{
    static SessionManager instance;
    return instance;
}

// 创建一个新的会话，返回会话ID
std::string SessionManager::CreateSession(uint32_t user_id, const std::string& role,
    std::chrono::seconds ttl)
{
    const auto token = GenerateToken();
    SessionInfo info;
    info.user_id = user_id;
    info.role = role;
    info.expire_at = std::chrono::steady_clock::now() + ttl;

	// 独占锁定以修改会话映射
    {
        std::unique_lock lock(_mutex);
        _sessions[token] = std::move(info);
    }

    return token;
}


std::optional<SessionInfo> SessionManager::ValidateSession(const std::string& session_id)
{
    // 共享锁 只读操作
    std::shared_lock lock(_mutex); 
    auto it = _sessions.find(session_id);
    if (it == _sessions.end())
    {
        return std::nullopt;
    }

	// 会话过期，先释放共享锁，再删除会话
    if (std::chrono::steady_clock::now() > it->second.expire_at)
    {
        lock.unlock();
        RemoveSession(session_id);
        return std::nullopt;
    }

    return it->second;
}

// 刷新会话的过期时间
void SessionManager::RefreshSession(const std::string& session_id, std::chrono::seconds ttl)
{
    std::unique_lock lock(_mutex);
    auto it = _sessions.find(session_id);
    if (it == _sessions.end())
    {
        return;
    }
    it->second.expire_at = std::chrono::steady_clock::now() + ttl;
}

// 移除指定会话
void SessionManager::RemoveSession(const std::string& session_id) noexcept
{
    std::unique_lock lock(_mutex);
    _sessions.erase(session_id);
}

// 移除所有过期的会话
void SessionManager::RemoveExpiredSessions() noexcept
{
    const auto now = std::chrono::steady_clock::now();
    std::unique_lock lock(_mutex);
    for (auto it = _sessions.begin(); it != _sessions.end();)
    {
        if (now > it->second.expire_at)
        {
			it = _sessions.erase(it); // erase 返回下一个有效迭代器
        }
        else
        {
            ++it;
        }
    }
}

// 生成唯一的会话令牌
std::string SessionManager::GenerateToken()
{
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    return boost::uuids::to_string(uuid);
}