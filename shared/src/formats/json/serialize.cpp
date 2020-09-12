#include <formats/json/serialize.hpp>

#include <array>
#include <fstream>
#include <memory>

#include <fmt/format.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>

#include <formats/json/exception.hpp>
#include <formats/json/value.hpp>
#include <utils/assert.hpp>

#include <formats/json/impl/json_tree.hpp>
#include <formats/json/impl/types_impl.hpp>

namespace formats::json {

namespace {

::rapidjson::CrtAllocator g_allocator;

std::string_view AsStringView(const impl::Value& jval) {
  return {jval.GetString(), jval.GetStringLength()};
}

void CheckKeyUniqueness(const impl::Value* root) {
  std::vector<impl::TreeIterFrame> stack;
  const impl::Value* value = root;

  stack.reserve(impl::kInitialStackDepth);
  stack.emplace_back();  // fake "top" frame to avoid extra checks for an empty
                         // stack inside walker loop

  for (;;) {
    stack.back().Advance();
    if (value->IsObject()) {
      // O(n²) just because our json objects are (hopefully) small
      const int count = value->MemberCount();
      const auto begin = value->MemberBegin();
      for (int i = 1; i < count; i++) {
        const std::string_view i_key = AsStringView(begin[i].name);
        for (int j = 0; j < i; j++) {
          const std::string_view j_key = AsStringView(begin[j].name);
          if (i_key == j_key)
            // TODO: add object path to message in TAXICOMMON-1658
            throw ParseException("Duplicate key: " + std::string(i_key) +
                                 " at " + impl::ExtractPath(stack));
        }
      }
    }

    if ((value->IsObject() && value->MemberCount() > 0) ||
        (value->IsArray() && value->Size() > 0)) {
      // descend
      stack.emplace_back(value);
    } else {
      while (!stack.back().HasMoreElements()) {
        stack.pop_back();
        if (stack.empty()) return;
      }
    }

    value = stack.back().CurrentValue();
  }
}

impl::VersionedValuePtr EnsureValid(impl::Document&& json) {
  if (!json.IsArray() && !json.IsObject()) {
    // keep message similar to what jsoncpp produces
    throw ParseException(
        "A valid JSON document must be either an array or an object value.");
  }

  CheckKeyUniqueness(&json);

  return impl::VersionedValuePtr::Create(std::move(json));
}

}  // namespace

formats::json::Value FromString(std::string_view doc) {
  if (doc.empty()) {
    throw ParseException("JSON document is empty");
  }

  impl::Document json{&g_allocator};
  rapidjson::ParseResult ok =
      json.Parse<rapidjson::kParseDefaultFlags |
                 rapidjson::kParseIterativeFlag |
                 rapidjson::kParseFullPrecisionFlag>(doc.data(), doc.size());
  if (!ok) {
    throw ParseException(fmt::format("JSON parse error: {} (at offset {})",
                                     rapidjson::GetParseError_En(ok.Code()),
                                     ok.Offset()));
  }

  return Value{EnsureValid(std::move(json))};
}

formats::json::Value FromStream(std::istream& is) {
  if (!is) {
    throw BadStreamException(is);
  }

  rapidjson::IStreamWrapper in(is);
  impl::Document json{&g_allocator};
  rapidjson::ParseResult ok =
      json.ParseStream<rapidjson::kParseDefaultFlags |
                       rapidjson::kParseIterativeFlag |
                       rapidjson::kParseFullPrecisionFlag>(in);
  if (!ok) {
    throw ParseException(fmt::format("JSON parse error: {} (at offset {})",
                                     rapidjson::GetParseError_En(ok.Code()),
                                     ok.Offset()));
  }

  return Value{EnsureValid(std::move(json))};
}

void Serialize(const formats::json::Value& doc, std::ostream& os) {
  rapidjson::OStreamWrapper out{os};
  rapidjson::Writer<rapidjson::OStreamWrapper> writer(out);
  doc.GetNative().Accept(writer);
  if (!os) {
    throw BadStreamException(os);
  }
}

std::string ToString(const formats::json::Value& doc) {
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  doc.GetNative().Accept(writer);
  return std::string(buffer.GetString(),
                     buffer.GetString() + buffer.GetLength());
}

namespace blocking {

formats::json::Value FromFile(const std::string& path) {
  std::ifstream is(path);
  return FromStream(is);
}

}  // namespace blocking

}  // namespace formats::json
