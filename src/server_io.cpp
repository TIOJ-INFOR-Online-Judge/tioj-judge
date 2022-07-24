#include "server_io.h"

#include <list>
#include <mutex>
#include <chrono>
#include <memory>
#include <unordered_set>
#include <condition_variable>

#include <httplib.h>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <nlohmann/json.hpp>

#include "database.h"
#include "http_utils.h"
#include "tioj/paths.h"
#include "tioj/utils.h"
#include <tioj/reporter.h>

std::string kTIOJUrl = "";
std::string kTIOJKey = "";
double kFetchInterval = 1.0;
int kMaxQueue = 500;

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

Database db;

// web client
struct Request {
  bool is_unique; //
  long key;       // for redundant elimination
  bool is_post;
  bool use_body;
  std::string endpoint;
  httplib::Params params; // only if not is_post
  nlohmann::json body; // only if is_post; key will be added automatically
};

using ParItem = httplib::Params::value_type;

void DoRequest(const Request& req) {
  httplib::Client cli(kTIOJUrl);
  if (req.is_post) {
    RequestRetry<HTTPPost>(cli, req.endpoint, req.body.dump(), "application/json");
  } else {
    RequestRetry<HTTPGet>(cli, req.endpoint, req.params, httplib::Headers());
  }
}

std::list<Request> request_queue;
std::unordered_map<long, std::list<Request>::iterator> request_map;
std::mutex request_queue_mtx;
std::condition_variable request_queue_cv;
void RequestLoop() {
  std::unique_lock lck(request_queue_mtx);
  while (true) {
    request_queue_cv.wait(lck, [](){ return request_queue.size(); });
    Request req = std::move(request_queue.front());
    if (req.is_unique) request_map.erase(req.key);
    request_queue.pop_front();
    lck.unlock();
    DoRequest(req);
    lck.lock();
  }
}

void PushRequest(Request&& req) {
  if (req.is_post) req.body["key"] = kTIOJKey;
  {
    std::lock_guard lck(request_queue_mtx);
    if (req.is_unique) {
      if (auto it = request_map.find(req.key); it != request_map.end()) {
        *(it->second) = std::move(req);
      } else {
        request_map[req.key] = request_queue.insert(request_queue.end(), std::move(req));
      }
    } else {
      request_queue.push_back(std::move(req));
    }
  }
  request_queue_cv.notify_one();
}

