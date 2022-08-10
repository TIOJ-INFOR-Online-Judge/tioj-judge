#include "example_problem.h"

#include <fstream>
#include <algorithm>
#include <tioj/paths.h>
#include <tioj/submission.h>
#include <tioj/reporter.h>
#include <tioj/utils.h>

class AssertACReporter : public Reporter {
 public:
  void ReportStartCompiling(const Submission&) {}
  void ReportOverallResult(const Submission& sub) {
    ASSERT_EQ(sub.verdict, Verdict::AC);
  }
  void ReportScoringResult(const Submission& sub, int subtask) {
    ASSERT_EQ(sub.td_results[subtask].verdict, Verdict::AC);
  }
  void ReportCEMessage(const Submission&) {}
  void ReportFinalized(const Submission&, size_t) {}
} reporter;

long SetupSubmission(Submission& sub, int id, Compiler lang, long time, const std::string& code,
    SpecjudgeType spec_type = SpecjudgeType::NORMAL, const std::string& specjudge_code = "") {
  sub.submission_id = id;
  long iid = sub.submission_internal_id = GetUniqueSubmissionInternalId();
  sub.submitter_id = 10;
  sub.submission_time = time;
  sub.lang = lang;
  sub.reporter = &reporter;
  fs::create_directories(SubmissionCodePath(iid));
  {
    std::ofstream fout(SubmissionUserCode(iid));
    fout << code;
  }
  if (spec_type != SpecjudgeType::NORMAL) {
    sub.specjudge_type = spec_type;
    sub.specjudge_lang = Compiler::GCC_CPP_17;
    std::ofstream fout(SubmissionJudgeCode(iid));
    fout << specjudge_code;
  }
  return iid;
}

void TeardownSubmission(long id) {
  fs::remove_all(SubmissionCodePath(id));
}

constexpr long kTime = 1655000000;

struct SubParam {
  int sub_id;
  int parallel;
  bool is_strict;
  Compiler lang;
  std::string code;
};

std::string ParamName(const ::testing::TestParamInfo<SubParam>& info) {
  std::string ret = CompilerName(info.param.lang);
  std::replace(ret.begin(), ret.end(), '+', 'p');
  if (info.param.is_strict) ret += "_strict";
  if (info.param.parallel > 1) ret += "_parallel";
  return ret;
};

class ExampleProblemOneSubmission : public ExampleProblem, public testing::WithParamInterface<SubParam> {};
TEST_P(ExampleProblemOneSubmission, Sub) {
  auto& param = GetParam();
  kMaxParallel = param.parallel;
  SetUp(1, 5, param.is_strict);
  long id = SetupSubmission(sub, param.sub_id, param.lang, kTime, param.code);
  PushSubmission(std::move(sub));
  WorkLoop(false);
  TeardownSubmission(id);
}
INSTANTIATE_TEST_SUITE_P(OneSubmission, ExampleProblemOneSubmission,
    testing::Values(
      (SubParam){1, 1, false, Compiler::GCC_CPP_17, R"(#include <cstdio>
int main(){ int a; scanf("%d",&a);printf("%d",a); })"},
      (SubParam){1, 4, true, Compiler::GCC_CPP_17, R"(#include <cstdio>
int main(){ int a; scanf("%d",&a);printf("%d",a); })"},
      (SubParam){2, 1, false, Compiler::HASKELL, "main = interact $ show . sum . map read . words"},
      (SubParam){2, 1, true, Compiler::HASKELL, "main = interact $ show . sum . map read . words"},
      (SubParam){3, 4, false, Compiler::PYTHON2, "print input()"},
      (SubParam){3, 1, true, Compiler::PYTHON2, "print input()"},
      (SubParam){4, 1, false, Compiler::PYTHON3, "print(input())"},
      (SubParam){4, 4, true, Compiler::PYTHON3, "import sys; print(sys.stdin.read())"}
    ),
    ParamName);

TEST_F(ExampleProblem, SpecjudgeOldProblemOneSubmission) {
  kMaxParallel = 2;
  SetUp(2, 3, true);
  long id = SetupSubmission(sub, 5, Compiler::GCC_CPP_17, kTime, R"(#include <cstdio>
int main(){ int a; scanf("%d",&a);printf("%d",12345); })", SpecjudgeType::SPECJUDGE_OLD, R"(#include <cstdio>
#include "nlohmann/json.hpp"
int main(){ puts("0"); })");
  PushSubmission(std::move(sub));
  WorkLoop(false);
  TeardownSubmission(id);
}

TEST_F(ExampleProblem, SpecjudgeNewProblemOneSubmission) {
  kMaxParallel = 2;
  SetUp(3, 3, true);
  long id = SetupSubmission(sub, 6, Compiler::GCC_CPP_17, kTime, R"(#include <cstdio>
int main(){ int a; scanf("%d",&a);printf("%d",12345); })", SpecjudgeType::SPECJUDGE_NEW, R"(#include <iostream>
#include "nlohmann/json.hpp"
int main(){ std::cout << nlohmann::json{{"verdict", "AC"}, {"score", "102.2"}}; })");
  PushSubmission(std::move(sub));
  WorkLoop(false);
  TeardownSubmission(id);
}

// TODO: multiple submission (normal, rejudge)
