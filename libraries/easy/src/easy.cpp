#include <userver/easy.hpp>

#include <fstream>

#include <boost/program_options.hpp>
#include <boost/algorithm/string/replace.hpp>

#include <userver/components/minimal_server_component_list.hpp>
#include <userver/components/run.hpp>
#include <userver/components/component_context.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/testsuite/testsuite_support.hpp>
#include <userver/clients/dns/component.hpp>
#include <userver/utils/daemon_run.hpp>
#include <userver/storages/postgres/component.hpp>

USERVER_NAMESPACE_BEGIN

namespace easy {

namespace {

constexpr std::string_view kConfigBase = R"~(
components_manager:
    task_processors:                  # Task processor is an executor for coroutine tasks
        main-task-processor:          # Make a task processor for CPU-bound coroutine tasks.
            worker_threads: 4         # Process tasks in 4 threads.

        fs-task-processor:            # Make a separate task processor for filesystem bound tasks.
            worker_threads: 1

    default_task_processor: main-task-processor  # Task processor in which components start.

    components:                       # Configuring components that were registered via component_list
        server:
            listener:                 # configuring the main listening socket...
                port: 8080            # ...to listen on this port and...
                task_processor: main-task-processor    # ...process incoming requests on this task processor.
        logging:
            fs-task-processor: fs-task-processor
            loggers:
                default:
                    file_path: '@stderr'
                    level: debug
                    overflow_behavior: discard  # Drop logs if the system is too busy to write them down.
)~";


constexpr std::string_view kConfigHandlerTemplate = R"~(
{0}:
    path: {0}                  # Registering handler by URL '{0}'.
    method: GET,PUT,POST,DELETE,PATCH
    task_processor: main-task-processor  # Run it on CPU bound task processor
)~";

class EmptyDeps final : public DependenciesBase {
    using DependenciesBase::DependenciesBase;
};

const DependenciesBase& GetDeps(const components::ComponentConfig& config, const components::ComponentContext& context) {
    const auto* deps = context.
    FindComponentOptional<DependenciesBase>();
    if (deps) {
        return *deps;
    }

    static const EmptyDeps empty{config, context};
    return empty;
}

std::unordered_map<std::string, impl::UnderlyingCallback> g_http_functions_;
std::optional<http::ContentType> default_content_type_;
std::string schema_;

}  // anonymous namespace


DependenciesBase::~DependenciesBase() = default;

namespace impl {

class HttpBase::Handle final : public server::handlers::HttpHandlerBase {
public:
    Handle(const components::ComponentConfig& config, const components::ComponentContext& context):
      HttpHandlerBase(config, context),
      deps_{GetDeps(config, context)},
      callback_{g_http_functions_[config.Name()]}
    {}

    std::string HandleRequestThrow(const HttpRequest& request, RequestContext&) const override {
        if (default_content_type_) {
            request.GetHttpResponse().SetContentType(*default_content_type_);
        }
        return callback_(request, deps_);
    }

private:
    const DependenciesBase& deps_;
    impl::UnderlyingCallback& callback_;
};

HttpBase::HttpBase(int argc, const char *const argv[]) : argc_{argc}, argv_{argv}, static_config_{kConfigBase},
      component_list_{components::MinimalServerComponentList()} {}

HttpBase::~HttpBase() {
    namespace po = boost::program_options;

    po::variables_map vm;
    po::options_description desc("Easy options");
    std::string config_dump;
    std::string schema_dump;

    // clang-format off
    desc.add_options()
      ("dump-config", po::value(&config_dump), "path to dump the server config")
      ("dump-schema", po::value(&schema_dump), "path to dump the DB schema")
    ;
    // clang-format on

    po::store(po::command_line_parser(argc_, argv_).options(desc).allow_unregistered().run(), vm);
    po::notify(vm);

    if (vm.count("dump-config")) {
        std::ofstream(config_dump) << static_config_;
        return;
    }

    if (vm.count("dump-schema")) {
        std::ofstream(schema_dump + "/0_pg.sql") << schema_;
    }

    if (argc_ <= 1) {
        components::Run(components::InMemoryConfig{static_config_}, component_list_);
        return;
    } else {
        const auto ret = utils::DaemonMain(argc_, argv_, component_list_);
        if (ret != 0) {
            std::exit(ret);
        }
    }
}

void HttpBase::DefaultContentType(http::ContentType content_type) {
    default_content_type_ = content_type;
}

void HttpBase::Route(std::string_view path, UnderlyingCallback&& func) {
    g_http_functions_.emplace(path, std::move(func));
    component_list_.Append<Handle>(path);
    AddHandleConfig(path);
}

void HttpBase::AddHandleConfig(std::string_view path) {
    AddComponentsConfig(fmt::format(kConfigHandlerTemplate, path));
}

void HttpBase::AddComponentsConfig(std::string_view config) {
    auto conf = fmt::format("\n{}\n", config);
    static_config_ += boost::algorithm::replace_all_copy(conf, "\n", "\n        ");
}

void HttpBase::Schema(std::string_view schema) { schema_ = schema; }
    
}  // namespace impl


PgDep::PgDep(const components::ComponentConfig& config, const components::ComponentContext& context)
: DependenciesBase{config, context}, pg_cluster_(context.FindComponent<components::Postgres>("postgres").GetCluster())
{
    pg_cluster_->Execute(storages::postgres::ClusterHostType::kMaster, schema_);
}

void DependenciesRegistration(HttpWith<PgDep>& app) {
    app.AddComponentsConfig(R"~(
postgres:
    dbconnection#env: POSTGRESQL
    dbconnection#fallback: 'postgresql://testsuite@localhost:15433/postgres'
    blocking_task_processor: fs-task-processor
    dns_resolver: async

testsuite-support:

dns-client:
    fs-task-processor: fs-task-processor
)~");

    app.AppendComponent<components::Postgres>("postgres");
    app.AppendComponent<components::TestsuiteSupport>();
    app.AppendComponent<clients::dns::Component>();
}

}  // namespace easy

USERVER_NAMESPACE_END
