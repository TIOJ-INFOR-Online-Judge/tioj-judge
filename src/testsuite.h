#ifndef TESTSUITE_H_
#define TESTSUITE_H_

#include <string>
#include <vector>
#include <cjail/cjail.h>
#include "utils.h"

extern int max_parallel;

enum class ProblemType {
  NORMAL,
  SPECJUDGE_OLD,
  SPECJUDGE_NEW,
};

enum class InterlibType {
  NONE,
  INCLUDE,
  // TODO: LINK,
};

enum class Compiler {
  GCC_CPP_98,
  GCC_CPP_11,
  GCC_CPP_14,
  GCC_CPP_17,
  GCC_CPP_20,
  GCC_C_90,
  GCC_C_98,
  GCC_C_11,
  HASKELL,
  PYTHON2,
  PYTHON3,
};

class Submission {
 public:
  // use for submission file management; must be unique in a run even if in the case of rejudge
  long submission_internal_id;
  // submission information
  int submission_id;
  int submitter_id;
  long submission_time; // UNIX timestamp
  std::string submitter;
  Compiler lang;
  // problem information
  int problem_id;
  ProblemType problem_type;
  InterlibType interlib_type;
  bool sandbox_strict; // false for backward-compatability
  // task information
  struct TestdataLimit {
    long vss, rss, output; // KiB
    long time; // us
  };
  std::vector<TestdataLimit> td_limits;
  // TODO: testdata group
  // TODO: skip_group
  // task result
  Verdict verdict;
  struct TestdataResult {
    struct cjail_result execute_result;
    long vss, rss; // KiB
    long time; // us
    int64_t score; // 10^(-6)
    Verdict verdict;
  };
  std::vector<TestdataResult> td_results;

  Submission() :
      submission_id(0), submitter_id(0), submission_time(0),
      problem_type(ProblemType::NORMAL),
      interlib_type(InterlibType::NONE),
      sandbox_strict(false) {}
};

// Call from main thread
void WorkLoop();
// This can be called from another thread
void PushSubmission(Submission&&);

#endif  // TESTSUITE_H_
