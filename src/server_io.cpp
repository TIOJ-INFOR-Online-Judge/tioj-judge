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
#include "websocket.h"
#include "http_utils.h"
#include "tioj/paths.h"
#include "tioj/utils.h"
#include <tioj/reporter.h>

std::string kTIOJUrl = "";
std::string kTIOJKey = "";
size_t kMaxQueue = 20;

namespace {

const std::string kChannelIdentifier = "{\"channel\":\"FetchChannel\"}";

/// --- paths & helpers ---
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

inline double MonotonicTimestamp() {
  auto dur = std::chrono::steady_clock::now().time_since_epoch();
  return std::chrono::duration<double>(dur).count();
}

/// --- database ---
Database db;

/// --- websocket client ---
constexpr double kUniqueReqMinInterval = 0.6;

// outgoing requests
struct Request {
  bool is_subscribe;
  bool is_unique;
  bool force_pop; // if force_pop, pop is_unique's with current key
  long key;
  std::string action;
  nlohmann::json body;

  std::string ToRequest() const {
    using nlohmann::json;
    json ret{{"identifier", kChannelIdentifier}, {"command", is_subscribe ? "subscribe" : "message"}};
    if (!is_subscribe) {
      json nbody(body);
      nbody["action"] = action;
      ret["data"] = nbody.dump(-1, ' ', false, json::error_handler_t::ignore);
    }
    return ret.dump();
  }
};

std::set<std::pair<double, long>> unsent_timestamps;
std::unordered_map<long, double> unique_timestamp_map;
std::unordered_map<long, Request> unique_requests;
std::list<Request> request_queue, backup_queue;
std::mutex request_queue_mtx, backup_queue_mtx;
std::condition_variable request_queue_cv;

void PushRequest(Request&& req) {
  {
    std::lock_guard lck(request_queue_mtx);
    if (req.is_unique) {
      if (auto it = unique_requests.find(req.key); it != unique_requests.end()) {
        // a request of same id waiting; replace
        it->second = std::move(req);
      } else if (auto it = unique_timestamp_map.find(req.key); it != unique_timestamp_map.end()) {
        // no request of same id waiting but one is sent previously; insert it
        unsent_timestamps.insert({it->second, req.key});
        unique_requests.insert({req.key, std::move(req)});
      } else {
        // first encounter of the id
        unsent_timestamps.insert({0.0, req.key});
        unique_timestamp_map[req.key] = 0.0;
        unique_requests.insert({req.key, std::move(req)});
      }
    } else if (req.force_pop) {
      if (auto it = unique_timestamp_map.find(req.key); it != unique_timestamp_map.end()) {
        unsent_timestamps.erase({it->second, req.key});
        // unique_timestamp_map will be erased at sent
        unique_requests.erase(req.key);
      }
      request_queue.push_back(std::move(req));
    } else if (req.is_subscribe) {
      request_queue.push_front(std::move(req));
    } else {
      request_queue.push_back(std::move(req));
    }
  }
  request_queue_cv.notify_one();
}

bool CheckUniqueRequests() {
  double ts = MonotonicTimestamp();
  while (unsent_timestamps.size()) {
    auto it = unsent_timestamps.begin();
    if (it->first >= ts - kUniqueReqMinInterval) break;
    auto req = unique_requests.find(it->second); // should always exist
    request_queue.push_back(std::move(req->second));
    unsent_timestamps.erase(it);
    unique_requests.erase(req);
  }
  return request_queue.size();
}

void ResendBackupRequests() {
  std::scoped_lock lck(backup_queue_mtx, request_queue_mtx);
  request_queue.splice(request_queue.begin(), backup_queue);
}

// judge requests
std::mutex judge_mtx;

bool DealOneSubmission(nlohmann::json&& data);

void OneSubmissionThread(nlohmann::json&& data) {
  int submission_id = data["submission_id"].get<int>();
  std::lock_guard lck(judge_mtx);
  // optionally reject submission here
  if (CurrentSubmissionQueueSize() >= kMaxQueue) {
    SendStatus(submission_id, "queued");
    return;
  }
  if (CurrentSubmissionQueueSize() + 1 < kMaxQueue) TryFetchSubmission();
  if (!DealOneSubmission(std::move(data))) {
    // send JE
    SendStatus(submission_id, VerdictToAbr(Verdict::JE));
    TryFetchSubmission();
  }
}

// websocket class
class TIOJClient : public WsClient {
  void ReconnectThread_() {
    ResendBackupRequests();
    std::thread([this]() {
      std::this_thread::sleep_for(std::chrono::seconds(3));
      Connect();
    }).detach();
  }
 public:
  // TODO: escape TIOJKey
  TIOJClient() : WsClient("ws" + kTIOJUrl.substr(4) + "/cable?key=" + kTIOJKey) {}

  double last_ping = 0;

  void OnOpen() override {
    spdlog::info("Connected to server websocket");
    last_ping = MonotonicTimestamp();
    Request req{};
    req.is_subscribe = true;
    PushRequest(std::move(req));
    TryFetchSubmission();
  }

  void OnFail() override {
    spdlog::warn("Failed to connect to server, reconnect in 3 seconds");
    ReconnectThread_();
  }
  void OnClose() override {
    spdlog::warn("Connection with server closed, reconnect in 3 seconds");
    ReconnectThread_();
  }

