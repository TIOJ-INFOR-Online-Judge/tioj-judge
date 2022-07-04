#ifndef TIOJ_SUBMISSION_H_
#define TIOJ_SUBMISSION_H_

#include <string>
#include <vector>

#include <cjail/cjail.h>
#include <nlohmann/json_fwd.hpp>
#include "reporter.h"

extern int kMaxParallel;

#define ENUM_SPECJUDGE_TYPE_ \
  X(NORMAL) \
  X(SPECJUDGE_OLD) \
  X(SPECJUDGE_NEW)
enum class SpecjudgeType {
#define X(name) name,
  ENUM_SPECJUDGE_TYPE_
#undef X
};

#define ENUM_INTERLIB_TYPE_ \
  X(NONE) \
  X(INCLUDE)
enum class InterlibType {
#define X(name) name,
  ENUM_INTERLIB_TYPE_
#undef X
};

#define ENUM_COMPILER_ \
  X(GCC_CPP_98, "c++98") \
  X(GCC_CPP_11, "c++11") \
  X(GCC_CPP_14, "c++14") \
  X(GCC_CPP_17, "c++17") \
  X(GCC_CPP_20, "c++20") \
  X(GCC_C_90, "c90") \
  X(GCC_C_98, "c98") \
  X(GCC_C_11, "c11") \
  X(HASKELL, "haskell") \
  X(PYTHON2, "python2") \
  X(PYTHON3, "python3")
enum class Compiler {
#define X(name, compname) name,
  ENUM_COMPILER_
#undef X
};

// the max of every subtasks would be the final result
#define ENUM_VERDICT_ \
  X(NUL, "", "nil") \
  /* verdicts after execution */ \
  X(AC, "AC", "Accepted") \
  X(WA, "WA", "Wrong Answer") \
  X(TLE, "TLE", "Time Limit Exceeded") \
  X(MLE, "MLE", "Memory Limit Exceeded") \
  X(OLE, "OLE", "Output Limit Exceeded") \
  X(RE, "RE", "Runtime Error (exited with nonzero status)") \
  X(SIG, "SIG", "Runtime Error (exited with signal)") \
  X(EE, "EE", "Execution Error") \
  /* verdicts after compilation */ \
  X(CE, "CE", "Compile Error") \
  X(CLE, "CLE", "Compilation Limit Exceeded") \
  X(ER, "ER", "WTF!")
enum class Verdict {
#define X(name, abr, desc) name,
  ENUM_VERDICT_
#undef X
};

class Submission {
 public:
  // use for submission file management; must be unique in a run even if in the case of rejudge
  long submission_internal_id;
  // submission information
  int submission_id;
  long priority;
  int64_t submission_time; // UNIX timestamp
  int submitter_id;
  std::string submitter_name, submitter_nickname;
  Compiler lang;
  // problem information
  int problem_id;
  SpecjudgeType specjudge_type;
  InterlibType interlib_type;
  Compiler specjudge_lang;
  bool sandbox_strict; // false for backward-compatability
  // task information
  struct TestdataLimit {
    int64_t vss, rss, output; // KiB
    int64_t time; // us
  };
  std::vector<TestdataLimit> td_limits;
  // TODO FEATURE(group): testdata group
  // TODO FEATURE(group): bool skip_group
  // task result
  struct TestdataResult {
    struct cjail_result execute_result;
    int64_t vss, rss; // KiB
    int64_t time; // us
    int64_t score; // 10^(-6)
    Verdict verdict;
    TestdataResult() : execute_result{}, vss{}, rss{}, time{}, score{}, verdict(Verdict::NUL) {}
  };
  std::vector<TestdataResult> td_results;
  // overall result
  Verdict verdict;
  std::string ce_message;
  // result reporting
  Reporter* reporter;

  Submission() :
      submission_id(0), submission_time(0), submitter_id(0),
      specjudge_type(SpecjudgeType::NORMAL),
      interlib_type(InterlibType::NONE),
      specjudge_lang(Compiler::GCC_CPP_17),
      sandbox_strict(false),
      verdict(Verdict::NUL),
      reporter(nullptr) {}

  nlohmann::json TestdataMeta(int subtask) const;
};

// Call from main thread
void WorkLoop(bool loop = true);
// Called from another thread
void PushSubmission(Submission&&);

#endif  // TIOJ_SUBMISSION_H_
