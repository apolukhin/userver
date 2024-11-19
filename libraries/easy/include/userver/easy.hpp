#pragma once

#include <functional>
#include <string>
#include <string_view>

#include <userver/components/component_base.hpp>
#include <userver/components/component_list.hpp>
#include <userver/http/content_type.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/server/request/request_context.hpp>

#include <userver/storages/postgres/cluster.hpp>

USERVER_NAMESPACE_BEGIN

/// @brief Top namespace for `easy` library
namespace easy {

class DependenciesBase : public components::ComponentBase {
public:
    static constexpr std::string_view kName = "easy-dependencies";
    using components::ComponentBase::ComponentBase;
    virtual ~DependenciesBase();
};

template <class Dependency>
struct OfDependency final {};

class HttpBase final {
public:
    using Callback = std::function<std::string(const server::http::HttpRequest&, const DependenciesBase&)>;

    void DefaultContentType(http::ContentType content_type);
    void Route(std::string_view path, Callback&& func, std::initializer_list<server::http::HttpMethod> methods);

    void AddComponentsConfig(std::string_view config);

    template <class Component>
    void AppendComponent(std::string_view name) {
        component_list_.Append<Component>(name);
    }
    template <class Component>
    void AppendComponent() {
        component_list_.Append<Component>();
    }

    void Schema(std::string_view schema);
    void Port(std::uint16_t port);
    void LogLevel(logging::Level level);

private:
    template <class>
    friend class HttpWith;

    HttpBase(int argc, const char* const argv[]);
    ~HttpBase();

    class Handle;

    const int argc_;
    const char* const* argv_;
    std::string static_config_;
    components::ComponentList component_list_;

    std::uint16_t port_ = 8080;
    logging::Level level_ = logging::Level::kDebug;
};

template <class Dependency = DependenciesBase>
class HttpWith final {
public:
    class Callback final {
    public:
        template <class Function>
        Callback(Function func);

        HttpBase::Callback Extract() && noexcept { return std::move(func_); }

    private:
        HttpBase::Callback func_;
    };

    HttpWith(int argc, const char* const argv[]) : impl_(argc, argv) { impl_.AppendComponent<Dependency>(); }
    ~HttpWith() { Registration(OfDependency<Dependency>{}, impl_); /* ADL is intentional */ }

    HttpWith& DefaultContentType(http::ContentType content_type) {
        return (impl_.DefaultContentType(content_type), *this);
    }

    HttpWith& Route(
        std::string_view path,
        Callback&& func,
        std::initializer_list<server::http::HttpMethod> methods =
            {
                server::http::HttpMethod::kGet,
                server::http::HttpMethod::kPost,
                server::http::HttpMethod::kDelete,
                server::http::HttpMethod::kPut,
                server::http::HttpMethod::kPatch,
            }
    ) {
        impl_.Route(path, std::move(func).Extract(), methods);
        return *this;
    }

    HttpWith& Get(std::string_view path, Callback&& func) {
        impl_.Route(path, std::move(func).Extract(), {server::http::HttpMethod::kGet});
        return *this;
    }

    HttpWith& Post(std::string_view path, Callback&& func) {
        impl_.Route(path, std::move(func).Extract(), {server::http::HttpMethod::kPost});
        return *this;
    }

    HttpWith& Del(std::string_view path, Callback&& func) {
        impl_.Route(path, std::move(func).Extract(), {server::http::HttpMethod::kDelete});
        return *this;
    }

    HttpWith& Put(std::string_view path, Callback&& func) {
        impl_.Route(path, std::move(func).Extract(), {server::http::HttpMethod::kPut});
        return *this;
    }

    HttpWith& Patch(std::string_view path, Callback&& func) {
        impl_.Route(path, std::move(func).Extract(), {server::http::HttpMethod::kPatch});
        return *this;
    }

    HttpWith& AddComponentsConfig(std::string_view config) {
        impl_.AddComponentsConfig(config);
        return *this;
    }

    template <class Component>
    HttpWith& AppendComponent(std::string_view name) {
        impl_.AppendComponent<Component>(name);
        return *this;
    }

    template <class Component>
    HttpWith& AppendComponent() {
        impl_.AppendComponent<Component>();
        return *this;
    }

    HttpWith& Schema(std::string_view schema) {
        impl_.Schema(schema);
        return *this;
    }

    HttpWith& Port(std::uint16_t port) {
        impl_.Port(port);
        return *this;
    }

    HttpWith& LogLevel(logging::Level level) {
        impl_.LogLevel(level);
        return *this;
    }

private:
    HttpBase impl_;
};

template <class T, class HttpBase>
inline void Registration(T, HttpBase&&) {
    static_assert(
        sizeof(T) && false,
        "Define `void Registration(easy::OfDependency<T>, HttpBase& app)` "
        "in the namespace of the your dependency type `T` to automatically add required configurations and "
        "components to the `app`.  For example:"
        "void Registration(easy::OfDependency<MyDeps>, easy::HttpBase& app) { app.AppendComponent<MyComponent>(); }"
    );
}

inline void Registration(OfDependency<DependenciesBase>, HttpBase&) {}

template <class Dependency>
template <class Function>
HttpWith<Dependency>::Callback::Callback(Function func) {
    using server::http::HttpRequest;
    if constexpr (std::is_invocable_r_v<std::string, Function, const HttpRequest&, const Dependency&>) {
        func_ = [f = std::move(func)](const HttpRequest& req, const DependenciesBase& deps) {
            return f(req, static_cast<const Dependency&>(deps));
        };
    } else if constexpr (std::is_invocable_r_v<std::string, Function, const HttpRequest&, const DependenciesBase&>) {
        func_ = std::move(func);
    } else {
        static_assert(std::is_invocable_r_v<std::string, Function, const HttpRequest&>);
        func_ = [f = std::move(func)](const HttpRequest& req, const DependenciesBase&) { return f(req); };
    }
}

class PgDep : public DependenciesBase {
public:
    PgDep(const components::ComponentConfig& config, const components::ComponentContext& context);

    storages::postgres::Cluster& pg() const noexcept { return *pg_cluster_; }

private:
    storages::postgres::ClusterPtr pg_cluster_;
};

void Registration(OfDependency<PgDep>, HttpBase& app);

}  // namespace easy

template <>
inline constexpr auto components::kConfigFileMode<easy::DependenciesBase> = ConfigFileMode::kNotRequired;

template <>
inline constexpr auto components::kConfigFileMode<easy::PgDep> = ConfigFileMode::kNotRequired;

USERVER_NAMESPACE_END
