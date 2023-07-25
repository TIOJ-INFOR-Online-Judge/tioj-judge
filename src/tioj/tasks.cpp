#include "tasks.h"

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/wait.h>
#include <numeric>
#include <map>
#include <unordered_map>

#include <spdlog/spdlog.h>
#include "paths.h"
#include "utils.h"
#include "sandbox_exec.h"

namespace {

std::vector<std::string> GccCompileCommand(
    Compiler lang, const std::string& input, const std::string& interlib, const std::string& output, bool is_static) {
  std::string prog, std;
  switch (lang) {
    case Compiler::GCC_CPP_98: prog = "g++", std = "-std=c++98"; break;
    case Compiler::GCC_CPP_11: prog = "g++", std = "-std=c++11"; break;
    case Compiler::GCC_CPP_14: prog = "g++", std = "-std=c++14"; break;
    case Compiler::GCC_CPP_17: prog = "g++", std = "-std=c++17"; break;
    case Compiler::GCC_CPP_20: prog = "g++", std = "-std=c++20"; break;
    case Compiler::GCC_C_90: prog = "gcc", std = "-ansi"; break;
    case Compiler::GCC_C_99: prog = "gcc", std = "-std=c99"; break;
    case Compiler::GCC_C_11: prog = "gcc", std = "-std=c11"; break;
    case Compiler::GCC_C_17: prog = "gcc", std = "-std=c17"; break;
    default: __builtin_unreachable();
  }
  std::vector<std::string> ret = {"/usr/bin/env", prog, std, "-O2", "-w"};
  if (is_static) ret.push_back("-static");
  ret.insert(ret.end(), {"-o", output, input});
  if (!interlib.empty()) ret.push_back(interlib);
  if (prog == "gcc") ret.push_back("-lm");
  return ret;
}

std::vector<std::string> ExecuteCommand(Compiler lang, const std::string& program) {
  switch (lang) {
    case Compiler::GCC_CPP_98: [[fallthrough]];
    case Compiler::GCC_CPP_11: [[fallthrough]];
    case Compiler::GCC_CPP_14: [[fallthrough]];
    case Compiler::GCC_CPP_17: [[fallthrough]];
    case Compiler::GCC_CPP_20: [[fallthrough]];
    case Compiler::GCC_C_90: [[fallthrough]];
    case Compiler::GCC_C_99: [[fallthrough]];
    case Compiler::GCC_C_11: [[fallthrough]];
    case Compiler::GCC_C_17: [[fallthrough]];
    case Compiler::HASKELL: return {program};
    case Compiler::PYTHON2: return {"/usr/bin/env", "python2", program};
    case Compiler::PYTHON3: return {"/usr/bin/env", "python3", program};
  }
  __builtin_unreachable();
}

/// child
// Invoke sandbox with correct settings
// Results will be parsed in testsuite.cpp
struct cjail_result RunCompile(const Submission& sub, const Task& task, int uid, int cpuid) {
  long id = sub.submission_internal_id;
  CompileSubtask subtask = (CompileSubtask)task.subtask;
  spdlog::debug("Generating compile settings: id={} subid={}, subtask={}",
                id, sub.submission_id, CompileSubtaskName(subtask));

  std::string interlib;
  Compiler lang;
  switch (subtask) {
    case CompileSubtask::USERPROG: {
      lang = sub.lang;
      if (sub.interlib_type == InterlibType::INCLUDE) {
        interlib = CompileBoxInterlibImpl(-1, lang, true);
      }
      break;
    }
    case CompileSubtask::SPECJUDGE: lang = sub.specjudge_lang; break;
    default: __builtin_unreachable();
  }
  std::string input = CompileBoxInput(-1, subtask, lang, true);
  std::string output = CompileBoxOutput(-1, subtask, lang, true);

