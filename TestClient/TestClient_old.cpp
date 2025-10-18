#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#include <json/json.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

namespace
{

    struct RequestDefinition // HTTP请求
    {
        http::verb method{ http::verb::get };
        std::string target;
        std::string description;
        std::map<std::string, std::string> headers;
        std::optional<Json::Value> body;
        bool enabled{ true };
        std::optional<unsigned int> expectedStatus;
    };

    struct Scenario
    {
        std::string name;
        std::string description;
        bool requiresLogin{ true };
        RequestDefinition login;
        std::vector<RequestDefinition> requests;
    };

    struct CmdOptions
    {
        std::string host{ "127.0.0.1" };
        unsigned short port{ 10086 };
        std::optional<std::string> configPath{};
        std::vector<std::string> scenarios{}; // 运行场景
        int repeat{ 1 }; // 重复几次
        bool listOnly{ false };
        bool printHeaders{ false };
        bool quiet{ false };
        bool showHelp{ false };
    };

    std::string ToUpper(std::string_view input)
    {
        std::string output;
        output.reserve(input.size());
        std::transform(input.begin(), input.end(), std::back_inserter(output), [](unsigned char ch)
            {
                return static_cast<char>(std::toupper(ch));
            });
        return output;
    }

    http::verb ParseVerb(std::string_view text)
    {
        const auto upper = ToUpper(text);
        if (upper == "GET") return http::verb::get;
        if (upper == "POST") return http::verb::post;
        if (upper == "PUT") return http::verb::put;
        if (upper == "DELETE") return http::verb::delete_;
        if (upper == "PATCH") return http::verb::patch;
        if (upper == "HEAD") return http::verb::head;
        if (upper == "OPTIONS") return http::verb::options;
        if (upper == "TRACE") return http::verb::trace;
        throw std::invalid_argument("Unsupported HTTP method: " + std::string(text));
    }

    std::string SerializeJson(const Json::Value& value)
    {
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        return Json::writeString(builder, value);
    }

