#include "HttpConnection.h"

#include "HttpServer.h"
#include "LogicSystem.h"
#include "Log.h"
#include "MysqlConnectionPool.h"
#include "RequestDispatcher.h"
#include "Tools.h"

#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <chrono>
#include <utility>

namespace
{
    constexpr std::chrono::minutes kConnectionTimeout{ 1 };
}

HttpConnection::HttpConnection(boost::asio::io_context& ioc, HttpServer& server) noexcept
    : _socket(ioc)
    , _uuid()
    , _server(server)
    , _timer(_socket.get_executor(), kConnectionTimeout)
    , _docRoot("Z:\\CppBackEnd\\root")
{
    boost::uuids::uuid a_uuid = boost::uuids::random_generator()();
    _uuid = boost::uuids::to_string(a_uuid);
}

void HttpConnection::StartRead()
{
    LOG_INFO("StartRead uuid- " << _uuid);
    _request.clear();
    _request.body().clear();
    _response.body().clear();

    beast::http::async_read(_socket, _buffer, _request,
        [self = shared_from_this()](beast::error_code ec, std::size_t bytes_transferred) {
            boost::ignore_unused(bytes_transferred);
            if (!ec)
            {
                LOG_INFO("Incoming target: " << self->_request.target());
                LOG_INFO("Incoming BODY: \n" << beast::buffers_to_string(self->_request.body().data()));
                // self->ResetTimer();
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
    static bool first = true;
    if (first)
    {
        _response.set(beast::http::field::content_type, "application/json");
        _response.set(beast::http::field::access_control_allow_origin, "*");
        _response.set(beast::http::field::access_control_allow_methods, "GET, POST, OPTIONS, DELETE");
        _response.set(beast::http::field::access_control_allow_headers, "Content-Type");

        first = false;
    }

    try
    {
        HandleRouting();
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("HandleRead exception: " << e.what());
        SetBadRequest(e.what());
        CloseConnection();
        return;
    }

}

void HttpConnection::ServeStatic(std::string_view target)
{
    beast::http::response<beast::http::file_body> res{ beast::http::status::ok, _request.version() };
    res.keep_alive(_request.keep_alive());

    std::string relPath;
    try {
        relPath = SantisizePath(target);
    }
    catch (std::exception const& e) {
        SetBadRequest(e.what());
        return;
    }

    std::filesystem::path full = std::filesystem::path(_docRoot) / relPath;
    beast::error_code ec;
    // 映射文件内容
    res.body().open(full.string().c_str(), beast::file_mode::scan, ec);
    if (ec == boost::system::errc::no_such_file_or_directory) {
        // 构造 404 文本响应
        beast::http::response<beast::http::file_body> notFound{ beast::http::status::not_found, _request.version() };
        notFound.set(beast::http::field::content_type, "text/html; charset=UTF-8");
		std::filesystem::path full = std::filesystem::path(_docRoot) / "404.html";
		notFound.body().open(full.string().c_str(), beast::file_mode::scan, ec);
        notFound.prepare_payload();
        beast::http::write(_socket, notFound);
        StartRead();
        return;
    }
    // 其它错误走统一错误处理或日志。
    if (ec) {
        LOG_ERROR("ServeStatic open error: " << ec.message());
        SetUnProcessableEntity("File open error");
        return;
    }

    auto ext = full.extension().string();
    // 设置 MIME 类型
    if (ext == ".html" || ext == ".htm")
        res.set(beast::http::field::content_type, "text/html; charset=UTF-8");
    else if (ext == ".css")
        res.set(beast::http::field::content_type, "text/css; charset=UTF-8");
    else if (ext == ".js")
        res.set(beast::http::field::content_type, "application/javascript; charset=UTF-8");
    else
        res.set(beast::http::field::content_type, "application/octet-stream");

    res.prepare_payload();
    beast::http::write(_socket, res);
    if (!res.keep_alive()) CloseConnection();
    else StartRead();
}

std::string HttpConnection::SantisizePath(std::string_view path)
{
    if (path.empty() || path[0] != '/')
        throw std::runtime_error("非法路径");

    std::string rel{ path };
    if (rel.find("..") != std::string::npos)
        throw std::runtime_error("路径穿越");
    if (rel == "/")
        rel = "/index.html";
#ifdef _WIN32
    std::replace(rel.begin(), rel.end(), '/', '\\');
#endif
    return rel.substr(1); // 去掉前导 '/'
}


void HttpConnection::StartWrite()
{
    
    if (_response.body().size() < 200)
        LOG_INFO("StartWrite response: \n" << beast::buffers_to_string(_response.body().data()));
    else 
        LOG_INFO("StartWrite response"); // \n" << _response

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
    _timer.cancel();
    _timer.expires_after(kConnectionTimeout);
    StartTimer();
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

void HttpConnection::SetBadRequest(const std::string& reason) noexcept
{
    _response.result(beast::http::status::bad_request);
    _response.set(beast::http::field::connection, "close");
    Json::Value send;
    send["result"] = false;
	send["reason"] = reason;
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
    StartRead();
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
	StartRead();
}

// 设置401 Unauthorized响应
void HttpConnection::SetUnauthorized(const std::string& reason) noexcept
{
    _response.result(beast::http::status::unauthorized);
    _response.set(beast::http::field::www_authenticate, "Bearer realm=\"HttpServer\"");
    Json::Value body;
    body["result"] = false;
    body["reason"] = reason;
    beast::ostream(_response.body()) << Json::writeString(_writerBuilder, body);
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

void HttpConnection::HandleLogin()
{
    try
    {
        const auto body_str = beast::buffers_to_string(_request.body().data());

        Json::Value recv;
        if (!ParseUserData(body_str, recv))
        {
            LOG_INFO("ParseUserData Failed");
            SetBadRequest("ParseUserData Failed");
            CloseConnection();
        }

        _user_id = static_cast<uint32_t>(std::stoul(recv["user_id"].asString()));
        _password = recv["password"].asString();
        _role = recv["role"].asString();
        if (_role == "teacher") _role = std::string("instructor");

        if (!_dataValidator.isUserExists(_user_id, _password, _role))
        {
            LOG_INFO("User Not Exist");
            SetBadRequest("User Not Exist");
            CloseConnection();
        }

        LOG_INFO("User Exists, Login Success");

        auto sessionId = SessionManager::Instance().CreateSession(_user_id, _role, std::chrono::minutes(30));
        const auto expiresAt = std::chrono::system_clock::now() + std::chrono::minutes(30);
        const std::time_t expiresTime = std::chrono::system_clock::to_time_t(expiresAt);
        std::tm gmt{};

#if defined(_WIN32)
        gmtime_s(&gmt, &expiresTime);
#else
        gmtime_r(&expiresTime, &gmt);
#endif
        char buffer[64]{};
        std::strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", &gmt);
        _response.set(beast::http::field::set_cookie,
            "SESSION_ID=" + sessionId + "; Expires=" + buffer +
            "; HttpOnly; Secure; SameSite=Lax");
		
        _response.result(beast::http::status::ok);
        Json::Value send;
        send["result"] = true;
        send["user_id"] = _user_id;
		send["session_id"] = sessionId;

        beast::ostream(_response.body()) << Json::writeString(_writerBuilder, send);
        StartWrite();

    }
    catch (const std::exception& e)
    {
        LOG_ERROR("HandleLogin exception: " << e.what());
        SetBadRequest(e.what());
        CloseConnection();
        // 这里得return true ，使得回调链中断
    }
}

void HttpConnection::HandleRouting()
{
    if (_request.method() == beast::http::verb::options)
    {
        _response.result(beast::http::status::ok);
        _response.prepare_payload();
        beast::http::write(_socket, _response);
        StartRead();
        return;
    }

    if (_request.method() == beast::http::verb::get &&
        (_request.target().starts_with("/static/") || _request.target() == "/")) {
        ServeStatic(_request.target());
        return;
    }

    const bool isLogin = (_request.method() == beast::http::verb::post &&
        _request.target() == "/api/login/post");

    if (isLogin)
    {
        HandleLogin(); // 登陆成功，进入StartWrite异步链，这里直接return
        return;
    }

	if (_role.length() == 0 && !AuthenticateRequest()) // 内部不匹配则返回401
    {
        StartRead();
        return;
    }

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
        LOG_INFO("NOT IN QUEUE");
        // 已匹配但未入队 → 说明调度器内部因为校验失败等原因已经写回错误响应，直接返回即可。
        return;
    }


    // 正常命中并入队 → 函数结尾注明任务已入队，等待逻辑线程处理。
}

void HttpConnection::CloseConnection() noexcept
{
    beast::error_code ec;
	LOG_INFO("CloseConnection: " << _uuid);
    _socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    _socket.close(ec);
    _server.ClearConnection(_uuid);
}

// 取出并验证请求中的 SESSION_ID
bool HttpConnection::AuthenticateRequest()
{
    auto token = ExtractSessionId();
    // 缺失时直接返回 401
    if (!token)
    {
        SetUnauthorized("Missing session token");
        return false;
    }
    // 确认 token 合法且未过期；无效即返回 401
    auto session = SessionManager::Instance().ValidateSession(*token);
    if (!session)
    {
        SetUnauthorized("Invalid session");
        return false;
    }

    _sessionId = *token;
    _user_id = session->user_id;
    _role = session->role;

    constexpr auto ttl = std::chrono::minutes(30);
    SessionManager::Instance().RefreshSession(*token, ttl); // 滑动续期

    const auto expiresAt = std::chrono::system_clock::now() + ttl;
    const std::time_t expiresTime = std::chrono::system_clock::to_time_t(expiresAt);
    std::tm gmt{}; // C 标准库定义的时间结构体，包含年、月、日、时、分、秒等字段，用于格式化时间。

    // gmtime_s(&gmt, &expiresTime): 把 time_t（Unix 时间戳）转换成协调世界时（UTC/GMT）下的 std::tm 结构；
#if defined(_WIN32)
    gmtime_s(&gmt, &expiresTime);
#else
    gmtime_r(&expiresTime, &gmt);
#endif
    char buffer[64]{};
    // 按照 HTTP 标准的 GMT 日期格式（RFC 7231）
    // 把 std::tm 写入字符缓冲区，形如 "Mon, 21 Oct 2024 11:30:00 GMT"，用于 Expires = 参数
    std::strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", &gmt);

    _response.set(beast::http::field::set_cookie,
        "SESSION_ID=" + _sessionId + "; Expires=" + buffer +
        "; HttpOnly; Secure; SameSite=Lax");

    return true;
}


// 实现从 Authorization: Bearer xxx 中取出 xxx
// 或者从 Cookie: SESSION_ID=xxx; other_cookie=yyy 中取出 xxx
std::optional<std::string> HttpConnection::ExtractSessionId() const
{
	// C++17 if with initializer
    // _request.base(): 返回 boost::beast::http::fields&，即请求首部字段容器（类似键值表）
    if (auto auth = _request.base().find("Authorization");
        auth != _request.base().end())
    {
		const std::string value = std::string(auth->value()); // string_view to string
        constexpr std::string_view bearer = "Bearer ";
        if (value.size() > bearer.size() &&
            std::equal(bearer.begin(), bearer.end(), value.begin()))
        {
            return value.substr(bearer.size());
        }
    }

    if (auto cookie = _request.base().find("Cookie");
        cookie != _request.base().end())
    {
		const std::string cookies = std::string(cookie->value()); // string_view to string
        constexpr std::string_view key = "SESSION_ID=";
        std::size_t pos = cookies.find(key);
        if (pos != std::string::npos)
        {
            pos += key.size();
            std::size_t end = cookies.find(';', pos);
            return cookies.substr(pos, end - pos);
        }
    }
    return std::nullopt;
}


DataValidator HttpConnection::_dataValidator;
