#include "http_utils.h"
#include <fmt/ranges.h>

namespace http_utils {

std::string FormatOneParam(const char* str) {
  return str;
}
std::string FormatOneParam(const std::string& str) {
  return str;
}
std::string FormatOneParam(const httplib::Params& params) {
  return fmt::format("{}", params);
}
std::string FormatOneParam(const httplib::Headers& headers) {
  return "";
}

std::string FormatParam() {
  return "(none)";
}

bool IsSuccess(int code) {
  return code >= 200 && code < 299;
}

} // namespace http_utils

bool IsSuccess(const httplib::Result& res) {
  return res && http_utils::IsSuccess(res->status);
}
