#include <unistd.h>
#include <thread>
#include <fstream>
#include <filesystem>

#include <tortellini.hh>
#include <spdlog/spdlog.h>
#include <argparse/argparse.hpp>
#include "tioj/paths.h"
#include "tioj/submission.h"
#include "server_io.h"

namespace {

bool ParseConfig(const fs::path& conf_path) {
  std::ifstream fin(conf_path);
  if (!fin) return false;
  tortellini::ini ini;
  fin >> ini;
  std::string box_root = ini[""]["box_root"] | "";
  std::string submission_root = ini[""]["submission_root"] | "";
  std::string data_dir = ini[""]["data_dir"] | "";
  if (box_root.size()) kBoxRoot = box_root;
  if (submission_root.size()) kSubmissionRoot = submission_root;
  if (data_dir.size()) kDataDir = data_dir;
  kMaxParallel = ini[""]["parallel"] | kMaxParallel;
  kFetchInterval = ini[""]["fetch_interval"] | kFetchInterval;
  kTIOJUrl = ini[""]["tioj_url"] | kTIOJUrl;
  kTIOJKey = ini[""]["tioj_key"] | kTIOJKey;
  return true;
}

void ParseArgs(int argc, char** argv) {
  int verbosity = 0;
  argparse::ArgumentParser parser(argc ? argv[0] : "tioj-judge");
  parser.add_argument("-c", "--config")
    .required().default_value(std::string("/etc/tioj-judge.conf"))
    .help("Path of configuration file");
  parser.add_argument("-v", "--verbose")
    .action([&](const auto &) { ++verbosity; })
    .append().default_value(false).implicit_value(true).nargs(0)
    .help("Verbose level");
  parser.add_argument("-p", "--parallel")
    .scan<'d', int>()
    .help("Number of maximum parallel judge tasks");
  parser.add_argument("-i", "--interval")
    .scan<'g', double>()
    .help("Submission fetching interval (in seconds)");

  try {
    parser.parse_args(argc, argv);
  } catch (const std::runtime_error& err) {
    std::cerr << err.what() << std::endl;
    std::cerr << parser;
    exit(1);
  }

  switch (verbosity) {
    case 0: spdlog::set_level(spdlog::level::warn); break;
    case 1: spdlog::set_level(spdlog::level::info); break;
    default: spdlog::set_level(spdlog::level::debug); break;
  }
  fs::path config_file = parser.get<std::string>("--config");
  if (!ParseConfig(config_file)) {
    spdlog::error("Failed to parse configuration file {}", std::string(config_file));
    exit(1);
  }
  if (auto val = parser.present<int>("--parallel")) {
    kMaxParallel = val.value();
  }
  if (auto val = parser.present<double>("--interval")) {
    kFetchInterval = val.value();
  }
}

} // namespace

int main(int argc, char** argv) {
  spdlog::set_pattern("[%t] %+");
  if (geteuid() != 0) {
    spdlog::error("Must be run as root.");
    return 0;
  }
  ParseArgs(argc, argv);
  std::thread server_thread(ServerWorkLoop);
  server_thread.detach();
  WorkLoop();
}
