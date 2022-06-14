#include "example_problem.h"

#include <fstream>
#include <algorithm>
#include <paths.h>
#include <utils.h>
#include <submission.h>

long SetupSubmission(Submission& sub, int id, Compiler lang, long time, const std::string& code) {
  static int sub_id = 0;
  sub.submission_id = id;
  long iid = sub.submission_internal_id = sub_id++;
  sub.submitter_id = 10;
  sub.submission_time = time;
  sub.lang = lang;
  CreateDirs(SubmissionCodePath(iid));
  std::ofstream fout(SubmissionUserCode(iid));
  fout << code;
  return iid;
}

void TeardownSubmission(long id) {
  RemoveAll(SubmissionCodePath(id));
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
  max_parallel = param.parallel;
  SetUp(param.sub_id, 5, param.is_strict);
  long id = SetupSubmission(sub, 1, param.lang, kTime, param.code);
  PushSubmission(std::move(sub));
  // TODO: auto-check if it is AC
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

// TODO: multiple submission (normal, rejudge)
