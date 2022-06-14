#ifndef UTILS_H_
#define UTILS_H_

#include <filesystem>

#include "tasks.h"
#include "submission.h"

namespace fs = std::filesystem;

#define IGNORE_RETURN(x) { auto _ __attribute__((unused)) = x; }

const char* VerdictToDesc(Verdict);
const char* VerdictToAbr(Verdict);
Verdict AbrToVerdict(const std::string&, bool runtime_only);
// logging
const char* CompilerName(Compiler);
const char* TaskTypeName(TaskType);
const char* CompileSubtaskName(CompileSubtask);
const char* SpecjudgeTypeName(SpecjudgeType);
const char* InterlibTypeName(InterlibType);

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

#endif  // UTILS_H_
