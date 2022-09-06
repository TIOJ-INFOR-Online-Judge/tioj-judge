#include <cmath>
#include <clocale>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <nlohmann/json.hpp>

static bool verbose = false;
static std::stringstream message;

inline void EOFMessage(bool ans_eof, size_t line, size_t user_lines) {
  if (ans_eof) message << "Unexpected line " << line;
  else message << "Unexpected EOF after line " << user_lines;
}

inline void DifferMessage(const std::string& ans, const std::string& usr) {
  size_t pos = 0;
  for (; pos < ans.size() && pos < usr.size() && ans[pos] == usr[pos]; pos++);
  message << "Expected: ";
  if (pos <= 40 || ans.size() <= 80) {
    message << ans;
  } else {
    message << "..." << ans.substr(pos - 40, 80);
    if (ans.size() > pos + 40) message << "...";
  }
  message << "\nGot: ";
  if (pos <= 40 || usr.size() <= 80) {
    message << usr;
  } else {
    message << "..." << usr.substr(pos - 40, 80);
    if (usr.size() > pos + 40) message << "...";
  }
}

bool LineCompare(std::ifstream& f_ans, std::ifstream& f_usr,
                 bool strip_tail = true, bool ignore_tail_empty_lines = true) {
  constexpr char kWhites[] = " \n\r\t";
  size_t line = 1;
  for (; f_ans.eof() == f_usr.eof(); line++) {
    if (f_ans.eof()) return true;
    std::string s, t;
    getline(f_ans, s);
    getline(f_usr, t);
    if (strip_tail) {
      // std::string::npos + 1 == 0
      s.erase(s.find_last_not_of(kWhites) + 1);
      t.erase(t.find_last_not_of(kWhites) + 1);
    }
    if (s != t) {
      if (verbose) {
        message << "Line " << line << " differ.\n";
        DifferMessage(s, t);
      }
      return false;
    }
  }
  size_t user_lines = line - 1;
  if (!ignore_tail_empty_lines) {
    if (verbose) EOFMessage(f_ans.eof(), line, user_lines);
    return false;
  }
  while (!f_ans.eof() || !f_usr.eof()) {
    std::string s;
    if (!f_ans.eof()) {
      getline(f_ans, s);
    } else {
      getline(f_usr, s);
    }
    if (s.find_last_not_of(kWhites) != std::string::npos) {
      if (verbose) EOFMessage(f_ans.eof(), line, user_lines);
      return false;
    }
    line++;
  }
  return true;
}

bool StrictCompare(std::ifstream& f_ans, std::ifstream& f_usr) {
  constexpr size_t kBufSize = 65536;
  static char buf1[kBufSize], buf2[kBufSize];
  size_t pos = 0;
  while (f_ans) {
    f_ans.read(buf1, kBufSize);
    f_usr.read(buf2, kBufSize);
    if (f_ans.eof() != f_usr.eof() || f_ans.gcount() != f_usr.gcount()) {
      if (verbose) {
        message << "Length differ: expected " << (pos + f_ans.gcount()) << " bytes, got "
            << (pos + f_usr.gcount()) << " bytes";
      }
      return false;
    }
    if (memcmp(buf1, buf2, f_ans.gcount()) != 0) {
      if (verbose) {
        long offset = 0;
        for (; buf1[offset] == buf2[offset] && offset < f_ans.gcount(); offset++);
        message << "Byte " << (pos + offset) << " differ: expected 0x"
            << std::hex << std::setfill('0') << std::setw(2) << (uint32_t)(uint8_t)buf1[offset] << ", got 0x"
            << std::setw(2) << (uint32_t)(uint8_t)buf2[offset];
      }
      return false;
    }
    pos += f_usr.gcount();
  }
  return true;
}

