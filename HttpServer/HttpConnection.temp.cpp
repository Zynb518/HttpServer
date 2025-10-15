#include "HttpConnection.temp.h"

#include "HttpServer.h"
#include "LogicSystem.h"
#include "Log.h"
#include "MysqlConnectionPool.h"
#include "RequestDispatcher.temp.hpp"
#include "Tools.h"

#include <boost/beast/core.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/system/error_code.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <chrono>
#include <utility>

namespace
{
    constexpr std::chrono::seconds kConnectionTimeout{ 60 };
}

HttpConnection::HttpConnection(boost::asio::io_context& ioc, HttpServer& server) noexcept
    : _socket(ioc)
    , _uuid()
    , _server(server)
    , _timer(_socket.get_executor(), kConnectionTimeout)
{
    boost::uuids::uuid a_uuid = boost::uuids::random_generator()();
    _uuid = boost::uuids::to_string(a_uuid);
}

boost::asio::ip::tcp::socket& HttpConnection::GetSocket() noexcept
{
    return _socket;
}

std::string_view HttpConnection::GetUuid() const noexcept
{
    return _uuid;
}

beast::http::response<beast::http::dynamic_body>& HttpConnection::GetResponse() noexcept
{
    return _response;
}

void HttpConnection::ReadLogin()
{
    LOG_INFO("Start ReadLogin");
    beast::http::async_read(_socket, _buffer, _request,
        [this, self = shared_from_this()](beast::error_code ec, std::size_t bytes_transferred) {
            boost::ignore_unused(bytes_transferred);
            if (!ec)
            {
                LOG_INFO("---- HandleLogin ---- target: " << _request.target());
                if (!HandleLogin())
                {
                    SetUnProcessableEntity("Login-Failure");
                    ReadLogin();
                }
            }
            else
            {
                LOG_INFO("ReadLogin async_read error: " << ec.message());
                CloseConnection();
            }
        });
}

void HttpConnection::StartWrite()
{
    LOG_INFO("After StartWrite response\n" << _response);
    _response.prepare_payload();
    beast::http::async_write(_socket, _response,
        [this, self = shared_from_this()](beast::error_code ec, std::size_t) {
            if (ec)
            {
                LOG_ERROR("HttpConnection async_write error: " << ec.message());
                CloseConnection();
                return;
            }
            StartRead();
        });
}

void HttpConnection::StartTimer()
{
    _timer.async_wait([self = shared_from_this()](boost::system::error_code ec) {
        if (!ec)
        {
            self->CloseConnection();
        }
    });
}

void HttpConnection::ResetTimer()
{
    boost::system::error_code ec;
    _timer.cancel(ec);
    _timer.expires_after(kConnectionTimeout);
    StartTimer();
}

bool HttpConnection::HandleLogin()
{
    try
    {
        _response.version(_request.version());
        _response.keep_alive(_request.keep_alive());
        _response.set(beast::http::field::content_type, "application/json");
        _response.set(beast::http::field::access_control_allow_origin, "*");
        _response.set(beast::http::field::access_control_allow_methods, "GET, POST, OPTIONS, DELETE");
        _response.set(beast::http::field::access_control_allow_headers, "Content-Type");

        if (_request.method() == beast::http::verb::options)
        {
            _response.result(beast::http::status::ok);
            _response.prepare_payload();
            beast::http::write(_socket, _response);
            ReadLogin();
            return true;
        }

        if (_request.method() == beast::http::verb::post &&
            _request.target() == "/api/login/post")
        {
            const auto body_str = beast::buffers_to_string(_request.body().data());

            Json::Value recv;
            if (!ParseUserData(body_str, recv))
            {
                LOG_INFO("ParseUserData Failed");
                return false;
            }

            _user_id = static_cast<uint32_t>(std::stoul(recv["user_id"].asString()));
            _password = recv["password"].asString();
            _role = recv["role"].asString();
            if (_role == "teacher") _role = "instructor";
            if (_role == "admin") _role = "administer";

            if (!_dataValidator.isUserExists(_user_id, _password, _role))
            {
                LOG_INFO("User Not Exist");
                return false;
            }

            LOG_INFO("User Exists, Login Success");
            _response.result(beast::http::status::ok);
            Json::Value send;
            send["result"] = true;
            send["role"] = _role;
            beast::ostream(_response.body()) << Json::writeString(_writerBuilder, send);
            StartWrite();
            return true;
        }

        SetUnProcessableEntity("Unsupported Login Request");
        return false;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("HandleLogin exception: " << e.what());
        SetBadRequest();
        CloseConnection();
        return false;
    }
}

