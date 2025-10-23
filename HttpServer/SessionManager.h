#pragma once

#include <boost/asio.hpp>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <optional>
#include <chrono>

struct SessionInfo
{
    uint32_t user_id{ 0 };
    std::string role;
    std::chrono::steady_clock::time_point expire_at;
};

class SessionManager
{
public:
    static SessionManager& Instance() noexcept;

    std::string CreateSession(uint32_t user_id, const std::string& role,
        std::chrono::seconds ttl = std::chrono::minutes(30));

    std::optional<SessionInfo> ValidateSession(const std::string& session_id);

    void RefreshSession(const std::string& session_id,
        std::chrono::seconds ttl = std::chrono::minutes(30));

    void RemoveSession(const std::string& session_id) noexcept;

    void RemoveExpiredSessions() noexcept;

private:
    SessionManager() = default;
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;

    std::string GenerateToken();

private:
    std::unordered_map<std::string, SessionInfo> _sessions;
    mutable std::shared_mutex _mutex;
};
