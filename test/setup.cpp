#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <tioj/paths.h>

spdlog::level::level_enum log_level;

class MyEnvironment : public ::testing::Environment {
 public:
  void SetUp() override {
    spdlog::set_pattern("[%P] %+");
    spdlog::set_level(log_level);
  }
  void TearDown() override {
    fs::remove_all(kSubmissionRoot);
  }
};

testing::Environment* const my_env = testing::AddGlobalTestEnvironment(new MyEnvironment);

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc) internal::kDataDir = fs::path(argv[0]).parent_path();
  log_level = spdlog::level::warn;
  if (argc > 1) {
    if (std::string("-v") == argv[1]) log_level = spdlog::level::info;
    if (std::string("-vv") == argv[1]) log_level = spdlog::level::debug;
  }
  return RUN_ALL_TESTS();
}
