#pragma once
#include <string>
#include <fstream>

namespace central {

// Log module (spec §3.3): prints logs and persists them to a file.
class Log {
public:
    explicit Log(const std::string& path);
    void line(const std::string& s);   // print to stdout AND append to the log file

private:
    std::ofstream f_;
};

}
