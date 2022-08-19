#ifndef TASKS_H_
#define TASKS_H_

#include <cjail/cjail.h>
#include <tioj/tasks.h>
#include "submission.h"

struct Task {
  TaskType type;
  // testdata # for EXECUTE & SCORING; (int)CompileSubtask for COMPILE
  int subtask;
  int stage;
};

// We're not sure whether cjail is thread-safe. Thus, we use fork() for every RunTask,
//  and provide an asynchronous interface to deal with tasks.

// RunTask returns a handle to obtain result
int RunTask(const Submission&, const Task&);

// return (handle, result); handle = -1 if error
std::pair<int, struct cjail_result> WaitAnyResult();
std::pair<int, struct cjail_result> WaitAnyResult(const std::vector<int>& handles);

#endif // TASKS_H_
