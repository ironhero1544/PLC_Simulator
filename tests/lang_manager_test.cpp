#include <fstream>
#include <iostream>
#include <string>
#include <unordered_set>

// Exercise the private file parser without changing saved user preferences
// or exposing a production API solely for testing.
#include "../src/lang/lang_manager.cpp"

int main(int argc, char** argv) {
  bool tested_large_file = false;
  for (int i = 1; i < argc; ++i) {
    if (!plc::LoadLangFile(argv[i]))
      return 1;
    std::ifstream input(argv[i]);
    std::string line;
    int count = 0;
    std::unordered_set<std::string> seen;
    const auto trim = [](const std::string& text) {
      const size_t first = text.find_first_not_of(" \t\r\n");
      if (first == std::string::npos)
        return std::string{};
      return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
    };
    while (std::getline(input, line)) {
      if (line.empty() || line.front() == '#')
        continue;
      const size_t separator = line.find('=');
      if (separator == std::string::npos)
        continue;
      const std::string key = trim(line.substr(0, separator));
      const std::string value = trim(line.substr(separator + 1));
      if (key.empty() || value.empty())
        continue;
      ++count;
      if (!seen.insert(key).second)
        continue;
      if (value != plc::Translate(key.c_str(), "MISSING_TRANSLATION")) {
        std::cerr << argv[i] << ": missing/mismatched key " << key << '\n';
        return 1;
      }
    }
    tested_large_file = tested_large_file || count > 512;
    std::cout << "Verified " << count << " translations in " << argv[i] << '\n';
  }
  return argc > 1 && tested_large_file ? 0 : 1;
}
