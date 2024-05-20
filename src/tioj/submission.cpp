#include "submission.h"

#include <unistd.h>
#include <sys/stat.h>
#include <mutex>
#include <queue>
#include <regex>
#include <fstream>
#include <unordered_set>
#include <unordered_map>
#include <condition_variable>

#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include "tasks.h"
#include "utils.h"
#include "paths.h"

int kMaxParallel = 1;
cpu_set_t kPinnedCpus = {};
long kMaxRSS = 2 * 1024 * 1024; // 2G
long kMaxOutput = 1 * 1024 * 1024; // 1G
double kTimeMultiplier = 1.0;

namespace {

inline long ToUs(const struct timeval& v) {
  return ((long)v.tv_sec * 1'000'000 + v.tv_usec) * (long double)kTimeMultiplier;
}

inline std::string ScoreToString(long score) {
  std::string frac = std::to_string(score % 1'000'000);
  frac = std::string(6 - frac.size(), '0') + frac;
  return std::to_string(score / 1'000'000) + "." + frac;
}

} // namespace

nlohmann::json SubmissionAndResult::TestdataMeta(int subtask, int stage) const {
  const Submission::TestdataItem& lim = sub.testdata[subtask];
  const SubmissionResult::TestdataResult& td_result = result.td_results[subtask];
  const struct cjail_result& res = td_result.execute_result;
  return {
    {"input_file", ScoringBoxTdInput(-1, -1, -1, true)},
    {"answer_file", ScoringBoxTdOutput(-1, -1, -1, true)},
    {"user_output_file", ScoringBoxUserOutput(-1, -1, -1, true)},
    {"user_code_file", ScoringBoxUserCode(-1, -1, -1, sub.lang, true)},
    {"problem_id", sub.problem_id},
    {"contest_id", sub.contest_id},
    {"submission_id", sub.submission_id},
    {"submitter_id", sub.submitter_id},
    {"submitter_name", sub.submitter_name},
    {"submitter_nickname", sub.submitter_nickname},
    {"submission_time", sub.submission_time},
    {"compiler", CompilerName(sub.lang)},
    {"testdata_index", subtask},
    {"current_stage", stage},
    {"original_verdict", VerdictToAbr(td_result.verdict)},
    {"current_time_us", td_result.time},
    {"message_type", td_result.message_type},
    {"message", td_result.message},
    {"limits", {
      {"time_us", lim.time},
      {"vss_kib", lim.vss},
      {"rss_kib", lim.rss},
      {"output_kib", lim.output},
    }},
    {"stats", {
      {"exit_code", res.info.si_status},
      {"real_us", ToUs(res.time)},
      {"user_us", ToUs(res.rus.ru_utime)},
      {"sys_us", ToUs(res.rus.ru_stime)},
      {"max_rss_kib", res.rus.ru_maxrss},
      {"max_vss_kib", res.stats.hiwater_vm},
    }},
  };
}

nlohmann::json SubmissionAndResult::SummaryMeta() const {
  nlohmann::json testdata = nlohmann::json::array();
  for (size_t i = 0; i < result.td_results.size(); i++) {
    const auto& td_result = result.td_results[i];
    testdata.push_back({
      {"verdict", VerdictToAbr(td_result.verdict)},
      {"score", ScoreToString(td_result.score)},
      {"time_us", td_result.time},
      {"vss_kib", td_result.vss},
      {"rss_kib", td_result.rss},
      {"message_type", td_result.message_type},
      {"message", td_result.message},
      {"ignore_verdict", sub.testdata[i].ignore_verdict},
      {"subtasks", sub.testdata[i].td_groups},
    });
  }
  nlohmann::json subtask_scores = nlohmann::json::array();
  for (long score : sub.group_score) {
    subtask_scores.push_back(ScoreToString(score));
  }
  return {
    {"user_code_file", SummaryBoxUserCode(-1, sub.lang, true)},
    {"problem_id", sub.problem_id},
    {"contest_id", sub.contest_id},
    {"submission_id", sub.submission_id},
    {"submitter_id", sub.submitter_id},
    {"submitter_name", sub.submitter_name},
    {"submitter_nickname", sub.submitter_nickname},
    {"submission_time", sub.submission_time},
    {"compiler", CompilerName(sub.lang)},
    {"ce_message", result.ce_message},
    {"ce_message_file", SummaryBoxCEMessage(-1, true)},
    {"verdict", VerdictToAbr(result.verdict)},
    {"testdata", testdata},
    {"subtask_scores", subtask_scores},
  };
}

namespace {

// TaskEntry will form a dependency directed graph
// Once a task is finished, it is removed from the graph
// Any tasks with indeg = 0 will be pushed into task_queue
struct TaskEntry {
  static long task_count;
  long id;
  long submission_internal_id;
  Task task;
  long priority;
  int task_order;
  int indeg;
  std::vector<long> edges;

