#include "tasks.h"

#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

#include <map>
#include <unordered_map>

namespace {

/// child
// Invoke sandbox with correct settings
// Results will be parsed in testsuite.cpp
// TODO
struct cjail_result RunCompile(const Submission& sub, const Task& task, int uid) {}
struct cjail_result RunExecute(const Submission& sub, const Task& task, int uid) {}
struct cjail_result RunScoring(const Submission& sub, const Task& task, int uid) {}

/// parent
constexpr int kUidBase = 50000, kUidPoolSize = 100;
std::vector<int> uid_pool;
bool pool_init = false;

fd_set running_fdset;
std::map<int, std::pair<int, int>> running; // fd -> (pid, uid)
std::unordered_map<int, struct cjail_result> finished; // fd -> result

void InitPool() {
  for (int i = 0; i < kUidPoolSize; i++) uid_pool.push_back(i + kUidBase);
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
    struct cjail_result res;
    read(i.first, &res, sizeof(struct cjail_result));
    close(i.first);
    waitpid(i.second.first, nullptr, 0);
    uid_pool.push_back(i.second.second);
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
  if (!pool_init) InitPool();
  int pipefd[2];
  if (pipe(pipefd) < 0) return -1;
  int uid = uid_pool.back();
  uid_pool.pop_back();
  pid_t pid = fork();
  if (pid < 0) {
    uid_pool.push_back(uid);
    close(pipefd[0]);
    close(pipefd[1]);
    return pid; // error
  }
  if (pid == 0) {
    close(pipefd[0]);
    struct cjail_result ret;
    switch (task.type) {
      case TaskType::COMPILE: ret = RunCompile(sub, task, uid); break;
      case TaskType::EXECUTE: ret = RunExecute(sub, task, uid); break;
      case TaskType::SCORING: ret = RunScoring(sub, task, uid); break;
      case TaskType::FINALIZE: __builtin_unreachable();
    }
    write(pipefd[1], &ret, sizeof(struct cjail_result));
    exit(0);
  }
  close(pipefd[1]);
  running[pipefd[0]] = {pid, uid};
  FD_SET(pipefd[0], &running_fdset);
  return pipefd[0];
}

std::pair<int, struct cjail_result> WaitAnyResult() {
  if (running.empty() && finished.empty()) return {-1, {}};
  if (finished.empty()) Wait();
  auto ret = *finished.begin();
  finished.erase(finished.begin());
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
        return ret;
      }
    }
  }
}
