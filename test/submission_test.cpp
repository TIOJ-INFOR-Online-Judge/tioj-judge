#include <tioj/utils.h>
#include <algorithm>

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
  std::replace(ret.begin(), ret.end(), ' ', '_');
  if (info.param.is_strict) ret += "_strict";
  if (info.param.parallel > 1) ret += "_parallel";
  return ret;
}

} // namespace

class ExampleProblemOneSubmission : public ExampleProblem, public testing::WithParamInterface<SubParam> {};
TEST_P(ExampleProblemOneSubmission, Sub) {
  auto& param = GetParam();
  SetUp(1, 5, param.parallel);
  AssertVerdictReporter reporter(Verdict::AC);
  sub.reporter = reporter.GetReporter();
  long id = SetupSubmission(sub, param.sub_id, param.lang, kTime, param.is_strict, param.code);
  if (param.lang == Compiler::PYTHON3) {
    for (auto& i : sub.testdata) i.vss = 524288; // numpy needs more VSS
  }
  RunAndTeardownSubmission(id);
}

namespace {

const std::string kCpp17Code = R"(#include <cstdio>
static_assert(__cplusplus == 201703L, "C++17 required");
int main() { int a; scanf("%d", &a); printf("%d", a);})";
const std::string kCpp20Code = R"(#include <cstdio>
static_assert(__cplusplus == 202002L, "C++20 required");
int main() { int a; scanf("%d", &a); printf("%d", a);})";
const std::string kCpp23Code = R"(#include <cstdio>
static_assert(__cplusplus == 202302L, "C++23 required");
int main() { int a; scanf("%d", &a); printf("%d", a);})";
const std::string kC23Code = R"(#include <stdio.h>
static_assert(__STDC_VERSION__ == 202311L, "C23 required");
int main(){ int a; scanf("%d",&a); printf("%d",a); })";

} // namespace

INSTANTIATE_TEST_SUITE_P(
    OneSubmission, ExampleProblemOneSubmission,
    testing::Values(
        (SubParam){1, 1, false, Compiler::GCC_CPP_17, kCpp17Code},
        (SubParam){1, 4, true, Compiler::GCC_CPP_17, kCpp17Code},
        (SubParam){6, 1, false, Compiler::GCC_CPP_20, kCpp20Code},
        (SubParam){6, 4, true, Compiler::GCC_CPP_20, kCpp20Code},
        (SubParam){7, 1, false, Compiler::GCC_CPP_23, kCpp23Code},
        (SubParam){7, 4, true, Compiler::GCC_CPP_23, kCpp23Code},
        (SubParam){8, 1, false, Compiler::GCC_C_23, kC23Code},
        (SubParam){8, 4, true, Compiler::GCC_C_23, kC23Code},
        (SubParam){2, 1, false, Compiler::HASKELL, "main = interact $ show . sum . map read . words"},
        (SubParam){2, 1, true, Compiler::HASKELL, "main = interact $ show . sum . map read . words"},
        (SubParam){3, 4, false, Compiler::PYTHON2, "print input()"},
        (SubParam){3, 1, true, Compiler::PYTHON2, "print input()"},
        (SubParam){4, 1, false, Compiler::PYTHON3, "import numpy; print(input())"},
        (SubParam){4, 4, true, Compiler::PYTHON3, "import sys, numpy; print(sys.stdin.read())"},
        (SubParam){5, 1, false, Compiler::RUSTC_RUST_2021, R"(use std::io::{self, Read};
fn main() { let mut s = String::new(); let _ = io::stdin().read_to_string(&mut s); println!("{s}"); })"},
        (SubParam){5, 4, true, Compiler::RUSTC_RUST_2021, R"(use std::io::{self, Read};
fn main() { let mut s = String::new(); let _ = io::stdin().read_to_string(&mut s); println!("{s}"); })"}),
    ParamName);

// TODO: multiple submission rejudge
