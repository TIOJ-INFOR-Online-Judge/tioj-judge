#ifndef TIOJ_UTILS_H_
#define TIOJ_UTILS_H_

#include <sys/wait.h>

#include <filesystem>

#include <tioj/utils.h>

namespace fs = std::filesystem;

#define IGNORE_RETURN(x) { auto _ __attribute__((unused)) = x; }

constexpr fs::perms kPerm666 = fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read |
                               fs::perms::group_write | fs::perms::others_read | fs::perms::others_write;

fs::path InsideBox(const fs::path& box, const fs::path& path);

bool SpliceProcess(int read_fd, int write_fd, size_t max_size = std::numeric_limits<size_t>::max());

bool MountTmpfs(const fs::path&, long size_kib);
bool Umount(const fs::path&);
bool CreateDirs(const fs::path&, fs::perms = fs::perms::unknown);
bool RemoveAll(const fs::path&, bool ignore_error = false);
// lchown recursively, without following symlinks. Abort immediately and returns false if any lchown fails
bool ChownAll(const fs::path&, uid_t, gid_t);

// These functions resolve symlinks; Move allows cross-device move
bool Move(const fs::path& from, const fs::path& to, fs::perms = fs::perms::unknown);
bool Copy(const fs::path& from, const fs::path& to, fs::perms = fs::perms::unknown);

// Run a function as root without changing the current process's uid/gid
template <class Func>
bool AsRoot(Func&& fn) {
  pid_t pid = fork();
  if (pid < 0) return false;
  if (pid == 0) {
    if (setresuid(0, 0, 0) != 0 || setresgid(0, 0, 0) != 0) _exit(2);
    bool ok = false;
    try {
      ok = fn();
    } catch (...) {
    }
    _exit(ok ? 0 : 1);
  }
  int status = 0;
  return waitpid(pid, &status, 0) >= 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

#endif // TIOJ_UTILS_H_
