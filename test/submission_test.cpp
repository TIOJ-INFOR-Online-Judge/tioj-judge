#include <algorithm>
#include <tioj/utils.h>

#include "example_problem.h"
#include "utils.h"

namespace {

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
}

} // namespace

class ExampleProblemOneSubmission : public ExampleProblem, public testing::WithParamInterface<SubParam> {};
TEST_P(ExampleProblemOneSubmission, Sub) {
  auto& param = GetParam();
  kMaxParallel = param.parallel;
  SetUp(1, 5);
  AssertVerdictReporter reporter(Verdict::AC);
  sub.reporter = &reporter;
  long id = SetupSubmission(sub, param.sub_id, param.lang, kTime, param.is_strict, param.code);
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
  SetUp(2, 3);
  AssertVerdictReporter reporter(Verdict::AC);
  sub.reporter = &reporter;
  long id = SetupSubmission(sub, 5, Compiler::GCC_CPP_17, kTime, true, R"(#include <cstdio>
int main(){ int a; scanf("%d",&a);printf("%d",12345); })", SpecjudgeType::SPECJUDGE_OLD, R"(#include <cstdio>
#include "nlohmann/json.hpp"
int main(){ puts("0"); })");
  PushSubmission(std::move(sub));
  WorkLoop(false);
  TeardownSubmission(id);
}

TEST_F(ExampleProblem, SpecjudgeNewProblemOneSubmission) {
  kMaxParallel = 2;
  SetUp(3, 3);
  AssertVerdictReporter reporter(Verdict::AC);
  sub.reporter = &reporter;
  long id = SetupSubmission(sub, 6, Compiler::GCC_CPP_17, kTime, true, R"(#include <cstdio>
int main(){ int a; scanf("%d",&a);printf("%d",12345); })", SpecjudgeType::SPECJUDGE_NEW, R"(#include <iostream>
#include "nlohmann/json.hpp"
int main(){ std::cout << nlohmann::json{{"verdict", "AC"}, {"score", "102.2"}}; })");
  PushSubmission(std::move(sub));
  WorkLoop(false);
  TeardownSubmission(id);
}

TEST_F(ExampleProblem, Multistage) {
  kMaxParallel = 2;
  SetUp(4, 2);
  AssertVerdictReporter reporter(Verdict::AC);
  sub.reporter = &reporter;
  sub.stages = 3;
  long id = SetupSubmission(sub, 7, Compiler::GCC_CPP_17, kTime, false, R"(#include <cstdio>
int main(int argc, char** argv){ int a; scanf("%d",&a); printf("%d", a+argv[1][0]-'1'); })");
  PushSubmission(std::move(sub));
  WorkLoop(false);
  TeardownSubmission(id);
}

TEST_F(ExampleProblem, MultistageTL) {
  kMaxParallel = 2;
  SetUp(4, 2);
  AssertVerdictReporter reporter(Verdict::TLE);
  sub.reporter = &reporter;
  sub.stages = 3;
  long id = SetupSubmission(sub, 8, Compiler::GCC_CPP_17, kTime, false, R"(#include <ctime>
int main(int argc, char** argv){ auto a = clock(); while (clock()-a<0.6*CLOCKS_PER_SEC); })",
      SpecjudgeType::SPECJUDGE_NEW, R"(#include <iostream>
#include <fstream>
#include "nlohmann/json.hpp"
int main(int argc, char**argv){
  std::ifstream fin(argv[1]); nlohmann::json data; fin >> data;
  std::cout << nlohmann::json{{"verdict", data["stats"]["original_verdict"].get<std::string>()}};
})");
  PushSubmission(std::move(sub));
  WorkLoop(false);
  TeardownSubmission(id);
}

// TODO: multiple submission rejudge
