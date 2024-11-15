#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <userver/components/component_list.hpp>
#include <userver/components/component_base.hpp>
#include <userver/http/content_type.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/server/request/request_context.hpp>

#include <userver/storages/postgres/cluster.hpp>

USERVER_NAMESPACE_BEGIN

/// @brief Top namespace for `easy` library
namespace easy {

using HttpRequest = server::http::HttpRequest;
using RequestContext = server::request::RequestContext;

class DependenciesBase: public components::ComponentBase {
public:
    static constexpr std::string_view kName = "easy-dependencies";
    using components::ComponentBase::ComponentBase;
    virtual ~DependenciesBase();
};

class Callback {
public:
    using Underlying = std::function<std::string(const HttpRequest&, const DependenciesBase&)>;

    template <class Function>
    Callback(Function func) {
        if constexpr (std::is_invocable_r_v<std::string, Function, const HttpRequest&, const DependenciesBase&>) {
            func_ = std::move(func);
        } else {
          static_assert(std::is_invocable_r_v<std::string, Function, const HttpRequest&>);
          func_ = [f = std::move(func)](const HttpRequest& req, const DependenciesBase&) {
            return f(req);
          };
        }
    }

    Underlying Extract() && noexcept { return std::move(func_); }

private:
    std::function<std::string(const HttpRequest&, const DependenciesBase&)> func_;
};

class Http final {
public:
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

///

template <class Dependency>
class CallbackWith : public Callback {
public:
    using Callback::Callback;

    CallbackWith(std::function<std::string(const HttpRequest&, const Dependency&)> func)
        : Callback([f = std::move(func)](const HttpRequest& req, const DependenciesBase& deps) {
            return f(req, static_cast<Dependency&>(deps));
        })
    {}
};

class Postgres : public DependenciesBase {
public:
    Postgres(const components::ComponentConfig& config, const components::ComponentContext& context);

    storages::postgres::Cluster& pg() const noexcept { return *pg_cluster_; }

private:
    storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace easy

template <>
inline constexpr auto components::kConfigFileMode<easy::DependenciesBase> = ConfigFileMode::kNotRequired;

template <>
inline constexpr auto components::kConfigFileMode<easy::Postgres> = ConfigFileMode::kNotRequired;

USERVER_NAMESPACE_END
