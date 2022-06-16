#ifndef TIOJ_PATHS_H_
#define TIOJ_PATHS_H_

#include <mutex>
#include <filesystem>
#include <unordered_map>

namespace fs = std::filesystem;

extern fs::path kBoxRoot;
extern fs::path kSubmissionRoot;
extern fs::path kDataDir;

class TdFileLock {
  std::mutex global_lock_;
  std::unordered_map<int, std::mutex> mutex_map_;
 public:
  std::mutex& operator[](int id);
  // TODO: free mutexes?
};
extern TdFileLock td_file_lock;

// for testdata download
fs::path TdRoot();
fs::path TdPath(int prob);
fs::path TdInput(int prob, int td);
fs::path TdOutput(int prob, int td);
fs::path DefaultScoringPath();

// for submission fetch
// TODO FEATURE(web-refactor): Specjudge & interlib code is bound with submission currently.
//   Is it possible to switch to binding with problem?
//   (potential downside: different version of interlib for each language?)
fs::path SubmissionCodePath(int id);
fs::path SubmissionUserCode(int id);
fs::path SubmissionJudgeCode(int id);
fs::path SubmissionInterlibCode(int id);

#endif  // TIOJ_PATHS_H_
