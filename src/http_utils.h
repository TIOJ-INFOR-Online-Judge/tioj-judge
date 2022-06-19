#ifndef HTTP_UTILS_H_
#define HTTP_UTILS_H_

#include <chrono>
#include <optional>
#include <type_traits>
#include <httplib.h>
#include <spdlog/spdlog.h>

#define ENUM_METHOD_ \
  X(GET, Get) \
  X(POST, Post)
enum class Method {
#define X(name, func) name,
  ENUM_METHOD_
#undef X
};

namespace http_utils {

template <class T>
std::string FormatParam(T&&) { return "(unknown)"; }
std::string FormatParam(const char*);
std::string FormatParam(const std::string&);
std::string FormatParam(const httplib::Params&);
std::string FormatParam(const httplib::Headers&);
std::string FormatParam();

template <class T, class... U>
std::string FormatParam(T&& head, U&&... tail) {
  return FormatParam(std::forward<T>(head)) + ' ' + FormatParam(std::forward<U>(tail)...);
}

#define X(name, func) \
template <typename T, typename... Args> class has_##func { \
  template <typename C, typename = decltype(std::declval<C>().func(std::declval<Args>()...) )> \
  static std::true_type test(int); \
  template <typename C> \
  static std::false_type test(...); \
 public: \
  static constexpr bool value = decltype(test<T>(0))::value; \
};
  ENUM_METHOD_
#undef X

} // namespace http_utils

struct HTTPGet {
  constexpr static char method_name[] = "GET";
  template <class... T>
  auto operator()(httplib::SSLClient& cli, const std::string& endpoint, T&&... params) {
    return cli.Get(endpoint.c_str(), std::forward<T>(params)...);
  }
};
struct HTTPPost {
  constexpr static char method_name[] = "POST";
  template <class... T>
  auto operator()(httplib::SSLClient& cli, const std::string& endpoint, T&&... params) {
    return cli.Post(endpoint.c_str(), std::forward<T>(params)...);
  }
};

template <class Method, class... T>
httplib::Result HTTPRequest(httplib::SSLClient& cli, const std::string& endpoint, T&&... params) {
  spdlog::debug("{} {} params {}", Method::method_name, endpoint, http_utils::FormatParam(params...));
  return Method()(cli, endpoint, std::forward<T>(params)...);
}

template <class Method, class Func, class... T>
std::optional<httplib::Result> RequestRetryInit(Func&& init, T&&... params) {
  const int kRetries = 5;
  using namespace std::chrono_literals;
  for (int i = 0; i < kRetries; i++) {
    init();
    if (auto res = HTTPRequest<Method>(std::forward<T>(params)...)) return res;
    std::this_thread::sleep_for(1s);
  }
  spdlog::warn("Request failed after {} retries", kRetries);
  return {};
}

template <class Method, class... T>
std::optional<httplib::Result> RequestRetry(T&&... params) {
  return RequestRetryInit<Method>([](){}, std::forward<T>(params)...);
}

#endif  // HTTP_UTILS_H_
