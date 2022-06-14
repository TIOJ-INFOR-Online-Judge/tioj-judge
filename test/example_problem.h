#ifndef EXAMPLE_PROBLEM_H_
#define EXAMPLE_PROBLEM_H_

#include <gtest/gtest.h>
#include <submission.h>

class ExampleProblem : public ::testing::Test {
 protected:
  void SetUp(int problem_id_, int td_num, bool sandbox_strict);
  void TearDown() override;

  Submission sub;
  int problem_id;
};

#endif  // EXAMPLE_PROBLEM_H_
