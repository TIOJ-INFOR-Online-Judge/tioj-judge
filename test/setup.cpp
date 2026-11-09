#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <tioj/paths.h>
#include "../src/tioj/utils.h"

spdlog::level::level_enum log_level;

class MyEnvironment : public ::testing::Environment {
 public:
  void SetUp() override {
    spdlog::set_pattern("[%P] %+");
    spdlog::set_level(log_level);
    spdlog::flush_on(log_level);
  }
  void TearDown() override { fs::remove_all(kSubmissionRoot); }
};

testing::Environment* const my_env = testing::AddGlobalTestEnvironment(new MyEnvironment);

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  fs::path test_dir = "/tmp/tioj_test";
  fs::path cur_dir = ".";
  if (argc) cur_dir = fs::path(argv[0]).parent_path();
  internal::kDataDir = test_dir / "data";
  kBoxRoot = test_dir / "box";
  kSubmissionRoot = test_dir / "submission";
  umask(0022);
  if (!CreateDirs(internal::kDataDir) ||
      !Copy(cur_dir / "sandbox-exec", internal::kDataDir / "sandbox-exec", fs::perms::all) ||
      !Copy(cur_dir / "default-scoring", internal::kDataDir / "default-scoring", fs::perms::all)) {
    spdlog::error("Failed to copy sandbox-exec to {}", (internal::kDataDir / "sandbox-exec").string());
    return 1;
  }
  std::filesystem::copy(cur_dir / "judge-headers", internal::kDataDir / "judge-headers",
                        std::filesystem::copy_options::recursive);
  if (!ChownAll(test_dir, 1, 1)) {
    spdlog::error("Failed to chown {} to 1:1", test_dir.string());
    return 1;
  }

  log_level = spdlog::level::warn;
  if (argc > 1) {
    if (std::string("-v") == argv[1]) log_level = spdlog::level::info;
    if (std::string("-vv") == argv[1]) log_level = spdlog::level::debug;
  }
  if (geteuid() != 0) {
    spdlog::error("Tests must be run as root.");
    return 1;
  }
  if (setresgid(1, 1, 1) != 0 || setresuid(1, 1, 0) != 0) {
    spdlog::error("Failed to setresuid/setresgid: {}", strerror(errno));
    return 1;
  }
  if (char* mounts = getenv("TIOJ_JUDGE_MOUNTS")) {
    kSandboxMounts = SplitSandboxPath(mounts);
  } else if (char* path = getenv("PATH")) {
    kSandboxMounts = SplitSandboxPath(path);
  }
  int ret = RUN_ALL_TESTS();
  AsRoot([&]() { return RemoveAll(test_dir); });
  return ret;
}
