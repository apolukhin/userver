#pragma once

/// @file userver/easy.hpp
/// @brief Headers of an library for `easy` prototyping

#include <functional>
#include <string>
#include <string_view>

#include <userver/components/component_base.hpp>
#include <userver/components/component_list.hpp>
#include <userver/formats/json.hpp>
#include <userver/http/content_type.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/server/request/request_context.hpp>

#include <userver/storages/postgres/cluster.hpp>

USERVER_NAMESPACE_BEGIN

/// @brief Top namespace for `easy` library
namespace easy {

/// @brief Base class for all the dependencies of the `easy` library.
class DependenciesBase : public components::ComponentBase {
public:
    static constexpr std::string_view kName = "easy-dependencies";
    using components::ComponentBase::ComponentBase;
    virtual ~DependenciesBase();

    /// @returns the \b last schema that was provided to the easy::HttpWith or easy::HttpBase
    static const std::string& GetSchema() noexcept;
};

/// @brief Tag for custom Dependency registration customization.
///
/// For a new dependency type `Dependency` a function `void Registration(OfDependency<Dependency>, HttpBase& app)`
/// should be provided to be able to use that dependency with the easy::HttpWith class.
template <class Dependency>
struct OfDependency final {};

/// @brief easy::HttpWith like class with erased dependencies information that should be used only in dependency
/// registration functions; use easy::HttpWith if not making a new dependency class.
class HttpBase final {
public:
    using Callback = std::function<std::string(const server::http::HttpRequest&, const DependenciesBase&)>;

    /// Sets the default Content-Type header for all the routes
    void DefaultContentType(http::ContentType content_type);

    /// Register an HTTP handler by `path` that supports the `methods` HTTP methods
    void Route(std::string_view path, Callback&& func, std::initializer_list<server::http::HttpMethod> methods);

    /// Add a comonent config to the service config
    void AddComponentsConfig(std::string_view config);

    /// Append a component to the component list of a service
    template <class Component>
    void AppendComponent(std::string_view name) {
        component_list_.Append<Component>(name);
    }

    /// @overload
    template <class Component>
    void AppendComponent() {
        component_list_.Append<Component>();
    }

    /// Stores the schema for further retrieval from DependenciesBase::GetSchema()
    void Schema(std::string_view schema);

    /// Set the HTTP server listen port, default is 8080.
    void Port(std::uint16_t port);

    /// Set the logging level for the service
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

/// @brief Class for describing the service functionality in simple declarative way that generates static configs,
/// applies schemas.
///
/// @see @ref scripts/docs/en/userver/libraries/easy.md
template <class Dependency = DependenciesBase>
class HttpWith final {
public:
    /// Helper class that can store any callback of the following signatures:
    ///
    /// * formats::json::Value(formats::json::Value, const Dependency&)
    /// * formats::json::Value(formats::json::Value)
    /// * formats::json::Value(const HttpRequest&, const Dependency&)
    /// * std::string(const HttpRequest&, const Dependency&)
    /// * formats::json::Value(const HttpRequest&)
    /// * std::string(const HttpRequest&)
    ///
    /// If callback returns formats::json::Value then the deafult content type is set to `application/json`
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

    /// @copydoc HttpBase::DefaultContentType
    HttpWith& DefaultContentType(http::ContentType content_type) {
        return (impl_.DefaultContentType(content_type), *this);
    }

    /// @copydoc HttpBase::Route
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

    /// Register an HTTP handler by `path` that supports the HTTP GET method.
    HttpWith& Get(std::string_view path, Callback&& func) {
        impl_.Route(path, std::move(func).Extract(), {server::http::HttpMethod::kGet});
        return *this;
    }

    /// Register an HTTP handler by `path` that supports the HTTP POST method.
    HttpWith& Post(std::string_view path, Callback&& func) {
        impl_.Route(path, std::move(func).Extract(), {server::http::HttpMethod::kPost});
        return *this;
    }

