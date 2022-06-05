#ifndef TASKS_H_
#define TASKS_H_

#include "sandbox.h"
#include "testsuite.h"

enum class TaskType {
  COMPILE,
  EXECUTE,
  SCORING, // special judge
};

// We're not sure whether cjail is thread-safe. Thus, we use fork() for every RunTask,
//  and provide an asynchronous interface to deal with tasks.

// RunTask returns a handle to obtain result
int RunTask(const Submission&, TaskType);

// return (handle, result); handle = -1 if error
std::pair<int, struct cjail_result> WaitAnyResult();
std::pair<int, struct cjail_result> WaitAnyResult(const std::vector<int>& handles);

#endif // TASKS_H_
