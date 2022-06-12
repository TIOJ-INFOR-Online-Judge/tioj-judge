#include "testsuite.h"

#include <unistd.h>
#include <sys/stat.h>
#include <mutex>
#include <queue>
#include <fstream>
#include <unordered_set>
#include <unordered_map>
#include <condition_variable>

#include "tasks.h"
#include "utils.h"
#include "paths.h"

int max_parallel = 1;

namespace {

inline long ToUs(const struct timeval& v) {
  return (long)v.tv_sec * 1'000'000 + v.tv_usec;
}

} // namespace

nlohmann::json Submission::TestdataMeta(int subtask) const {
  const struct cjail_result& res = td_results[subtask].execute_result;
  const TestdataLimit& lim = td_limits[subtask];

  long id = submission_internal_id;
  fs::path box_path = ScoringBoxPath(id, subtask);

  return {
    {"input_file", InsideBox(box_path, ScoringBoxTdInput(id, subtask))},
    {"answer_file", InsideBox(box_path, ScoringBoxTdOutput(id, subtask))},
    {"user_output_file", InsideBox(box_path, ScoringBoxUserOutput(id, subtask))},
    {"user_code_file", InsideBox(box_path, ScoringBoxCode(id, subtask, lang))},
    {"problem_id", problem_id},
    {"submission_id", submission_id},
    {"submitter_id", submitter_id},
    {"submitter_name", submitter},
    {"submission_time", submission_time},
    {"compiler", CompilerName(lang)},
    {"limits", {
      {"time_us", lim.time},
      {"vss_kb", lim.vss},
      {"rss_kb", lim.rss},
      {"output_kb", lim.output},
    }},
    {"stats", {
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
  // TODO: group_offset (the position in the same group)
  long priority;
  int indeg;
  std::vector<long> edges;

  TaskEntry() {}
  TaskEntry(int sub_id, const Task& task, long priority) :
      id(task_count++),
      submission_internal_id(sub_id),
      task(task), priority(priority),
      indeg(0) {}
  bool operator>(const TaskEntry& x) const {
    // TODO: group
    return std::make_tuple(priority, -submission_internal_id, -task.subtask) >
           std::make_tuple(x.priority, -x.submission_internal_id, -x.task.subtask);
  }
};
long TaskEntry::task_count = 0;

std::mutex task_mtx;
std::condition_variable task_cv;
std::unordered_map<long, TaskEntry> task_list;

struct PriorityCompare {
  bool operator()(long a, long b) {
    return task_list[a] > task_list[b];
  }
};

std::priority_queue<long, std::vector<long>, PriorityCompare> task_queue;
std::unordered_map<int, long> handle_map;
std::unordered_map<long, Submission> submission_list;

// cancelling related
std::unordered_map<long, long> submission_id_map; // submission id -> internal id
std::unordered_set<long> cancelled_list;
// TODO: cancelled group

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
  long id = sub.submission_internal_id;
  CompileSubtask subtask = (CompileSubtask)task.task.subtask;
  CreateDirs(Workdir(CompileBoxPath(id, subtask)), fs::perms::all);
  switch (subtask) {
    case CompileSubtask::USERPROG: {
      Copy(SubmissionUserCode(id), CompileBoxInput(id, subtask, sub.lang),
          fs::perms::all);
      switch (sub.interlib_type) {
        case InterlibType::NONE: break;
        case InterlibType::INCLUDE: {
          Copy(SubmissionInterlibCode(id), CompileBoxInterlib(id, sub.problem_id),
              fs::perms::all);
        }
      }
      break;
    }
    case CompileSubtask::SPECJUDGE: {
      Copy(SubmissionJudgeCode(id), CompileBoxInput(id, subtask, Compiler::GCC_CPP_17),
          fs::perms::all);
      break;
    }
  }
  return true;
}

void FinalizeCompile(Submission& sub, const TaskEntry& task, const struct cjail_result& res) {
  long id = sub.submission_internal_id;
  CompileSubtask subtask = (CompileSubtask)task.task.subtask;
  if (res.timekill || res.oomkill) {
    sub.verdict = subtask == CompileSubtask::USERPROG ? Verdict::CLE : Verdict::ER;
    return;
  }
  if (res.info.si_status != 0 || !fs::is_regular_file(CompileBoxOutput(id, subtask, sub.lang))) {
    sub.verdict = subtask == CompileSubtask::USERPROG ? Verdict::CE : Verdict::ER;
    return;
  }
  // normal
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
      sub.td_limits[subtask].output * 2;
    MountTmpfs(workdir, tmpfs_size_kib);
  }
  auto prog = ExecuteBoxProgram(id, subtask, sub.lang);
  Copy(CompileBoxOutput(id, CompileSubtask::USERPROG, sub.lang),
       prog, ExecuteBoxProgramPerm(sub.lang, sub.sandbox_strict));
  chown(prog.c_str(), 0, 0);
  if (sub.sandbox_strict) {
    CreateDirs(ExecuteBoxTdStrictPath(id, subtask), fs::perms::owner_all); // 700
    Copy(TdInput(sub.problem_id, subtask), ExecuteBoxInput(id, subtask, true),
        fs::perms::owner_read | fs::perms::owner_write); // 600
    fs::permissions(prog, fs::perms::all & ~(fs::perms::group_write)); // 755
  } else {
    fs::permissions(workdir, fs::perms::all);
    Copy(TdInput(sub.problem_id, subtask), ExecuteBoxInput(id, subtask, true), kPerm666);
    fs::permissions(prog, fs::perms::all); // 777
  }
  return true;
}

void FinalizeExecute(Submission& sub, const TaskEntry& task, const struct cjail_result& res) {
  long id = sub.submission_internal_id;
  int subtask = task.task.subtask;
  auto& td_result = sub.td_results[subtask];
  td_result.execute_result = res;
  Move(ExecuteBoxOutput(id, subtask, sub.sandbox_strict),
       ExecuteBoxFinalOutput(id, subtask));
  if (!sub.sandbox_strict) {
    auto workdir = Workdir(ExecuteBoxPath(id, subtask));
    Umount(workdir);
  }
  auto& lim = sub.td_limits[subtask];
  td_result.vss = res.stats.hiwater_vm;
  td_result.rss = res.rus.ru_maxrss;
  td_result.time = ToUs(res.rus.ru_utime) + ToUs(res.rus.ru_utime);
  td_result.score = 0;
  if (res.oomkill) {
    td_result.verdict = Verdict::MLE;
  } else if (res.timekill) {
    td_result.verdict = Verdict::TLE;
  } else if (WIFSIGNALED(res.info.si_status)) {
    if (WTERMSIG(res.info.si_status) == SIGXFSZ) {
      td_result.verdict = Verdict::OLE;
    } else {
      td_result.verdict = Verdict::SIG;
    }
  } else if (res.info.si_status != 0) {
    td_result.verdict = Verdict::RE;
  } else if ((lim.vss && td_result.vss > lim.vss) || (lim.rss && td_result.rss > lim.rss)) {
    td_result.verdict = Verdict::MLE;
  } else if (td_result.time > lim.time) {
    td_result.verdict = Verdict::TLE;
  } else {
    td_result.verdict = Verdict::NUL;
  }
}

bool SetupScoring(const Submission& sub, const TaskEntry& task) {
  if (sub.verdict != Verdict::NUL) return false; // CE check
  long id = sub.submission_internal_id;
  int subtask = task.task.subtask;
  if (cancelled_list.count(id)) return false; // cancellation check
  // if already TLE/MLE/etc, do not invoke old-style special judge
  if (sub.td_results[subtask].verdict != Verdict::NUL &&
      sub.specjudge_type != SpecJudgeType::SPECJUDGE_NEW) return false;
  CreateDirs(Workdir(ScoringBoxPath(id, subtask)), fs::perms::all);
  Move(ExecuteBoxFinalOutput(id, subtask),
       ScoringBoxUserOutput(id, subtask), kPerm666);
  Copy(TdInput(sub.problem_id, subtask), ScoringBoxTdInput(id, subtask), kPerm666);
  Copy(TdOutput(sub.problem_id, subtask), ScoringBoxTdOutput(id, subtask), kPerm666);
  Copy(SubmissionUserCode(id), ScoringBoxCode(id, subtask, sub.lang), kPerm666);
  // special judge program
  fs::path specjudge_prog = sub.specjudge_type == SpecJudgeType::NORMAL ?
      ""/*TODO: spec judge executable*/ : CompileBoxOutput(id, CompileSubtask::SPECJUDGE, Compiler::GCC_CPP_17);
  Copy(specjudge_prog, ScoringBoxProgram(id, subtask, Compiler::GCC_CPP_17), fs::perms::all);
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
  } else if (sub.specjudge_type == SpecJudgeType::SPECJUDGE_OLD) {
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
    } catch (nlohmann::detail::parse_error&) {
      // WA
    }
  }
  // remove testdata-related files
  std::error_code ec;
  fs::remove_all(ExecuteBoxPath(id, subtask), ec);
  fs::remove_all(ScoringBoxPath(id, subtask), ec);
  if (!cancelled_list.count(id)) {
    // TODO: send message to server, asynchronously
  }
}

