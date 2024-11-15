#include <userver/easy.hpp>

#include <userver/server/handlers/http_handler_base.hpp>

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

}

class Http::Handle final : public server::handlers::HttpHandlerBase, Function {
public:
    Handle(const components::ComponentConfig& config, const components::ComponentContext& context):
      HttpHandlerBase(config, context),
      callback_{Http::functions_[config.Name()]}
    {}

    std::string HandleRequestThrow(const HttpRequest& request, RequestContext&) const override {
        return callback_(request);
    }
private:
    Callback& callback_;
};

Http::Http(int argc, const char *const argv[]) : argc_{argc}, argv_{argv}, static_config_{kConfigBase} {}

Http::~Http() {
    functions_.clear();
}

Http& Http::Path(std::string_view path, Callback&& func) {
    functions_.insert(path, std::move(func));
    component_list_.Append<Handle>(path);
    AddHandleConfig(path);

    return *this;
}

int Http::Run() {
    return utils::DaemonMain(argc_, argv_, component_list_);
}

void Http::AddHandleConfig(std::string_view path) {
    static_config_ += fmt::format(kConfigHandlerTemplate, path);
}

}  // namespace easy

USERVER_NAMESPACE_END
