#include <array>
#include <gtest/gtest-matchers.h>
#include "example_problem.h"
#include "utils.h"

namespace {

constexpr long kTime = 1655000000;

struct ScoringParam {
  std::vector<std::string> default_scoring_args;
  std::array<Verdict, 10> verdicts;
};

std::string ParamName(const ::testing::TestParamInfo<ScoringParam>& info) {
  std::string ret;
  for (auto& i : info.param.default_scoring_args) ret += i + '_';
  for (auto& i : ret) {
    if (i == '-') i = '_';
  }
  if (ret.empty()) {
    ret = "empty";
  } else {
    ret.pop_back();
  }
  return ret;
}

}

class ExampleProblemDefaultScoring : public ExampleProblem, public testing::WithParamInterface<ScoringParam> {};
TEST_P(ExampleProblemDefaultScoring, Mode) {
  auto& param = GetParam();
  SetUp(2, {
    {"123\n234\n", "123\n234\n"}, // match: AC for all
    {"123\n234", "123 \n234 \n\n"}, // trailing whitespaces / blank lines: AC except strict
    {" 123 234\n", "123 234\n"}, // preceding whitespaces: AC for white-diff/float-diff
    {"123  234\n", "123 234\n"}, // additional whitespaces: AC for white-diff/float-diff
    {" 1.0000001  1.0000001 ", "1.0 1.0\n"}, // AC for float-diff >1e-7
    {" 1.0000001  1.0000001 \n a 1.0000001 -1", "1.0 1.0\na 1.0 -1\n"}, // AC for float-diff >1e-7
    {" 1.00001  1.00001 ", "1.0 1.0\n"}, // AC for float-diff >1e-5
    {" 100.00001  100.00001 ", "100.0 100.0\n"}, // AC for float-diff (absolute>1e-5) (absolute-relative/relative>1e-7)
    {" 100.00001 0.0100001\n", "100.0 0.01\n"}, // AC for float-diff (absolute-relative>1e-7) (absolute/relative>1e-5)
    {"1.0000001  1.0000001\n", "1.0 1\n"}, // unmatched on non-float: WA for all
  }, 2);
  AssertVerdictReporter reporter(Verdict::WA, true, false);
  sub.reporter = reporter.GetReporter();
  auto orig_score = sub.reporter.ReportScoringResult;
  sub.reporter.ReportScoringResult = [&](const Submission& sub, const SubmissionResult& res, int subtask, int stage) {
    orig_score(sub, res, subtask, stage);
    ASSERT_EQ(res.td_results[subtask].verdict, param.verdicts[subtask]) << "Subtask " << subtask;
  };
  sub.default_scoring_args = param.default_scoring_args;
  long id = SetupSubmission(sub, 5, Compiler::GCC_CPP_17, kTime, true, R"(#include <unistd.h>
char buf[256];
int main(){ write(1, buf, read(0, buf, 256)); })", SpecjudgeType::NORMAL);
  RunAndTeardownSubmission(id);
}

constexpr Verdict AC = Verdict::AC;
constexpr Verdict WA = Verdict::WA;

INSTANTIATE_TEST_SUITE_P(OneSubmission, ExampleProblemDefaultScoring,
    testing::Values(
      (ScoringParam){{"strict"},
        {{AC, WA, WA, WA, WA, WA, WA, WA, WA, WA}}},
      (ScoringParam){{"line"},
        {{AC, AC, WA, WA, WA, WA, WA, WA, WA, WA}}},
      (ScoringParam){{},
        {{AC, AC, WA, WA, WA, WA, WA, WA, WA, WA}}},
      (ScoringParam){{"white-diff"},
        {{AC, AC, AC, AC, WA, WA, WA, WA, WA, WA}}},
      (ScoringParam){{"float-diff", "absolute-relative", "1e-9"},
        {{AC, AC, AC, AC, WA, WA, WA, WA, WA, WA}}},
      (ScoringParam){{"float-diff", "absolute", "1e-6"},
        {{AC, AC, AC, AC, AC, AC, WA, WA, WA, WA}}},
      (ScoringParam){{"float-diff", "absolute"},
        {{AC, AC, AC, AC, AC, AC, WA, WA, WA, WA}}},
      (ScoringParam){{"float-diff", "relative"},
        {{AC, AC, AC, AC, AC, AC, WA, AC, WA, WA}}},
      (ScoringParam){{"float-diff", "absolute-relative"},
        {{AC, AC, AC, AC, AC, AC, WA, AC, AC, WA}}},
      (ScoringParam){{"float-diff"},
        {{AC, AC, AC, AC, AC, AC, WA, AC, AC, WA}}},
      (ScoringParam){{"float-diff", "relative", "1e-4"},
        {{AC, AC, AC, AC, AC, AC, AC, AC, AC, WA}}}
    ),
    ParamName);
