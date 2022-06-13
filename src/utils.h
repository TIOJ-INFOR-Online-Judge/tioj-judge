#ifndef UTILS_H_
#define UTILS_H_

#include <chrono>
#include <vector>
#include <iostream>
#include <filesystem>

#include "submission.h"

namespace fs = std::filesystem;

const char* VerdictToStr(Verdict);
const char* VerdictToAbr(Verdict);
Verdict AbrToVerdict(const std::string&, bool runtime_only);
const char* CompilerName(Compiler);

constexpr fs::perms kPerm666 =
    fs::perms::owner_read | fs::perms::owner_write |
    fs::perms::group_read | fs::perms::group_write |
    fs::perms::others_read | fs::perms::others_write;

fs::path InsideBox(const fs::path& box, const fs::path& path);

bool MountTmpfs(const fs::path&, long size_kib);
bool Umount(const fs::path&);
bool CreateDirs(const fs::path&, fs::perms = fs::perms::unknown);
// allow cross-device move
bool Move(const fs::path& from, const fs::path& to, fs::perms = fs::perms::unknown);
bool Copy(const fs::path& from, const fs::path& to, fs::perms = fs::perms::unknown);

// logging
extern bool enable_log;

template <class T>
void LogNL(const T& str) {
  std::cerr << str;
}

template <class T>
void LogNL(const std::vector<T>& vec) {
  if (!vec.empty()) {
    std::cerr << vec[0];
    for (size_t i = 1; i < vec.size(); i++) std::cerr << ' ' << vec[i];
  }
}

template <class T, class... U>
void LogNL(const T& str, U&&... tail) {
  LogNL(str);
  std::cerr << ' ';
  LogNL(std::forward<U>(tail)...);
}

template <class... T>
void Log(T&&... param) {
  static char buf[100];
  if (!enable_log) return;
  using namespace std::chrono;
  auto tnow = system_clock::now();
  time_t now = system_clock::to_time_t(tnow);
  int milli = duration_cast<milliseconds>(tnow - time_point_cast<seconds>(tnow))
                  .count();
  strftime(buf, 100, "%F %T ", localtime(&now));
  std::cerr << buf << std::setfill('0') << std::setw(3) << milli << " -- ";
  LogNL(std::forward<T>(param)...);
  std::cerr << std::endl;
}

#endif  // UTILS_H_
