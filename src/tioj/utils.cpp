#include "utils.h"

#include <fcntl.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <atomic>

#include <spdlog/spdlog.h>

namespace {

std::atomic_long submission_internal_id_seq = 0;

#if __has_include(<linux/close_range.h>)
#include <linux/close_range.h>
int CloseFrom(int minfd) {
  return close_range(minfd, ~0U, 0);
}
#else
#include <dirent.h>
int CloseFrom(int minfd) {
  DIR *fddir = opendir("/proc/self/fd");
  if (!fddir) goto error;
  {
    int dfd = dirfd(fddir);
    for (struct dirent *dent; (dent = readdir(fddir));) {
      if (!strcmp(dent->d_name, ".") || !strcmp(dent->d_name, "..")) continue;
      int fd = strtol(dent->d_name, NULL, 10);
      if (fd >= minfd && fd != dfd) {
        if (close(fd) && errno != EBADF) goto error_dir;
      }
    }
  }
  closedir(fddir);
  return 0;

error_dir:
  closedir(fddir);
error:
  return -1;
}
#endif // has_include(<linux/close_range.h>)

} // namespace

long GetUniqueSubmissionInternalId() {
  return ++submission_internal_id_seq;
}

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
  for (int i = (int)Verdict::CE; i <= (int)Verdict::JE; i++) {
    if (str == kVerdictAbrTable[i]) return (Verdict)i;
  }
  return Verdict::NUL;
}

static const char* kCompilerNameTable[] = {
#define X(name, abr) abr,
  ENUM_COMPILER_
#undef X
};

const char* CompilerName(Compiler compiler) {
  return kCompilerNameTable[(int)compiler];
}

Compiler GetCompiler(const std::string& str) {
  for (size_t i = 0; i < sizeof(kCompilerNameTable) / sizeof(kCompilerNameTable[0]); i++) {
    if (str == kCompilerNameTable[i]) return (Compiler)i;
  }
  return Compiler::GCC_CPP_14;
}

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

bool SpliceProcess(int read_fd, int write_fd) {
  pid_t pid = fork();
  if (pid < 0) return false;
  if (pid == 0) {
    pid_t pid2 = fork();
    if (pid2 < 0) _exit(1);
    if (pid2 == 0) {
      dup2(read_fd, 0);
      dup2(write_fd, 1);
      CloseFrom(2);
      while (splice(0, nullptr, 1, nullptr, 65536, 0) > 0);
    }
    _exit(0);
  }
  int status;
  if (waitpid(pid, &status, 0) < 0) return false;
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return false;
  return true;
}

bool MountTmpfs(const fs::path& path, long size_kib) {
  spdlog::debug("Mount tmpfs on {}, size {}", path.c_str(), size_kib);
  bool ret = 0 == mount("tmpfs", path.c_str(), "tmpfs", 0,
                        ("size=" + std::to_string(size_kib) + 'k').c_str());
  if (!ret) spdlog::warn("Failed mounting tmpfs on {}: {}", path.c_str(), strerror(errno));
  return ret;
}

bool Umount(const fs::path& path) {
  spdlog::debug("Umount {}", path.c_str());
  bool ret = 0 == umount(path.c_str());
  if (!ret) spdlog::warn("Failed unmounting {}: {}", path.c_str(), strerror(errno));
  return ret;
}

bool CreateDirs(const fs::path& path, fs::perms perms) {
  spdlog::debug("Create directories {}", path.c_str());
  std::error_code ec;
  fs::create_directories(path, ec);
  if (ec) goto err;
  if (perms == fs::perms::unknown) return true;
  fs::permissions(path, perms, ec);
  if (ec) goto err;
  return true;
err:
  spdlog::warn("Failed creating directory {}: {}", path.c_str(), strerror(ec.value()));
  return false;
}

bool RemoveAll(const fs::path& path) {
  spdlog::debug("Delete {}", path.c_str());
  std::error_code ec;
  fs::remove_all(path, ec);
  if (ec) goto err;
  return true;
err:
  spdlog::warn("Failed deleting {}: {}", path.c_str(), strerror(ec.value()));
  return false;
}

bool Move(const fs::path& from, const fs::path& to, fs::perms perms) {
  spdlog::debug("Move file {} -> {}", from.c_str(), to.c_str());
  std::error_code ec;
  fs::rename(from, to, ec);
  if (ec) {
    if (ec.value() != EXDEV) goto err;
    fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
    if (ec) goto err;
    fs::remove(from, ec);
  }
  if (perms == fs::perms::unknown) return true;
  fs::permissions(to, perms, ec);
  if (ec) goto err;
  return true;
err:
  spdlog::warn("Failed moving {} -> {}: {}", from.c_str(), to.c_str(), strerror(ec.value()));
  return false;
}

bool Copy(const fs::path& from, const fs::path& to, fs::perms perms) {
  spdlog::debug("Copy file {} -> {}", from.c_str(), to.c_str());
  std::error_code ec;
  fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
  if (ec) goto err;
  if (perms == fs::perms::unknown) return true;
  fs::permissions(to, perms, ec);
  if (ec) goto err;
  return true;
err:
  spdlog::warn("Failed copying {} -> {}: {}", from.c_str(), to.c_str(), strerror(ec.value()));
  return false;
}
