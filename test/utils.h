#ifndef TEST_UTILS_H_
#define TEST_UTILS_H_

#include <gtest/gtest.h>
#include <tioj/utils.h>
#include <tioj/submission.h>

class AssertVerdictReporter {
  bool has_scoring_result_;
  bool has_overall_result_;
  bool require_scoring_;
  bool assert_scoring_verdict_;

  void AssertResult_() {
    ASSERT_TRUE(has_overall_result_ && (!require_scoring_ || has_scoring_result_));
  }
 public:
  Verdict expect_verdict;

  AssertVerdictReporter(Verdict ver, bool require_scoring = true, bool assert_scoring_verdict = true) :
      has_scoring_result_(false),
      has_overall_result_(false),
      require_scoring_(require_scoring),
      assert_scoring_verdict_(assert_scoring_verdict),
      expect_verdict(ver) {}
  ~AssertVerdictReporter() { AssertResult_(); }

  void ReportOverallResult(const SubmissionResult& res) {
    ASSERT_EQ(res.verdict, expect_verdict);
    has_overall_result_ = true;
  }
  void ReportScoringResult(const SubmissionResult& res, int subtask) {
    if (assert_scoring_verdict_) {
      ASSERT_EQ(res.td_results[subtask].verdict, expect_verdict);
    }
    has_scoring_result_ = true;
  }

  Submission::Reporter GetReporter() {
    Submission::Reporter reporter;
    reporter.ReportOverallResult = [&](const Submission&, const SubmissionResult& res) {
      ReportOverallResult(res);
    };
    reporter.ReportScoringResult = [&](const Submission&, const SubmissionResult& res, int subtask, int) {
      ReportScoringResult(res, subtask);
    };
    reporter.ReportCEMessage = [&](const Submission&, const SubmissionResult& res) {
      if (expect_verdict != Verdict::CE) {
        EXPECT_TRUE(false) << "Unexpected CE message: " + res.ce_message;
      }
    };
    return reporter;
  }
};

long SetupSubmission(
    Submission& sub, int id, Compiler lang, long time, bool sandbox_strict, const std::string& code,
    SpecjudgeType spec_type = SpecjudgeType::NORMAL, const std::string& specjudge_code = "",
    SummaryType summary_type = SummaryType::NONE, const std::string& summary_code = "",
    int submitter_id = 10);

void TeardownSubmission(long id);

#endif // TEST_UTILS_H_
