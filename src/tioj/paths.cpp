#include "paths.h"

fs::path kBoxRoot = "/tmp/tioj_box";
fs::path kSubmissionRoot = "/tmp/tioj_submissions";
fs::path kDataDir = fs::path(TIOJ_DATA_DIR);

const char kWorkdirRelative[] = "workdir";
fs::path Workdir(fs::path&& path) {
  path /= kWorkdirRelative;
  return path;
}

namespace {

inline std::string PadInt(long x, size_t width) {
  std::string ret = std::to_string(x);
  if (ret.size() < width) ret = std::string(width - ret.size(), '0') + ret;
  return ret;
}

inline std::string CodeExtension(Compiler lang) {
  switch (lang) {
    case Compiler::GCC_CPP_98: [[fallthrough]];
    case Compiler::GCC_CPP_11: [[fallthrough]];
    case Compiler::GCC_CPP_14: [[fallthrough]];
    case Compiler::GCC_CPP_17: [[fallthrough]];
    case Compiler::GCC_CPP_20: return ".cpp";
    case Compiler::GCC_C_90: [[fallthrough]];
    case Compiler::GCC_C_99: [[fallthrough]];
    case Compiler::GCC_C_11: return ".c";
    case Compiler::HASKELL: return ".hs";
    case Compiler::PYTHON2: [[fallthrough]];
    case Compiler::PYTHON3: return ".py";
  }
  __builtin_unreachable();
}

inline std::string ProgramExtension(Compiler lang) {
  switch (lang) {
    case Compiler::GCC_CPP_98: [[fallthrough]];
    case Compiler::GCC_CPP_11: [[fallthrough]];
    case Compiler::GCC_CPP_14: [[fallthrough]];
    case Compiler::GCC_CPP_17: [[fallthrough]];
    case Compiler::GCC_CPP_20: [[fallthrough]];
    case Compiler::GCC_C_90: [[fallthrough]];
    case Compiler::GCC_C_99: [[fallthrough]];
    case Compiler::GCC_C_11: return "";
    case Compiler::HASKELL: return "";
    case Compiler::PYTHON2: [[fallthrough]];
    case Compiler::PYTHON3: return ".pyc";
  }
  __builtin_unreachable();
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

// Because of python2, it needs to be the same name as CompileCodeName
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

inline fs::path BoxRoot(fs::path root, bool inside_box) {
  return inside_box ? fs::path("/") : root;
}

} // namespace

fs::path SubmissionRunPath(long id) {
  return kBoxRoot / PadInt(id, 6);
}

fs::path CompileBoxPath(long id, CompileSubtask subtask) {
  return SubmissionRunPath(id) / ("compile" + std::to_string((int)subtask));
}
fs::path CompileBoxInput(long id, CompileSubtask subtask, Compiler lang, bool inside_box) {
  return Workdir(BoxRoot(CompileBoxPath(id, subtask), inside_box))
      / CompileCodeName(subtask, lang);
}
fs::path CompileBoxInterlib(long id, int problem_id, bool inside_box) {
  return Workdir(BoxRoot(CompileBoxPath(id, CompileSubtask::USERPROG), inside_box))
      / InterlibName(problem_id);
}
fs::path CompileBoxInterlibImpl(long id, Compiler lang, bool inside_box) {
  return Workdir(BoxRoot(CompileBoxPath(id, CompileSubtask::USERPROG), inside_box))
      / ("interlib" + CodeExtension(lang));
}
fs::path CompileBoxMessage(long id, CompileSubtask subtask, bool inside_box) {
  return Workdir(BoxRoot(CompileBoxPath(id, subtask), inside_box)) / "message";
}
fs::path CompileBoxOutput(long id, CompileSubtask subtask, Compiler lang, bool inside_box) {
  return Workdir(BoxRoot(CompileBoxPath(id, subtask), inside_box))
      / CompileResultName(subtask, lang);
}

fs::path ExecuteBoxPath(long id, int td) {
  return SubmissionRunPath(id) / ("execute" + PadInt(td, 3));
}
fs::path ExecuteBoxProgram(long id, int td, Compiler lang, bool inside_box) {
  return Workdir(BoxRoot(ExecuteBoxPath(id, td), inside_box))
      / ("prog" + ProgramExtension(lang));
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
    case Compiler::GCC_C_99: [[fallthrough]];
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
fs::path ExecuteBoxTdStrictPath(long id, int td, bool inside_box) {
  return BoxRoot(ExecuteBoxPath(id, td), inside_box) / "td";
}
fs::path ExecuteBoxInput(long id, int td, bool strict, bool inside_box) {
  if (strict) {
    return ExecuteBoxTdStrictPath(id, td, inside_box) / "input";
  } else {
    return Workdir(BoxRoot(ExecuteBoxPath(id, td), inside_box)) / "input";
  }
}
fs::path ExecuteBoxOutput(long id, int td, bool strict, bool inside_box) {
  if (strict) {
    return ExecuteBoxTdStrictPath(id, td, inside_box) / "output";
  } else {
    return Workdir(BoxRoot(ExecuteBoxPath(id, td), inside_box)) / "output";
  }
}
fs::path ExecuteBoxError(long id, int td, bool inside_box) {
  return Workdir(BoxRoot(ExecuteBoxPath(id, td), inside_box)) / "error";
}
fs::path ExecuteBoxFinalOutput(long id, int td) {
  return ExecuteBoxPath(id, td) / "output";
}

fs::path ScoringBoxPath(long id, int td) {
  return SubmissionRunPath(id) / ("scoring" + PadInt(td, 3));
}
fs::path ScoringBoxProgram(long id, int td, Compiler lang, bool inside_box) {
  return Workdir(BoxRoot(ScoringBoxPath(id, td), inside_box)) / ("prog" + std::string(ProgramExtension(lang)));
}
fs::path ScoringBoxCode(long id, int td, Compiler lang, bool inside_box) {
  return Workdir(BoxRoot(ScoringBoxPath(id, td), inside_box)) / ("code" + std::string(CodeExtension(lang)));
}
fs::path ScoringBoxUserOutput(long id, int td, bool inside_box) {
  return Workdir(BoxRoot(ScoringBoxPath(id, td), inside_box)) / "user_output";
}
fs::path ScoringBoxTdInput(long id, int td, bool inside_box) {
  return Workdir(BoxRoot(ScoringBoxPath(id, td), inside_box)) / "td_input";
}
fs::path ScoringBoxTdOutput(long id, int td, bool inside_box) {
  return Workdir(BoxRoot(ScoringBoxPath(id, td), inside_box)) / "td_output";
}
fs::path ScoringBoxMetaFile(long id, int td, bool inside_box) {
  return Workdir(BoxRoot(ScoringBoxPath(id, td), inside_box)) / "meta";
}
fs::path ScoringBoxOutput(long id, int td, bool inside_box) {
  return Workdir(BoxRoot(ScoringBoxPath(id, td), inside_box)) / "output";
}

std::mutex& TdFileLock::operator[](int id) {
  std::lock_guard lck(global_lock_);
  return mutex_map_[id];
}
TdFileLock td_file_lock;

fs::path TdRoot() {
  return kDataDir / "testdata";
}
fs::path TdPath(int prob) {
  return TdRoot() / PadInt(prob, 4);
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
fs::path DefaultScoringPath() {
  return kDataDir / "default-scoring";
}
fs::path SpecjudgeHeadersPath() {
  return kDataDir / "judge-headers";
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
fs::path SubmissionInterlibCode(int id) {
  return SubmissionCodePath(id) / "interlib";
}
fs::path SubmissionInterlibImplCode(int id) {
  return SubmissionCodePath(id) / "interlib_impl";
}
