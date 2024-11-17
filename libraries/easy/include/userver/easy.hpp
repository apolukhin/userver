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

namespace impl {

using UnderlyingCallback = std::function<std::string(const HttpRequest&, const DependenciesBase&)>;

struct AggressiveUnicorn; // Do not unleash

template <class Dependency>
using ConstDependencyRef = std::conditional_t<std::is_void_v<Dependency>, AggressiveUnicorn, Dependency> const&; 

class HttpBase {
public:
    HttpBase(int argc, const char *const argv[]);
    ~HttpBase();
    
    void DefaultContentType(http::ContentType content_type);
    void Route(std::string_view path, UnderlyingCallback&& func);

    void AddComponentsConfig(std::string_view config);

    template <class Component>
    void AppendComponent(std::string_view name) {
        component_list_.Append<Component>(name);
    }
    template <class Component>
    void AppendComponent() {
        component_list_.Append<Component>();
    }
    
    void Schema(std::string_view schema) { schema_ = schema; }
private:
    void AddHandleConfig(std::string_view path);

    class Handle;

    const int argc_;
    const char *const* argv_;
    std::string static_config_;
    std::string schema_;
    components::ComponentList component_list_;
};

}

template <class Dependency = void>
class HttpWith;

inline void DependenciesRegistration(HttpWith<void>&) {}

template <class T>
inline void DependenciesRegistration(HttpWith<T>&) {
    static_assert(sizeof(T) && false, "Define `void DependenciesRegistration(HttpWith<T>& app)` "
        "in the namespace of the your type `T` to automatically add required configurations and "
        "components to the `app` for your dependences type `T`.  For example:"
        "void DependenciesRegistration(HttpWith<MyDeps>& app) { app.AppendComponent<MyComponent>(); }"
    );
}

template <class Dependency>
class HttpWith final {
public:
    class Callback {
    public:
        template <class Function>
        Callback(Function func);

        auto Extract() && noexcept { return std::move(func_); }

    private:
        impl::UnderlyingCallback func_;
    };

    HttpWith(int argc, const char *const argv[]) : impl_(argc, argv) {}

    ~HttpWith() {
        DependenciesRegistration(*this);  // ADL is intentional
    }
    
    HttpWith& DefaultContentType(http::ContentType content_type) {
        impl_.DefaultContentType(content_type);
        return *this;
    }

    HttpWith& Route(std::string_view path, Callback&& func) {
        impl_.Route(path, std::move(func).Extract());
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

private:
    impl::HttpBase impl_;
};

template <class Dependency>
template <class Function>
HttpWith<Dependency>::Callback::Callback(Function func) {
    if constexpr (std::is_invocable_r_v<std::string, Function, const HttpRequest&, const DependenciesBase&>) {
        func_ = std::move(func);
    } else if constexpr (std::is_invocable_r_v<std::string, Function, const HttpRequest&, impl::ConstDependencyRef<Dependency> >) {
        func_ = [f = std::move(func)](const HttpRequest& req, const DependenciesBase& deps) {
            return f(req, static_cast<const Dependency&>(deps));
        };
    } else {
      static_assert(std::is_invocable_r_v<std::string, Function, const HttpRequest&>);
      func_ = [f = std::move(func)](const HttpRequest& req, const DependenciesBase&) {
        return f(req);
      };
    }
}

class PgDep : public DependenciesBase {
public:
    PgDep(const components::ComponentConfig& config, const components::ComponentContext& context);

    storages::postgres::Cluster& pg() const noexcept { return *pg_cluster_; }

private:
    storages::postgres::ClusterPtr pg_cluster_;
};

void DependenciesRegistration(HttpWith<PgDep>& app);


}  // namespace easy

template <>
inline constexpr auto components::kConfigFileMode<easy::DependenciesBase> = ConfigFileMode::kNotRequired;

template <>
inline constexpr auto components::kConfigFileMode<easy::PgDep> = ConfigFileMode::kNotRequired;

USERVER_NAMESPACE_END
