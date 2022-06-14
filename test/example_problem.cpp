#include "example_problem.h"

#include <fstream>
#include <paths.h>
#include <utils.h>

void ExampleProblem::SetUp(int problem_id_, int td_num, bool sandbox_strict) {
  problem_id = problem_id_;
  CreateDirs(TdPath(problem_id));
  for (int i = 0; i < td_num; i++) {
    std::ofstream f1(TdInput(problem_id, i)), f2(TdOutput(problem_id, i));
    f1 << i << std::endl;
    f2 << i << std::endl;
    sub.td_limits.push_back({65536, 65536, 65536, 1'000'000});
  }
  sub.sandbox_strict = sandbox_strict;
  sub.problem_id = problem_id;
}

void ExampleProblem::TearDown() {
  RemoveAll(TdPath(problem_id));
}
