#ifndef INCLUDE_TIOJ_TASKS_H_
#define INCLUDE_TIOJ_TASKS_H_

#define ENUM_TASK_TYPE_ \
  X(COMPILE) \
  X(EXECUTE) \
  X(SCORING) /* special judge */ \
  X(SUMMARY)
enum class TaskType {
#define X(name) name,
  ENUM_TASK_TYPE_
#undef X
};

#define ENUM_COMPILE_SUBTASK_ \
  X(USERPROG) \
  X(SPECJUDGE) \
  X(SUMMARY)
enum class CompileSubtask : int {
#define X(name) name,
  ENUM_COMPILE_SUBTASK_
#undef X
};

#endif  // INCLUDE_TIOJ_TASKS_H_
