#include "example_problem.h"

#include <fstream>
#include <tioj/paths.h>

void ExampleProblem::SetUp(int problem_id_, int td_num) {
  problem_id = problem_id_;
  fs::create_directories(TdPath(problem_id));
  for (int i = 0; i < td_num; i++) {
    std::ofstream f1(TdInput(problem_id, i)), f2(TdOutput(problem_id, i));
    f1 << i << std::endl;
    f2 << i << std::endl;
    sub.td_limits.push_back({65536, 65536, 65536, 1'000'000, false});
  }
  sub.problem_id = problem_id;
}

void ExampleProblem::TearDown() {
  fs::remove_all(TdPath(problem_id));
}
