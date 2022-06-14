#include "utils.h"

#include <sys/mount.h>

#include <spdlog/spdlog.h>

#define ENUM_SWITCH_FUNCTION(DEF, typ, mac) \
  DEF(typ param) { \
    switch (param) { \
      mac \
    } \
    __builtin_unreachable(); \
  }
#define X_RETURN_ARG1(cls, x, ...) case cls::x: return #x;
#define X_RETURN_ARG2(cls, x, y, ...) case cls::x: return y;
#define X_RETURN_ARG3(cls, x, y, z, ...) case cls::x: return z;

#define X(...) X_RETURN_ARG3(Verdict, __VA_ARGS__)
ENUM_SWITCH_FUNCTION(const char* VerdictToDesc, Verdict, ENUM_VERDICT_)
#undef X

static const char* kVerdictAbrTable[] = {
#define X(name, abr, desc) abr,
  ENUM_VERDICT_
#undef X
};

const char* VerdictToAbr(Verdict verdict) {
  return kVerdictAbrTable[(int)verdict];
}

Verdict AbrToVerdict(const std::string& str, bool runtime_only) {
  for (int i = (int)Verdict::AC; i < (int)Verdict::CE; i++) {
    if (str == kVerdictAbrTable[i]) return (Verdict)i;
  }
  if (runtime_only) return Verdict::NUL;
  for (int i = (int)Verdict::CE; i <= (int)Verdict::ER; i++) {
    if (str == kVerdictAbrTable[i]) return (Verdict)i;
  }
  return Verdict::NUL;
}

#define X(...) X_RETURN_ARG2(Compiler, __VA_ARGS__)
ENUM_SWITCH_FUNCTION(const char* CompilerName, Compiler, ENUM_COMPILER_)
#undef X

#define X(...) X_RETURN_ARG1(TaskType, __VA_ARGS__)
ENUM_SWITCH_FUNCTION(const char* TaskTypeName, TaskType, ENUM_TASK_TYPE_)
#undef X

#define X(...) X_RETURN_ARG1(CompileSubtask, __VA_ARGS__)
ENUM_SWITCH_FUNCTION(const char* CompileSubtaskName, CompileSubtask, ENUM_COMPILE_SUBTASK_)
#undef X

#define X(...) X_RETURN_ARG1(SpecjudgeType, __VA_ARGS__)
ENUM_SWITCH_FUNCTION(const char* SpecjudgeTypeName, SpecjudgeType, ENUM_SPECJUDGE_TYPE_)
#undef X

#define X(...) X_RETURN_ARG1(InterlibType, __VA_ARGS__)
ENUM_SWITCH_FUNCTION(const char* InterlibTypeName, InterlibType, ENUM_INTERLIB_TYPE_)
#undef X

#undef ENUM_SWITCH_FUNCTION
#undef X_RETURN_ARG1
#undef X_RETURN_ARG2
#undef X_RETURN_ARG3

fs::path InsideBox(const fs::path& box, const fs::path& path) {
  return "/" / path.lexically_relative(box);
}

bool MountTmpfs(const fs::path& path, long size_kib) {
  spdlog::debug("Mount tmpfs on {}, size {}", path.c_str(), size_kib);
  return 0 == mount("tmpfs", path.c_str(), "tmpfs", 0,
                    ("size=" + std::to_string(size_kib) + 'k').c_str());
}

bool Umount(const fs::path& path) {
  spdlog::debug("Umount {}", path.c_str());
  return 0 == umount(path.c_str());
}

bool CreateDirs(const fs::path& path, fs::perms perms) {
  spdlog::debug("Create directories {}", path.c_str());
  std::error_code ec;
  fs::create_directories(path, ec);
  if (ec) return false;
  if (perms == fs::perms::unknown) return true;
  fs::permissions(path, perms, ec);
  return !ec;
}

bool Move(const fs::path& from, const fs::path& to, fs::perms perms) {
  spdlog::debug("Move file {} -> {}", from.c_str(), to.c_str());
  std::error_code ec;
  fs::rename(from, to, ec);
  if (ec) {
    if (ec.value() != EXDEV) return false;
    fs::copy(from, to, ec);
    if (ec) return false;
  }
  if (perms == fs::perms::unknown) return true;
  fs::permissions(to, perms, ec);
  return !ec;
}

bool Copy(const fs::path& from, const fs::path& to, fs::perms perms) {
  spdlog::debug("Copy file {} -> {}", from.c_str(), to.c_str());
  std::error_code ec;
  fs::copy_file(from, to, ec);
  if (ec) return false;
  if (perms == fs::perms::unknown) return true;
  fs::permissions(to, perms, ec);
  return !ec;
}
