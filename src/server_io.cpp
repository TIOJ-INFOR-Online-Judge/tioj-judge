#include "server_io.h"

#include <mutex>
#include <queue>
#include <chrono>
#include <memory>
#include <unordered_set>
#include <condition_variable>

#include <httplib.h>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <sqlite_orm/sqlite_orm.h>

#include "http_utils.h"
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
fs::path TdPoolDir(long id) {
  return TdPool() / fmt::format("{:04d}", id / 100);
}
fs::path TdPoolPath(long id, bool is_input, bool is_temp) {
  std::string name = fmt::format("{:06d}.{}", id, is_input ? "in" : "out");
  if (is_temp) name += ".tmp";
  return TdPoolDir(id) / name;
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
  httplib::SSLClient cli(kTIOJUrl);
  if (req.is_post) {
    if (req.use_body) {
      RequestRetry<HTTPPost>(cli, req.endpoint, req.body.c_str(), req.format.c_str());
    } else {
      RequestRetry<HTTPPost>(cli, req.endpoint, req.params);
    }
  } else {
    RequestRetry<HTTPGet>(cli, req.endpoint, req.params, httplib::Headers());
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
  void ReportStartCompiling(const Submission& sub) override {
    SendValidating(sub.submission_id);
  }
  void ReportOverallResult(const Submission& sub) override {
    SendResult(sub, true);
  }
  void ReportScoringResult(const Submission& sub, int subtask) override {
    SendResult(sub, false);
  }
  void ReportCEMessage(const Submission& sub) override {
    SendCEMessage(sub);
  }
} server_reporter;

// helper
template <class Method, class... T>
inline auto DownloadFile(const fs::path& path, T&&... params) {
  std::ofstream fout;
  auto reciever = [&fout](const char *data, size_t data_length) {
    fout.write(data, data_length);
    return true;
  };
  return RequestRetryInit<Method>(
      [&](){
        if (fout.is_open()) fout.close();
        fout.open(path);
      },
      std::forward<T>(params)...,
      reciever);
}

} // namespace

bool FetchOneSubmission() {
  using namespace sqlite_orm;

  if (!db) db = std::make_unique<Storage>(InitDatabase());
  httplib::SSLClient cli(kTIOJUrl);
  // fetch submission
  auto res = RequestRetry<HTTPGet>(cli, "submission");
  if (!res) return false;
  Submission sub;
  {
    std::stringstream ss(res.value()->body);
    ss >> sub.submission_id;
    if (sub.submission_id < 0) return false;
    int problem_type;
    std::string lang;
    ss >> sub.problem_id >> problem_type >> sub.submitter_id >> lang;
    static constexpr SpecjudgeType kSpecjudgeTypeMap[] = {
      SpecjudgeType::NORMAL,
      SpecjudgeType::SPECJUDGE_OLD,
      SpecjudgeType::NORMAL,
    };
    static constexpr InterlibType kInterlibTypeMap[] = {
      InterlibType::NONE,
      InterlibType::NONE,
      InterlibType::INCLUDE,
    };
    sub.specjudge_type = kSpecjudgeTypeMap[problem_type];
    sub.interlib_type = kInterlibTypeMap[problem_type];
    sub.lang = GetCompiler(lang);
    sub.specjudge_lang = Compiler::GCC_CPP_17;
  }
  // fetch limits & sj & interlib
  // TODO
  // fetch testdata metadata
  res = RequestRetry<HTTPGet>(cli, "testdata_meta",
      (httplib::Params){{"problem_id", std::to_string(sub.problem_id)}}, httplib::Headers());
  if (!res) return false;
  int td_count = 0, orig_td_count = 0;
  std::vector<long> to_download, to_delete;
  std::vector<std::pair<int, long>> to_update_position;
  std::vector<Testdata> new_meta;
  {
    std::unordered_map<long, Testdata> orig_td;
    std::unordered_set<long> new_td;
    for (auto& i : db->iterate<Testdata>(where(c(&Testdata::problem_id) == sub.problem_id))) {
      orig_td[i.testdata_id] = i;
      if (i.order >= orig_td_count) orig_td_count = i.order + 1;
    }
    std::stringstream ss(res.value()->body);
    ss >> td_count;
    for (int i = 0; i < td_count; i++) {
      Testdata td;
      ss >> td.testdata_id >> td.timestamp;
      td.order = i;
      td.problem_id = sub.problem_id;
      auto it = orig_td.find(td.testdata_id);
      if (it == orig_td.end() || it->second.timestamp != td.timestamp) {
        to_download.push_back(td.testdata_id);
      }
      if (it == orig_td.end() || it->second.order != i) {
        to_update_position.push_back({i, td.testdata_id});
      }
      new_meta.push_back(td);
      new_td.insert(td.testdata_id);
    }
    for (auto& i : orig_td) {
      if (!new_td.count(i.first)) to_delete.push_back(i.first);
    }
  }
  // download testdata
  for (long testdata_id : to_download) {
    if (!CreateDirs(TdPoolDir(testdata_id))) return false;
    // input
    res = DownloadFile<HTTPGet>(TdPoolPath(testdata_id, true, true), cli, "testdata",
        (httplib::Params){{"tid", std::to_string(testdata_id)}, {"input", ""}}, httplib::Headers());
    if (!res) return false;
    // output
    res = DownloadFile<HTTPGet>(TdPoolPath(testdata_id, false, true), cli, "testdata",
        (httplib::Params){{"tid", std::to_string(testdata_id)}}, httplib::Headers());
    if (!res) return false;
  }
  // update symlinks
  {
    std::lock_guard lck(td_file_lock[sub.problem_id]);
    std::error_code ec;
    for (long testdata_id : to_download) {
      // rename & replace only, so it should be fast
      fs::rename(TdPoolPath(testdata_id, true, true), TdPoolPath(testdata_id, true, false), ec);
      if (ec) return false;
      fs::rename(TdPoolPath(testdata_id, false, true), TdPoolPath(testdata_id, false, false), ec);
      if (ec) return false;
    }
    for (long testdata_id : to_delete) {
      fs::remove(TdPoolPath(testdata_id, true, false), ec);
      fs::remove(TdPoolPath(testdata_id, false, false), ec);
    }
    CreateDirs(TdPath(sub.problem_id));
    for (auto [order, testdata_id] : to_update_position) {
      auto target_in = TdInput(sub.problem_id, order);
      auto target_out = TdOutput(sub.problem_id, order);
      // ignore error (might not exist)
      fs::remove(target_in, ec);
      fs::remove(target_out, ec);
      fs::create_symlink(TdPoolPath(testdata_id, true, false), target_in, ec);
      if (ec) return false;
      fs::create_symlink(TdPoolPath(testdata_id, false, false), target_out, ec);
      if (ec) return false;
    }
  }
  // update database meta
  db->replace_range(new_meta.begin(), new_meta.end());
  // fetch submission
  // TODO
  sub.submission_internal_id = GetUniqueSubmissionInternalId();
  sub.reporter = &server_reporter;
  return false;
}

void SendResult(const Submission& sub, bool done) {
  std::string data;
  Verdict ver = sub.verdict;
  if (ver == Verdict::CE || ver == Verdict::CLE || ver == Verdict::ER) {
    data = VerdictToAbr(ver);
    // TODO FEATURE(web-update-1): support more verdicts & set score
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
