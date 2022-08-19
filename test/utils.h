#ifndef TEST_UTILS_H_
#define TEST_UTILS_H_

#include <gtest/gtest.h>
#include <tioj/utils.h>
#include <tioj/reporter.h>
#include <tioj/submission.h>

class AssertVerdictReporter : public Reporter {
 public:
  Verdict expect_verdict;

  AssertVerdictReporter(Verdict ver) : expect_verdict(ver) {}

  void ReportStartCompiling(const Submission&) {}
  void ReportOverallResult(const Submission& sub) {
    ASSERT_EQ(sub.verdict, expect_verdict);
  }
  void ReportScoringResult(const Submission& sub, int subtask) {
    ASSERT_EQ(sub.td_results[subtask].verdict, expect_verdict);
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
