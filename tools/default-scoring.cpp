#include <fstream>
#include <iostream>
#include <filesystem>
#include <nlohmann/json.hpp>

bool LineCompare(std::ifstream& f_ans, std::ifstream& f_usr,
                 bool strip_tail = true, bool ignore_tail_empty_lines = true) {
  while (f_ans.eof() == f_usr.eof()) {
    if (f_ans.eof()) return true;
    std::string s, t;
    getline(f_ans, s);
    getline(f_usr, t);
    if (strip_tail) {
      // std::string::npos + 1 == 0
      s.erase(s.find_last_not_of(" \n\r\t") + 1);
      t.erase(t.find_last_not_of(" \n\r\t") + 1);
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
    if (s.find_last_not_of(" \n\r\t") != std::string::npos) return false;
  }
  return true;
}

int main(int argc, char** argv) {
  // TODO FEATURE(scoring-style): Add different scoring style such as strict compare, white-diff (CMS) & numerical compare
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
  bool res = LineCompare(f_ans, f_usr);
  if (res) {
    std::cout << nlohmann::json{{"verdict", "AC"}};
  } else {
    std::cout << nlohmann::json{{"verdict", "WA"}};
  }
}
