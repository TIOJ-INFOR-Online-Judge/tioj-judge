#include "example_problem.h"

#include <fstream>
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

TEST_F(ExampleProblem, OneCppSubmission) {
  max_parallel = 1;
  SetUp(1, 5, false);
  long id = SetupSubmission(sub, 1, Compiler::GCC_CPP_17, kTime, R"(#include <cstdio>
int main(){ int a; scanf("%d",&a);printf("%d",a); })");
  PushSubmission(std::move(sub));
  WorkLoop(false);
  TeardownSubmission(id);
}

TEST_F(ExampleProblem, OneCppSubmissionParallelStrict) {
  max_parallel = 4;
  SetUp(2, 5, true);
  long id = SetupSubmission(sub, 1, Compiler::GCC_CPP_17, kTime, R"(#include <cstdio>
int main(){ int a; scanf("%d",&a);printf("%d",a); })");
  PushSubmission(std::move(sub));
  WorkLoop(false);
  TeardownSubmission(id);
}
