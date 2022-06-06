#include "testsuite.h"

#include <mutex>
#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <condition_variable>

#include "tasks.h"

int max_parallel = 1;

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
    return priority > x.priority ||
        (priority == x.priority && task.subtask > x.task.subtask);
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
// TODO: cancelled group

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

void FinalizeSubmission(Submission& sub, const TaskEntry& task) {
  // TODO: send finalized message to server, asynchronously
  submission_list.erase(sub.submission_internal_id);
}

void FinalizeTask(long id, struct cjail_result res) {
  auto& entry = task_list[id];
  auto& sub = submission_list[entry.submission_internal_id];
  switch (entry.task.type) {
    // TODO
    case TaskType::COMPILE: break;
    case TaskType::EXECUTE: break;
    case TaskType::SCORING: break;
    case TaskType::FINALIZE: FinalizeSubmission(sub, entry); break;
  }
  for (long nxt : entry.edges) {
    auto& nxt_task = task_list[nxt];
    if (!--nxt_task.indeg) task_queue.push(nxt);
  }
  task_list.erase(id);
}

int DispatchTask(long id) {
  auto& entry = task_list[id];
  switch (entry.task.type) {
    // TODO: setup
    case TaskType::COMPILE: break;
    case TaskType::EXECUTE: break;
    case TaskType::SCORING: break;
    case TaskType::FINALIZE: {
      FinalizeTask(id, {});
      return 0;
    }
  }
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
        // if this is a finalize task, it will finish immediately without adding any running task
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
  // dependency graph:
  //    compile_sj ------------+-----+
  //                           |     |
  //                           |     v
  // compile ---+-> execute------> scoring ---+---> finalize
  //            |              |              |
  //            +--> execute --+-> scoring ---+
  //
  // We use submissions time as priority; this can be adjusted if needed
  //   (e.g. put contest submissions before normal submissions
  int sub_id = sub.submission_internal_id;
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
  if (sub.problem_type == ProblemType::SPECJUDGE_OLD ||
      sub.problem_type == ProblemType::SPECJUDGE_NEW) {
    TaskEntry compile(sub_id, {TaskType::COMPILE, (int)CompileSubtask::SPECJUDGE}, priority);
    for (auto& i : scorings) Link(compile, i);
    if (executes.empty()) Link(compile, finalize);
    InsertTaskList(std::move(compile));
  }
  for (auto& i : executes) InsertTaskList(std::move(i));
  for (auto& i : scorings) InsertTaskList(std::move(i));
  InsertTaskList(std::move(finalize));
  submission_list.insert({sub_id, std::move(sub)});
}
