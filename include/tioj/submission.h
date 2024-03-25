#ifndef INCLUDE_TIOJ_SUBMISSION_H_
#define INCLUDE_TIOJ_SUBMISSION_H_

#include <string>
#include <vector>
#include <filesystem>
#include <functional>

#include <cjail/cjail.h>

extern int kMaxParallel;
extern cpu_set_t kPinnedCpus;
// KiB
extern long kMaxRSS;
extern long kMaxOutput;
// Real time * kTimeMultiplier = Indicated time
extern double kTimeMultiplier;

#define ENUM_SPECJUDGE_TYPE_ \
  X(NORMAL) \
  X(SPECJUDGE_OLD) \
  X(SPECJUDGE_NEW) \
  X(SKIP) // should be the last one
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

#define ENUM_SUMMARY_TYPE_ \
  X(NONE) \
  X(CUSTOM)
enum class SummaryType {
#define X(name) name,
  ENUM_SUMMARY_TYPE_
#undef X
};

#define ENUM_COMPILER_ \
  X(GCC_CPP_98, "c++98") \
  X(GCC_CPP_11, "c++11") \
  X(GCC_CPP_14, "c++14") \
  X(GCC_CPP_17, "c++17") \
  X(GCC_CPP_20, "c++20") \
  X(GCC_C_90, "c90") \
  X(GCC_C_99, "c99") \
  X(GCC_C_11, "c11") \
  X(GCC_C_17, "c17") \
  X(HASKELL, "haskell") \
  X(PYTHON2, "python2") \
  X(PYTHON3, "python3") \
  X(CUSTOM, "custom")
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
  X(ER, "ER", "Judge Compilation Error") \
  X(JE, "JE", "Judge Error")
enum class Verdict {
#define X(name, abr, desc) name,
  ENUM_VERDICT_
#undef X
};

class SubmissionResult;

class Submission {
 public:
  // use for submission file management; must be unique in a run even if in the case of rejudge
  long submission_internal_id;
  // submission information
  int submission_id;
  int contest_id;
  long priority;
  int64_t submission_time; // UNIX timestamp, microseconds
  int submitter_id;
  std::string submitter_name, submitter_nickname;
  Compiler lang;

  // problem information
  int problem_id;
  SpecjudgeType specjudge_type;
  InterlibType interlib_type;
  SummaryType summary_type;
  Compiler specjudge_lang;
  Compiler summary_lang;
  std::string user_compile_args, specjudge_compile_args;
  int stages;
  bool judge_between_stages;
  bool sandbox_strict; // false for backward-compatability
  int process_limit;
  std::vector<std::string> default_scoring_args;

  // task information
  struct TestdataItem {
    // files
    std::filesystem::path input_file, answer_file;
    // limits
    // vss & rss & output can be zero if unlimited; time must always be set
    int64_t vss, rss, output; // KiB
    int64_t time; // us
    bool ignore_verdict; // ignore this testdata in overall verdict calculation
    std::vector<int> td_groups; // which groups it belongs to
  };
  std::vector<TestdataItem> testdata;
  bool skip_group; // skip a group of testdata if any of them got non-AC

  // judge behavior
  struct Reporter {
    // these functions should not block
    std::function<void(const Submission&, const SubmissionResult&)> ReportStartCompiling;
    std::function<void(const Submission&, const SubmissionResult&)> ReportOverallResult;
    std::function<void(const Submission&, const SubmissionResult&, int subtask, int stage)> ReportScoringResult;
    std::function<void(const Submission&, const SubmissionResult&)> ReportCEMessage;
    std::function<void(const Submission&, const SubmissionResult&)> ReportERMessage;
    std::function<void(const Submission&, const SubmissionResult&, size_t queue_size_before_pop)> ReportFinalized;
  };
  Reporter reporter; // callbacks for result reporting
  bool report_intermediate_stage; // whether to call ReportScoringResult in intermediate stages
  bool remove_submission; // remove submission code after judge

  Submission() :
      submission_id(0),
      contest_id(0),
      submission_time(0),
      submitter_id(0),
      lang(Compiler::GCC_CPP_17),
      problem_id(0),
      specjudge_type(SpecjudgeType::NORMAL),
      interlib_type(InterlibType::NONE),
      summary_type(SummaryType::NONE),
      specjudge_lang(Compiler::GCC_CPP_17),
      summary_lang(Compiler::GCC_CPP_17),
      stages(1),
      judge_between_stages(false),
      sandbox_strict(false),
      process_limit(1),
      report_intermediate_stage(false),
      remove_submission(true) {}
};

class SubmissionResult {
 public:
  // task result
  struct TestdataResult {
    struct cjail_result execute_result;
    int64_t vss, rss; // KiB
    int64_t time; // us
    int64_t score; // 10^(-6)
    Verdict verdict;
    std::string message_type, message;
    bool skip_stage;
    TestdataResult() :
        execute_result{}, vss{}, rss{}, time{}, score{}, verdict(Verdict::NUL),
        skip_stage(false) {}
  };
  std::vector<TestdataResult> td_results;
  // overall result
  Verdict verdict;
  std::string ce_message, er_message;

  SubmissionResult() : verdict(Verdict::NUL) {}
};

// Call from main thread
void WorkLoop(bool loop = true);

// Judge queue information; DO NOT call these in reporter because of deadlocks!
size_t CurrentSubmissionQueueSize();
// External ID, for server communication
std::vector<int> GetQueuedSubmissionID();

// Called from any thread
// 0 = no limit; return false only if queue size exceeded
bool PushSubmission(Submission&&, size_t max_queue = 0);

#endif  // INCLUDE_TIOJ_SUBMISSION_H_
