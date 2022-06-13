#ifndef TASKS_H_
#define TASKS_H_

#include "sandbox.h"
#include "submission.h"

enum class TaskType {
  COMPILE,
  EXECUTE,
  SCORING, // special judge
  FINALIZE,
};

enum class CompileSubtask : int {
  USERPROG,
  SPECJUDGE,
};

struct Task {
  TaskType type;
  int subtask;
  // testdata # for EXECUTE & SCORING; (int)CompileSubtask for COMPILE
  // TODO FEATURE(multistage,link): subsubtask
};

// We're not sure whether cjail is thread-safe. Thus, we use fork() for every RunTask,
//  and provide an asynchronous interface to deal with tasks.

// RunTask returns a handle to obtain result
int RunTask(const Submission&, const Task&);

// return (handle, result); handle = -1 if error
std::pair<int, struct cjail_result> WaitAnyResult();
std::pair<int, struct cjail_result> WaitAnyResult(const std::vector<int>& handles);

#endif // TASKS_H_
