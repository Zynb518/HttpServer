#pragma once

#include <boost/asio.hpp>
#include <boost/beast/http.hpp>
#include <json/json.h>

#include <memory>
#include <string>
#include <string_view>

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

    tcp::socket& GetSocket() noexcept;
    std::string_view GetUuid() const noexcept;
    beast::http::response<beast::http::dynamic_body>& GetResponse() noexcept;

    uint32_t GetUserId() const noexcept { return _user_id; }
    std::string& GetRole() noexcept { return _role; }
    std::string& GetPassword() noexcept { return _password; }

    void ReadLogin();
    void StartWrite();

    void SetUnProcessableEntity(Json::Value& message) noexcept;
    void SetUnProcessableEntity(const std::string& reason) noexcept;
    void SetBadRequest() noexcept;

private:
    void StartTimer();
    void ResetTimer();

    bool HandleLogin();
    bool ParseUserData(const std::string& body, Json::Value& out);
    void WriteBadResponse() noexcept;

    void StartRead();
    void HandleRead();
    void HandleRouting();

    void CloseConnection() noexcept;

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

    static DataValidator _dataValidator;
    MysqlStReqHandler _studentHandler;
    MysqlInstrReqHandler _instructorHandler;
    MysqlAdmReqHandler _adminHandler;
};