// reporter
class ServerReporter : public Reporter {
 public:
  // these functions should not block
  void ReportStartCompiling(const Submission& sub) override {
    SendStatus(sub.submission_id, "Validating");
  }
  void ReportOverallResult(const Submission& sub) override {
    SendFinalResult(sub);
  }
  void ReportScoringResult(const Submission& sub, int subtask) override {
    SendResult(sub);
  }
  void ReportCEMessage(const Submission& sub) override {
    // do nothing; since ReportOverallResult will send the message anyways
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

httplib::Params AddKey(httplib::Params&& params) {
  params.insert({"key", kTIOJKey});
  return params;
}

class TempDirectory { // RAII tempdir
  fs::path path_;
 public:
  const fs::path& Path() const { return path_; }
  fs::path UserCodePath() const { return path_ / "code"; }
  fs::path SpecjudgePath() const { return path_ / "sjcode"; }
  fs::path InterlibPath() const { return path_ / "interlib"; }
  fs::path InterlibImplPath() const { return path_ / "interlib_impl"; }
  TempDirectory() {
    char path[] = "/tmp/tmpsub.XXXXXX";
    char* res = mkdtemp(path);
    if (res) path_ = res;
  }
  ~TempDirectory() {
    if (!path_.empty()) fs::remove_all(path_);
  }
};

bool DealOneSubmission(httplib::Client& cli, nlohmann::json&& data) {
  using namespace httplib;
  using namespace sqlite_orm;
  using nlohmann::json;

  db.Init();

  Submission sub;
  TempDirectory tempdir;
  if (tempdir.Path().empty()) return false;

  int td_count = 0, orig_td_count = 0;
  std::vector<long> to_download, to_delete;
  std::vector<std::pair<int, long>> to_update_position;
  std::vector<Testdata> new_meta;
  try {
    if (data.empty()) return false;
    sub.submission_id = data["submission_id"].get<int>();
    sub.contest_id = data["contest_id"].get<int>();
    sub.priority = data["priority"].get<long>();
    sub.lang = GetCompiler(data["compiler"].get<std::string>());
    sub.submission_time = data["time"].get<int64_t>();
    std::ofstream(tempdir.UserCodePath()) << data["code"].get<std::string>();

    // user information
    auto& user = data["user"];
    sub.submitter_id = user["id"].get<int>();
    sub.submitter_name = user["name"].get<std::string>();
    sub.submitter_nickname = user["nickname"].get<std::string>();

    // problem information
    auto& problem = data["problem"];
    sub.problem_id = problem["id"].get<int>();
    sub.specjudge_type = (SpecjudgeType)problem["specjudge_type"].get<int>();
    if (sub.specjudge_type != SpecjudgeType::NORMAL) {
      sub.specjudge_lang = GetCompiler(problem["specjudge_compiler"].get<std::string>());
    }
    sub.interlib_type = (InterlibType)problem["interlib_type"].get<int>();
    if (sub.specjudge_type != SpecjudgeType::NORMAL) {
      std::ofstream(tempdir.SpecjudgePath()) << problem["sjcode"].get<std::string>();
    }
    if (sub.interlib_type != InterlibType::NONE) {
      std::ofstream(tempdir.InterlibPath()) << problem["interlib"].get<std::string>();
      std::ofstream(tempdir.InterlibImplPath()) << problem["interlib_impl"].get<std::string>();
    }

    // testdata & limits
    auto& td = data["td"];
    sub.td_limits.resize(td.size(), Submission::TestdataLimit{});
    td_count = td.size();
    {
      std::unordered_map<long, Testdata> orig_td;
      std::unordered_set<long> new_td;
      for (auto& i : db.ProblemTd(sub.problem_id)) {
        orig_td[i.testdata_id] = i;
        if (i.order >= orig_td_count) orig_td_count = i.order + 1;
      }
      for (int i = 0; i < td_count; i++) {
        // compare to determine which to download
        auto& td_item = td[i];
        Testdata td;
        td.testdata_id = td_item["id"].get<int>();
        td.timestamp = td_item["updated_at"].get<long>();
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

        // limits
        auto& lim = sub.td_limits[i];
        lim.time = td_item["time"].get<int64_t>();
        lim.vss = td_item["vss"].get<int64_t>();
        lim.rss = td_item["rss"].get<int64_t>();
        lim.output = td_item["output"].get<int64_t>();
        if (sub.lang == Compiler::HASKELL && lim.vss > 0) {
          // Haskell uses a lot of VSS, thus we limit RSS instead
          lim.rss = lim.rss == 0 ? lim.vss : std::min(lim.vss, lim.rss);
          lim.vss = 0;
        }
        lim.ignore_verdict = false;
      }
      for (auto& i : orig_td) {
        if (!new_td.count(i.first)) to_delete.push_back(i.first);
      }
    }
    // TODO FEATURE(group): read tasks
  } catch (json::exception& err) {
    spdlog::warn("Submission parsing error: {}", err.what());
    return false;
  }
  // download testdata
  for (long testdata_id : to_download) {
    if (!CreateDirs(TdPoolDir(testdata_id))) return false;
    // input
    auto res = DownloadFile<HTTPGet>(TdPoolPath(testdata_id, true, true), cli, "/fetch/testdata",
        AddKey({{"tid", std::to_string(testdata_id)}, {"input", ""}}), Headers());
    if (!IsSuccess(res)) return false;
    // output
    res = DownloadFile<HTTPGet>(TdPoolPath(testdata_id, false, true), cli, "/fetch/testdata",
        AddKey({{"tid", std::to_string(testdata_id)}}), Headers());
    if (!IsSuccess(res)) return false;
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
    // old symlinks
    for (int i = td_count; i < orig_td_count; i++) {
      fs::remove(TdInput(sub.problem_id, i), ec);
      fs::remove(TdOutput(sub.problem_id, i), ec);
    }
  }
  // update database meta
  db.UpdateTd(new_meta);
  // finalize & push
  sub.submission_internal_id = GetUniqueSubmissionInternalId();
  sub.reporter = &server_reporter;
  sub.remove_submission = true;
  CreateDirs(SubmissionCodePath(sub.submission_internal_id));
  Move(tempdir.UserCodePath(), SubmissionUserCode(sub.submission_internal_id));
  if (sub.specjudge_type != SpecjudgeType::NORMAL) {
    Move(tempdir.SpecjudgePath(), SubmissionJudgeCode(sub.submission_internal_id));
  }
  if (sub.interlib_type != InterlibType::NONE) {
    Move(tempdir.InterlibPath(), SubmissionInterlibCode(sub.submission_internal_id));
    Move(tempdir.InterlibImplPath(), SubmissionInterlibImplCode(sub.submission_internal_id));
  }
  // we only have one thread pushing submissions here, so no need to use
  //   this race-prevent version to limit submission queue size
  // if (!PushSubmission(std::move(sub), kMaxQueue)) {
  //   RemoveAll(SubmissionCodePath(sub.submission_internal_id));
  //   return false;
  // }
  PushSubmission(std::move(sub));
  return true;
}

} // namespace

bool FetchOneSubmission() {
  using namespace httplib;
  using nlohmann::json;

  Client cli(kTIOJUrl);
  // fetch submission
  auto res = RequestRetry<HTTPGet>(cli, "/fetch/submission_new", AddKey({}), httplib::Headers());
  if (!IsSuccess(res)) return false;

  json data;
  int submission_id;
  try {
    data = json::parse(res->body);
    if (data.empty()) return false;
    submission_id = data["submission_id"].get<int>();
    // optionally reject submission here
    //  SendStatus(submission_id, "queued");
  } catch (json::exception& err) {
    spdlog::warn("JSON decoding error: {}", err.what());
    return false;
  }
  if (!DealOneSubmission(cli, std::move(data))) {
    // send JE
    Submission sub;
    sub.submission_id = submission_id;
    sub.verdict = Verdict::JE;
    SendFinalResult(sub);
    return true;
  }
  return true;
}

void SendResult(const Submission& sub) {
  nlohmann::json data;
  data["submission_id"] = sub.submission_id;
  auto& tds = data["results"];
  tds = nlohmann::json::array();
  for (size_t i = 0; i < sub.td_results.size(); i++) {
    auto& nowtd = sub.td_results[i];
    if (nowtd.verdict == Verdict::NUL) continue;
    nlohmann::json tddata{
        {"position", i},
        {"verdict", VerdictToAbr(nowtd.verdict)},
        {"time", nowtd.time},
        {"rss", nowtd.rss},
        {"score", nowtd.score}};
    if (nowtd.vss == 0) {
      tddata["vss"] = nullptr;
    } else {
      tddata["vss"] = nowtd.vss;
    }
    tds.push_back(std::move(tddata));
  }
  Request req{};
  req.is_unique = true;
  req.key = sub.submission_id;
  req.is_post = true;
  req.endpoint = "/fetch/td_result";
  req.body = std::move(data);
  PushRequest(std::move(req));
}

void SendFinalResult(const Submission& sub) {
  nlohmann::json data{{"submission_id", sub.submission_id}, {"verdict", VerdictToAbr(sub.verdict)}};
  if (sub.verdict == Verdict::CE || sub.verdict == Verdict::CLE) {
    data["message"] = sub.ce_message;
  }
  Request req{};
  req.is_post = true;
  req.endpoint = "/fetch/submission_result";
  req.body = std::move(data);
  PushRequest(std::move(req));
}

void SendStatus(int submission_id, const std::string& status) {
  nlohmann::json data{{"submission_id", submission_id}, {"verdict", status}};
  Request req{};
  req.is_post = true;
  req.endpoint = "/fetch/submission_result";
  req.body = std::move(data);
  PushRequest(std::move(req));
}

void ServerWorkLoop() {
  std::thread thr(RequestLoop);
  thr.detach();
  while (true) {
    if (kMaxQueue > 0 && CurrentSubmissionQueueSize() < (size_t)kMaxQueue) FetchOneSubmission();
    std::this_thread::sleep_for(std::chrono::duration<double>(kFetchInterval));
  }
}
