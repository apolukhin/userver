// Note: this is for the purposes of samples only
#include <userver/utest/using_namespace_userver.hpp>

#include <userver/easy.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

#include <userver/clients/http/component.hpp>
#include <userver/components/component_context.hpp>

constexpr std::string_view kSchema = R"~(
CREATE TABLE IF NOT EXISTS events_table (
  id serial NOT NULL,
	action VARCHAR PRIMARY KEY
)
)~";

class ActionClient : public components::ComponentBase {
public:
    static constexpr std::string_view kName = "action-client";

    ActionClient(const components::ComponentConfig& config, const components::ComponentContext& context)
        : ComponentBase{config, context},
          service_url_(config["service-url"].As<std::string>()),
          http_client_(context.FindComponent<components::HttpClient>().GetHttpClient()) {}

    auto CreateHttpRequest(std::string action) const {
        return http_client_.CreateRequest().url(service_url_).post().data(std::move(action)).perform();
    }

    static yaml_config::Schema GetStaticConfigSchema() {
        return yaml_config::MergeSchemas<components::ComponentBase>(R"(
    type: object
    description: My dependencies schema
    additionalProperties: false
    properties:
        service-url:
            type: string
            description: URL of the service to send the actions to
    )");
    }

private:
    const std::string service_url_;
    clients::http::Client& http_client_;
};

class ActionDep {
public:
    explicit ActionDep(const components::ComponentContext& config) : component_{config.FindComponent<ActionClient>()} {}
    auto CreateActionRequest(std::string action) const { return component_.CreateHttpRequest(std::move(action)); }

protected:
    static void RegisterOn(easy::HttpBase& app) {
        app.TryAddComponent<ActionClient>(ActionClient::kName, "service-url: http://some-service.example/v1/action");
        easy::HttpDep::RegisterOn(app);
    }

private:
    ActionClient& component_;
};

int main(int argc, char* argv[]) {
    using Deps = easy::Dependencies<ActionDep, easy::PgDep>;

    easy::HttpWith<Deps>(argc, argv)
        .DbSchema(kSchema)
        .DefaultContentType(http::content_type::kTextPlain)
        .Post("/log", [](const server::http::HttpRequest& req, const Deps& deps) {
            const auto& action = req.GetArg("action");
            deps.pg().Execute(
                storages::postgres::ClusterHostType::kMaster, "INSERT INTO events_table(action) VALUES($1)", action
            );
            return deps.CreateActionRequest(action)->body();
        });
}