#include "http_utils.h"
#include <fmt/ranges.h>

namespace http_utils {

std::string FormatParam(const char* str) {
  return str;
}
std::string FormatParam(const std::string& str) {
  return str;
}
std::string FormatParam(const httplib::Params& params) {
  return fmt::format("{}", params);
}
std::string FormatParam(const httplib::Headers& headers) {
  return "";
}
std::string FormatParam() {
  return "(none)";
}


} // namespace http_utils
