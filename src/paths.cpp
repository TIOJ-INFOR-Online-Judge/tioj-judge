#include "paths.h"

namespace {

inline std::string PadInt(long x, size_t width) {
  std::string ret = std::to_string(x);
  if (ret.size() < width) ret = std::string(width - ret.size(), '0') + ret;
  return ret;
}

} // namespace

fs::path kTestdataRoot = fs::path(TIOJ_DATA_DIR);

fs::path TdRoot() {
  return kTestdataRoot / "testdata";
}
fs::path TdPath(int prob) {
  return TdRoot() / PadInt(prob, 4);
}
fs::path TdInput(int prob, int td) {
  return TdPath(prob) / ("input" + PadInt(td, 3));
}
fs::path TdAnswer(int prob, int td) {
  return TdPath(prob) / ("output" + PadInt(td, 3));
}
