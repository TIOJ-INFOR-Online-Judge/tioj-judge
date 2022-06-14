#ifndef PATHS_H_
#define PATHS_H_

#include <filesystem>
#include <string>

#include "tasks.h"

extern const std::filesystem::path kBoxRoot;
extern const std::filesystem::path kTestdataRoot;
extern const std::filesystem::path kSubmissionRoot;
extern const std::filesystem::path kDefaultScoringProgram;

extern const char kWorkdirRelative[];
std::filesystem::path Workdir(std::filesystem::path&&);

// for sandbox
// if inside_box = true, id (and subtask of Execute/Scoring) is not used
// those calls will have id (and subtask) marked as -1
std::filesystem::path SubmissionRunPath(long id);
std::filesystem::path CompileBoxPath(long id, CompileSubtask subtask);
std::filesystem::path CompileBoxInput(long id, CompileSubtask subtask, Compiler lang, bool inside_box = false);
std::filesystem::path CompileBoxInterlib(long id, int problem_id, bool inside_box = false);
// TODO FEATURE(link): CompileBoxInterlibImpl
std::filesystem::path CompileBoxOutput(long id, CompileSubtask subtask, Compiler lang, bool inside_box = false);
std::filesystem::path CompileBoxMessage(long id, CompileSubtask subtask, bool inside_box = false);
std::filesystem::path ExecuteBoxPath(long id, int td);
std::filesystem::path ExecuteBoxProgram(long id, int td, Compiler lang, bool inside_box = false);
std::filesystem::perms ExecuteBoxProgramPerm(Compiler lang, bool strict);
std::filesystem::path ExecuteBoxTdStrictPath(long id, int td, bool inside_box = false);
// TODO FEATURE(multistage): add stage; use previous output as input
std::filesystem::path ExecuteBoxInput(long id, int td, bool strict, bool inside_box = false);
std::filesystem::path ExecuteBoxOutput(long id, int td, bool strict, bool inside_box = false);
std::filesystem::path ExecuteBoxError(long id, int td, bool inside_box = false);
std::filesystem::path ExecuteBoxFinalOutput(long id, int td);
// TODO FEATURE(pin)
std::filesystem::path ScoringBoxPath(long id, int td);
std::filesystem::path ScoringBoxProgram(long id, int td, Compiler lang, bool inside_box = false);
std::filesystem::path ScoringBoxCode(long id, int td, Compiler lang, bool inside_box = false);
std::filesystem::path ScoringBoxUserOutput(long id, int td, bool inside_box = false);
std::filesystem::path ScoringBoxTdInput(long id, int td, bool inside_box = false);
std::filesystem::path ScoringBoxTdOutput(long id, int td, bool inside_box = false);
std::filesystem::path ScoringBoxMetaFile(long id, int td, bool inside_box = false);
std::filesystem::path ScoringBoxOutput(long id, int td, bool inside_box = false);

// for testdata download
std::filesystem::path TdPath(int prob);
std::filesystem::path TdMeta(int prob, int td);
std::filesystem::path TdInput(int prob, int td);
std::filesystem::path TdOutput(int prob, int td);

// for submission fetch
std::filesystem::path SubmissionCodePath(int id);
std::filesystem::path SubmissionUserCode(int id);
std::filesystem::path SubmissionJudgeCode(int id);
std::filesystem::path SubmissionInterlibCode(int id);

#endif  // PATHS_H_
