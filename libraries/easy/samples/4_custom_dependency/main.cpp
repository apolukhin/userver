// Note: this is for the purposes of samples only
#include <userver/utest/using_namespace_userver.hpp>

#include <userver/easy.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

constexpr std::string_view kSchema = R"~(
CREATE TABLE IF NOT EXISTS events_table (
  id serial NOT NULL,
	action VARCHAR PRIMARY KEY,
	host_id INTEGER
)
)~";

class MyDependency : public easy::PgDep {
public:
    MyDependency(const components::ComponentConfig& config, const components::ComponentContext& context)
        : PgDep(config, context), host_id_(config["host_id"].As<int>()) {}

    int host_id() const { return host_id_; }

    static yaml_config::Schema GetStaticConfigSchema() {
        return yaml_config::MergeSchemas<easy::PgDep>(R"(
    type: object
    description: ID of the host
    additionalProperties: false
    properties:
        host_id:
            type: integer
            description: ID of the host
    )");
    }

private:
    const int host_id_;
};

void Registration(easy::OfDependency<MyDependency>, easy::HttpBase& app) {
    Registration(easy::OfDependency<easy::PgDep>{}, app);  // base class initilization

    app.AddComponentsConfig(R"~(
easy-dependencies:
    host_id#env: HOST_ID
    host_id#fallback: 42
)~");
}

template <>
inline constexpr auto components::kConfigFileMode<MyDependency> = ConfigFileMode::kNotRequired;

int main(int argc, char* argv[]) {
    easy::HttpWith<MyDependency>(argc, argv)
        .Schema(kSchema)
        .DefaultContentType(http::content_type::kTextPlain)
        .Post("/log", [](const server::http::HttpRequest& req, const MyDependency& dep) {
            dep.pg().Execute(
                storages::postgres::ClusterHostType::kMaster,
                "INSERT INTO events_table(action, host_id) VALUES($1, $2)",
                req.GetArg("action"),
                dep.host_id()
            );
            return std::string{};
        });
}
