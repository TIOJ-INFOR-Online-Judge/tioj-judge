#ifndef TIOJ_PATHS_H_
#define TIOJ_PATHS_H_

#include <filesystem>

namespace fs = std::filesystem;

extern fs::path kBoxRoot;
extern fs::path kSubmissionRoot;
extern fs::path kDataDir;

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

#endif  // TIOJ_PATHS_H_