  void OnMessage(const std::string& msg) override {
    using nlohmann::json;
    {
      std::lock_guard lck(backup_queue_mtx);
      backup_queue.clear();
    }
    json data;
    std::string msg_type;
    try {
      data = json::parse(msg);
      last_ping = MonotonicTimestamp();
      spdlog::debug("Message from server: {}", msg);
      if (data.contains("type")) return; // confirm_subscription or ping
      msg_type = data["message"]["type"].get<std::string>();
    } catch (json::exception& err) {
      spdlog::warn("JSON decoding error: {}", err.what());
      return;
    }
    if (msg_type == "notify") {
      TryFetchSubmission();
    } else if (msg_type == "submission") {
      std::thread(OneSubmissionThread, std::move(data["message"]["data"])).detach();
    }
  }

  bool Send(const std::string& msg) {
    bool ret = WsClient::Send(msg);
    spdlog::debug("Send message: {}, result={}", msg, ret);
    return ret;
  }
};

// This function deal with all outgoing messages
void RequestLoop() {
  TIOJClient cli;
  cli.Connect();
  std::unique_lock lck(request_queue_mtx);
  while (true) {
    request_queue_cv.wait_for(lck, std::chrono::duration<double>(kUniqueReqMinInterval / 2),
        [&cli](){ return cli.CanSend() && CheckUniqueRequests(); });
    if (cli.CanSend() && cli.last_ping > 0 && MonotonicTimestamp() - cli.last_ping > 8) {
      cli.Close();
      continue;
    }
    while (request_queue.size()) {
      Request req = std::move(request_queue.front());
      if (req.is_unique) {
        unique_timestamp_map[req.key] = MonotonicTimestamp();
      } else if (req.force_pop) {
        unique_timestamp_map.erase(req.key);
      }
      request_queue.pop_front();
      lck.unlock();
      cli.Send(req.ToRequest());
      {
        std::lock_guard lck(backup_queue_mtx);
        backup_queue.push_back(std::move(req));
      }
      lck.lock();
    }
  }
}

/// --- reporter ---
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
  void ReportCEMessage(const Submission&) override {
    // do nothing; ReportOverallResult will send the message anyways
  }
  void ReportFinalized(const Submission&, size_t queue_size_before_pop) override {
    if (queue_size_before_pop == kMaxQueue) TryFetchSubmission();
  }
} server_reporter;

// --- helpers ---
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

bool DealOneSubmission(nlohmann::json&& data) {
  using namespace httplib;
  using namespace sqlite_orm;
  using nlohmann::json;

  Client cli(kTIOJUrl);
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
    sub.sandbox_strict = problem["strict_mode"].get<bool>();
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
        lim.ignore_verdict = td_item["verdict_ignore"].get<bool>();
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
  PushSubmission(std::move(sub));
  return true;
}

nlohmann::json TdResultsJSON(const Submission& sub) {
  nlohmann::json tds = nlohmann::json::array();
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
    if (!nowtd.message.empty()) {
      tddata["message_type"] = nowtd.message_type;
      tddata["message"] = nowtd.message;
    }
    tds.push_back(std::move(tddata));
  }
  return tds;
}

} // namespace

void TryFetchSubmission() {
  Request req{};
  req.is_unique = true;
  req.key = -1;
  req.action = "fetch_submission";
  PushRequest(std::move(req));
}

void SendResult(const Submission& sub) {
  nlohmann::json data;
  data["submission_id"] = sub.submission_id;
  data["results"] = TdResultsJSON(sub);
  Request req{};
  req.is_unique = true;
  req.key = sub.submission_id;
  req.action = "td_result";
  req.body = std::move(data);
  PushRequest(std::move(req));
}

void SendFinalResult(const Submission& sub) {
  nlohmann::json data{{"submission_id", sub.submission_id}, {"verdict", VerdictToAbr(sub.verdict)}};
  if (sub.verdict == Verdict::CE || sub.verdict == Verdict::CLE) {
    data["message"] = sub.ce_message;
  } else {
    data["td_results"] = TdResultsJSON(sub);
  }
  Request req{};
  req.force_pop = true;
  req.key = sub.submission_id;
  req.action = "submission_result";
  req.body = std::move(data);
  PushRequest(std::move(req));
}

void SendStatus(int submission_id, const std::string& status) {
  nlohmann::json data{{"submission_id", submission_id}, {"verdict", status}};
  Request req{};
  req.force_pop = true;
  req.key = submission_id;
  req.action = "submission_result";
  req.body = std::move(data);
  PushRequest(std::move(req));
}

void SendQueuedSubmissions(bool send_if_empty) {
  std::vector<int> ids = GetQueuedSubmissionID();
  if (ids.empty() && !send_if_empty) return;
  nlohmann::json data{{"submission_ids", ids}};
  Request req{};
  req.is_unique = true;
  req.key = -2;
  req.action = "report_queued";
  req.body = std::move(data);
  PushRequest(std::move(req));
}

void ServerWorkLoop() {
  // main thread: send current received submissions
  // thread 2: RequestLoop (send all outgoing requests via queue)
  // thread 3...: WsClient (created by RequestLoop)
  std::thread thr(RequestLoop);
  thr.detach();
  int empty_cnt = 0;
  while (true) {
    std::this_thread::sleep_for(std::chrono::duration<double>(10));
    // if empty, send every 30 seconds
    SendQueuedSubmissions(empty_cnt == 2);
    size_t queue_size = CurrentSubmissionQueueSize();
    if (queue_size == 0) {
      if (++empty_cnt == 3) empty_cnt = 0;
    } else {
      empty_cnt = 0;
    }
    // fetch every 10 seconds anyways
    if (queue_size < kMaxQueue) TryFetchSubmission();
  }
}