/// Submission tasks env teardown
void FinalizeSubmission(Submission& sub, const TaskEntry& task) {
  long id = sub.submission_internal_id;
  std::error_code ec;
  fs::remove_all(SubmissionCodePath(id), ec);
  fs::remove_all(SubmissionRunPath(id), ec);
  if (auto it = cancelled_list.find(id); it != cancelled_list.end()) {
    // cancelled, don't send anything to server
    cancelled_list.erase(it);
  } else {
    // TODO: send finalized message to server, asynchronously
  }
  submission_id_map.erase(id);
  submission_list.erase(id);
}

void FinalizeTask(long id, const struct cjail_result& res) {
  auto& entry = task_list[id];
  auto& sub = submission_list[entry.submission_internal_id];
  switch (entry.task.type) {
    case TaskType::COMPILE: FinalizeCompile(sub, entry, res); break;
    case TaskType::EXECUTE: FinalizeExecute(sub, entry, res); break;
    case TaskType::SCORING: FinalizeScoring(sub, entry, res); break;
    case TaskType::FINALIZE: FinalizeSubmission(sub, entry); break;
  }
  Remove(entry);
}

int DispatchTask(long id) {
  auto& entry = task_list[id];
  auto& sub = submission_list[entry.submission_internal_id];
  bool res = false;
  switch (entry.task.type) {
    // TODO: setup
    case TaskType::COMPILE: res = SetupCompile(sub, entry); break;
    case TaskType::EXECUTE: res = SetupExecute(sub, entry); break;
    case TaskType::SCORING: res = SetupScoring(sub, entry); break;
    case TaskType::FINALIZE: {
      FinalizeTask(id, {});
      return 0;
    }
  }
  if (!res) return 0;
  int handle = RunTask(submission_list[entry.submission_internal_id], entry.task);
  handle_map[handle] = id;
  return 1;
}

