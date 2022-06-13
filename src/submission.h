#ifndef SUBMISSION_H_
#define SUBMISSION_H_

#include <vector>
#include <cjail/cjail.h>
#include <nlohmann/json.hpp>

extern int max_parallel;

enum class SpecJudgeType {
  NORMAL,
  SPECJUDGE_OLD,
  SPECJUDGE_NEW,
};

enum class InterlibType {
  NONE,
  INCLUDE,
  // TODO FEATURE(link): LINK,
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

// the max of every subtasks would be the final result
enum class Verdict {
  NUL,
  AC, WA, TLE, MLE, OLE, RE, SIG, // verdicts after execution
  CE, CLE, ER, // verdicts after compilation
};

class Submission {
 public:
  // use for submission file management; must be unique in a run even if in the case of rejudge
  long submission_internal_id;
  // submission information
  int submission_id;
  int submitter_id;
  int64_t submission_time; // UNIX timestamp
  std::string submitter;
  Compiler lang;
  // problem information
  int problem_id;
  SpecJudgeType specjudge_type;
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
  };
  std::vector<TestdataResult> td_results;
  // overall result
  Verdict verdict;
  std::string message;

  Submission() :
      submission_id(0), submitter_id(0), submission_time(0),
      specjudge_type(SpecJudgeType::NORMAL),
      interlib_type(InterlibType::NONE),
      specjudge_lang(Compiler::GCC_CPP_17),
      sandbox_strict(false),
      verdict(Verdict::NUL) {}

  nlohmann::json TestdataMeta(int subtask) const;
};

// Call from main thread
void WorkLoop(bool loop = true);
// Called from another thread
void PushSubmission(Submission&&);

#endif  // SUBMISSION_H_
