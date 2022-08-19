#include <cmath>
#include <clocale>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <nlohmann/json.hpp>

bool LineCompare(std::ifstream& f_ans, std::ifstream& f_usr,
                 bool strip_tail = true, bool ignore_tail_empty_lines = true) {
  constexpr char kWhites[] = " \n\r\t";
  while (f_ans.eof() == f_usr.eof()) {
    if (f_ans.eof()) return true;
    std::string s, t;
    getline(f_ans, s);
    getline(f_usr, t);
    if (strip_tail) {
      // std::string::npos + 1 == 0
      s.erase(s.find_last_not_of(kWhites) + 1);
      t.erase(t.find_last_not_of(kWhites) + 1);
    }
    if (s != t) return false;
  }
  if (!ignore_tail_empty_lines) return false;
  while (!f_ans.eof() || !f_usr.eof()) {
    std::string s;
    if (!f_ans.eof()) {
      getline(f_ans, s);
    } else {
      getline(f_usr, s);
    }
    if (s.find_last_not_of(kWhites) != std::string::npos) return false;
  }
  return true;
}

bool StrictCompare(std::ifstream& f_ans, std::ifstream& f_usr) {
  constexpr size_t kBufSize = 65536;
  static char buf1[kBufSize], buf2[kBufSize];
  while (f_ans) {
    f_ans.read(buf1, kBufSize);
    f_usr.read(buf2, kBufSize);
    if (f_ans.eof() != f_usr.eof() || f_ans.gcount() != f_usr.gcount()) return false;
    if (memcmp(buf1, buf2, f_ans.gcount()) != 0) return false;
  }
  return true;
}

template <class Func>
bool WordCompare(std::ifstream& f_ans, std::ifstream& f_usr, Func&& func) {
  constexpr char kWhites[] = " \n\r\t\x0b\x0c"; // same as CMS
  while (f_ans.eof() == f_usr.eof()) {
    if (f_ans.eof()) return true;
    std::string s, t;
    getline(f_ans, s);
    getline(f_usr, t);
    for (size_t i1 = 0, i2 = 0;;) {
      i1 = s.find_first_not_of(kWhites, i1);
      i2 = t.find_first_not_of(kWhites, i2);
      if ((i1 == std::string::npos) != (i2 == std::string::npos)) return false;
      if (i1 == std::string::npos) break;
      size_t j1 = s.find_first_of(kWhites, i1);
      size_t j2 = t.find_first_of(kWhites, i2);
      if (j1 == std::string::npos) j1 = s.size();
      if (j2 == std::string::npos) j2 = t.size();
      if (!func(s.substr(i1, j1 - i1), t.substr(i2, j2 - i2))) return false;
      i1 = j1, i2 = j2;
    }
  }
  while (!f_ans.eof() || !f_usr.eof()) {
    std::string s;
    if (!f_ans.eof()) {
      getline(f_ans, s);
    } else {
      getline(f_usr, s);
    }
    if (s.find_last_not_of(kWhites) != std::string::npos) return false;
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
  std::ifstream f_ans(json["answer_file"].get<std::string>()), f_usr(user_output);
  bool res = false;
  if (argc > 2 && std::string(argv[2]) == "strict") {
    res = StrictCompare(f_ans, f_usr);
  } else if (argc > 2 && std::string(argv[2]) == "white-diff") {
    res = WordCompare(f_ans, f_usr, [](std::string&& ans, std::string&& usr) { return ans == usr; });
  } else if (argc > 3 && std::string(argv[2]) == "float-diff") {
    long double threshold = 1e-6;
    if (argc > 4) {
      try {
        threshold = std::stold(std::string(argv[4]));
      } catch (...) {}
    }
    std::string type = argv[3];
    auto check = [](auto func) {
      return [func](std::string&& ans, std::string&& usr) {
        try {
          long double fans = std::stold(ans), fusr = std::stold(usr);
          // this avoids treating integers as floating point
          if (ans.find_first_of(".eExXnN")) return func(fans, fusr);
          return ans == usr;
        } catch (...) {
          return ans == usr;
        }
      };
    };
    if (type == "absolute") {
      res = WordCompare(f_ans, f_usr, check([&](long double ans, long double usr) {
        return std::fabs(ans - usr) <= threshold;
      }));
    } else if (type == "relative") {
      res = WordCompare(f_ans, f_usr, check([&](long double ans, long double usr) {
        return std::fabs(ans - usr) <= threshold * std::fabs(ans);
      }));
    } else {
      res = WordCompare(f_ans, f_usr, check([&](long double ans, long double usr) {
        return std::fabs(ans - usr) <= threshold * std::max(1.0L, std::fabs(ans));
      }));
    }
  } else {
    res = LineCompare(f_ans, f_usr);
  }
  if (res) {
    std::cout << nlohmann::json{{"verdict", "AC"}};
  } else {
    std::cout << nlohmann::json{{"verdict", "WA"}};
  }
}
