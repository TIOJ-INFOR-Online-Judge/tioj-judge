#include "paths.h"

const std::filesystem::path kBoxRoot = "/tmp/tioj_box";
const std::filesystem::path kTestdataRoot = "./testdata";

static const char kWorkdirRelative[] = "workdir";

std::string PadInt(long x, size_t width) {
  std::string ret = std::to_string(x);
  if (ret.size() < width) ret = std::string(width - ret.size(), '0') + ret;
  return ret;
}

static inline const char* CodeExtension(Compiler lang) {
  switch (lang) {
    case Compiler::GCC_CPP_98: [[fallthrough]];
    case Compiler::GCC_CPP_11: [[fallthrough]];
    case Compiler::GCC_CPP_14: [[fallthrough]];
    case Compiler::GCC_CPP_17: [[fallthrough]];
    case Compiler::GCC_CPP_20: return ".cpp";
    case Compiler::GCC_C_90: [[fallthrough]];
    case Compiler::GCC_C_98: [[fallthrough]];
    case Compiler::GCC_C_11: return ".c";
    case Compiler::HASKELL: return ".hs";
    case Compiler::PYTHON2: [[fallthrough]];
    case Compiler::PYTHON3: return ".py";
  }
  __builtin_unreachable();
}

static inline const char* ProgramExtension(Compiler lang) {
  switch (lang) {
    case Compiler::GCC_CPP_98: [[fallthrough]];
    case Compiler::GCC_CPP_11: [[fallthrough]];
    case Compiler::GCC_CPP_14: [[fallthrough]];
    case Compiler::GCC_CPP_17: [[fallthrough]];
    case Compiler::GCC_CPP_20: [[fallthrough]];
    case Compiler::GCC_C_90: [[fallthrough]];
    case Compiler::GCC_C_98: [[fallthrough]];
    case Compiler::GCC_C_11: return "";
    case Compiler::HASKELL: return "";
    case Compiler::PYTHON2: [[fallthrough]];
    case Compiler::PYTHON3: return ".pyc";
  }
  __builtin_unreachable();
}

std::filesystem::path SubPath(long id) {
  return kBoxRoot / PadInt(id, 6);
}
std::filesystem::path CompileBoxPath(long id, CompileSubtask subtask) {
  return SubPath(id) / ("compile" + std::to_string((int)subtask));
}
std::filesystem::path CompileBoxInput(long id, CompileSubtask subtask, Compiler lang) {
  std::string extension = CodeExtension(lang);
  switch (subtask) {
    case CompileSubtask::USERPROG:
      return CompileBoxPath(id, subtask) / kWorkdirRelative / ("prog" + extension);
    case CompileSubtask::SPECJUDGE:
      return CompileBoxPath(id, subtask) / kWorkdirRelative / ("judge" + extension);
  }
  __builtin_unreachable();
}
std::filesystem::path CompileBoxOutput(long id, CompileSubtask subtask, Compiler lang) {
  std::string extension = ProgramExtension(lang);
  switch (subtask) {
    case CompileSubtask::USERPROG:
      return CompileBoxPath(id, subtask) / kWorkdirRelative / ("prog" + extension);
    case CompileSubtask::SPECJUDGE:
      return CompileBoxPath(id, subtask) / kWorkdirRelative / ("judge" + extension);
  }
  __builtin_unreachable();
}
std::filesystem::path ExecuteBoxPath(long id, int td) {
  return SubPath(id) / ("execute" + PadInt(td, 3));
}
std::filesystem::path ExecuteBoxProgram(long id, int td, Compiler lang) {
  return ExecuteBoxPath(id, td) / kWorkdirRelative / ("prog" + std::string(ProgramExtension(lang)));
}
std::filesystem::path ExecuteBoxTdStrictPath(long id, int td) {
  return ExecuteBoxPath(id, td) / "td";
}
std::filesystem::path ExecuteBoxInput(long id, int td, bool strict) {
  if (strict) {
    return ExecuteBoxTdStrictPath(id, td) / "input";
  } else {
    return ExecuteBoxPath(id, td) / kWorkdirRelative / "input";
  }
}
std::filesystem::path ExecuteBoxOutput(long id, int td, bool strict) {
  if (strict) {
    return ExecuteBoxTdStrictPath(id, td) / "output";
  } else {
    return ExecuteBoxPath(id, td) / kWorkdirRelative / "output";
  }
}
std::filesystem::path ExecuteBoxError(long id, int td) {
  return ExecuteBoxPath(id, td) / kWorkdirRelative / "error";
}
// TODO: pin
std::filesystem::path ScoringBoxPath(long id, int td) {
  return SubPath(id) / ("scoring" + PadInt(td, 3));
}
std::filesystem::path ScoringBoxProgram(long id, int td, Compiler lang) {
  return ScoringBoxPath(id, td) / kWorkdirRelative / ("prog" + std::string(ProgramExtension(lang)));
}
std::filesystem::path ScoringBoxCode(long id, int td, Compiler lang) {
  return ScoringBoxPath(id, td) / kWorkdirRelative / ("code" + std::string(CodeExtension(lang)));
}
std::filesystem::path ScoringBoxUserOutput(long id, int td) {
  return ScoringBoxPath(id, td) / kWorkdirRelative / "user_output";
}
std::filesystem::path ScoringBoxTdInput(long id, int td) {
  return ScoringBoxPath(id, td) / kWorkdirRelative / "td_input";
}
std::filesystem::path ScoringBoxTdOutput(long id, int td) {
  return ScoringBoxPath(id, td) / kWorkdirRelative / "td_output";
}
std::filesystem::path ScoringBoxMetaFile(long id, int td) {
  return ScoringBoxPath(id, td) / kWorkdirRelative / "meta";
}
std::filesystem::path ScoringBoxOutput(long id, int td) {
  return ScoringBoxPath(id, td) / kWorkdirRelative / "output";
}

std::filesystem::path TdPath(int prob) {
  return kTestdataRoot / PadInt(prob, 4);
}
std::filesystem::path TdMeta(int prob, int td) {
  return TdPath(prob) / ("input" + PadInt(td, 3) + ".meta");
}
std::filesystem::path TdInput(int prob, int td) {
  return TdPath(prob) / ("input" + PadInt(td, 3));
}
std::filesystem::path TdOutput(int prob, int td) {
  return TdPath(prob) / ("output" + PadInt(td, 3));
}
