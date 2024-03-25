#ifndef TIOJ_PATHS_H_
#define TIOJ_PATHS_H_

#include <tioj/paths.h>

#include "tasks.h"
#include "submission.h"

extern const char kWorkdirRelative[];
fs::path Workdir(fs::path&&);

// for sandbox
// if inside_box = true, id (and subtask of Execute/Scoring) is not used
// those calls will have id (and subtask) marked as -1
fs::path SubmissionRunPath(long id);
fs::path CompileBoxPath(long id, CompileSubtask subtask);
fs::path CompileBoxInput(long id, CompileSubtask subtask, Compiler lang, bool inside_box = false);
fs::path CompileBoxInterlib(long id, int problem_id, bool inside_box = false);
fs::path CompileBoxInterlibImpl(long id, Compiler lang, bool inside_box = false);
fs::path CompileBoxOutput(long id, CompileSubtask subtask, Compiler lang, bool inside_box = false);
fs::path CompileBoxMessage(long id, CompileSubtask subtask, bool inside_box = false);
fs::path ExecuteBoxPath(long id, int td, int stage);
fs::path ExecuteBoxProgram(long id, int td, int stage, Compiler lang, bool inside_box = false);
fs::perms ExecuteBoxProgramPerm(Compiler lang, bool strict);
fs::path ExecuteBoxTdStrictPath(long id, int td, int stage, bool inside_box = false);
fs::path ExecuteBoxInput(long id, int td, int stage, bool strict, bool inside_box = false);
fs::path ExecuteBoxOutput(long id, int td, int stage, bool strict, bool inside_box = false);
fs::path ExecuteBoxError(long id, int td, int stage, bool inside_box = false);
fs::path ExecuteBoxFinalOutput(long id, int td, int stage);
// TODO FEATURE(pin)
fs::path ScoringBoxPath(long id, int td, int stage);
fs::path ScoringBoxProgram(long id, int td, int stage, Compiler lang, bool inside_box = false);
fs::path ScoringBoxCode(long id, int td, int stage, Compiler lang, bool inside_box = false);
fs::path ScoringBoxUserOutput(long id, int td, int stage, bool inside_box = false);
fs::path ScoringBoxTdInput(long id, int td, int stage, bool inside_box = false);
fs::path ScoringBoxTdOutput(long id, int td, int stage, bool inside_box = false);
fs::path ScoringBoxMetaFile(long id, int td, int stage, bool inside_box = false);
fs::path ScoringBoxOutput(long id, int td, int stage, bool inside_box = false);
fs::path SummaryBoxPath(long id);
fs::path SummaryBoxProgram(long id, Compiler lang, bool inside_box = false);
fs::path SummaryBoxMetaFile(long id, bool inside_box);
fs::path SummaryBoxOutput(long id, bool inside_box);

fs::path DefaultScoringPath();
fs::path SpecjudgeHeadersPath();

#endif  // TIOJ_PATHS_H_
