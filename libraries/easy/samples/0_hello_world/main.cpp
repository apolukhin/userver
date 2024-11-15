// Note: this is for the purposes of samples only
#include <userver/utest/using_namespace_userver.hpp>

#include <userver/easy.hpp>

int main(int argc, char* argv[]) {
    easy::Http(argc, argv) //
      .DefaultContentType(http::content_type::kTextPlain)
      .Route("/hello", [](const easy::HttpRequest& req) {
        return "Hello world";
      })
      .Route("/hello/to/{user}", [](const easy::HttpRequest& req) {
        return "Hello, " + req.GetPathArg("user");
      })
      .Route("/hi", [](const easy::HttpRequest& req) {
        return "Hi, " + req.GetArg("name");
      });
}
