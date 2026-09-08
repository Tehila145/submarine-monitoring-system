#include "central/Log.h"
#include <iostream>

namespace central {

Log::Log(const std::string& path) : f_(path, std::ios::app) {}

void Log::line(const std::string& s) {
    std::cout << s << std::endl;
    if (f_) { f_ << s << '\n'; f_.flush(); }
}

}
