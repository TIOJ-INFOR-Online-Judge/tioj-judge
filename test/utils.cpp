#include "utils.h"

#include <fstream>
#include <filesystem>
#include <tioj/paths.h>

long SetupSubmission(
    Submission& sub, int id, Compiler lang, long time, bool sandbox_strict, const std::string& code,
    SpecjudgeType spec_type, const std::string& specjudge_code,
    SummaryType summary_type, const std::string& summary_code, int submitter_id) {
  sub.submission_id = id;
  long iid = sub.submission_internal_id = GetUniqueSubmissionInternalId();
  sub.submitter_id = submitter_id;
  sub.submission_time = time;
  sub.lang = lang;
  sub.sandbox_strict = sandbox_strict;
  fs::create_directories(SubmissionCodePath(iid));
  {
    std::ofstream fout(SubmissionUserCode(iid));
    fout << code;
  }
  if (spec_type != SpecjudgeType::NORMAL) {
    sub.specjudge_type = spec_type;
    sub.specjudge_lang = Compiler::GCC_CPP_17;
    std::ofstream fout(SubmissionJudgeCode(iid));
    fout << specjudge_code;
  }
  if (summary_type != SummaryType::NONE) {
    sub.summary_type = summary_type;
    sub.summary_lang = Compiler::GCC_CPP_17;
    std::ofstream fout(SubmissionSummaryCode(iid));
    fout << summary_code;
  }
  return iid;
}

void TeardownSubmission(long id) {
  fs::remove_all(SubmissionCodePath(id));
}
