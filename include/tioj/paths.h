#ifndef INCLUDE_TIOJ_PATHS_H_
#define INCLUDE_TIOJ_PATHS_H_

#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

extern fs::path kBoxRoot;
extern fs::path kSubmissionRoot;
extern std::vector<fs::path> kSandboxMounts;

std::vector<fs::path> SplitSandboxPath(const std::string& sandbox_path);

namespace internal {

// does not meant to be publicly used; only for testing
extern fs::path kDataDir;

} // namespace internal

class TdFileLock {
  std::mutex global_lock_;
  std::unordered_map<int, std::mutex> mutex_map_;

 public:
  std::mutex& operator[](int id);
  // TODO: free mutexes?
};
extern TdFileLock td_file_lock;

// for submission fetch
// TODO FEATURE(web-refactor): Specjudge & interlib code is bound with submission currently.
//   Is it possible to switch to binding with problem?
//   (potential downside: different version of interlib for each language?)
fs::path SubmissionCodePath(int id);
fs::path SubmissionUserCode(int id);
fs::path SubmissionProblemProgCode(int id);
fs::path SubmissionJudgeCode(int id);
fs::path SubmissionSummaryCode(int id);
fs::path SubmissionInterlibCode(int id);
fs::path SubmissionInterlibImplCode(int id);

#endif // INCLUDE_TIOJ_PATHS_H_
