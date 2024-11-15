#include <userver/easy.hpp>

#include <fstream>

#include <boost/program_options.hpp>

#include <userver/components/minimal_server_component_list.hpp>
#include <userver/components/run.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
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


std::unordered_map<std::string, Http::Callback> g_http_functions_;

}  // anonymous namespace

class Http::Handle final : public server::handlers::HttpHandlerBase {
public:
    Handle(const components::ComponentConfig& config, const components::ComponentContext& context):
      HttpHandlerBase(config, context),
      callback_{g_http_functions_[config.Name()]}
    {}

    std::string HandleRequestThrow(const HttpRequest& request, RequestContext&) const override {
        return callback_(request);
    }
private:
    Http::Callback& callback_;
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

Http& Http::Path(std::string_view path, Callback&& func) {
    g_http_functions_.emplace(path, std::move(func));
    component_list_.Append<Handle>(path);
    AddHandleConfig(path);

    return *this;
}

void Http::AddHandleConfig(std::string_view path) {
    static_config_ += fmt::format(kConfigHandlerTemplate, path);
}

}  // namespace easy

USERVER_NAMESPACE_END