  TaskEntry() {}
  TaskEntry(int sub_id, const Task& task, long priority, int order = 0) :
      id(task_count++),
      submission_internal_id(sub_id),
      task(task), priority(priority),
      task_order(order), indeg(0) {}
  bool operator<(const TaskEntry& x) const {
    return std::make_tuple(priority, -submission_internal_id, -task_order) <
           std::make_tuple(x.priority, -x.submission_internal_id, -x.task_order);
  }
};
long TaskEntry::task_count = 0;

std::mutex task_mtx;
std::condition_variable task_cv;
std::unordered_map<long, TaskEntry> task_list;

struct PriorityCompare {
  bool operator()(long a, long b) {
    return task_list[a] < task_list[b];
  }
};

std::priority_queue<long, std::vector<long>, PriorityCompare> task_queue;
std::unordered_map<int, long> handle_map;
std::unordered_map<long, SubmissionAndResult> submission_list;

// cancelling related
std::unordered_map<long, long> submission_id_map; // submission id -> internal id
std::unordered_set<long> cancelled_list;
std::unordered_map<long, std::unordered_set<int>> cancelled_group; // internal id -> (group id)

/// Helpers for manipulating graphs
inline void InsertTaskList(TaskEntry&& task) {
  long tid = task.id;
  int indeg = task.indeg;
  task_list.insert({tid, std::move(task)});
  if (!indeg) task_queue.push(tid);
}
inline void Link(TaskEntry& a, TaskEntry& b) {
  a.edges.push_back(b.id);
  b.indeg++;
}
inline void Remove(const TaskEntry& task) {
  for (long nxt : task.edges) {
    if (auto& nxt_task = task_list[nxt]; !--nxt_task.indeg) task_queue.push(nxt);
  }
  task_list.erase(task.id);
}

inline bool IsCancelled(long id, const std::vector<int>& groups) {
  if (cancelled_list.count(id)) return true;
  auto it = cancelled_group.find(id);
  if (it == cancelled_group.end() || groups.empty()) return false;
  for (int group : groups) {
    if (!it->second.count(group)) return false;
  }
  return true;
}

inline long NormalizeScore(long double score) {
  constexpr long double kMax = 1'000'000;
  if (score > kMax) score = kMax;
  if (score < -kMax) score = -kMax;
  return std::lround(score * 1'000'000);
}

inline Compiler GetLang(const Submission& sub, CompileSubtask subtask) {
  switch (subtask) {
    case CompileSubtask::USERPROG: return sub.lang;
    case CompileSubtask::SPECJUDGE: return sub.specjudge_lang;
    case CompileSubtask::SUMMARY: return sub.summary_lang;
  }
  __builtin_unreachable();
}

/// Task env setup
bool SetupCompile(const SubmissionAndResult& sub_and_result, const TaskEntry& task) {
  const Submission& sub = sub_and_result.sub;
  long id = sub.submission_internal_id;
  CompileSubtask subtask = (CompileSubtask)task.task.subtask;
  if (cancelled_list.count(id)) return false; // cancellation check

  CreateDirs(Workdir(CompileBoxPath(id, subtask)), fs::perms::all);
  fs::path code_dest = CompileBoxInput(id, subtask, GetLang(sub, subtask));
  switch (subtask) { // copy code
    case CompileSubtask::USERPROG: {
      Copy(SubmissionUserCode(id), code_dest, kPerm666);
      break;
    }
    case CompileSubtask::SPECJUDGE: {
      Copy(SubmissionJudgeCode(id), code_dest, kPerm666);
      break;
    }
    case CompileSubtask::SUMMARY: {
      Copy(SubmissionSummaryCode(id), code_dest, kPerm666);
      break;
    }
  }
  switch (subtask) { // copy other dependencies
    case CompileSubtask::USERPROG: {
      if (sub.reporter.ReportStartCompiling) sub.reporter.ReportStartCompiling(sub, sub_and_result.result);
      switch (sub.interlib_type) {
        case InterlibType::NONE: break;
        case InterlibType::INCLUDE: {
          Copy(SubmissionInterlibCode(id), CompileBoxInterlib(id, sub.problem_id), kPerm666);
          Copy(SubmissionInterlibImplCode(id), CompileBoxInterlibImpl(id, sub.lang), kPerm666);
          break;
        }
      }
      break;
    }
    case CompileSubtask::SPECJUDGE: [[fallthrough]];
    case CompileSubtask::SUMMARY: {
      fs::path src = SpecjudgeHeadersPath();
      fs::path box = Workdir(CompileBoxPath(id, subtask));
      for (auto& dir_entry : fs::recursive_directory_iterator(src)) {
        if (dir_entry.is_directory()) continue;
        fs::path p = dir_entry.path();
        const auto parent_dirs = box / fs::relative(p, src).parent_path();
        CreateDirs(parent_dirs, fs::perms::all);
        Copy(p, parent_dirs / p.filename(), kPerm666);
      }
      break;
    }
  }
  return true;
}

void FinalizeCompile(SubmissionAndResult& sub_and_result, const TaskEntry& task, const struct cjail_result& cjail_res) {
  const Submission& sub = sub_and_result.sub;
  SubmissionResult& sub_res = sub_and_result.result;

  constexpr size_t kMaxMsgLen = 4000;
  static const std::regex kFilterRegex(
      "(^|\\n)In file included from[\\S\\s]*?(\\n/workdir/prog|$)");
  static const std::string kFilterReplace = "$1[Error messages from headers removed]$2";

  long id = sub.submission_internal_id;
  CompileSubtask subtask = (CompileSubtask)task.task.subtask;
  auto lang = GetLang(sub, subtask);
  if (cjail_res.timekill == -1) {
    sub_res.verdict = Verdict::JE;
  } else if (cjail_res.timekill || cjail_res.oomkill > 0 || cjail_res.info.si_status != 0 ||
      !fs::is_regular_file(CompileBoxOutput(id, subtask, lang))) {
    Verdict verd;
    if (subtask != CompileSubtask::USERPROG) {
      verd = Verdict::ER;
    } else if (cjail_res.timekill || cjail_res.oomkill > 0) {
      verd = Verdict::CLE;
    } else {
      verd = Verdict::CE;
    }
    sub_res.verdict = std::max(sub_res.verdict, verd);
    spdlog::info("Compilation failed: id={} subtask={} verdict={}",
                 id, CompileSubtaskName(subtask), VerdictToAbr(sub_res.verdict));

    fs::path path = CompileBoxMessage(id, subtask);
    std::string message;
    if (fs::is_regular_file(path)) {
      size_t total_length = fs::file_size(path);
      char buf[kMaxMsgLen];
      std::ifstream fin(path);
      fin.read(buf, sizeof(buf));
      message.assign(buf, fin.gcount());
      bool truncated = total_length > kMaxMsgLen;
      message = std::regex_replace(message, kFilterRegex, kFilterReplace);
      if (truncated) {
        message += "\n[Error message truncated after " + std::to_string(kMaxMsgLen) + " bytes]";
      }
      spdlog::debug("Message: {}", message);
    }
    switch (subtask) {
      case CompileSubtask::USERPROG: {
        sub_res.ce_message = std::move(message);
        if (sub.reporter.ReportCEMessage) sub.reporter.ReportCEMessage(sub, sub_res);
        break;
      }
      case CompileSubtask::SPECJUDGE: {
        sub_res.er_message = std::move(message);
        if (sub.reporter.ReportERMessage) sub.reporter.ReportERMessage(sub, sub_res);
        break;
      }
      case CompileSubtask::SUMMARY: {
        if (sub_res.er_message.empty()) sub_res.er_message = std::move(message);
        // do nothing
        break;
      }
    }
  } else {
    // success
    spdlog::info("Compilation successful: id={} subtask={}", id, CompileSubtaskName(subtask));
  }
}

bool SetupExecute(SubmissionAndResult& sub_and_result, const TaskEntry& task) {
  const Submission& sub = sub_and_result.sub;
  SubmissionResult& res = sub_and_result.result;

  if (res.verdict != Verdict::NUL) return false; // CE check
  long id = sub.submission_internal_id;
  int subtask = task.task.subtask;
  int stage = task.task.stage;
  if (IsCancelled(id, sub.testdata[subtask].td_groups)) return false; // cancellation check

  auto& td_result = res.td_results[subtask];
  if (stage > 0 && (td_result.skip_stage || td_result.verdict != Verdict::NUL)) return false;
  auto workdir = Workdir(ExecuteBoxPath(id, subtask, stage));
  CreateDirs(workdir);
  if (!sub.sandbox_strict) { // for non-strict: mount a tmpfs to limit overall filesize
    // TODO FEATURE(io-interactive): create FIFOs outside of workdir by hardlink
    long tmpfs_size_kib =
      (fs::file_size(CompileBoxOutput(id, CompileSubtask::USERPROG, sub.lang)) / 4096 + 1) * 4 +
      (fs::file_size(sub.testdata[subtask].input_file) / 4096 + 1) * 4 +
      std::min(sub.testdata[subtask].output * 2, kMaxOutput);
    MountTmpfs(workdir, tmpfs_size_kib);
  }
  auto prog = ExecuteBoxProgram(id, subtask, stage, sub.lang);
  Copy(CompileBoxOutput(id, CompileSubtask::USERPROG, sub.lang),
       prog, ExecuteBoxProgramPerm(sub.lang, sub.sandbox_strict));
  auto input_file = ExecuteBoxInput(id, subtask, stage, sub.sandbox_strict);
  if (sub.sandbox_strict) {
    CreateDirs(ExecuteBoxTdStrictPath(id, subtask, stage), fs::perms::owner_all); // 700
    if (stage == 0) {
      std::lock_guard lck(td_file_lock[sub.problem_id]);
      Copy(sub.testdata[subtask].input_file, input_file,
          fs::perms::owner_read | fs::perms::owner_write); // 600
    } else {
      Move(ExecuteBoxFinalOutput(id, subtask, stage - 1), input_file);
      fs::permissions(input_file, fs::perms::owner_read | fs::perms::owner_write); // 600
    }
  } else {
    fs::permissions(workdir, fs::perms::all);
    if (stage == 0) {
      std::lock_guard lck(td_file_lock[sub.problem_id]);
      Copy(sub.testdata[subtask].input_file, input_file, kPerm666);
    } else {
      Move(ExecuteBoxFinalOutput(id, subtask, stage - 1), input_file);
      fs::permissions(input_file, kPerm666);
    }
  }
  return true;
}

void FinalizeExecute(SubmissionAndResult& sub_and_result, const TaskEntry& task, const struct cjail_result& cjail_res) {
  const Submission& sub = sub_and_result.sub;
  SubmissionResult& sub_res = sub_and_result.result;

  long id = sub.submission_internal_id;
  int subtask = task.task.subtask;
  int stage = task.task.stage;
  if (subtask >= (int)sub_res.td_results.size()) sub_res.td_results.resize(subtask + 1);
  auto& td_result = sub_res.td_results[subtask];
  td_result.execute_result = cjail_res;
  Move(ExecuteBoxOutput(id, subtask, stage, sub.sandbox_strict),
       ExecuteBoxFinalOutput(id, subtask, stage));
  IGNORE_RETURN(chown(ExecuteBoxFinalOutput(id, subtask, stage).c_str(), 0, 0));
  {
    auto workdir = Workdir(ExecuteBoxPath(id, subtask, stage));
    if (!sub.sandbox_strict) Umount(workdir);
    RemoveAll(workdir);
  }
  if (stage > 0) {
    RemoveAll(ExecuteBoxPath(id, subtask, stage - 1));
  }
  auto& lim = sub.testdata[subtask];
  if (stage == 0) {
    td_result.vss = cjail_res.stats.hiwater_vm;
    td_result.rss = cjail_res.rus.ru_maxrss;
    td_result.time = ToUs(cjail_res.rus.ru_utime) + ToUs(cjail_res.rus.ru_stime);
    td_result.score = 0;
  } else {
    td_result.vss = std::max(td_result.vss, (int64_t)cjail_res.stats.hiwater_vm);
    td_result.rss = std::max(td_result.rss, (int64_t)cjail_res.rus.ru_maxrss);
    td_result.time += ToUs(cjail_res.rus.ru_utime) + ToUs(cjail_res.rus.ru_stime);
  }
  if (cjail_res.timekill == -1) {
    // timekill = -1 means SandboxExec error (see sandbox_exec.cpp, sandbox_main.cpp)
    td_result.verdict = Verdict::EE;
  } else if (cjail_res.oomkill > 0) {
    // oomkill = -1 means failed to read oom (see cjail/cjail.h)
    td_result.verdict = Verdict::MLE;
  } else if (cjail_res.timekill) {
    td_result.verdict = Verdict::TLE;
  } else if (cjail_res.info.si_code == CLD_KILLED || cjail_res.info.si_code == CLD_DUMPED) {
    if (cjail_res.info.si_status == SIGXFSZ || (sub.sandbox_strict && cjail_res.info.si_status == SIGPIPE)) {
      td_result.verdict = Verdict::OLE;
    } else if ((lim.vss && td_result.vss > lim.vss) || (lim.rss && td_result.rss > lim.rss)) {
      // MLE will likely cause SIGSEGV or std::bad_alloc (SIGABRT), so we check it before SIG
      td_result.verdict = Verdict::MLE;
    } else {
      td_result.verdict = Verdict::SIG;
    }
  } else if ((lim.vss && td_result.vss > lim.vss) || (lim.rss && td_result.rss > lim.rss)) {
    td_result.verdict = Verdict::MLE;
  } else if (cjail_res.info.si_status != 0) {
    td_result.verdict = Verdict::RE;
  } else if (td_result.time > lim.time) {
    td_result.verdict = Verdict::TLE;
  } else {
    td_result.verdict = Verdict::NUL;
  }
  spdlog::info("Execute finished: id={} subtask={} stage={} code={} status={} verdict={} time={} vss={} rss={}",
               id, subtask, stage, cjail_res.info.si_code, cjail_res.info.si_status, VerdictToAbr(td_result.verdict),
               td_result.time, td_result.vss, td_result.rss);
}

void FinalizeScoring(SubmissionAndResult& sub_and_result, const TaskEntry& task,
                     const struct cjail_result& res, bool skipped = false);

bool SetupScoring(SubmissionAndResult& sub_and_result, const TaskEntry& task) {
  const Submission& sub = sub_and_result.sub;
  SubmissionResult& res = sub_and_result.result;

  if (res.verdict != Verdict::NUL) return false; // CE check
  long id = sub.submission_internal_id;
  int subtask = task.task.subtask;
  int stage = task.task.stage;
  if (IsCancelled(id, sub.testdata[subtask].td_groups)) return false; // cancellation check

  auto& td_result = res.td_results[subtask];
  // if specjudge demends skip, skip anyway
  if (td_result.skip_stage) return false;
  // if already TLE/MLE/etc, do not invoke old-style special judge
  if (td_result.verdict != Verdict::NUL &&
      sub.specjudge_type != SpecjudgeType::SPECJUDGE_NEW) {
    FinalizeScoring(sub_and_result, task, {}, true);
    return false;
  }
  CreateDirs(Workdir(ScoringBoxPath(id, subtask, stage)), fs::perms::all);
  // special judge program
  fs::path specjudge_prog = sub.specjudge_type == SpecjudgeType::NORMAL ?
      DefaultScoringPath() : CompileBoxOutput(id, CompileSubtask::SPECJUDGE, sub.specjudge_lang);
  Copy(specjudge_prog, ScoringBoxProgram(id, subtask, stage, sub.specjudge_lang), fs::perms::all);
  // user code
  Copy(SubmissionUserCode(id), ScoringBoxUserCode(id, subtask, stage, sub.lang), kPerm666);
  { // user output
    auto user_output = ExecuteBoxFinalOutput(id, subtask, stage);
    if (sub.specjudge_type == SpecjudgeType::SKIP) {
      Move(user_output, ScoringBoxOutput(id, subtask, stage));
      FinalizeScoring(sub_and_result, task, {});
      return false;
    }
    auto scoring_user_output = ScoringBoxUserOutput(id, subtask, stage);
    if (fs::exists(user_output)) {
      Move(user_output, scoring_user_output, kPerm666);
    } else {
      // touch file if not exist (if multistage skipped)
      std::ofstream(scoring_user_output).close();
      fs::permissions(scoring_user_output, kPerm666);
    }
  }
  { // input and answer
    std::lock_guard lck(td_file_lock[sub.problem_id]);
    Copy(sub.testdata[subtask].input_file, ScoringBoxTdInput(id, subtask, stage), kPerm666);
    Copy(sub.testdata[subtask].answer_file, ScoringBoxTdOutput(id, subtask, stage), kPerm666);
  }
  { // write meta file
    std::ofstream fout(ScoringBoxMetaFile(id, subtask, stage));
    fout << sub_and_result.TestdataMeta(subtask, stage).dump(-1, ' ', false, nlohmann::json::error_handler_t::ignore);
  }
  return true;
}

void ReadOldSpecjudgeResult(const fs::path& output_path, bool last_stage, SubmissionResult::TestdataResult& td_result) {
  int x;
  std::ifstream fin(output_path);
  bool success = bool(fin >> x); // x would be set to 0 if failed, so this is necessary
  if (success && x == 0) {
    if (last_stage) {
      td_result.verdict = Verdict::AC;
      td_result.score = 100'000'000;
    } // else: continue (NUL)
    std::string cmd;
    bool score_overriden = false;
    while (fin >> cmd) {
      if (cmd == "SPECJUDGE_OVERRIDE_SCORE") {
        long double score;
        if (fin >> score) {
          td_result.score = NormalizeScore(score);
          score_overriden = true;
        }
      } else if (cmd == "SPECJUDGE_OVERRIDE_VERDICT") {
        if (fin >> cmd) {
          td_result.verdict = AbrToVerdict(cmd, true);
          if (!score_overriden && td_result.verdict == Verdict::AC) td_result.score = 100'000'000;
        }
      } else {
        break;
      }
    }
  } else {
    td_result.verdict = Verdict::WA;
  }
}

void ReadNewSpecjudgeResult(const fs::path& output_path, SubmissionResult::TestdataResult& td_result) {
  nlohmann::json json;
  {
    std::ifstream fin(output_path);
    fin >> json;
  }
  if (auto it = json.find("verdict"); it != json.end() && it->is_string()) {
    td_result.verdict = AbrToVerdict(it->get<std::string>(), true);
    if (td_result.verdict == Verdict::AC) td_result.score = 100'000'000;
  } // else: WA
  if (auto it = json.find("score"); it != json.end()) {
    try {
      if (it->is_string()) {
        td_result.score = NormalizeScore(std::stold(it->get<std::string>()));
      } else if (it->is_number()) {
        td_result.score = NormalizeScore(it->get<long double>());
      }
    } catch (...) {}
  }
  auto ReadNumber = [&](const char* attr, long& target) {
    if (auto it = json.find(attr); it != json.end() && it->is_number()) {
      try {
        target = it->get<long>();
      } catch (...) {}
    }
  };
  ReadNumber("time_us", td_result.time);
  ReadNumber("vss_kib", td_result.vss);
  ReadNumber("rss_kib", td_result.rss);
  bool has_message = false;
  if (auto it_type = json.find("message_type"), it_msg = json.find("message");
      it_type != json.end() && it_msg != json.end() &&
      it_type->is_string() && it_msg->is_string()) {
    try {
      auto& msg_type = it_type->get_ref<std::string&>();
      if (msg_type == "text" || msg_type == "html") {
        constexpr size_t kMaxLen = 32768;
        td_result.message_type = std::move(msg_type);
        td_result.message = std::move(it_msg->get_ref<std::string&>());
        if (td_result.message.size() > kMaxLen) td_result.message.resize(kMaxLen);
        has_message = true;
      }
    } catch (...) {}
  }
  if (!has_message) td_result.message_type = td_result.message = "";
}

void FinalizeScoring(SubmissionAndResult& sub_and_result, const TaskEntry& task,
                     const struct cjail_result& cjail_res, bool skipped) {
  const Submission& sub = sub_and_result.sub;
  SubmissionResult& sub_res = sub_and_result.result;

  long id = sub.submission_internal_id;
  int subtask = task.task.subtask;
  int stage = task.task.stage;
  auto& td_result = sub_res.td_results[subtask];
  auto output_path = ScoringBoxOutput(id, subtask, stage);

  bool last_stage = stage == sub.stages - 1;
  // default: WA or keep verdict (if TLE etc.) for last_stage,
  //          continue otherwise
  td_result.score = 0;
  if (skipped) {
    // skipped because of TLE/MLE/etc in old-style; do nothing
  } else if (!fs::is_regular_file(output_path) || cjail_res.info.si_status != 0) {
    // skip remaining stages
    if (td_result.verdict == Verdict::NUL) td_result.verdict = Verdict::WA;
  } else if (sub.specjudge_type == SpecjudgeType::SPECJUDGE_OLD) {
    ReadOldSpecjudgeResult(output_path, last_stage, td_result);
  } else {
    try {
      ReadNewSpecjudgeResult(output_path, td_result);
    } catch (nlohmann::json::exception&) {
      // WA
    }
  }
  if (!last_stage && td_result.verdict != Verdict::NUL) {
    td_result.skip_stage = true;
  } else if (last_stage && td_result.verdict == Verdict::NUL) {
    td_result.verdict = Verdict::WA;
  }
  if (sub.skip_group && td_result.verdict != Verdict::NUL && td_result.verdict != Verdict::AC) {
    const auto& groups = sub.testdata[subtask].td_groups;
    cancelled_group[id].insert(groups.begin(), groups.end());
  }

  // move possibly modified user output back to original position
  // so that it can be read by the next stage
  if (!last_stage && !skipped && !td_result.skip_stage && sub.specjudge_type != SpecjudgeType::SKIP) {
    Move(ScoringBoxUserOutput(id, subtask, stage), ExecuteBoxFinalOutput(id, subtask, stage));
  }

  // remove testdata-related files
  if (last_stage) RemoveAll(ExecuteBoxPath(id, subtask, sub.stages - 1));
  if (!skipped) RemoveAll(ScoringBoxPath(id, subtask, stage));
  if (!cancelled_list.count(id)) {
    spdlog::info("Scoring {}: id={} subtask={} verdict={} score={} time={} vss={} rss={}",
                 skipped ? "skipped" : "finished", id, subtask, VerdictToAbr(td_result.verdict),
                 td_result.score, td_result.time, td_result.vss, td_result.rss);
    if (sub.reporter.ReportScoringResult &&
        (sub.report_intermediate_stage || last_stage || td_result.skip_stage)) {
      sub.reporter.ReportScoringResult(sub, sub_res, subtask, stage);
    }
  }
}

bool SetupSummary(SubmissionAndResult& sub_and_result, const TaskEntry& task) {
  const Submission& sub = sub_and_result.sub;
  SubmissionResult& res = sub_and_result.result;

  if (res.verdict == Verdict::NUL) { // calculate verdict first
    res.verdict = Verdict::AC;
    for (size_t i = 0; i < res.td_results.size(); i++) {
      if (sub.testdata[i].ignore_verdict) continue;
      auto& td = res.td_results[i];
      if ((int)res.verdict < (int)td.verdict) res.verdict = td.verdict;
    }
  }
  if (sub.summary_type == SummaryType::NONE || res.verdict == Verdict::ER) return false;

  long id = sub.submission_internal_id;
  CreateDirs(Workdir(SummaryBoxPath(id)), fs::perms::all);
  Move(CompileBoxOutput(id, CompileSubtask::SUMMARY, sub.summary_lang),
       SummaryBoxProgram(id, sub.summary_lang), fs::perms::all);
  Copy(SubmissionUserCode(id), SummaryBoxUserCode(id, sub.lang), kPerm666);
  if (fs::path ce_message_src = CompileBoxMessage(id, CompileSubtask::USERPROG);
      fs::is_regular_file(ce_message_src)) {
    Move(ce_message_src, SummaryBoxCEMessage(id), kPerm666);
  } else {
    // touch file
    std::ofstream(SummaryBoxCEMessage(id)).close();
    fs::permissions(SummaryBoxCEMessage(id), kPerm666);
  }
  { // write meta file
    std::ofstream fout(SummaryBoxMetaFile(id));
    fout << sub_and_result.SummaryMeta().dump(-1, ' ', false, nlohmann::json::error_handler_t::ignore);
  }
  return true;
}

void ReadSummaryResult(const fs::path& output_path, SubmissionResult& res) {
  nlohmann::json json;
  {
    std::ifstream fin(output_path);
    fin >> json;
  }
  if (auto it = json.find("verdict"); it != json.end() && it->is_string()) {
    res.verdict = AbrToVerdict(it->get<std::string>(), false);
  }
  if (res.verdict == Verdict::NUL) res.verdict = Verdict::WA;
  if (auto it = json.find("score"); it != json.end()) {
    try {
      if (it->is_string()) {
        res.total_score = NormalizeScore(std::stold(it->get<std::string>()));
      } else if (it->is_number()) {
        res.total_score = NormalizeScore(it->get<long double>());
      }
    } catch (...) {}
  }
  auto ReadNumber = [&](const char* attr, long& target) {
    if (auto it = json.find(attr); it != json.end() && it->is_number()) {
      try {
        target = it->get<long>();
      } catch (...) {}
    }
  };
  ReadNumber("total_time_us", res.total_time);
  ReadNumber("total_memory_kib", res.total_memory);
  if (auto it = json.find("ce_message"); it != json.end() && it->is_string()) {
    res.ce_message = it->get<std::string>();
  }
}

/// Submission tasks env teardown
void FinalizeSummary(SubmissionAndResult& sub_and_result, const TaskEntry& task, const struct cjail_result& cjail_res, bool skipped) {
  const Submission& sub = sub_and_result.sub;
  SubmissionResult& res = sub_and_result.result;
  long id = sub.submission_internal_id;

  res.total_score = 0;
  if ((int)res.verdict < (int)Verdict::CE) {
    constexpr int64_t kInfScore = 2'000'000'000'000L;
    res.total_memory = 0;
    res.total_time = 0;
    std::vector<int64_t> subtask_scores(sub.group_score.size(), kInfScore);
    for (size_t i = 0; i < res.td_results.size(); i++) {
      if (sub.testdata[i].ignore_verdict) continue;
      auto& td = res.td_results[i];
      res.total_memory = std::max(res.total_memory, td.rss);
      res.total_time += td.time;
      for (int group : sub.testdata[i].td_groups) {
        subtask_scores[group] = std::min(subtask_scores[group], td.score);
      }
    }
    __int128_t total_score = 0;
    for (size_t i = 0; i < sub.group_score.size(); i++) {
      int64_t score = subtask_scores[i];
      if (score == kInfScore) score = 100'000'000;
      total_score += (__int128_t)score * sub.group_score[i];
    }
    res.total_score = NormalizeScore((long double)total_score / (100'000'000LL * 1'000'000));
  }
  if (!skipped) {
    auto output_path = SummaryBoxOutput(id);
    if (!fs::is_regular_file(output_path) || cjail_res.info.si_status != 0) {
      res.verdict = Verdict::WA;
      res.total_memory = 0;
      res.total_time = 0;
    } else {
      try {
        ReadSummaryResult(output_path, res);
      } catch (nlohmann::json::exception&) {
        res.verdict = Verdict::WA;
        res.total_memory = 0;
        res.total_time = 0;
      }
    }
  }
  if (sub.remove_submission) RemoveAll(SubmissionCodePath(id));
  RemoveAll(SubmissionRunPath(id));
  if (auto it = cancelled_list.find(id); it != cancelled_list.end()) {
    // cancelled, don't send anything to server
    cancelled_list.erase(it);
  } else {
    if (sub.reporter.ReportOverallResult) sub.reporter.ReportOverallResult(sub, res);
  }
  cancelled_group.erase(id);
  spdlog::info("Submission finished: id={} sub_id={} list_size={}", id, sub.submission_id, submission_list.size());
  if (sub.reporter.ReportFinalized) sub.reporter.ReportFinalized(sub, res, submission_list.size());
  submission_id_map.erase(id);
  submission_list.erase(id);
}

// Call corresponding Finalize if not skipped & pop task from queue
void FinalizeTask(long id, const struct cjail_result& res, bool skipped = false) {
  auto& entry = task_list[id];
  spdlog::info("Finalizing task: id={} taskid={} tasktype={} subtask={} stage={} skipped={}",
               entry.submission_internal_id, id, TaskTypeName(entry.task.type), entry.task.subtask, entry.task.stage, skipped);
  if (!skipped || entry.task.type == TaskType::SUMMARY) {
    auto& sub = submission_list.at(entry.submission_internal_id);
    switch (entry.task.type) {
      case TaskType::COMPILE: FinalizeCompile(sub, entry, res); break;
      case TaskType::EXECUTE: FinalizeExecute(sub, entry, res); break;
      case TaskType::SCORING: FinalizeScoring(sub, entry, res); break;
      case TaskType::SUMMARY: FinalizeSummary(sub, entry, res, skipped); break;
    }
  }
  Remove(entry);
}

bool DispatchTask(long id) {
  auto& entry = task_list[id];
  auto& sub = submission_list.at(entry.submission_internal_id);
  spdlog::info("Dispatching task: id={} taskid={} tasktype={} subtask={} stage={}",
               entry.submission_internal_id, id, TaskTypeName(entry.task.type), entry.task.subtask, entry.task.stage);
  bool res = false;
  switch (entry.task.type) {
    case TaskType::COMPILE: res = SetupCompile(sub, entry); break;
    case TaskType::EXECUTE: res = SetupExecute(sub, entry); break;
    case TaskType::SCORING: res = SetupScoring(sub, entry); break;
    case TaskType::SUMMARY: res = SetupSummary(sub, entry); break;
  }
  if (!res) {
    FinalizeTask(id, {}, true);
    return false;
  }
  int handle = RunTask(submission_list.at(entry.submission_internal_id), entry.task);
  handle_map[handle] = id;
  return true;
}

std::pair<long, struct cjail_result> WaitTask() {
  std::pair<int, struct cjail_result> res = WaitAnyResult();
  auto it = handle_map.find(res.first);
  long ret = it->second;
  handle_map.erase(it);
  return {ret, res.second};
}

} // namespace

