#ifndef PATHS_H_
#define PATHS_H_

#include <filesystem>

namespace fs = std::filesystem;

extern fs::path kTestdataRoot;

// for testdata download
fs::path TdRoot();
fs::path TdPath(int prob);
fs::path TdInput(int prob, int td);
fs::path TdAnswer(int prob, int td);

#endif
