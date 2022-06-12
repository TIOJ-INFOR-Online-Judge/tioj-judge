#ifndef PATHS_H_
#define PATHS_H_

#include <filesystem>
#include <string>

#include "tasks.h"

extern const std::filesystem::path kBoxRoot;
extern const std::filesystem::path kTestdataRoot;
extern const std::filesystem::path kSubmissionRoot;

extern const char kWorkdirRelative[];
std::filesystem::path Workdir(std::filesystem::path&&);

// for submission fetch
std::filesystem::path SubmissionRunPath(long id);
std::filesystem::path CompileBoxPath(long id, CompileSubtask subtask);
std::filesystem::path CompileBoxInput(long id, CompileSubtask subtask, Compiler lang);
std::filesystem::path CompileBoxInterlib(long id, int problem_id);
std::filesystem::path CompileBoxOutput(long id, CompileSubtask subtask, Compiler lang);
std::filesystem::path ExecuteBoxPath(long id, int td);
std::filesystem::path ExecuteBoxProgram(long id, int td, Compiler lang);
std::filesystem::perms ExecuteBoxProgramPerm(Compiler lang, bool strict);
std::filesystem::path ExecuteBoxTdStrictPath(long id, int td);
std::filesystem::path ExecuteBoxInput(long id, int td, bool strict);
std::filesystem::path ExecuteBoxOutput(long id, int td, bool strict);
std::filesystem::path ExecuteBoxError(long id, int td);
std::filesystem::path ExecuteBoxFinalOutput(long id, int td);
// TODO: pin
std::filesystem::path ScoringBoxPath(long id, int td);
std::filesystem::path ScoringBoxProgram(long id, int td, Compiler lang);
std::filesystem::path ScoringBoxCode(long id, int td, Compiler lang);
std::filesystem::path ScoringBoxUserOutput(long id, int td);
std::filesystem::path ScoringBoxTdInput(long id, int td);
std::filesystem::path ScoringBoxTdOutput(long id, int td);
std::filesystem::path ScoringBoxMetaFile(long id, int td);
std::filesystem::path ScoringBoxOutput(long id, int td);

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
