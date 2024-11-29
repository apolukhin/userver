#include <userver/easy.hpp>

#include <fstream>
#include <iostream>
#include <unordered_map>

#include <boost/algorithm/string/replace.hpp>
#include <boost/program_options.hpp>

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_context.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/components/run.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/testsuite/testsuite_support.hpp>
#include <userver/utils/daemon_run.hpp>

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
)~";

constexpr std::string_view kConfigServerTemplate = R"~(
server:
    listener:                 # configuring the main listening socket...
        port: {}            # ...to listen on this port and...
        task_processor: main-task-processor    # ...process incoming requests on this task processor.
)~";

constexpr std::string_view kConfigLoggingTemplate = R"~(
logging:
    fs-task-processor: fs-task-processor
    loggers:
        default:
            file_path: '@stderr'
            level: {}
            overflow_behavior: discard  # Drop logs if the system is too busy to write them down.
)~";

constexpr std::string_view kConfigHandlerTemplate = R"~(
{0}:
    path: {1}                  # Registering handler by URL '{1}'.
    method: {2}
    task_processor: main-task-processor  # Run it on CPU bound task processor
)~";

struct SharedPyaload {
    std::unordered_map<std::string, HttpBase::Callback> http_functions;
    std::optional<http::ContentType> default_content_type;
    std::string schema;
};

SharedPyaload globals{};

}  // anonymous namespace

DependenciesBase::~DependenciesBase() = default;

const std::string& DependenciesBase::GetSchema() noexcept { return globals.schema; }

class HttpBase::Handle final : public server::handlers::HttpHandlerBase {
public:
    Handle(const components::ComponentConfig& config, const components::ComponentContext& context)
        : HttpHandlerBase(config, context),
          deps_{context.FindComponent<DependenciesBase>()},
          callback_{globals.http_functions.at(config.Name())} {}

    std::string HandleRequestThrow(const server::http::HttpRequest& request, server::request::RequestContext&)
        const override {
        if (globals.default_content_type) {
            request.GetHttpResponse().SetContentType(*globals.default_content_type);
        }
        return callback_(request, deps_);
    }

private:
    const DependenciesBase& deps_;
    HttpBase::Callback& callback_;
};

HttpBase::HttpBase(int argc, const char* const argv[])
    : argc_{argc},
      argv_{argv},
      static_config_{kConfigBase},
      component_list_{components::MinimalServerComponentList()} {}

HttpBase::~HttpBase() {
    AddComponentsConfig(fmt::format(kConfigServerTemplate, port_));
    AddComponentsConfig(fmt::format(kConfigLoggingTemplate, ToString(level_)));

    namespace po = boost::program_options;
    po::variables_map vm;
    auto desc = utils::BaseRunOptions();
    std::string config_dump;
    std::string schema_dump;

    // clang-format off
    desc.add_options()
      ("dump-config", po::value(&config_dump), "path to dump the server config")
      ("dump-schema", po::value(&schema_dump), "path to dump the DB schema")
      ("config,c", po::value<std::string>(), "path to server config")
    ;
    // clang-format on

    po::store(po::parse_command_line(argc_, argv_, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cerr << desc << '\n';
        return;
    }

    if (vm.count("dump-config")) {
        std::ofstream(config_dump) << static_config_;
        return;
    }

    if (vm.count("dump-schema")) {
        std::ofstream(schema_dump + "/0_pg.sql") << globals.schema;
    }

    if (argc_ <= 1) {
        components::Run(components::InMemoryConfig{static_config_}, component_list_);
    } else {
        const auto ret = utils::DaemonMain(vm, component_list_);
        if (ret != 0) {
            std::exit(ret);
        }
    }
}

void HttpBase::DefaultContentType(http::ContentType content_type) { globals.default_content_type = content_type; }

void HttpBase::Route(std::string_view path, Callback&& func, std::initializer_list<server::http::HttpMethod> methods) {
    auto component_name = fmt::format("{}-{}", path, fmt::join(methods, ","));

    globals.http_functions.emplace(component_name, std::move(func));
    component_list_.Append<Handle>(component_name);
    AddComponentsConfig(fmt::format(kConfigHandlerTemplate, component_name, path, fmt::join(methods, ",")));
}

void HttpBase::AddComponentsConfig(std::string_view config) {
    auto conf = fmt::format("\n{}\n", config);
    static_config_ += boost::algorithm::replace_all_copy(conf, "\n", "\n        ");
}

void HttpBase::Schema(std::string_view schema) { globals.schema = schema; }

void HttpBase::Port(std::uint16_t port) { port_ = port; }

void HttpBase::LogLevel(logging::Level level) { level_ = level; }

PgDep::PgDep(const components::ComponentConfig& config, const components::ComponentContext& context)
    : DependenciesBase{config, context},
      pg_cluster_(context.FindComponent<components::Postgres>("postgres").GetCluster()) {
    pg_cluster_->Execute(storages::postgres::ClusterHostType::kMaster, GetSchema());
}

void Registration(OfDependency<PgDep>, HttpBase& app) {
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
