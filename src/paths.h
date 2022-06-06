#ifndef PATHS_H_
#define PATHS_H_

#include <filesystem>
#include <string>

#include "tasks.h"

extern const std::filesystem::path kBoxRoot;
extern const std::filesystem::path kTestdataRoot;

std::string PadInt(int x, size_t width = 0);
std::filesystem::path SubPath(long id);
std::filesystem::path CompileBoxPath(long id);
std::filesystem::path CompileBoxInput(long id, CompileSubtask subtask, Compiler lang);
std::filesystem::path CompileBoxOutput(long id, CompileSubtask subtask, Compiler lang);
std::filesystem::path ExecuteBoxPath(long id, int td);
std::filesystem::path ExecuteBoxProgram(long id, int td, Compiler lang);
std::filesystem::path ExecuteBoxTdStrictPath(long id, int td);
std::filesystem::path ExecuteBoxInput(long id, int td, bool strict);
std::filesystem::path ExecuteBoxOutput(long id, int td, bool strict);
std::filesystem::path ExecuteBoxError(long id, int td, bool strict);
// TODO: pin
std::filesystem::path ScoringBoxPath(long id, int td);
std::filesystem::path ScoringBoxProgram(long id, int td, Compiler lang);
std::filesystem::path ScoringBoxCode(long id, int td, Compiler lang);
std::filesystem::path ScoringBoxUserOutput(long id, int td);
std::filesystem::path ScoringBoxTdInput(long id, int td);
std::filesystem::path ScoringBoxTdOutput(long id, int td);
std::filesystem::path ScoringBoxMetaFile(long id, int td);
std::filesystem::path ScoringBoxOutput(long id, int td);

std::filesystem::path TdPath(int prob);
std::filesystem::path TdMeta(int prob, int td);
std::filesystem::path TdInput(int prob, int td);
std::filesystem::path TdOutput(int prob, int td);

#endif  // PATHS_H_
