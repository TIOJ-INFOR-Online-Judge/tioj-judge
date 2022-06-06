#include "utils.h"

#include <sys/mount.h>

const char* VerdictToStr(Verdict verdict) {
  switch (verdict) {
    case AC:
      return "Accepted";
    case WA:
      return "Wrong Answer";
    case TLE:
      return "Time Limit Exceeded";
    case MLE:
      return "Memory Limit Exceeded";
    case OLE:
      return "Output Limit Exceeded";
    case RE:
      return "Runtime Error (exited with nonzero status)";
    case SIG:
      return "Runtime Error (exited with signal)";
    case CE:
      return "Compile Error";
    case CO:
      return "Compilation Timed Out";
    case ER:
      return "WTF!";
    default:
      return "nil";
  }
}

const char* VerdictToAbr(Verdict verdict) {
  switch (verdict) {
    case AC:
      return "AC";
    case WA:
      return "WA";
    case TLE:
      return "TLE";
    case MLE:
      return "MLE";
    case OLE:
      return "OLE";
    case RE:
      return "RE";
    case SIG:
      return "SIG";
    case CE:
      return "CE";
    case CO:
      return "CO";
    case ER:
      return "ER";
    default:
      return "";
  }
}

bool MountTmpfs(const std::filesystem::path& path, long size_kib) {
  return 0 == mount("tmpfs", path.c_str(), "tmpfs", 0,
                    ("size=" + std::to_string(size_kib) + 'k').c_str());
}

bool Umount(const std::filesystem::path& path) {
  return 0 == umount(path.c_str());
}

bool Move(const std::filesystem::path& from, const std::filesystem::path& to) {
  std::error_code ec;
  std::filesystem::rename(from, to, ec);
  if (!ec) return true;
  if (ec.value() != EXDEV) return false;
  std::filesystem::copy(from, to, ec);
  return !ec;
}
