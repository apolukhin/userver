// Note: this is for the purposes of samples only
#include <userver/utest/using_namespace_userver.hpp>

#include <userver/easy.hpp>

int main(int argc, char* argv[]) {
    easy::Http(argc, argv) //
      .Path("/hello", [](const easy::HttpRequest& req) {
        req.GetHttpResponse().SetContentType(http::content_type::kTextPlain);
        return "Hello world";
      });
}
