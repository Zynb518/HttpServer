#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <json/json.h>

#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <optional>
#include <filesystem>

#include "SessionManager.h"
#include "DataValidator.h"
#include "MysqlStReqHandler.h"
#include "MysqlInstrReqHandler.h"
#include "MysqlAdmReqHandler.h"


namespace beast = boost::beast;

class HttpServer;

class HttpConnection : public std::enable_shared_from_this<HttpConnection>
{
    using tcp = boost::asio::ip::tcp;

public:
    HttpConnection(boost::asio::io_context& ioc, HttpServer& server) noexcept;

    inline tcp::socket& GetSocket() noexcept { return _socket; }
	inline std::string_view GetUuid() const noexcept { return _uuid; }
	inline beast::http::response<beast::http::dynamic_body>& GetResponse() noexcept { return _response; }

    inline uint32_t GetUserId() const noexcept { return _user_id; }
    inline std::string& GetRole() noexcept { return _role; }
    inline std::string& GetPassword() noexcept { return _password; }

    void StartRead();
    void StartWrite();

    void SetUnProcessableEntity(Json::Value& message) noexcept;
    void SetUnProcessableEntity(const std::string& reason) noexcept;
    void SetUnauthorized(const std::string& reason) noexcept;
    void SetBadRequest(const std::string& reason = "") noexcept;

private:
    void StartTimer();
    void ResetTimer();

    void HandleLogin();
    bool ParseUserData(const std::string& body, Json::Value& out);
    void WriteBadResponse() noexcept;

    void HandleRead();
    void ServeStatic(std::string_view target);   // ¡û ÐÂÔö
    std::string SantisizePath(std::string_view path);
    void HandleRouting();

    void CloseConnection() noexcept;

private:
    bool AuthenticateRequest();
    std::optional<std::string> ExtractSessionId() const;
    std::string _sessionId;

private:
    tcp::socket _socket;
    beast::flat_buffer _buffer;
    std::string _uuid;
    HttpServer& _server;
    boost::asio::steady_timer _timer;

    beast::http::request<beast::http::dynamic_body> _request;
    beast::http::response<beast::http::dynamic_body> _response;

    Json::StreamWriterBuilder _writerBuilder;
    Json::CharReaderBuilder _readerBuilder;

    uint32_t _user_id{ 0 };
    std::string _password;
    std::string _role;
    std::string _docRoot;

    static DataValidator _dataValidator;
    MysqlStReqHandler _studentHandler;
    MysqlInstrReqHandler _instructorHandler;
    MysqlAdmReqHandler _adminHandler;
};
