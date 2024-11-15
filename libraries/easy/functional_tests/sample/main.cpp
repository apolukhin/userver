#include <userver/utest/using_namespace_userver.hpp>  // IWYU pragma: keep

#include <userver/easy.hpp>

int main(int argc, char* argv[]) {
    return easy::Http(argc, argv) //
      .Path("/hello", [](easy::HttpRequest& req) {
        return "Hello world";
      }) //
      .Run();
}
