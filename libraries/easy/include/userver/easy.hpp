#pragma once

#include <string_view>
#include <unordered_map>

#include <userver/components/component_list.hpp>

USERVER_NAMESPACE_BEGIN

/// @brief Top namespace for `easy` library
namespace easy {

using HttpRequest = server::http::HttpRequest;
using RequestContext = server::request::RequestContext;

class Http final {
public:
    using Callback = std::function<std::string(HttpRequest&);

    Http(int argc, const char *const argv[]);
    ~Http();

    Http& Path(std::string_view path, Callback&& func);

    int Run();

private:
    static std::unordered_map<std::string, Callback> functions_;

    void AddHandleConfig(std::string_view path);

    class Handle;

    const int argc_;
    const char *const argv_[];
    components::ComponentList component_list_ = MinimalServerComponentList();
};

}  // namespace easy

USERVER_NAMESPACE_END