  SandboxOptions opt;
  opt.boxdir = CompileBoxPath(id, subtask);
  switch (lang) {
    case Compiler::GCC_CPP_98: [[fallthrough]];
    case Compiler::GCC_CPP_11: [[fallthrough]];
    case Compiler::GCC_CPP_14: [[fallthrough]];
    case Compiler::GCC_CPP_17: [[fallthrough]];
    case Compiler::GCC_CPP_20: [[fallthrough]];
    case Compiler::GCC_C_90: [[fallthrough]];
    case Compiler::GCC_C_99: [[fallthrough]];
    case Compiler::GCC_C_11: [[fallthrough]];
    case Compiler::GCC_C_17:
      opt.command = GccCompileCommand(lang, input, interlib, output, sub.sandbox_strict); break;
    case Compiler::HASKELL: {
      opt.command = {"/usr/bin/env", "ghc", "-w", "-O", "-tmpdir", ".", "-o", output, input};
      if (sub.sandbox_strict) {
        opt.command.insert(opt.command.end(), {"-static", "-optl-pthread", "-optl-static", "-optl-fuse-ld=bfd"});
      }
      break;
    }
    case Compiler::PYTHON2:
      opt.command = {"/usr/bin/env", "python2", "-m", "py_compile", input}; break;
    case Compiler::PYTHON3: {
      // note: we assume input & output name has no quotes here
      std::string script = ("import py_compile;py_compile.compile(\'\'\'" +
                            input + "\'\'\',\'\'\'" + output + "\'\'\')");
      opt.command = {"/usr/bin/env", "python3", "-c", script};
      break;
    }
    default: __builtin_unreachable();
  }
  if (char* path = getenv("PATH")) opt.envs.push_back(std::string("PATH=") + path);
  opt.workdir = Workdir("/");
  opt.fd_output = open(CompileBoxMessage(id, subtask).c_str(), O_WRONLY | O_APPEND | O_CREAT, 0666);
  opt.fd_error = opt.fd_output;
  if (cpuid != -1) opt.cpu_set.push_back(cpuid);
  opt.uid = opt.gid = uid;
  opt.wall_time = 60L * 1'000'000;
  opt.wall_time /= kTimeMultiplier;
  opt.rss = kMaxRSS;
  opt.proc_num = 10;
  opt.fsize = kMaxOutput;
  opt.dirs = {"/usr", "/var/lib", "/lib", "/lib64", "/etc/alternatives", "/bin"};
  opt.FilterDirs();
  return SandboxExec(opt);
  // we don't need to close the opened files because the process is about to terminate
}

// TODO FEATURE(io-interactive): fork & run multiple cjails and merge them into one cjail_result
struct cjail_result RunExecute(const Submission& sub, const Task& task, int uid, int cpuid) {
  long id = sub.submission_internal_id;
  int subtask = task.subtask;
  int stage = task.stage;
  spdlog::debug("Generating execute settings: id={} subid={}, subtask={} stage={}",
      id, sub.submission_id, subtask, stage);
  auto& lim = sub.testdata[subtask];
  std::string program = ExecuteBoxProgram(-1, -1, -1, sub.lang, true);

