#ifndef TIOJ_SUBMISSION_H_
#define TIOJ_SUBMISSION_H_

#include <tioj/submission.h>
#include <nlohmann/json_fwd.hpp>

struct SubmissionAndResult {
  const Submission sub;
  SubmissionResult result;

  SubmissionAndResult(Submission&& sub) : sub(std::move(sub)), result() {}
  SubmissionAndResult(Submission&) = delete;
  SubmissionAndResult& operator=(const SubmissionAndResult&) = delete;

  nlohmann::json TestdataMeta(int subtask, int stage) const;
  nlohmann::json SummaryMeta() const;
};

#endif  // TIOJ_SUBMISSION_H_