    std::string NowString()
    {
        const auto now = std::chrono::system_clock::now();
        const auto time = std::chrono::system_clock::to_time_t(now);
#ifdef _WIN32
        std::tm tm{};
        localtime_s(&tm, &time);
#else
        std::tm tm{};
        localtime_r(&time, &tm);
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm, "%F %T");
        return oss.str();
    }

    std::string ResponseBodyToString(const http::response<http::dynamic_body>& res)
    {
        return beast::buffers_to_string(res.body().data());
    }

    class HttpTestSession
    {
    public:
        HttpTestSession(net::io_context& ioc, std::string host, unsigned short port)
            : _resolver(net::make_strand(ioc))
            , _stream(net::make_strand(ioc))
            , _host(std::move(host))
            , _portString(std::to_string(port))
        {
        }

        void SetHttpVersion(int version) noexcept
        {
            _httpVersion = version;
        }

        void Connect()
        {
            if (_connected)
            {
                return;
            }

            beast::error_code ec;
            auto results = _resolver.resolve(_host, _portString, ec);
            if (ec)
            {
                throw beast::system_error(ec, "resolve");
            }

            _stream.connect(results, ec);
            if (ec)
            {
                throw beast::system_error(ec, "connect");
            }

            _connected = true;
        }

        bool IsConnected() const noexcept
        {
            return _connected;
        }

        void Close() noexcept
        {
            if (!_connected)
            {
                return;
            }

            beast::error_code ec;
            _stream.socket().shutdown(tcp::socket::shutdown_both, ec);
            _stream.close();
            _connected = false;
        }

        http::response<http::dynamic_body> Execute(const RequestDefinition& requestDef)
        {
            if (!_connected)
            {
                Connect();
            }

            http::request<http::string_body> request{ requestDef.method, requestDef.target, static_cast<unsigned>(_httpVersion) };
            request.set(http::field::host, _host);
            request.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
            request.set(http::field::accept, "application/json");
            request.keep_alive(true);

            for (const auto& header : requestDef.headers)
            {
                request.set(header.first, header.second);
            }

            std::string body;
            if (requestDef.body.has_value())
            {
                body = SerializeJson(requestDef.body.value());
                request.set(http::field::content_type, "application/json");
                request.body() = body;
            }
            request.prepare_payload();

            _stream.expires_after(std::chrono::seconds(30));

            beast::error_code ec;
            http::write(_stream, request, ec);
            if (ec)
            {
                throw beast::system_error(ec, "write");
            }

            http::response<http::dynamic_body> response;
            http::read(_stream, _buffer, response, ec);
            if (ec)
            {
                throw beast::system_error(ec, "read");
            }

            _buffer.consume(_buffer.size());
            return response;
        }

    private:
        tcp::resolver _resolver;
        beast::tcp_stream _stream;
        beast::flat_buffer _buffer;
        std::string _host;
        std::string _portString;
        int _httpVersion{ 11 };
        bool _connected{ false };
    };

    class ScenarioRunner
    {
    public:
        ScenarioRunner(HttpTestSession& session, const CmdOptions& options)
            : _session(session)
            , _options(options)
        {
        }

        bool Run(const Scenario& scenario)
        {
            bool scenarioOk = true;

            if (!_options.quiet)
            {
                std::cout << "\n[" << NowString() << "] === Scenario: " << scenario.name << " ===\n";
                if (!scenario.description.empty())
                {
                    std::cout << scenario.description << '\n';
                }
            }

            try
            {
                _session.Connect();
            }
            catch (const std::exception& ex)
            {
                std::cerr << "  Failed to connect: " << ex.what() << '\n';
                return false;
            }

            if (scenario.requiresLogin)
            {
                scenarioOk &= RunRequest("Login", scenario.login);
            }

            int index = 1;
            for (const auto& request : scenario.requests)
            {
                const auto label = "Step " + std::to_string(index++);
                if (!request.enabled)
                {
                    if (!_options.quiet)
                    {
                        std::cout << "  " << label << " (" << (request.description.empty() ? request.target : request.description)
                            << ") skipped (disabled in config)\n";
                    }
                    continue;
                }
                scenarioOk &= RunRequest(label, request);
            }

            _session.Close();
            return scenarioOk;
        }

    private:
        bool RunRequest(const std::string& label, const RequestDefinition& request)
        {
            try
            {
                const auto response = _session.Execute(request);
                const auto status = response.result_int();
                const bool ok = status < 400;

                if (!_options.quiet)
                {
                    std::cout << "  " << label << ": " << (request.description.empty() ? request.target : request.description) << '\n';
                    std::cout << "    -> " << status << ' ' << response.reason() << '\n';
                    if (_options.printHeaders)
                    {
                        for (const auto& field : response.base())
                        {
                            std::cout << "       " << field.name_string() << ": " << field.value() << '\n';
                        }
                    }

                    const auto body = ResponseBodyToString(response);
                    if (!body.empty())
                    {
                        std::cout << "       Body: " << body << '\n';
                    }
                }

                if (request.expectedStatus.has_value() && status != request.expectedStatus.value())
                {
                    std::cerr << "    Expected status " << request.expectedStatus.value() << " but received " << status << '\n';
                    return false;
                }

                return ok;
            }
            catch (const std::exception& ex)
            {
                std::cerr << "  " << label << " failed: " << ex.what() << '\n';
                return false;
            }
        }

    private:
        HttpTestSession& _session;
        const CmdOptions& _options;
    };

    Json::Value LoadJsonFromFile(const std::string& path)
    {
        std::ifstream input(path);
        if (!input)
        {
            throw std::runtime_error("Unable to open config file: " + path);
        }

        Json::Value root;
        Json::CharReaderBuilder builder;
        std::string errs;
        if (!Json::parseFromStream(builder, input, &root, &errs))
        {
            throw std::runtime_error("Failed to parse config: " + errs);
        }

        return root;
    }

    RequestDefinition ParseRequestNode(const Json::Value& node, http::verb defaultVerb, const std::string& defaultTarget)
    {
        RequestDefinition def;
        def.method = node.isMember("method") ? ParseVerb(node["method"].asString()) : defaultVerb;
        def.target = node.isMember("target") ? node["target"].asString() : defaultTarget;
        def.description = node.isMember("description") ? node["description"].asString() : std::string{};
        def.enabled = node.isMember("enabled") ? node["enabled"].asBool() : true;

        if (node.isMember("expected_status"))
        {
            def.expectedStatus = static_cast<unsigned>(node["expected_status"].asUInt());
        }

        if (node.isMember("headers"))
        {
            const auto& headers = node["headers"];
            for (const auto& key : headers.getMemberNames())
            {
                def.headers.emplace(key, headers[key].asString());
            }
        }

        if (node.isMember("body"))
        {
            def.body = node["body"];
        }

        return def;
    }

    Scenario ParseScenario(const Json::Value& node, const RequestDefinition& defaultLogin)
    {
        if (!node.isMember("name"))
        {
            throw std::runtime_error("Scenario missing name");
        }

        Scenario scenario;
        scenario.name = node["name"].asString();
        scenario.description = node.isMember("description") ? node["description"].asString() : std::string{};
        scenario.requiresLogin = node.isMember("requires_login") ? node["requires_login"].asBool() : true;
        scenario.login = node.isMember("login") ? ParseRequestNode(node["login"], defaultLogin.method, defaultLogin.target) : defaultLogin;

        if (node.isMember("requests"))
        {
            const auto& requestsNode = node["requests"];
            if (!requestsNode.isArray())
            {
                throw std::runtime_error("Scenario requests must be an array");
            }

            for (const auto& reqNode : requestsNode)
            {
                scenario.requests.emplace_back(ParseRequestNode(reqNode, http::verb::get, "/"));
            }
        }

        return scenario;
    }

    Json::Value BuildDefaultConfig()
    {
        Json::Value root;
        root["http_version"] = 11;

        // Student scenario
        Json::Value student;
        student["name"] = "student-basic";
        student["description"] = "Login as a student and exercise core read APIs.";
        student["requires_login"] = true;

        Json::Value studentLogin;
        studentLogin["method"] = "POST";
        studentLogin["target"] = "/api/login/post";
        studentLogin["description"] = "Authenticate as student2 (adjust if needed)";
        Json::Value studentLoginBody;
        studentLoginBody["user_id"] = 2;
        studentLoginBody["password"] = "student2";
        studentLoginBody["role"] = "student";
        studentLogin["body"] = studentLoginBody;
        student["login"] = studentLogin;

        Json::Value studentRequests(Json::arrayValue);

        Json::Value studentReq1;
        studentReq1["method"] = "GET";
        studentReq1["target"] = "/api/student_infoCheck";
        studentReq1["description"] = "Fetch student profile";
        studentRequests.append(studentReq1);

        Json::Value studentReq2;
        studentReq2["method"] = "GET";
        studentReq2["target"] = "/api/student_select/get_all";
        studentReq2["description"] = "Browse available course sections";
        studentRequests.append(studentReq2);

        Json::Value studentReq3;
        studentReq3["method"] = "POST";
        studentReq3["target"] = "/api/student_select/submit";
        studentReq3["description"] = "Submit course enrollment (update section_id)";
        studentReq3["enabled"] = false;
        Json::Value studentReq3Body;
        studentReq3Body["section_id"] = "1001";
        studentReq3["body"] = studentReq3Body;
        studentRequests.append(studentReq3);

        student["requests"] = studentRequests;
        root["scenarios"].append(student);

        //// Instructor scenario
        //Json::Value instructor;
        //instructor["name"] = "instructor-basic";
        //instructor["description"] = "Instructor login and information retrieval.";
        //instructor["requires_login"] = true;

        //Json::Value instructorLogin;
        //instructorLogin["method"] = "POST";
        //instructorLogin["target"] = "/api/login/post";
        //instructorLogin["description"] = "Authenticate as teacher2 (adjust IDs/password)";
        //Json::Value instructorLoginBody;
        //instructorLoginBody["user_id"] = 1;
        //instructorLoginBody["password"] = "teacher1";
        //instructorLoginBody["role"] = "teacher";
        //instructorLogin["body"] = instructorLoginBody;
        //instructor["login"] = instructorLogin;

        //Json::Value instructorRequests(Json::arrayValue);

        //Json::Value instructorReq1;
        //instructorReq1["method"] = "GET";
        //instructorReq1["target"] = "/api/teacher_infoCheck/ask";
        //instructorReq1["description"] = "Fetch instructor profile";
        //instructorRequests.append(instructorReq1);

        //Json::Value instructorReq2;
        //instructorReq2["method"] = "GET";
        //instructorReq2["target"] = "/api/teacher_scoreIn/ask?section_id=1001";
        //instructorReq2["description"] = "Fetch section roster (adjust section_id)";
        //instructorReq2["enabled"] = false;
        //instructorRequests.append(instructorReq2);

        //instructor["requests"] = instructorRequests;
        //root["scenarios"].append(instructor);

        //// Admin scenario
        //Json::Value admin;
        //admin["name"] = "admin-basic";
        //admin["description"] = "Administrator login and roster queries.";
        //admin["requires_login"] = true;

        //Json::Value adminLogin;
        //adminLogin["method"] = "POST";
        //adminLogin["target"] = "/api/login/post";
        //adminLogin["description"] = "Authenticate as admin1 (adjust as needed)";
        //Json::Value adminLoginBody;
        //adminLoginBody["user_id"] = 1;
        //adminLoginBody["password"] = "admin1";
        //adminLoginBody["role"] = "admin";
        //adminLogin["body"] = adminLoginBody;
        //admin["login"] = adminLogin;

        //Json::Value adminRequests(Json::arrayValue);

        //Json::Value adminReq1;
        //adminReq1["method"] = "GET";
        //adminReq1["target"] = "/api/admin_accountManage/getInfo?role=teacher";
        //adminReq1["description"] = "Retrieve teacher roster";
        //adminRequests.append(adminReq1);

        //Json::Value adminReq2;
        //adminReq2["method"] = "GET";
        //adminReq2["target"] = "/api/admin_courseManage/getAllCourse?semester=2024%E7%A7%8B";
        //adminReq2["description"] = "List courses for semester (adjust encoded semester)";
        //adminReq2["enabled"] = false;
        //adminRequests.append(adminReq2);

        //admin["requests"] = adminRequests;
        //root["scenarios"].append(admin);

        return root;
    }

    std::vector<Scenario> BuildScenarios(const Json::Value& root, const RequestDefinition& defaultLogin)
    {
        std::vector<Scenario> scenarios;

        if (!root.isMember("scenarios") || !root["scenarios"].isArray())
        {
            throw std::runtime_error("Config is missing 'scenarios' array");
        }

        for (const auto& node : root["scenarios"])
        {
            scenarios.emplace_back(ParseScenario(node, defaultLogin));
        }

        return scenarios;
    }

    std::vector<Scenario> SelectScenarios(const std::vector<Scenario>& all, const std::vector<std::string>& names)
    {
        if (names.empty())
        {
            return all;
        }

        std::vector<Scenario> selected;
        std::set<std::string> missing;
        for (const auto& name : names)
        {
            const auto it = std::find_if(all.begin(), all.end(), [&](const Scenario& scenario)
                {
                    return scenario.name == name;
                });
            if (it == all.end())
            {
                missing.insert(name);
            }
            else
            {
                selected.push_back(*it);
            }
        }

        if (!missing.empty())
        {
            std::ostringstream oss;
            oss << "Unknown scenario(s): ";
            bool first = true;
            for (const auto& name : missing)
            {
                if (!first) oss << ", ";
                oss << name;
                first = false;
            }
            throw std::runtime_error(oss.str());
        }

        return selected;
    }

    void PrintScenarioList(const std::vector<Scenario>& scenarios)
    {
        std::cout << "Available scenarios:\n";
        for (const auto& scenario : scenarios)
        {
            std::cout << "  - " << scenario.name;
            if (!scenario.description.empty())
            {
                std::cout << ": " << scenario.description;
            }
            std::cout << '\n';
        }
    }

    CmdOptions ParseArgs(int argc, char* argv[])
    {
        CmdOptions options;

        for (int i = 1; i < argc; ++i)
        {
            std::string arg(argv[i]);
            if (arg == "--host" && i + 1 < argc)
            {
                options.host = argv[++i];
            }
            else if (arg == "--port" && i + 1 < argc)
            {
                const int port = std::stoi(argv[++i]);
                if (port < 0 || port > 65535)
                {
                    throw std::runtime_error("Port value out of range");
                }
                options.port = static_cast<unsigned short>(port);
            }
            else if (arg == "--config" && i + 1 < argc)
            {
                options.configPath = argv[++i];
            }
            else if ((arg == "--scenario" || arg == "-s") && i + 1 < argc)
            {
                options.scenarios.emplace_back(argv[++i]);
            }
            else if (arg == "--repeat" && i + 1 < argc)
            {
                const int repeat = std::max(1, std::stoi(argv[++i]));
                options.repeat = repeat;
            }
            else if (arg == "--list")
            {
                options.listOnly = true;
            }
            else if (arg == "--headers")
            {
                options.printHeaders = true;
            }
            else if (arg == "--quiet")
            {
                options.quiet = true;
            }
            else if (arg == "--help" || arg == "-h")
            {
                options.showHelp = true;
            }
            else
            {
                throw std::runtime_error("Unknown argument: " + arg);
            }
        }

        return options;
    }

    void PrintUsage(const char* program)
    {
        std::cout << "Usage: " << program << " [options]\n\n"
            << "Options:\n"
            << "  --host <address>       Target host (default 127.0.0.1)\n"
            << "  --port <port>          Target port (default 10086)\n"
            << "  --config <file>        JSON config with scenarios\n"
            << "  --scenario <name>      Run only the named scenario (repeatable)\n"
            << "  --repeat <count>       Repeat the selected scenarios N times\n"
            << "  --headers              Print response headers\n"
            << "  --quiet                Suppress per-request output\n"
            << "  --list                 List available scenarios and exit\n"
            << "  --help                 Show this message\n";
    }

} // namespace

