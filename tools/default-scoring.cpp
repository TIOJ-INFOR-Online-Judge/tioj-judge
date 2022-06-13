#include <fstream>
#include <iostream>
#include <filesystem>
#include <nlohmann/json.hpp>

int main(int argc, char** argv) {
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
  std::ifstream tsol(json["answer_file"].get<std::string>()), mout(user_output);
  while (true) {
    if (tsol.eof() != mout.eof()) {
      while (tsol.eof() != mout.eof()) {
        std::string s;
        if (tsol.eof()) {
          getline(mout, s);
        } else {
          getline(tsol, s);
        }
        s.erase(s.find_last_not_of(" \n\r\t") + 1);
        if (s != "") {
          std::cout << nlohmann::json{{"verdict", "WA"}};
          return 0;
        }
      }
      break;
    }
    if (tsol.eof() && mout.eof()) {
      break;
    }
    std::string s, t;
    getline(tsol, s);
    getline(mout, t);
    s.erase(s.find_last_not_of(" \n\r\t") + 1);
    t.erase(t.find_last_not_of(" \n\r\t") + 1);
    if (s != t) {
      std::cout << nlohmann::json{{"verdict", "WA"}};
      return 0;
    }
  }
  std::cout << nlohmann::json{{"verdict", "AC"}};
}
