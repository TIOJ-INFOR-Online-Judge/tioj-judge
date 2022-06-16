#ifndef TIOJ_REPORTER_H_
#define TIOJ_REPORTER_H_

class Submission;

class Reporter {
 public:
  // these functions should not block
  virtual void ReportStartCompiling(const Submission&) = 0;
  virtual void ReportOverallResult(const Submission&) = 0;
  virtual void ReportScoringResult(const Submission&, int subtask) = 0;
  virtual void ReportCEMessage(const Submission&) = 0;
};

#endif  // TIOJ_REPORTER_H_
