#include "example_problem.h"

#include <cstdlib>
#include <string>
#include <fstream>
#include <tioj/paths.h>

void ExampleProblem::SetUp(int problem_id_, int td_num) {
  {
    char td_path_tmp[256] = "/tmp/td_test_XXXXXX";
    if (!mkdtemp(td_path_tmp)) throw std::runtime_error("Failed to create");
    td_path = td_path_tmp;
  }
  problem_id = problem_id_;
  fs::create_directories(td_path);
  for (int i = 0; i < td_num; i++) {
    std::filesystem::path in_path = td_path / (std::to_string(td_num) + ".in");
    std::filesystem::path out_path = td_path / (std::to_string(td_num) + ".out");
    std::ofstream f1(in_path), f2(out_path);
    f1 << i << std::endl;
    f2 << i << std::endl;
    sub.testdata.push_back({in_path, out_path, 65536, 65536, 65536, 1'000'000, false});
  }
  sub.problem_id = problem_id;
}

void ExampleProblem::TearDown() {
  fs::remove_all(td_path);
}
