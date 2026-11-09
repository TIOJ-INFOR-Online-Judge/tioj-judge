#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <filesystem>
#include <fstream>
#include <thread>

#include <spdlog/spdlog.h>
#include <argparse/argparse.hpp>
#include <tortellini.hh>

#include <tioj/logger.h>
#include "cpuset.h"
#include "paths.h"
#include "server_io.h"
#include "tioj/paths.h"
#include "tioj/submission.h"
#include "tioj/utils.h"

namespace {

bool to_lock = true;
uid_t uid = 0;
gid_t gid = (gid_t)-1;

void SetPinnedCPU(const std::string& cpu_str) {
  if (!CpusetParse(cpu_str.c_str(), &kPinnedCpus, get_nprocs())) {
    throw std::runtime_error("Invalid CPU mask");
  }
}

void ParseConfig(const fs::path& conf_path) {
  std::ifstream fin(conf_path);
  if (!fin) throw std::runtime_error("File not found");
  tortellini::ini ini;
  fin >> ini;
  std::string box_root = ini[""]["box_root"] | "";
  std::string submission_root = ini[""]["submission_root"] | "";
  std::string testdata_root = ini[""]["testdata_root"] | "";
  if (box_root.size()) kBoxRoot = box_root;
  if (submission_root.size()) kSubmissionRoot = submission_root;
  if (testdata_root.size()) kTestdataRoot = testdata_root;

  std::string sandbox_mounts = ini[""]["sandbox_mounts"] | "";
  if (sandbox_mounts.empty()) {
    if (char* mounts = getenv("TIOJ_JUDGE_MOUNTS")) {
      sandbox_mounts = mounts;
    } else if (char* path = getenv("PATH")) {
      sandbox_mounts = path;
    }
  }
  kSandboxMounts = SplitSandboxPath(sandbox_mounts);

  kMaxParallel = ini[""]["parallel"] | kMaxParallel;
  SetPinnedCPU(ini[""]["pinned_cpus"] | "none");
  kMaxRSS = (ini[""]["max_rss_per_task_mb"] | (kMaxRSS / 1024)) * 1024;
  kMaxOutput = (ini[""]["max_output_per_task_mb"] | (kMaxRSS / 1024)) * 1024;
  kMaxQueue = ini[""]["max_submission_queue_size"] | (kMaxParallel + 2);
  kTimeMultiplier = ini[""]["time_multiplier"] | kTimeMultiplier;
  kTIOJUrl = ini[""]["tioj_url"] | kTIOJUrl;
  kTIOJKey = ini[""]["tioj_key"] | kTIOJKey;

  uid = ini[""]["uid"] | uid;
  gid = ini[""]["gid"] | gid;
}

void ParseArgs(int argc, char** argv) {
  int verbosity = 0;
  argparse::ArgumentParser parser(argc ? argv[0] : "tioj-judge", kVersionCode,
                                  argparse::default_arguments::help);
  parser.add_argument("--version").default_value(false).implicit_value(true).help("Show version and exit");
  parser.add_argument("-c", "--config")
      .required()
      .default_value(std::string("/etc/tioj-judge.conf"))
      .help("Path of configuration file");
  parser.add_argument("-v", "--verbose")
      .action([&](const auto&) { ++verbosity; })
      .append()
      .default_value(false)
      .implicit_value(true)
      .nargs(0)
      .help("Verbose level");
  parser.add_argument("-p", "--parallel").scan<'d', int>().help("Number of maximum parallel judge tasks");
  parser.add_argument("-m", "--time-multiplier")
      .scan<'g', double>()
      .help("Ratio of real time to indicated time");
  parser.add_argument("-u", "--uid").scan<'d', uid_t>().help("User ID to run as");
  parser.add_argument("-g", "--gid").scan<'d', gid_t>().help("Group ID to run as");
  parser.add_argument("--no-lock")
      .default_value(false)
      .implicit_value(true)
      .help("Not check for other running instances");
  parser.add_argument("--pinned-cpus")
      .help("Comma-separated list of CPUs to pin or simply \"all\" or \"none\"");

  try {
    parser.parse_args(argc, argv);
  } catch (const std::runtime_error& err) {
    std::cerr << err.what() << std::endl;
    std::cerr << parser;
    exit(1);
  }

  if (parser["--version"] == true) {
    std::cout << kVersionCode << std::endl;
    exit(0);
  }
  switch (verbosity) {
    case 0: spdlog::set_level(spdlog::level::warn); break;
    case 1: spdlog::set_level(spdlog::level::info); break;
    default: spdlog::set_level(spdlog::level::debug); break;
  }
  fs::path config_file = parser.get<std::string>("--config");
  try {
    ParseConfig(config_file);
  } catch (const std::runtime_error& err) {
    spdlog::error("Failed to parse configuration file {}: {}", std::string(config_file), err.what());
    exit(1);
  }
  if (auto val = parser.present<int>("--parallel")) {
    kMaxParallel = val.value();
  }
  if (auto val = parser.present<double>("--time-multiplier")) {
    kTimeMultiplier = val.value();
  }
  to_lock = parser["--no-lock"] == false;
  if (auto val = parser.present<std::string>("--pinned-cpus")) {
    try {
      SetPinnedCPU(val.value());
    } catch (const std::runtime_error& err) {
      spdlog::error("{}", err.what());
      exit(1);
    }
  }
  if (CPU_COUNT(&kPinnedCpus) && CPU_COUNT(&kPinnedCpus) < kMaxParallel) {
    spdlog::warn("Parallelism larger than the number of pinned CPUs. Some tasks may not be pinned.");
  }
  if (auto val = parser.present<uid_t>("--uid")) uid = val.value();
  if (auto val = parser.present<gid_t>("--gid")) gid = val.value();
}

bool LockFile() {
  fs::path lock_file = kTestdataRoot / "lock";
  int fd = open(lock_file.c_str(), O_RDWR | O_CREAT, 0644);
  if (fd < 0) return false;
  struct flock lock{};
  lock.l_type = F_WRLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = lock.l_len = 0;
  if (fcntl(fd, F_SETLK, &lock) < 0) return false;
  return true;
}

bool InitDir(fs::path path) {
  try {
    if (fs::exists(path)) {
      struct stat st{};
      if (stat(path.c_str(), &st) != 0) {
        spdlog::error("Failed to stat {}: {}", path.string(), strerror(errno));
        return false;
      }
      if (st.st_uid != uid || st.st_gid != gid) {
        spdlog::debug("Changing ownership of {} to {}:{}", path.string(), uid, gid);
        if (!AsRoot([&]() { return ChownAll(path, uid, gid); })) {
          spdlog::error("Failed to chown {} to {}:{}", path.string(), uid, gid);
          return false;
        }
        // change everything to 755 or 644
        for (const fs::directory_entry& entry :
             fs::recursive_directory_iterator(path, fs::directory_options::none)) {
          fs::perms perms = fs::status(entry.path()).permissions();
          fs::perms new_perms =
              static_cast<fs::perms>((perms & fs::perms::owner_exec) == fs::perms::none ? 0644 : 0755);
          if (perms != new_perms) fs::permissions(entry.path(), new_perms);
        }
      }
    } else {
      spdlog::debug("Creating directory {}", path.string());
      fs::create_directories(path);
    }
  } catch (const std::exception& e) {
    spdlog::error("Failed to initialize directory {}: {}", path.string(), e.what());
    return false;
  }
  return true;
}

// initialize directories to the correct ownership
bool InitDirs() {
  return InitDir(kBoxRoot) && InitDir(kSubmissionRoot) && InitDir(kTestdataRoot) &&
         InitDir(internal::kDataDir);
}

} // namespace

int main(int argc, char** argv) {
  spdlog::set_pattern("[%t] %+");
  InitLogger();
  umask(0022);
  ParseArgs(argc, argv);
  if (geteuid() != 0) {
    spdlog::error("Must be run as root.");
    return 1;
  }
  if (uid == 0) {
    spdlog::error("A non-root user ID must be specified.");
    return 1;
  }
  if (gid == (gid_t)-1) {
    spdlog::error("A group ID must be specified.");
    return 1;
  }
  if (to_lock && !LockFile()) {
    spdlog::error("Another judge instance is running.");
    return 1;
  }
  if (setresgid(gid, gid, gid) != 0 || setresuid(uid, uid, 0) != 0) {
    spdlog::error("Failed to setresuid/setresgid: {}", strerror(errno));
    return 1;
  }
  if (!InitDirs()) return 1;
  std::thread server_thread(ServerWorkLoop);
  server_thread.detach();
  WorkLoop();
}
