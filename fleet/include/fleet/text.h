#pragma once
#include <string>
#include <vector>

namespace fleet {
std::string trim(const std::string& s);
std::vector<std::string> split(const std::string& s, char delim);
std::string join(const std::vector<std::string>& parts, const std::string& sep);
}
