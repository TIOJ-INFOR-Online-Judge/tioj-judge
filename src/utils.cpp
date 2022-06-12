#include "utils.h"

#include <sys/mount.h>

const char* VerdictToStr(Verdict verdict) {
  switch (verdict) {
    case Verdict::NUL: return "nil";
    case Verdict::AC: return "Accepted";
    case Verdict::WA: return "Wrong Answer";
    case Verdict::TLE: return "Time Limit Exceeded";
    case Verdict::MLE: return "Memory Limit Exceeded";
    case Verdict::OLE: return "Output Limit Exceeded";
    case Verdict::RE: return "Runtime Error (exited with nonzero status)";
    case Verdict::SIG: return "Runtime Error (exited with signal)";
    case Verdict::CE: return "Compile Error";
    case Verdict::CLE: return "Compilation Limit Exceeded";
    case Verdict::ER: return "WTF!";
  }
  __builtin_unreachable();
}

const char* VerdictToAbr(Verdict verdict) {
  switch (verdict) {
    case Verdict::NUL: return "";
    case Verdict::AC: return "AC";
    case Verdict::WA: return "WA";
    case Verdict::TLE: return "TLE";
    case Verdict::MLE: return "MLE";
    case Verdict::OLE: return "OLE";
    case Verdict::RE: return "RE";
    case Verdict::SIG: return "SIG";
    case Verdict::CE: return "CE";
    case Verdict::CLE: return "CLE";
    case Verdict::ER: return "ER";
  }
  __builtin_unreachable();
}

Verdict AbrToVerdict(const std::string& str, bool runtime_only) {
  if (str == "AC") return Verdict::AC;
  if (str == "WA") return Verdict::WA;
  if (str == "TLE") return Verdict::TLE;
  if (str == "MLE") return Verdict::MLE;
  if (str == "OLE") return Verdict::OLE;
  if (str == "RE") return Verdict::RE;
  if (str == "SIG") return Verdict::SIG;
  if (runtime_only) return Verdict::NUL;
  if (str == "CE") return Verdict::CE;
  if (str == "CLE") return Verdict::CLE;
  if (str == "ER") return Verdict::ER;
  return Verdict::NUL;
}

const char* CompilerName(Compiler compiler) {
  switch (compiler) {
    case Compiler::GCC_CPP_98: return "c++98";
    case Compiler::GCC_CPP_11: return "c++11";
    case Compiler::GCC_CPP_14: return "c++14";
    case Compiler::GCC_CPP_17: return "c++17";
    case Compiler::GCC_CPP_20: return "c++20";
    case Compiler::GCC_C_90: return "c90";
    case Compiler::GCC_C_98: return "c98";
    case Compiler::GCC_C_11: return "c11";
    case Compiler::HASKELL: return "haskell";
    case Compiler::PYTHON2: return "python2";
    case Compiler::PYTHON3: return "python3";
  }
  __builtin_unreachable();
}

fs::path InsideBox(const fs::path& box, const fs::path& path) {
  return "/" / path.lexically_relative(box);
}

bool MountTmpfs(const fs::path& path, long size_kib) {
  return 0 == mount("tmpfs", path.c_str(), "tmpfs", 0,
                    ("size=" + std::to_string(size_kib) + 'k').c_str());
}

bool Umount(const fs::path& path) {
  return 0 == umount(path.c_str());
}

bool CreateDirs(const fs::path& path, fs::perms perms) {
  std::error_code ec;
  fs::create_directories(path, ec);
  if (ec) return false;
  if (perms == fs::perms::unknown) return true;
  fs::permissions(path, perms, ec);
  return !ec;
}

bool Move(const fs::path& from, const fs::path& to, fs::perms perms) {
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
  std::error_code ec;
  fs::copy_file(from, to, ec);
  if (ec) return false;
  if (perms == fs::perms::unknown) return true;
  fs::permissions(to, perms, ec);
  return !ec;
}
