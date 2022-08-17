#ifndef UTILS_H_
#define UTILS_H_

#include <filesystem>

#include <tioj/utils.h>

namespace fs = std::filesystem;

#define IGNORE_RETURN(x) { auto _ __attribute__((unused)) = x; }

constexpr fs::perms kPerm666 =
    fs::perms::owner_read | fs::perms::owner_write |
    fs::perms::group_read | fs::perms::group_write |
    fs::perms::others_read | fs::perms::others_write;

fs::path InsideBox(const fs::path& box, const fs::path& path);

bool SpliceProcess(int read_fd, int write_fd);

bool MountTmpfs(const fs::path&, long size_kib);
bool Umount(const fs::path&);
bool CreateDirs(const fs::path&, fs::perms = fs::perms::unknown);
bool RemoveAll(const fs::path&);

// These functions resolve symlinks; Move allows cross-device move
bool Move(const fs::path& from, const fs::path& to, fs::perms = fs::perms::unknown);
bool Copy(const fs::path& from, const fs::path& to, fs::perms = fs::perms::unknown);

#endif  // UTILS_H_
