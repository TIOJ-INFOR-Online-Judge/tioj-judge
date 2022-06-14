#ifndef REPORTER_H_
#define REPORTER_H_

class Submission;

class Reporter {
 public:
  // these functions should not block
  virtual void ReportOverallResult(const Submission&) = 0;
  virtual void ReportScoringResult(const Submission&, int subtask) = 0;
  virtual void ReportCEMessage(const Submission&) = 0;
};

#endif  // REPORTER_H_