void WorkLoop(bool loop) {
  umask(0022);
  std::unique_lock lck(task_mtx);
  do {
    // no task running here
    task_cv.wait(lck, []{ return !task_queue.empty(); });
    int task_running = 0;
    while (task_running || !task_queue.empty()) {
      if (task_running < kMaxParallel && task_queue.size()) {
        long tid = task_queue.top();
        task_queue.pop();
        task_running += DispatchTask(tid);
        // if this is a finalize task or a skipped stage (such as execute/scoring stage of a CE submission),
        //  it will finish & finalize immediately without adding any running task
      } else {
        lck.unlock();
        std::pair<long, struct cjail_result> tid = WaitTask();
        lck.lock();
        FinalizeTask(tid.first, tid.second);
        task_running--;
      }
    }
  } while (loop);
}

size_t CurrentSubmissionQueueSize() {
  std::lock_guard lck(task_mtx);
  return submission_list.size();
}

std::vector<int> GetQueuedSubmissionID() {
  std::vector<int> st;
  {
    std::lock_guard lck(task_mtx);
    for (auto& i : submission_list) {
      if (cancelled_list.count(i.first)) continue;
      st.push_back(i.second.sub.submission_id);
    }
  }
  return st;
}

bool PushSubmission(Submission&& sub, size_t max_queue) {
  // Build this dependency graph (for 2 tds, 2 stages, judge between stages):
  //    compile_sj --------------+--------+
  //                             |        |                       compile_summary ---+
  //                             |        v                                          |
  // compile ---+-> execute_0_0 ---> scoring_0_0---> execute_0_1 ---> scoring_0_1 ---+--->  summary
  //            |                |                                                   |
  //            +-> execute_1_0 -+-> scoring_1_0---> execute_1_1 -+-> scoring_1_1 ---+
  //
  std::unique_lock lck(task_mtx);
  if (max_queue > 0 && submission_list.size() >= max_queue) return false;

  sub.stages = std::min(std::max(1, sub.stages), 32);
  int id = sub.submission_internal_id;
  long priority = sub.priority;
  int num_tds = sub.testdata.size();
  std::vector<int> td_order(num_tds);
  if (sub.skip_group) {
    // A simple greedy heuristic to try to skip a group as soon as possible
    //  expected O(T lg G), where T is the total td-group relationships and
    //  G is the number of layers (G <= N where N is the number of testdatum)
    // Partition all testdata into multiple "layers" by inserting a testdata
    //  the first layer that does not contain all the groups that testdata
    //  belongs to. That is, it tries to fill each layer with as many groups
    //  as possible before proceeding to the next layer.
    std::vector<int> td_layer(num_tds);
    {
      std::vector<int> scan_order(num_tds);
      for (int i = 0; i < num_tds; i++) scan_order[i] = i;
      // Process testdata with more groups first
      std::stable_sort(
          scan_order.begin(), scan_order.end(),
          [&sub](int a, int b) { return sub.testdata[a].td_groups.size() > sub.testdata[b].td_groups.size(); });
      std::vector<std::unordered_set<int>> group_layers;
      for (int td : scan_order) {
        auto& td_group = sub.testdata[td].td_groups;
        size_t pos = std::lower_bound(
            group_layers.begin(), group_layers.end(), true,
            [&td_group](const auto& layer, bool _) {
              return std::all_of(
                  td_group.begin(), td_group.end(),
                  [&layer](int group) { return layer.count(group); });
            }) - group_layers.begin();
        if (pos == group_layers.size()) group_layers.emplace_back();
        group_layers[pos].insert(td_group.begin(), td_group.end());
        td_layer[td] = pos;
      }
    }
    std::vector<int> inv_order(num_tds);
    for (int i = 0; i < num_tds; i++) inv_order[i] = i;
    std::stable_sort(inv_order.begin(), inv_order.end(),
                     [&](int a, int b) { return td_layer[a] < td_layer[b]; });
    for (int i = 0; i < num_tds; i++) td_order[inv_order[i]] = i;
  } else {
    for (int i = 0; i < num_tds; i++) td_order[i] = i;
  }
  std::vector<std::vector<TaskEntry>> executes, scorings;
  TaskEntry summary(id, {TaskType::SUMMARY, 0, 0}, priority);
  for (int i = 0; i < num_tds; i++) {
    executes.emplace_back(); // index i
    scorings.emplace_back();
    if (sub.judge_between_stages) {
      for (int j = 0; j < sub.stages; j++) {
        executes[i].emplace_back(id, (Task){TaskType::EXECUTE, i, j}, priority, td_order[i]);
        scorings[i].emplace_back(id, (Task){TaskType::SCORING, i, j}, priority, td_order[i]);
        Link(executes[i][j], scorings[i][j]);
        if (j > 0) Link(scorings[i][j - 1], executes[i][j]);
      }
    } else {
      for (int j = 0; j < sub.stages; j++) {
        executes[i].emplace_back(id, (Task){TaskType::EXECUTE, i, j}, priority, td_order[i]);
        if (j > 0) Link(executes[i][j - 1], executes[i][j]);
      }
      scorings[i].emplace_back(id, (Task){TaskType::SCORING, i, sub.stages - 1}, priority, td_order[i]);
      Link(executes[i].back(), scorings[i].back());
    }
    Link(scorings[i].back(), summary);
  }
  {
    TaskEntry compile(id, {TaskType::COMPILE, (int)CompileSubtask::USERPROG}, priority);
    for (auto& i : executes) Link(compile, i[0]);
    if (executes.empty()) Link(compile, summary);
    InsertTaskList(std::move(compile));
  }
  if (sub.specjudge_type == SpecjudgeType::SPECJUDGE_OLD ||
      sub.specjudge_type == SpecjudgeType::SPECJUDGE_NEW) {
    TaskEntry compile(id, {TaskType::COMPILE, (int)CompileSubtask::SPECJUDGE}, priority);
    for (auto& i : scorings) Link(compile, i[0]);
    if (executes.empty()) Link(compile, summary);
    InsertTaskList(std::move(compile));
  }
  if (sub.summary_type == SummaryType::CUSTOM) {
    TaskEntry compile(id, {TaskType::COMPILE, (int)CompileSubtask::SUMMARY}, priority);
    Link(compile, summary);
    InsertTaskList(std::move(compile));
  }
  for (auto& i : executes) for (auto& j : i) InsertTaskList(std::move(j));
  for (auto& i : scorings) for (auto& j : i) InsertTaskList(std::move(j));
  InsertTaskList(std::move(summary));
  submission_list.insert({id, std::move(sub)});
  if (auto it = submission_id_map.insert({sub.submission_id, id}); !it.second) {
    // if the same submission is already judging, mark it as cancelled
    cancelled_list.insert(it.first->second);
    it.first->second = id;
  }
  spdlog::info("Submission enqueued: id={} sub_id={} prob_id={} list_size={}", id, sub.submission_id, sub.problem_id, submission_list.size());
  lck.unlock();
  task_cv.notify_one();
  return true;
}