  SandboxOptions opt;
  opt.boxdir = ExecuteBoxPath(id, subtask, stage);
  opt.command = ExecuteCommand(sub.lang, program);
  if (sub.stages > 1) opt.command.push_back(std::to_string(stage));
  opt.workdir = Workdir("/");
  if (cpuid != -1) opt.cpu_set.push_back(cpuid);
  opt.uid = opt.gid = uid;
  long lim_time = lim.time;
  if (stage > 0) {
    lim_time -= sub.td_results[subtask].time;
  }
  if (lim_time < 0) lim_time = 0;
  opt.wall_time = std::max(long(lim_time * 1.2), lim_time + 1'000'000);
  opt.cpu_time = lim_time + 50'000; // a little bit of margin just in case
  opt.wall_time /= kTimeMultiplier;
  opt.cpu_time /= kTimeMultiplier;
  if (opt.cpu_time <= 0) opt.cpu_time = 1; // avoid being regarded as no limit
  opt.rss = lim.rss + 1024;
  if (lim.rss == 0 || opt.rss > kMaxRSS) opt.rss = kMaxRSS;
  opt.vss = lim.vss ? lim.vss + 2048 : 0; // add some margin so we can determine whether it is MLE
  opt.proc_num = 1;
  // file limit is not needed since we have already limited the total size by mounting tmpfs
  opt.fsize = lim.output;
  if (opt.fsize == 0 || opt.fsize > kMaxOutput) opt.fsize = kMaxOutput;
  if (sub.sandbox_strict) {
    if (sub.lang == Compiler::PYTHON2 || sub.lang == Compiler::PYTHON3) {
      // TODO: is it possible to run without /usr/bin and /bin? (maybe copy python executable to workdir)
      opt.dirs = {"/usr", "/lib", "/lib64", "/etc/alternatives", "/bin"};
    }
    int fd_input = open(ExecuteBoxInput(id, subtask, stage, sub.sandbox_strict).c_str(), O_RDONLY);
    int fd_output = open(ExecuteBoxOutput(id, subtask, stage, sub.sandbox_strict).c_str(), O_WRONLY | O_CREAT, 0600);
    int pipes[2][2];
    if (pipe(pipes[0]) < 0 || !SpliceProcess(fd_input, pipes[0][1]) || close(pipes[0][1]) < 0 ||
        pipe(pipes[1]) < 0 || !SpliceProcess(pipes[1][0], fd_output, opt.fsize * 1024) || close(pipes[1][0]) < 0) {
      // after returned, this process will terminate, closing the pipes and cause SpliceProcess' (if any) to exit
      goto err;
    }
    close(fd_input), close(fd_output);
    opt.fd_input = pipes[0][0];
    opt.fd_output = pipes[1][1];
    opt.error = "/dev/null";
  } else {
    opt.dirs = {"/usr", "/lib", "/lib64", "/etc/alternatives", "/bin"};
    opt.input = ExecuteBoxInput(-1, -1, -1, sub.sandbox_strict, true);
    opt.output = ExecuteBoxOutput(-1, -1, -1, sub.sandbox_strict, true);
    opt.error = ExecuteBoxError(-1, -1, -1, true);
    // Output file is accounted in cgroups, so we need to extend RSS limit
    // MLE check will still be done by the original limit
    opt.rss += opt.fsize;
  }
  opt.FilterDirs();
  // we don't need to close the opened files because the process is about to terminate
  return SandboxExec(opt);
 err:
  spdlog::warn("SandboxExec error: errno={} {}", errno, strerror(errno));
  struct cjail_result ret;
  ret.oomkill = errno;
  ret.timekill = -1;
  return ret;
}

struct cjail_result RunScoring(const Submission& sub, const Task& task, int uid, int cpuid) {
  long id = sub.submission_internal_id;
  int subtask = task.subtask;
  int stage = task.stage;
  spdlog::debug("Generating scoring settings: id={} subid={}, subtask={}", id, sub.submission_id, task.subtask);
  std::string program = ScoringBoxProgram(-1, -1, -1, sub.specjudge_lang, true);

  SandboxOptions opt;
  opt.boxdir = ScoringBoxPath(id, subtask, stage);
  opt.command = ExecuteCommand(sub.specjudge_lang, program);
  if (sub.specjudge_type == SpecjudgeType::SPECJUDGE_OLD) {
    opt.command.insert(opt.command.end(), {
      ScoringBoxUserOutput(-1, -1, -1, true),
      ScoringBoxTdInput(-1, -1, -1, true),
      ScoringBoxTdOutput(-1, -1, -1, true),
      CompilerName(sub.lang),
      ScoringBoxCode(-1, -1, -1, sub.lang, true),
      std::to_string(stage),
    });
  } else {
    opt.command.push_back(ScoringBoxMetaFile(-1, -1, -1, true));
    if (sub.specjudge_type == SpecjudgeType::NORMAL) {
      opt.command.insert(opt.command.end(), sub.default_scoring_args.begin(), sub.default_scoring_args.end());
    }
  }
  opt.workdir = Workdir("/");
  opt.output = ScoringBoxOutput(-1, -1, -1, true);
  opt.error = "/dev/null";
  if (cpuid != -1) opt.cpu_set.push_back(cpuid);
  opt.uid = opt.gid = uid;
  opt.wall_time = 60L * 1'000'000;
  opt.wall_time /= kTimeMultiplier;
  opt.rss = kMaxRSS;
  opt.proc_num = 10;
  opt.fsize = kMaxOutput;
  opt.dirs = {"/usr", "/var/lib", "/lib", "/lib64", "/etc/alternatives", "/bin"};
  opt.FilterDirs();
  return SandboxExec(opt);
}

/// parent
constexpr int kUidBase = 50000, kUidPoolSize = 100;
std::vector<int> uid_pool, cpuid_pool;
bool pool_init = false;

fd_set running_fdset;
std::map<int, std::tuple<int, int, int>> running; // fd -> (pid, uid, cpuid)
std::unordered_map<int, struct cjail_result> finished; // fd -> result

void InitPools() {
  for (int i = 0; i < kUidPoolSize; i++) uid_pool.push_back(i + kUidBase);
  for (int i = 0, N = get_nprocs(); i < N; i++) {
    if (CPU_ISSET(i, &kPinnedCpus)) cpuid_pool.push_back(i);
  }
  FD_ZERO(&running_fdset);
  pool_init = true;
}

bool Wait() {
  if (running.empty()) return false;
  int max_fd = running.rbegin()->first;
  fd_set select_fdset = running_fdset;
  if (select(max_fd + 1, &select_fdset, nullptr, nullptr, nullptr) <= 0) return false;
  std::vector<int> to_remove;
  for (auto& i : running) {
    if (!FD_ISSET(i.first, &select_fdset)) continue;
    spdlog::debug("Task handle={} finished", i.first);
    struct cjail_result res = {};
    IGNORE_RETURN(read(i.first, &res, sizeof(struct cjail_result)));
    // we don't close(i.first) here, because it will release the handle and make it clash
    waitpid(std::get<0>(i.second), nullptr, 0);
    uid_pool.push_back(std::get<1>(i.second));
    if (int cpuid = std::get<2>(i.second); cpuid != -1) {
      cpuid_pool.push_back(cpuid);
    }
    finished[i.first] = res;
    to_remove.push_back(i.first);
  }
  for (int i : to_remove) {
    running.erase(i);
    FD_CLR(i, &running_fdset);
  }
  return true;
}

} // namespace