    /// Register an HTTP handler by `path` that supports the HTTP DELETE method.
    HttpWith& Del(std::string_view path, Callback&& func) {
        impl_.Route(path, std::move(func).Extract(), {server::http::HttpMethod::kDelete});
        return *this;
    }

    /// Register an HTTP handler by `path` that supports the HTTP PUT method.
    HttpWith& Put(std::string_view path, Callback&& func) {
        impl_.Route(path, std::move(func).Extract(), {server::http::HttpMethod::kPut});
        return *this;
    }

    /// Register an HTTP handler by `path` that supports the HTTP PATCH method.
    HttpWith& Patch(std::string_view path, Callback&& func) {
        impl_.Route(path, std::move(func).Extract(), {server::http::HttpMethod::kPatch});
        return *this;
    }

    /// @copydoc HttpBase::AddComponentsConfig
    HttpWith& AddComponentsConfig(std::string_view config) {
        impl_.AddComponentsConfig(config);
        return *this;
    }

    /// @copydoc HttpBase::AppendComponent
    template <class Component>
    HttpWith& AppendComponent(std::string_view name) {
        impl_.AppendComponent<Component>(name);
        return *this;
    }

    /// @copydoc HttpBase::AppendComponent
    template <class Component>
    HttpWith& AppendComponent() {
        impl_.AppendComponent<Component>();
        return *this;
    }

    /// @copydoc HttpBase::Schema
    HttpWith& Schema(std::string_view schema) {
        impl_.Schema(schema);
        return *this;
    }

    /// @copydoc HttpBase::Port
    HttpWith& Port(std::uint16_t port) {
        impl_.Port(port);
        return *this;
    }

    /// @copydoc HttpBase::LogLevel
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

    if constexpr (std::is_invocable_r_v<formats::json::Value, Function, formats::json::Value, const Dependency&>) {
        func_ = [f = std::move(func)](const HttpRequest& req, const DependenciesBase& deps) {
            req.GetHttpResponse().SetContentType(http::content_type::kApplicationJson);
            return formats::json::ToString(
                f(formats::json::FromString(req.RequestBody()), static_cast<const Dependency&>(deps))
            );
        };
    } else if constexpr (std::is_invocable_r_v<formats::json::Value, Function, formats::json::Value>) {
        func_ = [f = std::move(func)](const HttpRequest& req, const DependenciesBase&) {
            req.GetHttpResponse().SetContentType(http::content_type::kApplicationJson);
            return formats::json::ToString(f(formats::json::FromString(req.RequestBody())));
        };
    } else if constexpr (std::is_invocable_r_v<formats::json::Value, Function, const HttpRequest&, const Dependency&>) {
        func_ = [f = std::move(func)](const HttpRequest& req, const DependenciesBase& deps) {
            req.GetHttpResponse().SetContentType(http::content_type::kApplicationJson);
            return formats::json::ToString(f(req, static_cast<const Dependency&>(deps)));
        };
    } else if constexpr (std::is_invocable_r_v<std::string, Function, const HttpRequest&, const Dependency&>) {
        func_ = [f = std::move(func)](const HttpRequest& req, const DependenciesBase& deps) {
            return f(req, static_cast<const Dependency&>(deps));
        };
    } else if constexpr (std::is_invocable_r_v<formats::json::Value, Function, const HttpRequest&>) {
        func_ = [f = std::move(func)](const HttpRequest& req, const DependenciesBase&) {
            req.GetHttpResponse().SetContentType(http::content_type::kApplicationJson);
            return formats::json::ToString(f(req));
        };
    } else {
        static_assert(std::is_invocable_r_v<std::string, Function, const HttpRequest&>);
        func_ = [f = std::move(func)](const HttpRequest& req, const DependenciesBase&) { return f(req); };
    }
}

/// @brief Predefined dependencies class that provides a PostgreSQL cluster client.
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
