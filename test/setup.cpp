#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <paths.h>
#include <utils.h>

class MyEnvironment : public ::testing::Environment {
 public:
  void SetUp() override {
    spdlog::set_pattern("[%P] %+");
    spdlog::set_level(spdlog::level::warn);
  }
  void TearDown() override {
    RemoveAll(kTestdataRoot);
    RemoveAll(kSubmissionRoot);
  }
};

testing::Environment* const my_env = testing::AddGlobalTestEnvironment(new MyEnvironment);
