#ifndef TEST_UTILS_H_
#define TEST_UTILS_H_

#include <gtest/gtest.h>
#include <tioj/utils.h>
#include <tioj/reporter.h>
#include <tioj/submission.h>

class AssertVerdictReporter : public Reporter {
  bool has_scoring_result_;
  bool has_overall_result_;
  bool require_scoring_;

  void AssertResult_() {
    ASSERT_TRUE(has_overall_result_ && (!require_scoring_ || has_scoring_result_));
  }
 public:
  Verdict expect_verdict;

  AssertVerdictReporter(Verdict ver, bool require_scoring = true) :
      has_scoring_result_(false),
      has_overall_result_(false),
      require_scoring_(require_scoring),
      expect_verdict(ver) {}
  ~AssertVerdictReporter() { AssertResult_(); }

  void ReportStartCompiling(const Submission&) {}
  void ReportOverallResult(const Submission& sub) {
    ASSERT_EQ(sub.verdict, expect_verdict);
    has_overall_result_ = true;
  }
  void ReportScoringResult(const Submission& sub, int subtask) {
    ASSERT_EQ(sub.td_results[subtask].verdict, expect_verdict);
    has_scoring_result_ = true;
  }
  void ReportCEMessage(const Submission&) {}
  void ReportFinalized(const Submission&, size_t) {}
};

long SetupSubmission(
    Submission& sub, int id, Compiler lang, long time, bool sandbox_strict, const std::string& code,
    SpecjudgeType spec_type = SpecjudgeType::NORMAL, const std::string& specjudge_code = "",
    int submitter_id = 10);

void TeardownSubmission(long id);

#endif // TEST_UTILS_H_