int RunTask(const Submission& sub, const Task& task) {
  if (task.type == TaskType::FINALIZE) return -1;
  if (!pool_init) InitPools();
  int pipefd[2];
  if (pipe(pipefd) < 0) return -1;
  int uid = uid_pool.back();
  uid_pool.pop_back();
  int cpuid = -1;
  if (cpuid_pool.empty()) {
    if (CPU_COUNT(&kPinnedCpus)) {
      spdlog::warn("No available cpu; task won\'t be pinned");
    }
  } else {
    cpuid = cpuid_pool.back();
    cpuid_pool.pop_back();
  }
  pid_t pid = fork();
  if (pid < 0) {
    uid_pool.push_back(uid);
    if (cpuid != -1) cpuid_pool.push_back(cpuid);
    close(pipefd[0]);
    close(pipefd[1]);
    return pid; // error
  }
  if (pid == 0) {
    close(pipefd[0]);
    struct cjail_result ret;
    switch (task.type) {
      case TaskType::COMPILE: ret = RunCompile(sub, task, uid, cpuid); break;
      case TaskType::EXECUTE: ret = RunExecute(sub, task, uid, cpuid); break;
      case TaskType::SCORING: ret = RunScoring(sub, task, uid, cpuid); break;
      case TaskType::FINALIZE: __builtin_unreachable();
    }
    IGNORE_RETURN(write(pipefd[1], &ret, sizeof(struct cjail_result)));
    _exit(0); // since forked, some atexit() may hang by deadlocks
  }
  close(pipefd[1]);
  running[pipefd[0]] = {pid, uid, cpuid};
  FD_SET(pipefd[0], &running_fdset);
  spdlog::debug("Task type={} subtask={} of {} started, handle={} pid={} uid={} cpuid={}",
                TaskTypeName(task.type), task.subtask, sub.submission_internal_id, pipefd[0], pid, uid, cpuid);
  return pipefd[0];
}

std::pair<int, struct cjail_result> WaitAnyResult() {
  if (running.empty() && finished.empty()) return {-1, {}};
  if (finished.empty()) Wait();
  auto ret = *finished.begin();
  finished.erase(finished.begin());
  close(ret.first); // delay close
  spdlog::debug("Task handle={} returned", ret.first);
  return ret;
}

std::pair<int, struct cjail_result> WaitAnyResult(const std::vector<int>& handles) {
  bool flag = false;
  for (int i : handles) {
    auto it = finished.find(i);
    if (it != finished.end()) {
      auto ret = *it;
      finished.erase(it);
      return ret;
    }
    if (!flag && running.count(i)) flag = true;
  }
  if (!flag) return {-1, {}};
  while (true) {
    Wait();
    for (int i : handles) {
      auto it = finished.find(i);
      if (it != finished.end()) {
        auto ret = *it;
        finished.erase(it);
        close(ret.first); // delay close
        spdlog::debug("Task handle={} returned", ret.first);
        return ret;
      }
    }
  }
}
