// Note: this is for the purposes of samples only
#include <userver/utest/using_namespace_userver.hpp>

#include <userver/easy.hpp>

constexpr std::string_view kSchema = R"~(
CREATE TABLE IF NOT EXISTS key_value_table (
	key VARCHAR PRIMARY KEY,
	value VARCHAR
)
)~";

int main(int argc, char* argv[]) {
    easy::HttpWith<easy::PgDep>(argc, argv) //
      .Schema(kSchema)
      .DefaultContentType(http::content_type::kTextPlain)
      .Route("/hello", [](const easy::HttpRequest& /*req*/) {
        return "Hello world";
      })
      .Route("/hello/to/{user}", [](const easy::HttpRequest& req) {
        return "Hello, " + req.GetPathArg("user");
      })
      .Route("/hi", [](const easy::HttpRequest& req) {
        return "Hi, " + req.GetArg("name");
      })
      .Route("/kv", [](const easy::HttpRequest& req, const easy::PgDep& dep) {
        const auto& key = req.GetArg("key");
        if (req.GetMethod() == server::http::HttpMethod::kGet) {
            auto res = dep.pg().Execute(storages::postgres::ClusterHostType::kSlave, "SELECT value FROM key_value_table WHERE key=$1", key);
            return res[0][0].As<std::string>();
        } else {
            dep.pg().Execute(storages::postgres::ClusterHostType::kMaster, "INSERT INTO key_value_table(key, value) VALUES($1, $2) ON CONFLICT (key) DO UPDATE SET value = $2", key, req.GetArg("value"));
            return std::string{};
        }
      });
}