bool HttpConnection::ParseUserData(const std::string& body, Json::Value& out)
{
    std::unique_ptr<Json::CharReader> jsonReader(_readerBuilder.newCharReader());
    std::string err;
    if (!jsonReader->parse(body.c_str(), body.c_str() + body.length(), &out, &err))
    {
        LOG_ERROR("ParseUserData Error: " << err);
        return false;
    }
    return true;
}

void HttpConnection::SetBadRequest() noexcept
{
    _response.result(beast::http::status::bad_request);
    _response.set(beast::http::field::connection, "close");
    Json::Value send;
    send["result"] = false;
    beast::ostream(_response.body()) << Json::writeString(_writerBuilder, send);
    _response.prepare_payload();
    WriteBadResponse();
}

void HttpConnection::SetUnProcessableEntity(Json::Value& message) noexcept
{
    _response.result(beast::http::status::unprocessable_entity);
    beast::ostream(_response.body()) << Json::writeString(_writerBuilder, message);
    _response.prepare_payload();
    WriteBadResponse();
}

void HttpConnection::SetUnProcessableEntity(const std::string& reason) noexcept
{
    LOG_ERROR(reason);
    _response.result(beast::http::status::unprocessable_entity);
    Json::Value root;
    root["result"] = false;
    root["reason"] = reason;
    beast::ostream(_response.body()) << Json::writeString(_writerBuilder, root);
    _response.prepare_payload();
    WriteBadResponse();
}

void HttpConnection::WriteBadResponse() noexcept
{
    beast::http::async_write(_socket, _response,
        [self = shared_from_this()](beast::error_code ec, std::size_t) {
            if (ec)
            {
                LOG_ERROR("WriteBadResponse async_write error: " << ec.message());
            }
        });
}

void HttpConnection::StartRead()
{
    _response.body().clear();

    beast::http::async_read(_socket, _buffer, _request,
        [self = shared_from_this()](beast::error_code ec, std::size_t bytes_transferred) {
            boost::ignore_unused(bytes_transferred);
            if (!ec)
            {
                LOG_INFO("Incoming target: " << self->_request.target());
                self->HandleRead();
            }
            else
            {
                LOG_INFO("HttpConnection read error: " << ec.message());
                self->CloseConnection();
            }
        });
}

void HttpConnection::HandleRead()
{
    _response.version(_request.version());
    _response.keep_alive(_request.keep_alive());

    try
    {
        if (_request.method() == beast::http::verb::options)
        {
            _response.result(beast::http::status::ok);
            _response.prepare_payload();
            beast::http::write(_socket, _response);
            StartRead();
            return;
        }

        ResetTimer();
        HandleRouting();
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("HandleRead exception: " << e.what());
        SetBadRequest();
        CloseConnection();
        return;
    }

    StartRead();
}

void HttpConnection::HandleRouting()
{
    RequestDispatcher dispatcher(shared_from_this(), _request,
        _studentHandler, _instructorHandler, _adminHandler);

    const auto result = dispatcher.Dispatch();

    if (!result.matched)
    {
        SetUnProcessableEntity("Route Not Found");
        return;
    }

    if (!result.queued)
    {
        // Dispatcher already responded on validation failure.
        return;
    }

    // Task enqueued for execution on logic thread.
}

void HttpConnection::CloseConnection() noexcept
{
    beast::error_code ec;
    _socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    _socket.close(ec);
    _server.ClearConnection(_uuid);
}

DataValidator HttpConnection::_dataValidator;
