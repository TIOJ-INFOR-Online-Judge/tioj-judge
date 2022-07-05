# tioj-judge

This is the new TIOJ judge client. It is meant to be a replacement of [miku](https://github.com/TIOJ-INFOR-Online-Judge/miku), while being safer, more efficient and more feature-rich.

It also provides a judging library interface `libtioj` that can be used independently.

## Judge Server

### Installation

```
mkdir build
cmake ..
make -j8
sudo make install
```

This will also install `libtioj` and its dependencies (namely `nlohmann_json` and `cjail`). Specify `-DTIOJ_INSTALL_LIBTIOJ=0` if only the judge client is needed.

### Usage

Set up the `/etc/tioj-judge.conf` configuration file:

```
tioj_url = https://url.to.tioj.web.server
tioj_key = some_random_key
parallel = 2
fetch_interval = 1
```

Run `sudo tioj-judge` to start the judge.

## Library

A minimal working C++ example:

```c++
#include <cstdio>
#include <tioj/paths.h>
#include <tioj/utils.h>
#include <tioj/reporter.h>
#include <tioj/submission.h>

// Callbacks for judge results
class MyReporter : public Reporter {
  void ReportStartCompiling(const Submission& sub) override {}
  void ReportOverallResult(const Submission& sub) override {
    printf("Result: %s\n", VerdictToAbr(sub.verdict));
  }
  void ReportScoringResult(const Submission& sub, int subtask) override {}
  void ReportCEMessage(const Submission& sub) {
    puts("CE message:");
    printf("%s\n", sub.ce_message.c_str());
  }
} reporter;

int main() {
  // run as root
  constexpr int submission_id = 1, problem_id = 1, num_td = 3;
  Submission sub;
  sub.submission_internal_id = GetUniqueSubmissionInternalId();
  sub.submission_id = submission_id;
  sub.lang = Compiler::GCC_CPP_17;
  sub.problem_id = problem_id;
  sub.specjudge_type = SpecjudgeType::NORMAL;
  sub.interlib_type = InterlibType::NONE;
  for (int i = 0; i < num_td; i++) {
    Submission::TestdataLimit limit;
    limit.vss = 65536; // 64 KiB
    limit.rss = 0;
    limit.output = 65536;
    limit.time = 1000000; // 1 sec
    sub.td_limits.push_back(limit);
  }
  sub.reporter = &reporter;
  { // submission files
    // mkdir TdPath(problem_id)
    for (int td = 0; td < num_td; td++) {
      // write input of testdata #td into TdInput(problem_id, td);
      // write answer of testdata #td into TdOutput(problem_id, td);
    }
    // mkdir SubmissionCodePath(submission_id)
    // write submission code into SubmissionUserCode(submission_id)
  }
  PushSubmission(std::move(sub));
  WorkLoop(false); // keep judging until all submissions in the queue are finished
}
```

Remember to link `-ltioj -lpthread` when compiling.
