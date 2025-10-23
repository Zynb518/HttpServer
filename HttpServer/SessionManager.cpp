#include "SessionManager.h"

#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

SessionManager& SessionManager::Instance() noexcept
{
    static SessionManager instance;
    return instance;
}

// ����һ���µĻỰ�����ػỰID
std::string SessionManager::CreateSession(uint32_t user_id, const std::string& role,
    std::chrono::seconds ttl)
{
    const auto token = GenerateToken();
    SessionInfo info;
    info.user_id = user_id;
    info.role = role;
    info.expire_at = std::chrono::steady_clock::now() + ttl;

	// ��ռ�������޸ĻỰӳ��
    {
        std::unique_lock lock(_mutex);
        _sessions[token] = std::move(info);
    }

    return token;
}


std::optional<SessionInfo> SessionManager::ValidateSession(const std::string& session_id)
{
    // ������ ֻ������
    std::shared_lock lock(_mutex); 
    auto it = _sessions.find(session_id);
    if (it == _sessions.end())
    {
        return std::nullopt;
    }

	// �Ự���ڣ����ͷŹ���������ɾ���Ự
    if (std::chrono::steady_clock::now() > it->second.expire_at)
    {
        lock.unlock();
        RemoveSession(session_id);
        return std::nullopt;
    }

    return it->second;
}

// ˢ�»Ự�Ĺ���ʱ��
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

// �Ƴ�ָ���Ự
void SessionManager::RemoveSession(const std::string& session_id) noexcept
{
    std::unique_lock lock(_mutex);
    _sessions.erase(session_id);
}

// �Ƴ����й��ڵĻỰ
void SessionManager::RemoveExpiredSessions() noexcept
{
    const auto now = std::chrono::steady_clock::now();
    std::unique_lock lock(_mutex);
    for (auto it = _sessions.begin(); it != _sessions.end();)
    {
        if (now > it->second.expire_at)
        {
			it = _sessions.erase(it); // erase ������һ����Ч������
        }
        else
        {
            ++it;
        }
    }
}

// ����Ψһ�ĻỰ����
std::string SessionManager::GenerateToken()
{
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    return boost::uuids::to_string(uuid);
}