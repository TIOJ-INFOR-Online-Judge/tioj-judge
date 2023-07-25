#ifndef INCLUDE_TIOJ_REPORTER_H_
#define INCLUDE_TIOJ_REPORTER_H_

class Submission;

class Reporter {
 public:
  // these functions should not block
  virtual void ReportStartCompiling(const Submission&) {}
  virtual void ReportOverallResult(const Submission&) {}
  virtual void ReportScoringResult(const Submission&, int subtask) {}
  virtual void ReportCEMessage(const Submission&) {}
  virtual void ReportERMessage(const Submission&) {}
  virtual void ReportFinalized(const Submission&, size_t queue_size_before_pop) {}
};

#endif  // INCLUDE_TIOJ_REPORTER_H_
