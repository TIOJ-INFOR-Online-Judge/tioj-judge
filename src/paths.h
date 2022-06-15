#ifndef PATHS_H_
#define PATHS_H_

#include <filesystem>
#include <string>

#include "tasks.h"

namespace fs = std::filesystem;

extern fs::path kBoxRoot;
extern fs::path kSubmissionRoot;
extern fs::path kDataDir;

extern const char kWorkdirRelative[];
fs::path Workdir(fs::path&&);

// for sandbox
// if inside_box = true, id (and subtask of Execute/Scoring) is not used
// those calls will have id (and subtask) marked as -1
fs::path SubmissionRunPath(long id);
fs::path CompileBoxPath(long id, CompileSubtask subtask);
fs::path CompileBoxInput(long id, CompileSubtask subtask, Compiler lang, bool inside_box = false);
fs::path CompileBoxInterlib(long id, int problem_id, bool inside_box = false);
// TODO FEATURE(link): CompileBoxInterlibImpl
fs::path CompileBoxOutput(long id, CompileSubtask subtask, Compiler lang, bool inside_box = false);
fs::path CompileBoxMessage(long id, CompileSubtask subtask, bool inside_box = false);
fs::path ExecuteBoxPath(long id, int td);
fs::path ExecuteBoxProgram(long id, int td, Compiler lang, bool inside_box = false);
fs::perms ExecuteBoxProgramPerm(Compiler lang, bool strict);
fs::path ExecuteBoxTdStrictPath(long id, int td, bool inside_box = false);
// TODO FEATURE(multistage): add stage; use previous output as input
fs::path ExecuteBoxInput(long id, int td, bool strict, bool inside_box = false);
fs::path ExecuteBoxOutput(long id, int td, bool strict, bool inside_box = false);
fs::path ExecuteBoxError(long id, int td, bool inside_box = false);
fs::path ExecuteBoxFinalOutput(long id, int td);
// TODO FEATURE(pin)
fs::path ScoringBoxPath(long id, int td);
fs::path ScoringBoxProgram(long id, int td, Compiler lang, bool inside_box = false);
fs::path ScoringBoxCode(long id, int td, Compiler lang, bool inside_box = false);
fs::path ScoringBoxUserOutput(long id, int td, bool inside_box = false);
fs::path ScoringBoxTdInput(long id, int td, bool inside_box = false);
fs::path ScoringBoxTdOutput(long id, int td, bool inside_box = false);
fs::path ScoringBoxMetaFile(long id, int td, bool inside_box = false);
fs::path ScoringBoxOutput(long id, int td, bool inside_box = false);

// for testdata download
fs::path TdRoot();
fs::path TdPath(int prob);
fs::path TdMeta(int prob, int td);
fs::path TdInput(int prob, int td);
fs::path TdOutput(int prob, int td);
fs::path DefaultScoringPath();

// for submission fetch
fs::path SubmissionCodePath(int id);
fs::path SubmissionUserCode(int id);
fs::path SubmissionJudgeCode(int id);
fs::path SubmissionInterlibCode(int id);

#endif  // PATHS_H_
