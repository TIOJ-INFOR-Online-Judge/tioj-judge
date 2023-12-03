#include "example_problem.h"

#include <cstdlib>
#include <string>
#include <fstream>
#include <tioj/paths.h>

#include "utils.h"

void ExampleProblem::SetUp(int problem_id_, int td_num, int max_parallel) {
  std::vector<std::pair<std::string, std::string>> tds;
  for (int i = 0; i < td_num; i++) tds.emplace_back(std::to_string(i) + '\n', std::to_string(i) + '\n');
  SetUp(problem_id_, tds, max_parallel);
}

void ExampleProblem::SetUp(int problem_id_, const std::vector<std::pair<std::string, std::string>>& tds, int max_parallel) {
  kMaxParallel = max_parallel;
  {
    char td_path_tmp[256] = "/tmp/td_test_XXXXXX";
    if (!mkdtemp(td_path_tmp)) throw std::runtime_error("Failed to create");
    td_path = td_path_tmp;
  }
  problem_id = problem_id_;
  fs::create_directories(td_path);
  for (int i = 0; i < (int)tds.size(); i++) {
    std::filesystem::path in_path = td_path / (std::to_string(i) + ".in");
    std::filesystem::path out_path = td_path / (std::to_string(i) + ".out");
    std::ofstream f1(in_path), f2(out_path);
    f1 << tds[i].first;
    f2 << tds[i].second;
    sub.testdata.push_back({in_path, out_path, 65536, 65536, 65536, 1'000'000, false});
  }
  sub.problem_id = problem_id;
}

void ExampleProblem::TearDown() {
  fs::remove_all(td_path);
}

void ExampleProblem::RunAndTeardownSubmission(long id) {
  PushSubmission(std::move(sub));
  WorkLoop(false);
  TeardownSubmission(id);
}
