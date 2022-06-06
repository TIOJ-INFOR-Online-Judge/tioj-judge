#ifndef UTILS_H_
#define UTILS_H_

#include <filesystem>

enum Verdict { OK = 0, AC = 10, WA, TLE, MLE, OLE, RE, SIG, CE, CO, ER };

const char* VerdictToStr(Verdict verdict);
const char* VerdictToAbr(Verdict verdict);

bool MountTmpfs(const std::filesystem::path&, long size_kib);
bool Umount(const std::filesystem::path&);
// allow cross-device move
bool Move(const std::filesystem::path& from, const std::filesystem::path& to);

#endif  // UTILS_H_
