#pragma once
#include <string>

namespace fleet {
// Stand-in for the Part-1 Central Computer; owned by each CombatSubmarine.
class CentralComputer {
public:
    CentralComputer() = default;
    std::string status() const { return "operational"; }
};
}
