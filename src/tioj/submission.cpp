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
long kMaxRSS = 2 * 1024 * 1024; // 2G
long kMaxOutput = 1 * 1024 * 1024; // 1G

namespace {

inline long ToUs(const struct timeval& v) {
  return (long)v.tv_sec * 1'000'000 + v.tv_usec;
}

} // namespace

nlohmann::json Submission::TestdataMeta(int subtask) const {
  const struct cjail_result& res = td_results[subtask].execute_result;
  const TestdataLimit& lim = td_limits[subtask];
  return {
    {"input_file", ScoringBoxTdInput(-1, -1, true)},
    {"answer_file", ScoringBoxTdOutput(-1, -1, true)},
    {"user_output_file", ScoringBoxUserOutput(-1, -1, true)},
    {"user_code_file", ScoringBoxCode(-1, -1, lang, true)},
    {"problem_id", problem_id},
    {"submission_id", submission_id},
    {"submitter_id", submitter_id},
    {"submitter_name", submitter_name},
    {"submitter_nickname", submitter_nickname},
    {"submission_time", submission_time},
    {"compiler", CompilerName(lang)},
    {"testdata_index", subtask},
    {"limits", {
      {"time_us", lim.time},
      {"vss_kb", lim.vss},
      {"rss_kb", lim.rss},
      {"output_kb", lim.output},
    }},
    {"stats", {
      {"original_verdict", VerdictToAbr(td_results[subtask].verdict)},
      {"exit_code", res.info.si_status},
      {"real_us", ToUs(res.time)},
      {"user_us", ToUs(res.rus.ru_utime)},
      {"sys_us", ToUs(res.rus.ru_stime)},
      {"max_rss_kb", res.rus.ru_maxrss},
      {"max_vss_kb", res.stats.hiwater_vm},
    }},
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
  // TODO FEATURE(group): group_offset (the position in the same group)
  long priority;
  int indeg;
  std::vector<long> edges;

  TaskEntry() {}
  TaskEntry(int sub_id, const Task& task, long priority) :
      id(task_count++),
      submission_internal_id(sub_id),
      task(task), priority(priority),
      indeg(0) {}
  bool operator<(const TaskEntry& x) const {
    // TODO FEATURE(group): order by group_offset first
    return std::make_tuple(priority, -submission_internal_id, -task.subtask) <
           std::make_tuple(x.priority, -x.submission_internal_id, -x.task.subtask);
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
std::unordered_map<long, Submission> submission_list;

// cancelling related
std::unordered_map<long, long> submission_id_map; // submission id -> internal id
std::unordered_set<long> cancelled_list;
// TODO FEATURE(group): cancelled group

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


/// Task env setup
bool SetupCompile(const Submission& sub, const TaskEntry& task) {
  if (sub.reporter) sub.reporter->ReportStartCompiling(sub);
  long id = sub.submission_internal_id;
  CompileSubtask subtask = (CompileSubtask)task.task.subtask;
  CreateDirs(Workdir(CompileBoxPath(id, subtask)), fs::perms::all);
  switch (subtask) {
    case CompileSubtask::USERPROG: {
      Copy(SubmissionUserCode(id), CompileBoxInput(id, subtask, sub.lang), kPerm666);
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
    case CompileSubtask::SPECJUDGE: {
      Copy(SubmissionJudgeCode(id), CompileBoxInput(id, subtask, sub.specjudge_lang), kPerm666);
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

void FinalizeCompile(Submission& sub, const TaskEntry& task, const struct cjail_result& res) {
  constexpr size_t kMaxMsgLen = 4000;
  static const std::regex kFilterRegex(
      "(^|\\n)In file included from[\\S\\s]*?(\\n/workdir/prog|$)");
  static const std::string kFilterReplace = "$1[Error messages from headers removed]$2";

  long id = sub.submission_internal_id;
  CompileSubtask subtask = (CompileSubtask)task.task.subtask;
  if (res.timekill == -1) {
    sub.verdict = Verdict::JE;
  } else if (res.timekill || res.oomkill > 0 || res.info.si_status != 0 ||
      !fs::is_regular_file(CompileBoxOutput(id, subtask, sub.lang))) {
    if (res.timekill || res.oomkill > 0) {
      sub.verdict = subtask == CompileSubtask::USERPROG ? Verdict::CLE : Verdict::ER;
    } else {
      sub.verdict = subtask == CompileSubtask::USERPROG ? Verdict::CE : Verdict::ER;
    }
    spdlog::info("Compilation failed: id={} subtask={} verdict={}",
                 id, CompileSubtaskName(subtask), VerdictToAbr(sub.verdict));
    if (subtask == CompileSubtask::USERPROG) {
      fs::path path = CompileBoxMessage(id, subtask);
      if (fs::is_regular_file(path)) {
        char buf[kMaxMsgLen + 1];
        std::ifstream fin(path);
        fin.read(buf, sizeof(buf));
        sub.ce_message.assign(buf, fin.gcount());
        spdlog::debug("Message: {}", sub.ce_message);
        bool truncated = sub.ce_message.size() > kMaxMsgLen;
        if (truncated) {
          sub.ce_message.resize(kMaxMsgLen);
        }
        sub.ce_message = std::regex_replace(sub.ce_message, kFilterRegex, kFilterReplace);
        if (truncated) {
          sub.ce_message += "\n[Error message truncated after " +
              std::to_string(kMaxMsgLen) + " bytes]";
        }
      }
      if (sub.reporter) sub.reporter->ReportCEMessage(sub);
    } else if (spdlog::get_level() == spdlog::level::debug) {
      fs::path path = CompileBoxMessage(id, subtask);
      if (fs::is_regular_file(path)) {
        char buf[4096];
        std::ifstream fin(path);
        fin.read(buf, sizeof(buf));
        std::string str(buf, fin.gcount());
        spdlog::debug("Message: {}", str);
      }
    }
  } else {
    // success
    spdlog::info("Compilation successful: id={} subtask={}", id, CompileSubtaskName(subtask));
  }
}

bool SetupExecute(const Submission& sub, const TaskEntry& task) {
  if (sub.verdict != Verdict::NUL) return false; // CE check
  long id = sub.submission_internal_id;
  int subtask = task.task.subtask;
  if (cancelled_list.count(id)) return false; // cancellation check
  auto workdir = Workdir(ExecuteBoxPath(id, subtask));
  CreateDirs(workdir);
  if (!sub.sandbox_strict) { // for non-strict: mount a tmpfs to limit overall filesize
    long tmpfs_size_kib =
      (fs::file_size(CompileBoxOutput(id, CompileSubtask::USERPROG, sub.lang)) / 4096 + 1) * 4 +
      (fs::file_size(TdInput(sub.problem_id, subtask)) / 4096 + 1) * 4 +
      std::min(sub.td_limits[subtask].output * 2, kMaxOutput);
    MountTmpfs(workdir, tmpfs_size_kib);
  }
  auto prog = ExecuteBoxProgram(id, subtask, sub.lang);
  Copy(CompileBoxOutput(id, CompileSubtask::USERPROG, sub.lang),
       prog, ExecuteBoxProgramPerm(sub.lang, sub.sandbox_strict));
  if (sub.sandbox_strict) {
    CreateDirs(ExecuteBoxTdStrictPath(id, subtask), fs::perms::owner_all); // 700
    {
      std::lock_guard lck(td_file_lock[sub.problem_id]);
      Copy(TdInput(sub.problem_id, subtask), ExecuteBoxInput(id, subtask, sub.sandbox_strict),
          fs::perms::owner_read | fs::perms::owner_write); // 600
    }
    fs::permissions(prog, fs::perms::all & ~(fs::perms::group_write)); // 755
  } else {
    fs::permissions(workdir, fs::perms::all);
    {
      std::lock_guard lck(td_file_lock[sub.problem_id]);
      Copy(TdInput(sub.problem_id, subtask), ExecuteBoxInput(id, subtask, sub.sandbox_strict), kPerm666);
    }
    fs::permissions(prog, fs::perms::all); // 777
  }
  return true;
}

void FinalizeExecute(Submission& sub, const TaskEntry& task, const struct cjail_result& res) {
  long id = sub.submission_internal_id;
  int subtask = task.task.subtask;
  if (subtask >= (int)sub.td_results.size()) sub.td_results.resize(subtask + 1);
  auto& td_result = sub.td_results[subtask];
  td_result.execute_result = res;
  Move(ExecuteBoxOutput(id, subtask, sub.sandbox_strict),
       ExecuteBoxFinalOutput(id, subtask));
  IGNORE_RETURN(chown(ExecuteBoxFinalOutput(id, subtask).c_str(), 0, 0));
  if (!sub.sandbox_strict) {
    auto workdir = Workdir(ExecuteBoxPath(id, subtask));
    Umount(workdir);
  }
  auto& lim = sub.td_limits[subtask];
  td_result.vss = res.stats.hiwater_vm;
  td_result.rss = res.rus.ru_maxrss;
  td_result.time = ToUs(res.rus.ru_utime) + ToUs(res.rus.ru_stime);
  td_result.score = 0;
  if (res.timekill == -1) {
    // timekill = -1 means SandboxExec error (see sandbox_exec.cpp, sandbox_main.cpp)
    td_result.verdict = Verdict::EE;
  } else if (res.oomkill > 0) {
    // oomkill = -1 means failed to read oom (see cjail/cjail.h)
    td_result.verdict = Verdict::MLE;
  } else if (res.timekill) {
    td_result.verdict = Verdict::TLE;
  } else if (res.info.si_code == CLD_KILLED || res.info.si_code == CLD_DUMPED) {
    if (res.info.si_status == SIGXFSZ) {
      td_result.verdict = Verdict::OLE;
    } else if ((lim.vss && td_result.vss > lim.vss) || (lim.rss && td_result.rss > lim.rss)) {
      // MLE will likely cause SIGSEGV or std::bad_alloc (SIGABRT), so we check it here
      td_result.verdict = Verdict::MLE;
    } else {
      td_result.verdict = Verdict::SIG;
    }
  } else if ((lim.vss && td_result.vss > lim.vss) || (lim.rss && td_result.rss > lim.rss)) {
    td_result.verdict = Verdict::MLE;
  } else if (res.info.si_status != 0) {
    td_result.verdict = Verdict::RE;
  } else if (td_result.time > lim.time) {
    td_result.verdict = Verdict::TLE;
  } else {
    td_result.verdict = Verdict::NUL;
  }
  spdlog::info("Execute finished: id={} subtask={} code={} status={} verdict={} time={} vss={} rss={}",
               id, subtask, res.info.si_code, res.info.si_status, VerdictToAbr(td_result.verdict),
               td_result.time, td_result.vss, td_result.rss);
}

bool SetupScoring(const Submission& sub, const TaskEntry& task) {
  if (sub.verdict != Verdict::NUL) return false; // CE check
  long id = sub.submission_internal_id;
  int subtask = task.task.subtask;
  if (cancelled_list.count(id)) return false; // cancellation check
  // if already TLE/MLE/etc, do not invoke old-style special judge
  if (sub.td_results[subtask].verdict != Verdict::NUL &&
      sub.specjudge_type != SpecjudgeType::SPECJUDGE_NEW) {
    if (sub.reporter) sub.reporter->ReportScoringResult(sub, subtask);
    return false;
  }
  CreateDirs(Workdir(ScoringBoxPath(id, subtask)), fs::perms::all);
  Move(ExecuteBoxFinalOutput(id, subtask),
       ScoringBoxUserOutput(id, subtask), kPerm666);
  {
    std::lock_guard lck(td_file_lock[sub.problem_id]);
    Copy(TdInput(sub.problem_id, subtask), ScoringBoxTdInput(id, subtask), kPerm666);
    Copy(TdOutput(sub.problem_id, subtask), ScoringBoxTdOutput(id, subtask), kPerm666);
  }
  Copy(SubmissionUserCode(id), ScoringBoxCode(id, subtask, sub.lang), kPerm666);
  // special judge program
  fs::path specjudge_prog = sub.specjudge_type == SpecjudgeType::NORMAL ?
      DefaultScoringPath() : CompileBoxOutput(id, CompileSubtask::SPECJUDGE, sub.specjudge_lang);
  Copy(specjudge_prog, ScoringBoxProgram(id, subtask, sub.specjudge_lang), fs::perms::all);
  { // write meta file
    std::ofstream fout(ScoringBoxMetaFile(id, subtask));
    fout << sub.TestdataMeta(subtask);
  }
  return true;
}

void FinalizeScoring(Submission& sub, const TaskEntry& task, const struct cjail_result& res) {
  long id = sub.submission_internal_id;
  int subtask = task.task.subtask;
  auto& td_result = sub.td_results[subtask];
  auto output_path = ScoringBoxOutput(id, subtask);

  td_result.verdict = Verdict::WA;
  td_result.score = 0;
  if (!fs::is_regular_file(output_path) || res.info.si_status != 0) {
    // WA
  } else if (sub.specjudge_type == SpecjudgeType::SPECJUDGE_OLD) {
    int x = 1;
    std::ifstream fin(output_path);
    if (fin >> x && x == 0) {
      td_result.verdict = Verdict::AC;
      td_result.score = 100'000'000;
    }
  } else {
    // parse judge result
    try {
      nlohmann::json json;
      {
        std::ifstream fin(output_path);
        fin >> json;
      }
      if (auto it = json.find("verdict"); it != json.end()) {
        if (it->is_string()) {
          td_result.verdict = AbrToVerdict(it->get<std::string>(), true);
          if (td_result.verdict == Verdict::NUL) td_result.verdict = Verdict::WA;
          if (td_result.verdict == Verdict::AC) td_result.score = 100'000'000;
        }
      }
      if (auto it = json.find("score"); it != json.end()) {
        long double score = td_result.score / 1'000'000;
        try {
          if (it->is_string()) {
            score = std::stold(it->get<std::string>());
          } else if (it->is_number()) {
            score = it->get<long double>();
          }
        } catch (...) {}
        if (score > 1e+6) score = 1e+6;
        if (score < -1e+6) score = -1e+6;
        td_result.score = (score * 1'000'000) + 0.5;
      }
      if (auto it = json.find("time_us"); it != json.end()) {
        try {
          if (it->is_number()) td_result.time = it->get<long>();
        } catch (...) {}
      }
      if (auto it = json.find("vss_kib"); it != json.end()) {
        try {
          if (it->is_number()) td_result.vss = it->get<long>();
        } catch (...) {}
      }
      if (auto it = json.find("rss_kib"); it != json.end()) {
        try {
          if (it->is_number()) td_result.rss = it->get<long>();
        } catch (...) {}
      }
    } catch (nlohmann::json::exception&) {
      // WA
    }
  }
  // remove testdata-related files
  std::error_code ec;
  RemoveAll(ExecuteBoxPath(id, subtask));
  RemoveAll(ScoringBoxPath(id, subtask));
  if (!cancelled_list.count(id)) {
    spdlog::info("Scoring finished: id={} subtask={} verdict={} score={} time={} vss={} rss={}",
                 id, subtask, VerdictToAbr(td_result.verdict),
                 td_result.score, td_result.time, td_result.vss, td_result.rss);
    if (sub.reporter) sub.reporter->ReportScoringResult(sub, subtask);
  }
}

/// Submission tasks env teardown
void FinalizeSubmission(Submission& sub, const TaskEntry& task) {
  long id = sub.submission_internal_id;
  if (sub.remove_submission) RemoveAll(SubmissionCodePath(id));
  RemoveAll(SubmissionRunPath(id));
  if (sub.td_results.size()) {
    sub.verdict = Verdict::AC;
    for (size_t i = 0; i < sub.td_results.size(); i++) {
      if (sub.td_limits[i].ignore_verdict) continue;
      auto& td = sub.td_results[i];
      if ((int)sub.verdict < (int)td.verdict) sub.verdict = td.verdict;
    }
  }
  if (auto it = cancelled_list.find(id); it != cancelled_list.end()) {
    // cancelled, don't send anything to server
    cancelled_list.erase(it);
  } else {
    spdlog::info("Submission finished: id={} sub_id={}", id, sub.submission_id);
    if (sub.reporter) sub.reporter->ReportOverallResult(sub);
  }
  submission_id_map.erase(id);
  submission_list.erase(id);
}

void FinalizeTask(long id, const struct cjail_result& res, bool skipped = false) {
  auto& entry = task_list[id];
  spdlog::info("Finalizing task: id={} taskid={} tasktype={} subtask={} skipped={}",
               entry.submission_internal_id, id, TaskTypeName(entry.task.type), entry.task.subtask, skipped);
  if (!skipped) {
    auto& sub = submission_list[entry.submission_internal_id];
    switch (entry.task.type) {
      case TaskType::COMPILE: FinalizeCompile(sub, entry, res); break;
      case TaskType::EXECUTE: FinalizeExecute(sub, entry, res); break;
      case TaskType::SCORING: FinalizeScoring(sub, entry, res); break;
      case TaskType::FINALIZE: __builtin_unreachable(); // skipped = true if FINALIZE
    }
  }
  Remove(entry);
}

bool DispatchTask(long id) {
  auto& entry = task_list[id];
  auto& sub = submission_list[entry.submission_internal_id];
  spdlog::info("Dispatching task: id={} taskid={} tasktype={} subtask={}",
               entry.submission_internal_id, id, TaskTypeName(entry.task.type), entry.task.subtask);
  bool res = false;
  switch (entry.task.type) {
    case TaskType::COMPILE: res = SetupCompile(sub, entry); break;
    case TaskType::EXECUTE: res = SetupExecute(sub, entry); break;
    case TaskType::SCORING: res = SetupScoring(sub, entry); break;
    case TaskType::FINALIZE: res = false; FinalizeSubmission(sub, entry); break;
  }
  if (!res) {
    FinalizeTask(id, {}, true);
    return false;
  }
  int handle = RunTask(submission_list[entry.submission_internal_id], entry.task);
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

bool PushSubmission(Submission&& sub, size_t max_queue) {
  // Build this dependency graph:
  //    compile_sj ------------+-----+
  //                           |     |
  //                           |     v
  // compile ---+-> execute_0 ---> scoring ---+---> finalize
  //            |              |              |
  //            +-> execute_n -+-> scoring ---+
  //
  std::unique_lock lck(task_mtx);
  if (max_queue > 0 && submission_list.size() >= max_queue) return false;

  int id = sub.submission_internal_id;
  long priority = sub.priority;
  std::vector<TaskEntry> executes, scorings;
  TaskEntry finalize(id, {TaskType::FINALIZE, 0}, priority);
  for (int i = 0; i < (int)sub.td_limits.size(); i++) {
    executes.emplace_back(id, (Task){TaskType::EXECUTE, i}, priority);
    scorings.emplace_back(id, (Task){TaskType::SCORING, i}, priority);
    Link(executes.back(), scorings.back());
    Link(scorings.back(), finalize);
  }
  {
    TaskEntry compile(id, {TaskType::COMPILE, (int)CompileSubtask::USERPROG}, priority);
    for (auto& i : executes) Link(compile, i);
    if (executes.empty()) Link(compile, finalize);
    InsertTaskList(std::move(compile));
  }
  if (sub.specjudge_type == SpecjudgeType::SPECJUDGE_OLD ||
      sub.specjudge_type == SpecjudgeType::SPECJUDGE_NEW) {
    TaskEntry compile(id, {TaskType::COMPILE, (int)CompileSubtask::SPECJUDGE}, priority);
    for (auto& i : scorings) Link(compile, i);
    if (executes.empty()) Link(compile, finalize);
    InsertTaskList(std::move(compile));
  }
  for (auto& i : executes) InsertTaskList(std::move(i));
  for (auto& i : scorings) InsertTaskList(std::move(i));
  InsertTaskList(std::move(finalize));
  submission_list.insert({id, std::move(sub)});
  if (auto it = submission_id_map.insert({sub.submission_id, id}); !it.second) {
    // if the same submission is already judging, mark it as cancelled
    cancelled_list.insert(it.first->second);
    it.first->second = id;
  }
  spdlog::info("Submission enqueued: id={} sub_id={} prob_id={}", id, sub.submission_id, sub.problem_id);
  lck.unlock();
  task_cv.notify_one();
  return true;
}
