#include <algorithm>
#include <tioj/utils.h>

#include "example_problem.h"
#include "utils.h"

namespace {

constexpr long kTime = 1660000000;

struct SubParam {
  int sub_id;
  Verdict verdict;
  std::string code;
};

std::string ParamName(const ::testing::TestParamInfo<SubParam>& info) {
  std::string ret = std::string("verdict_") + VerdictToAbr(info.param.verdict);
  if (info.param.sub_id > 0) ret += "_" + std::to_string(info.param.sub_id);
  return ret;
}

} // namespace

class ExampleProblemVerdict : public ExampleProblem, public testing::WithParamInterface<SubParam> {};
TEST_P(ExampleProblemVerdict, Ver) {
  auto& param = GetParam();
  kMaxParallel = 2;
  SetUp(1, 2);
  AssertVerdictReporter reporter(param.verdict, (int)param.verdict < (int)Verdict::CE);
  sub.reporter = reporter.GetReporter();
  Submission sub1 = sub, sub2 = sub;
  int id1 = SetupSubmission(sub1, param.sub_id+1, Compiler::GCC_CPP_17, kTime, true, param.code);
  int id2 = SetupSubmission(sub2, param.sub_id+2, Compiler::GCC_CPP_17, kTime, false, param.code);
  PushSubmission(std::move(sub1));
  PushSubmission(std::move(sub2));
  WorkLoop(false);
  TeardownSubmission(id1);
  TeardownSubmission(id2);
}
INSTANTIATE_TEST_SUITE_P(OneSubmission, ExampleProblemVerdict,
    testing::Values(
      (SubParam){0, Verdict::AC, R"(#include <cstdio>
int main(){ int a; scanf("%d",&a);printf("%d",a); })"},
      (SubParam){0, Verdict::WA, R"(#include <cstdio>
int main(){ int a; scanf("%d",&a);printf("%d",a+1); })"},
      (SubParam){0, Verdict::TLE, R"(int main(){ while (true); })"},
      (SubParam){0, Verdict::MLE, R"(#include <cstdlib>
#include <cstring>
int main(){ while (true) memset(malloc(65536), 0x1, 65536); })"},
      (SubParam){0, Verdict::OLE, R"(#include <cstdio>
int main(){ while (true) puts("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"); })"},
      (SubParam){0, Verdict::RE, R"(int main(){ return 1; })"},
      (SubParam){1, Verdict::SIG, R"(char* p; int main(){ *p = 123; })"},
      (SubParam){2, Verdict::SIG, R"(int c; int main(){ c /= 0; })"},
      (SubParam){0, Verdict::CE, ""}
    ),
    ParamName);