int main(int argc, char* argv[])
{
    try
    {
        const auto options = ParseArgs(argc, argv);
        if (options.showHelp)
        {
            PrintUsage(argv[0]);
            return EXIT_SUCCESS;
        }

        Json::Value configRoot = options.configPath.has_value()
            ? LoadJsonFromFile(options.configPath.value())
            : BuildDefaultConfig();

        const int httpVersion = configRoot.isMember("http_version")
            ? configRoot["http_version"].asInt()
            : 11;

        RequestDefinition defaultLogin;
        defaultLogin.method = http::verb::post;
        defaultLogin.target = "/api/login/post";
        defaultLogin.description = "Default login";

        const auto scenarios = BuildScenarios(configRoot, defaultLogin);

        if (scenarios.empty())
        {
            std::cerr << "No scenarios defined. Check TestClient configuration.\n";
            return EXIT_FAILURE;
        }

        if (options.listOnly)
        {
            PrintScenarioList(scenarios);
            return EXIT_SUCCESS;
        }

        const auto selectedScenarios = SelectScenarios(scenarios, options.scenarios);

        bool allOk = true;
        for (int i = 0; i < options.repeat; ++i)
        {
            for (const auto& scenario : selectedScenarios)
            {
                net::io_context ioc;
                HttpTestSession session(ioc, options.host, options.port);
                session.SetHttpVersion(httpVersion);
                ScenarioRunner runner(session, options);
                const bool ok = runner.Run(scenario);
                allOk = allOk && ok;
            }
        }

        return allOk ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Test client error: " << ex.what() << '\n';
        return EXIT_FAILURE;
    }
}
