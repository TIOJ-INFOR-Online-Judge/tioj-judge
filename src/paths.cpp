#include "paths.h"

namespace fs = std::filesystem;

const fs::path kBoxRoot = "/tmp/tioj_box";
const fs::path kTestdataRoot = "./testdata";
const fs::path kSubmissionRoot = "/tmp/submissions";

const char kWorkdirRelative[] = "workdir";
fs::path Workdir(fs::path&& path) {
  path /= kWorkdirRelative;
  return path;
}

inline std::string PadInt(long x, size_t width) {
  std::string ret = std::to_string(x);
  if (ret.size() < width) ret = std::string(width - ret.size(), '0') + ret;
  return ret;
}

static inline std::string CodeExtension(Compiler lang) {
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

static inline std::string ProgramExtension(Compiler lang) {
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

fs::path SubmissionRunPath(long id) {
  return kBoxRoot / PadInt(id, 6);
}


inline std::string CompileCodeName(CompileSubtask subtask, Compiler lang) {
  std::string extension = CodeExtension(lang);
  switch (subtask) {
    case CompileSubtask::USERPROG:
      return "prog" + extension;
    case CompileSubtask::SPECJUDGE:
      return "judge" + extension;
  }
  __builtin_unreachable();
}
inline std::string CompileResultName(CompileSubtask subtask, Compiler lang) {
  std::string extension = ProgramExtension(lang);
  switch (subtask) {
    case CompileSubtask::USERPROG:
      return "prog" + extension;
    case CompileSubtask::SPECJUDGE:
      return "judge" + extension;
  }
  __builtin_unreachable();
}
inline std::string InterlibName(int problem_id) {
  return "lib" + PadInt(problem_id, 4) + ".h";
}

fs::path CompileBoxPath(long id, CompileSubtask subtask) {
  return SubmissionRunPath(id) / ("compile" + std::to_string((int)subtask));
}
fs::path CompileBoxInput(long id, CompileSubtask subtask, Compiler lang) {
  return Workdir(CompileBoxPath(id, subtask)) / CompileCodeName(subtask, lang);
}
fs::path CompileBoxInterlib(long id, int problem_id) {
  return Workdir(CompileBoxPath(id, CompileSubtask::USERPROG)) / InterlibName(problem_id);
}
fs::path CompileBoxOutput(long id, CompileSubtask subtask, Compiler lang) {
  return Workdir(CompileBoxPath(id, subtask)) / CompileResultName(subtask, lang);
}
fs::path ExecuteBoxPath(long id, int td) {
  return SubmissionRunPath(id) / ("execute" + PadInt(td, 3));
}
fs::path ExecuteBoxProgram(long id, int td, Compiler lang) {
  return Workdir(ExecuteBoxPath(id, td)) / ("prog" + ProgramExtension(lang));
}
fs::perms ExecuteBoxProgramPerm(Compiler lang, bool strict) {
  if (!strict) return fs::perms::all;
  switch (lang) {
    case Compiler::GCC_CPP_98: [[fallthrough]];
    case Compiler::GCC_CPP_11: [[fallthrough]];
    case Compiler::GCC_CPP_14: [[fallthrough]];
    case Compiler::GCC_CPP_17: [[fallthrough]];
    case Compiler::GCC_CPP_20: [[fallthrough]];
    case Compiler::GCC_C_90: [[fallthrough]];
    case Compiler::GCC_C_98: [[fallthrough]];
    case Compiler::GCC_C_11:
      return fs::perms::owner_all | fs::perms::group_exec | fs::perms::others_exec; // 711
    case Compiler::HASKELL:
      return fs::perms::owner_all | fs::perms::group_exec | fs::perms::others_exec; // 711
    case Compiler::PYTHON2: [[fallthrough]];
    case Compiler::PYTHON3:
      return fs::perms::owner_all |
             fs::perms::group_read | fs::perms::group_exec |
             fs::perms::others_read | fs::perms::others_exec; // 755
  }
  __builtin_unreachable();
}
fs::path ExecuteBoxTdStrictPath(long id, int td) {
  return ExecuteBoxPath(id, td) / "td";
}
fs::path ExecuteBoxInput(long id, int td, bool strict) {
  if (strict) {
    return ExecuteBoxTdStrictPath(id, td) / "input";
  } else {
    return Workdir(ExecuteBoxPath(id, td)) / "input";
  }
}
fs::path ExecuteBoxOutput(long id, int td, bool strict) {
  if (strict) {
    return ExecuteBoxTdStrictPath(id, td) / "output";
  } else {
    return Workdir(ExecuteBoxPath(id, td)) / "output";
  }
}
fs::path ExecuteBoxError(long id, int td) {
  return Workdir(ExecuteBoxPath(id, td)) / "error";
}
fs::path ExecuteBoxFinalOutput(long id, int td) {
  return ExecuteBoxPath(id, td) / "output";
}
// TODO: pin
fs::path ScoringBoxPath(long id, int td) {
  return SubmissionRunPath(id) / ("scoring" + PadInt(td, 3));
}
fs::path ScoringBoxProgram(long id, int td, Compiler lang) {
  return Workdir(ScoringBoxPath(id, td)) / ("prog" + std::string(ProgramExtension(lang)));
}
fs::path ScoringBoxCode(long id, int td, Compiler lang) {
  return Workdir(ScoringBoxPath(id, td)) / ("code" + std::string(CodeExtension(lang)));
}
fs::path ScoringBoxUserOutput(long id, int td) {
  return Workdir(ScoringBoxPath(id, td)) / "user_output";
}
fs::path ScoringBoxTdInput(long id, int td) {
  return Workdir(ScoringBoxPath(id, td)) / "td_input";
}
fs::path ScoringBoxTdOutput(long id, int td) {
  return Workdir(ScoringBoxPath(id, td)) / "td_output";
}
fs::path ScoringBoxMetaFile(long id, int td) {
  return Workdir(ScoringBoxPath(id, td)) / "meta";
}
fs::path ScoringBoxOutput(long id, int td) {
  return Workdir(ScoringBoxPath(id, td)) / "output";
}

fs::path TdPath(int prob) {
  return kTestdataRoot / PadInt(prob, 4);
}
fs::path TdMeta(int prob, int td) {
  return TdPath(prob) / ("input" + PadInt(td, 3) + ".meta");
}
fs::path TdInput(int prob, int td) {
  return TdPath(prob) / ("input" + PadInt(td, 3));
}
fs::path TdOutput(int prob, int td) {
  return TdPath(prob) / ("output" + PadInt(td, 3));
}

fs::path SubmissionCodePath(int id) {
  return kSubmissionRoot / PadInt(id, 6);
}
fs::path SubmissionUserCode(int id) {
  return SubmissionCodePath(id) / "prog";
}
fs::path SubmissionJudgeCode(int id) {
  return SubmissionCodePath(id) / "judge";
}
fs::path SubmissionInterlib(int id) {
  return SubmissionCodePath(id) / "interlib";
}
