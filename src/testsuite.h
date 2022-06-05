#ifndef TESTSUITE_H_
#define TESTSUITE_H_

#include "utils.h"

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

class Submission {
 public:
  // submission information
  int submission_id;
  int submitter_id;
  long submission_time; // UNIX timestamp
  std::string submitter;
  std::string lang, std;
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
  // task result
  struct TestdataResult {
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

Verdict Testsuite(Submission&);

#endif  // TESTSUITE_H_
