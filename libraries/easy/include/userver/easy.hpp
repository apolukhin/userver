#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <userver/components/component_list.hpp>
#include <userver/http/content_type.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/server/request/request_context.hpp>

USERVER_NAMESPACE_BEGIN

/// @brief Top namespace for `easy` library
namespace easy {

using HttpRequest = server::http::HttpRequest;
using RequestContext = server::request::RequestContext;

class Http final {
public:
    using Callback = std::function<std::string(const HttpRequest&)>;

    Http(int argc, const char *const argv[]);
    ~Http();

    
    Http& DefaultContentType(http::ContentType content_type);

    Http& Route(std::string_view path, Callback&& func);
private:
    void AddHandleConfig(std::string_view path);

    class Handle;

    const int argc_;
    const char *const* argv_;
    std::string static_config_;
    components::ComponentList component_list_;
};

}  // namespace easy

USERVER_NAMESPACE_END