template <class Func>
bool WordCompare(std::ifstream& f_ans, std::ifstream& f_usr, Func&& func) {
  constexpr char kWhites[] = " \n\r\t\x0b\x0c"; // same as CMS
  size_t line = 1;
  for (; f_ans.eof() == f_usr.eof(); line++) {
    if (f_ans.eof()) return true;
    std::string s, t;
    getline(f_ans, s);
    getline(f_usr, t);
    for (size_t i1 = 0, i2 = 0, word = 1;; word++) {
      i1 = s.find_first_not_of(kWhites, i1);
      i2 = t.find_first_not_of(kWhites, i2);
      if ((i1 == std::string::npos) != (i2 == std::string::npos)) {
        if (verbose) {
          if (i1 == std::string::npos) message << "Unexpected word: line " << line << ", word " << word;
          else message << "Unexpected EOL after line " << line << ", word " << word - 1;
        }
        return false;
      }
      if (i1 == std::string::npos) break;
      size_t j1 = s.find_first_of(kWhites, i1);
      size_t j2 = t.find_first_of(kWhites, i2);
      if (j1 == std::string::npos) j1 = s.size();
      if (j2 == std::string::npos) j2 = t.size();
      if (!func(s.substr(i1, j1 - i1), t.substr(i2, j2 - i2))) {
        if (verbose) {
          message << "Line " << line << ", word " << word << " differ.\n";
          DifferMessage(s.substr(i1, j1 - i1), t.substr(i2, j2 - i2));
        }
        return false;
      }
      i1 = j1, i2 = j2;
    }
  }
  size_t user_lines = line - 1;
  while (!f_ans.eof() || !f_usr.eof()) {
    std::string s;
    if (!f_ans.eof()) {
      getline(f_ans, s);
    } else {
      getline(f_usr, s);
    }
    if (s.find_last_not_of(kWhites) != std::string::npos) {
      if (verbose) EOFMessage(f_ans.eof(), line, user_lines);
      return false;
    }
  }
  return true;
}

int main(int argc, char** argv) {
  setlocale(LC_ALL, "C"); // ensure portable behavior of stold
  nlohmann::json json;
  {
    std::ifstream fin(argv[1]);
    fin >> json;
  }
  std::string user_output = json["user_output_file"].get<std::string>();
  if (!std::filesystem::is_regular_file(user_output)) {
    std::cout << nlohmann::json{{"verdict", "WA"}};
    return 0;
  }

  // parse arguments
  std::string type = "line", subtype;
  double threshold = 1e-6;
  {
    for (int i = 2; i < argc; i++) {
      std::string str(argv[i]);
      if (str == "verbose") {
        verbose = true;
        continue;
      }
      type = std::move(str);
      if (type == "float-diff" && i + 1 < argc) {
        subtype = argv[++i];
        if (i + 1 < argc) {
          try {
            threshold = std::stold(std::string(argv[i]));
            i++;
          } catch (...) {}
        }
      }
    }
  }

  std::ifstream f_ans(json["answer_file"].get<std::string>()), f_usr(user_output);
  bool res = false;
  if (type == "strict") {
    res = StrictCompare(f_ans, f_usr);
  } else if (type == "line") {
    res = LineCompare(f_ans, f_usr);
  } else if (type == "white-diff") {
    res = WordCompare(f_ans, f_usr, [](std::string&& ans, std::string&& usr) { return ans == usr; });
  } else if (type == "float-diff") {
    auto check = [](auto func) {
      return [func](std::string&& ans, std::string&& usr) {
        try {
          long double fans = std::stold(ans), fusr = std::stold(usr);
          // this avoids treating integers as floating point
          if (ans.find_first_of(".eExXnN") != std::string::npos) return func(fans, fusr);
          return ans == usr;
        } catch (...) {
          return ans == usr;
        }
      };
    };
    if (subtype == "absolute") {
      res = WordCompare(f_ans, f_usr, check([&](long double ans, long double usr) {
        return std::fabs(ans - usr) <= threshold;
      }));
    } else if (subtype == "relative") {
      res = WordCompare(f_ans, f_usr, check([&](long double ans, long double usr) {
        return std::fabs(ans - usr) <= threshold * std::fabs(ans);
      }));
    } else if (subtype == "absolute-relative") {
      res = WordCompare(f_ans, f_usr, check([&](long double ans, long double usr) {
        return std::fabs(ans - usr) <= threshold * std::max(1.0L, std::fabs(ans));
      }));
    }
    // else: WA
  }
  // else: WA
  if (res) {
    std::cout << nlohmann::json{{"verdict", "AC"}};
  } else {
    nlohmann::json res{{"verdict", "WA"}};
    if (verbose) {
      res["message_type"] = "text";
      res["message"] = message.str();
    }
    std::cout << res.dump(-1, ' ', false, nlohmann::json::error_handler_t::ignore);
  }
}
