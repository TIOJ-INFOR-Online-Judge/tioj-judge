#include "server_io.h"

#include <mutex>
#include <queue>
#include <chrono>
#include <memory>
#include <condition_variable>

#include <httplib.h>
#include <fmt/core.h>
#include <sqlite_orm/sqlite_orm.h>

#include "tioj/paths.h"
#include "tioj/utils.h"
#include <tioj/reporter.h>

std::string kTIOJUrl = "";
std::string kTIOJKey = "";
double kFetchInterval = 1.0;

namespace {

// paths
fs::path TdPool() {
  return kDataDir / "td-pool";
}
fs::path DatabasePath() {
  return kDataDir / "db.sqlite";
}

// database
// TODO FEATURE(web-refactor): add td limits
struct Testdata {
  int testdata_id;
  int problem_id;
  int order;
  long timestamp;
};

inline auto InitDatabase() {
  using namespace sqlite_orm;
  auto storage = make_storage(DatabasePath(),
      make_index("idx_testdata_problem_order", &Testdata::problem_id, &Testdata::order),
      make_table("testdata",
                 make_column("testdata_id", &Testdata::testdata_id, primary_key()),
                 make_column("problem_id", &Testdata::problem_id),
                 make_column("order", &Testdata::order),
                 make_column("timestamp", &Testdata::timestamp)));
  storage.sync_schema(true);
  return storage;
}

using Storage = decltype(InitDatabase());
std::unique_ptr<Storage> db;

// web client
struct Request {
  bool is_post;
  bool use_body;
  std::string endpoint; // use append_query_params if necessary
  httplib::Params params;
  std::string body;   //
  std::string format; // only if use_body
};

using ParItem = httplib::Params::value_type;

void DoRequest(const Request& req) {
  auto Retry = [](auto&& func) {
    using namespace std::chrono_literals;
    for (int i = 0; i < 15; i++) {
      if (auto res = func()) break;
      std::this_thread::sleep_for(500ms);
    }
  };
  httplib::SSLClient cli(kTIOJUrl);
  if (req.is_post) {
    if (req.use_body) {
      Retry([&](){ return cli.Post(req.endpoint.c_str(), req.body, req.format.c_str()); });
    } else {
      Retry([&](){ return cli.Post(req.endpoint.c_str(), req.params); });
    }
  } else {
    Retry([&](){ return cli.Get(req.endpoint.c_str(), req.params, httplib::Headers()); });
  }
}

std::queue<Request> request_queue;
std::mutex request_queue_mtx;
std::condition_variable request_queue_cv;
void RequestLoop() {
  std::unique_lock lck(request_queue_mtx);
  while (true) {
    request_queue_cv.wait(lck, [](){ return request_queue.size(); });
    Request req = std::move(request_queue.front());
    request_queue.pop();
    lck.unlock();
    DoRequest(req);
    lck.lock();
  }
}

void PushRequest(Request&& req) {
  {
    std::lock_guard lck(request_queue_mtx);
    request_queue.push(std::move(req));
  }
  request_queue_cv.notify_one();
}

// reporter
class ServerReporter : public Reporter {
 public:
  // these functions should not block
  virtual void ReportStartCompiling(const Submission& sub) {
    SendValidating(sub.submission_id);
  }
  virtual void ReportOverallResult(const Submission& sub) {
    SendResult(sub, true);
  }
  virtual void ReportScoringResult(const Submission& sub, int subtask) {
    SendResult(sub, false);
  }
  virtual void ReportCEMessage(const Submission& sub) {
    SendCEMessage(sub);
  }
} server_reporter;

} // namespace

bool FetchOneSubmission() {
  if (!db) db = std::unique_ptr<Storage>(new Storage(InitDatabase()));
  Submission sub;
  sub.reporter = &server_reporter;
  // TODO
  return false;
}

void SendResult(const Submission& sub, bool done) {
  std::string data;
  Verdict ver = sub.verdict;
  if (ver == Verdict::CE || ver == Verdict::CLE || ver == Verdict::ER) {
    data = VerdictToAbr(ver);
    // TODO FEATURE(web-update-1): support more verdicts
    if (data == "CLE") data = "CE";
  } else {
    for (size_t i = 0; i < sub.td_limits.size(); i++) {
      if (i < sub.td_results.size()) {
        auto& nowtd = sub.td_results[i];
        std::string ver_str = VerdictToAbr(nowtd.verdict);
        // TODO FEATURE(web-update-1)
        if (ver_str == "EE" || ver_str == "OLE" || ver_str == "SIG") ver_str = "RE";
        data += fmt::format("{}/{}/{}/", VerdictToAbr(nowtd.verdict), nowtd.time / 1000, nowtd.rss);
      } else {
        data += "/0/0/";
      }
    }
  }
  Request req{};
  req.is_post = false;
  req.endpoint = "/write_result";
  req.params = {{"sid", std::to_string(sub.submission_id)}, {"result", data},
                {"status", done ? "OK" : "NO"}, {"key", kTIOJKey}};
  PushRequest(std::move(req));
}

void SendCEMessage(const Submission& sub) {
  Request req{};
  req.is_post = true;
  req.use_body = false;
  req.endpoint = httplib::append_query_params("/write_message",
      {{"sid", std::to_string(sub.submission_id)}, {"key", kTIOJKey}});
  req.params = {{"message", sub.ce_message}};
  PushRequest(std::move(req));
}

void SendValidating(int submission_id) {
  Request req{};
  req.is_post = false;
  req.endpoint = "/validating";
  req.params = {{"sid", std::to_string(submission_id)}, {"key", kTIOJKey}};
  PushRequest(std::move(req));
}

void ServerWorkLoop() {
  std::thread thr(RequestLoop);
  thr.detach();
  while (true) {
    FetchOneSubmission();
    std::this_thread::sleep_for(std::chrono::duration<double>(kFetchInterval));
  }
}