std::pair<long, struct cjail_result> WaitTask() {
  std::pair<int, struct cjail_result> res = WaitAnyResult();
  auto it = handle_map.find(res.first);
  long ret = it->second;
  handle_map.erase(it);
  return {ret, res.second};
}

} // namespace

void WorkLoop() {
  umask(0022);
  std::unique_lock lck(task_mtx);
  while (true) {
    // no task running here
    task_cv.wait(lck, []{ return !task_queue.empty(); });
    int task_running = 0;
    while (!task_queue.empty()) {
      if (task_running < max_parallel && task_queue.size()) {
        long tid = task_queue.top();
        task_queue.pop();
        task_running += DispatchTask(tid);
        // if this is a finalize task or a skipped stage (such as execute/scoring stage of a CE submission),
        //  it will finish immediately without adding any running task
      } else {
        lck.unlock();
        std::pair<long, struct cjail_result> tid = WaitTask();
        lck.lock();
        FinalizeTask(tid.first, tid.second);
        task_running--;
      }
    }
  }
}

void PushSubmission(Submission&& sub) {
  // Build this dependency graph:
  //    compile_sj ------------+-----+
  //                           |     |
  //                           |     v
  // compile ---+-> execute_0 ---> scoring ---+---> finalize
  //            |              |              |
  //            +-> execute_n -+-> scoring ---+
  //
  int sub_id = sub.submission_internal_id;
  // We use submissions time as priority; this can be adjusted if needed
  //   (e.g. put contest submissions before normal submissions
  long priority = -sub.submission_time;
  std::vector<TaskEntry> executes, scorings;
  TaskEntry finalize(sub_id, {TaskType::FINALIZE, 0}, priority);
  for (int i = 0; i < (int)sub.td_limits.size(); i++) {
    executes.emplace_back(sub_id, (Task){TaskType::EXECUTE, i}, priority);
    scorings.emplace_back(sub_id, (Task){TaskType::EXECUTE, i}, priority);
    Link(executes.back(), scorings.back());
    Link(scorings.back(), finalize);
  }
  std::lock_guard<std::mutex> lck(task_mtx);
  {
    TaskEntry compile(sub_id, {TaskType::COMPILE, (int)CompileSubtask::USERPROG}, priority);
    for (auto& i : executes) Link(compile, i);
    if (executes.empty()) Link(compile, finalize);
    InsertTaskList(std::move(compile));
  }
  if (sub.specjudge_type == SpecJudgeType::SPECJUDGE_OLD ||
      sub.specjudge_type == SpecJudgeType::SPECJUDGE_NEW) {
    TaskEntry compile(sub_id, {TaskType::COMPILE, (int)CompileSubtask::SPECJUDGE}, priority);
    for (auto& i : scorings) Link(compile, i);
    if (executes.empty()) Link(compile, finalize);
    InsertTaskList(std::move(compile));
  }
  for (auto& i : executes) InsertTaskList(std::move(i));
  for (auto& i : scorings) InsertTaskList(std::move(i));
  InsertTaskList(std::move(finalize));
  submission_list.insert({sub_id, std::move(sub)});
  if (auto it = submission_id_map.insert({sub.submission_id, sub_id}); !it.second) {
    // if the same submission is already judging, mark it as cancelled
    cancelled_list.insert(it.first->second);
    it.first->second = sub_id;
  }
}
