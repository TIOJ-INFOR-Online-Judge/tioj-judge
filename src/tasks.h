#ifndef TASKS_H_
#define TASKS_H_

#include <cjail/cjail.h>
#include "submission.h"

#define ENUM_TASK_TYPE_ \
  X(COMPILE) \
  X(EXECUTE) \
  X(SCORING) /* special judge */ \
  X(FINALIZE)
enum class TaskType {
#define X(name) name,
  ENUM_TASK_TYPE_
#undef X
};

#define ENUM_COMPILE_SUBTASK_ \
  X(USERPROG) \
  X(SPECJUDGE)
enum class CompileSubtask : int {
#define X(name) name,
  ENUM_COMPILE_SUBTASK_
#undef X
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
