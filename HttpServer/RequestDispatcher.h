#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <json/json.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace beast = boost::beast;

class HttpConnection;
class MysqlStReqHandler;
class MysqlInstrReqHandler;
class MysqlAdmReqHandler;
#include "DataValidator.h"

class RequestDispatcher
{
public:
    using Task = std::function<void()>;

    struct DispatchResult
    {
		bool matched{ false }; // 是否匹配到路由
		bool queued{ false }; // 是否成功加入任务队列
    };

    RequestDispatcher(std::shared_ptr<HttpConnection> connection,
        beast::http::request<beast::http::dynamic_body>& request,
        MysqlStReqHandler& studentHandler,
        MysqlInstrReqHandler& instructorHandler,
        MysqlAdmReqHandler& adminHandler) noexcept;

    DispatchResult Dispatch();
    std::optional<Task> ResolveTaskOnly();

private:
    struct Resolved
    {
        bool matched{ false };
        std::optional<Task> task{};
    };

    Resolved Resolve();
    Resolved ResolveStudent(std::string_view target);
    Resolved ResolveInstructor(std::string_view target);
    Resolved ResolveAdmin(std::string_view target);

    Resolved ResolveStudentGet(std::string_view target);
    Resolved ResolveStudentPost(std::string_view target);

    Resolved ResolveInstructorGet(std::string_view target);
    Resolved ResolveInstructorPost(std::string_view target);

    Resolved ResolveAdminGet(std::string_view target);
    Resolved ResolveAdminPost(std::string_view target);
    Resolved ResolveAdminPut(std::string_view target);
    Resolved ResolveAdminDelete(std::string_view target);

    template<typename Functor>
    std::optional<Task> MakeTask(Functor&& functor) noexcept
    {
        return std::optional<Task>{ Task(std::forward<Functor>(functor)) };
    }

    void ReportInvalid(std::string_view reason);
    bool ParseRequestBody(Json::Value& out);
    static bool StartsWith(std::string_view text, std::string_view prefix) noexcept;

    bool ParseUserData(const std::string& body, Json::Value& out);

private:
    std::shared_ptr<HttpConnection> _connection;
    beast::http::request<beast::http::dynamic_body>& _request;
    MysqlStReqHandler& _studentHandler;
    MysqlInstrReqHandler& _instructorHandler;
    MysqlAdmReqHandler& _adminHandler;

    static DataValidator _dataValidator;
    static Json::CharReaderBuilder _readerBuilder;
};
