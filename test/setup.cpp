#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <tioj/paths.h>

class MyEnvironment : public ::testing::Environment {
 public:
  void SetUp() override {
    spdlog::set_pattern("[%P] %+");
    spdlog::set_level(spdlog::level::debug);
  }
  void TearDown() override {
    fs::remove_all(TdRoot());
    fs::remove_all(kSubmissionRoot);
  }
};

testing::Environment* const my_env = testing::AddGlobalTestEnvironment(new MyEnvironment);

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc) kDataDir = fs::path(argv[0]).parent_path();
  return RUN_ALL_TESTS();
}
