#include <userver/easy.hpp>

#include <fstream>

#include <boost/program_options.hpp>

#include <userver/components/minimal_server_component_list.hpp>
#include <userver/components/run.hpp>
#include <userver/components/component_context.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
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
            method: GET,POST              # It will only reply to GET (HEAD) and POST requests.
            task_processor: main-task-processor  # Run it on CPU bound task processor
)~";

class EmptyDeps final : public DependenciesBase {
    using DependenciesBase::DependenciesBase;
};

const DependenciesBase& GetDeps(const components::ComponentConfig& config, const components::ComponentContext& context) {
    const auto* deps = context.FindComponentOptional<DependenciesBase>();
    if (deps) {
        return *deps;
    }

    static const EmptyDeps empty{config, context};
    return empty;
}

std::unordered_map<std::string, Callback::Underlying> g_http_functions_;
std::optional<http::ContentType> default_content_type_;

}  // anonymous namespace


DependenciesBase::~DependenciesBase() = default;

class Http::Handle final : public server::handlers::HttpHandlerBase {
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
    Callback::Underlying& callback_;
};

Http::Http(int argc, const char *const argv[]) : argc_{argc}, argv_{argv}, static_config_{kConfigBase},
      component_list_{components::MinimalServerComponentList()} {}

Http::~Http() {
    namespace po = boost::program_options;

    po::variables_map vm;
    po::options_description desc("Easy options");
    std::string config_dump;

    // clang-format off
    desc.add_options()
      ("dump-config", po::value(&config_dump), "path to dump the server config")
    ;
    // clang-format on

    po::store(po::command_line_parser(argc_, argv_).options(desc).allow_unregistered().run(), vm);
    po::notify(vm);

    if (vm.count("dump-config")) {
        std::ofstream(config_dump) << static_config_;
        return;
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

Http& Http::DefaultContentType(http::ContentType content_type) {
    default_content_type_ = content_type;
    return *this;
}

Http& Http::Route(std::string_view path, Callback&& func) {
    g_http_functions_.emplace(path, std::move(func).Extract());
    component_list_.Append<Handle>(path);
    AddHandleConfig(path);

    return *this;
}

void Http::AddHandleConfig(std::string_view path) {
    static_config_ += fmt::format(kConfigHandlerTemplate, path);
}


Postgres::Postgres(const components::ComponentConfig& config, const components::ComponentContext& context)
: DependenciesBase{config, context}, pg_cluster_(context.FindComponent<components::Postgres>("key-value-database").GetCluster()) {}

}  // namespace easy

USERVER_NAMESPACE_END
