# tioj-judge

`tioj-judge` is the new judge client of [TIOJ](https://github.com/TIOJ-INFOR-Online-Judge/tioj). It is based on [cjail](https://github.com/Leo1003/cjail) code execution engine, and implements submission scheduling, testdata management and interaction logic with the web server.

## Features

- Limit memory by VSS and/or RSS
- Detailed execution stats compared to [miku](https://github.com/TIOJ-INFOR-Online-Judge/miku), including verdicts such as SIG, OLE, MLE and VSS reporting
- Judge multiple submissions in parallel
- Isolated and size-limited tmpfs to prevent DoS attacks
- New powerful special judge mode that can access lots of information, set arbitrary score and override results
- Multistage problems (similar to CMS's TwoStep mode)
- Optional strict mode for more isolation:
    - Compiles static executables and run them without any shared libraries present (only for C/C++/Haskell)
    - Use pipes for standard input / output to avoid seeking or re-opening
- A library interface `libtioj` that can be used independently
- A time multiplier to compensate speed difference of multiple judge clients
- Support some basic judging modes (such as floating-point compare, strict compare and white-diff compare) without the need of user-provided special judge program

## Judge Server

### Installation

Boost is required to compile the judge server.

```
mkdir build
cmake ..
make -j8
sudo make install
```

This will also install `libtioj` and its dependencies (namely `nlohmann_json` and `cjail`). Specify `-DTIOJ_INSTALL_LIBTIOJ=0` if only the judge client is needed.

### Usage

Set up the `/etc/tioj-judge.conf` configuration file, and then run `sudo tioj-judge` to start the judge. The configuration file format is as follows:

```
tioj_url = https://url.to.tioj.web.server
tioj_key = some_random_key
parallel = 1
max_rss_per_task_mb = 2048
max_output_per_task_mb = 1024
max_submission_queue_size = 20
time_multiplier = 1.0
```

- The indicated values except `tioj_url` and `tioj_key` are the default values.
- `time_multiplier` is the ratio of the indicated time to the real time. Thus, the multiplier should be larger if the computer is faster, and smaller if the computer is slower.

### Docker Usage

A Dockerfile is provided to run the judge easily.

When running the docker, `--privileged --net=host --pid=host` must be given, and the fetch key should be given via environment variable `TIOJ_KEY`.

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
  sub.remove_submission = true;
  sub.reporter = &reporter;
  { // submission files
    // mkdir TdPath(problem_id)
    for (int td = 0; td < num_td; td++) {
      // write input of testdata #td into TdInput(problem_id, td);
      // write answer of testdata #td into TdOutput(problem_id, td);
    }
    // mkdir SubmissionCodePath(sub.submission_internal_id)
    // write submission code into SubmissionUserCode(sub.submission_internal_id)

    // note that submission code / testdata files can be symbolic links
  }
  PushSubmission(std::move(sub));
  WorkLoop(false); // keep judging until all submissions in the queue are finished
}
```

Remember to link `-ltioj -lpthread` when compiling.
