#ifndef TIOJ_UTILS_H_
#define TIOJ_UTILS_H_

#include "tasks.h"
#include "submission.h"

long GetUniqueSubmissionInternalId();

const char* VerdictToDesc(Verdict);
const char* VerdictToAbr(Verdict);
Verdict AbrToVerdict(const std::string&, bool runtime_only);

const char* CompilerName(Compiler);
Compiler GetCompiler(const std::string&);

// logging
const char* TaskTypeName(TaskType);
const char* CompileSubtaskName(CompileSubtask);
const char* SpecjudgeTypeName(SpecjudgeType);
const char* InterlibTypeName(InterlibType);

#endif  // TIOJ_UTILS_H_
