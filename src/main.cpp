#include <fstream>
#include <filesystem>

#include <tortellini.hh>
#include "paths.h"
#include "submission.h"

void ParseConfig(const fs::path& conf_path) {
  std::ifstream fin(conf_path);
  tortellini::ini ini;
  fin >> ini;
  std::string box_root = ini[""]["box_root"] | "";
  std::string submission_root = ini[""]["submission_root"] | "";
  std::string data_dir = ini[""]["data_dir"] | "";
  if (box_root.size()) kBoxRoot = box_root;
  if (submission_root.size()) kSubmissionRoot = submission_root;
  if (data_dir.size()) kDataDir = data_dir;
  max_parallel = ini[""]["parallel"] | 1;
}

//int main() {}
