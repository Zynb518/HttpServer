#pragma once
#include <memory>
#include <json/json.h>

class HttpConnection;

class RequestHandler {
public:
    virtual ~RequestHandler() = default;
    virtual void handleRequest(std::shared_ptr<HttpConnection> con, const Json::Value& request) = 0;
    virtual bool validateRequest(const Json::Value& request) = 0;
protected:
    void ParseTimeString(std::string_view str_v, Json::Value& timeArr);
    Json::StreamWriterBuilder _writer;
};