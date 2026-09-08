#include "fleet/CombatSubmarine.h"
#include "fleet/text.h"
#include <istream>
#include <ostream>
#include <algorithm>
#include <string>

namespace fleet {

CombatSubmarine::CombatSubmarine(std::string serial, std::string name)
    : Submarine(std::move(serial), std::move(name)) {}

std::string CombatSubmarine::typeName() const { return "Combat"; }

void CombatSubmarine::updateMissionDetails(std::istream& in, std::ostream& out) {
    std::string line;
    out << "Mission description: ";
    std::getline(in, line);
    missionDescription_ = trim(line);
    out << "Commander name: ";
    std::getline(in, line);
    commanderName_ = trim(line);
    out << "Combat personnel: ";
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty()) { combatPersonnel_ = 0; break; }   // blank = 0
        try {
            std::size_t used = 0;
            int n = std::stoi(line, &used);
            if (used == line.size() && n >= 0) { combatPersonnel_ = n; break; }
        } catch (const std::exception&) { /* fall through to re-prompt */ }
        out << "  Please enter a whole number (crew count): ";  // reject "Dave", "-3", "5x"
    }
}

void CombatSubmarine::display(std::ostream& out) const {
    printHeader(out);
    std::vector<std::string> serials;
    for (const auto* p : partners_) serials.push_back(p->serial());
    out << "\n  Mission: " << missionDescription_
        << "\n  Commander: " << commanderName_
        << " | Personnel: " << combatPersonnel_
        << "\n  Partners: " << join(serials, ", ") << "\n";
}

void CombatSubmarine::addPartner(CombatSubmarine* other) {
    if (!other || other == this) return;
    if (std::find(partners_.begin(), partners_.end(), other) == partners_.end())
        partners_.push_back(other);
    if (std::find(other->partners_.begin(), other->partners_.end(), this) == other->partners_.end())
        other->partners_.push_back(this);
}

const std::vector<CombatSubmarine*>& CombatSubmarine::partners() const { return partners_; }

bool CombatSubmarine::sharesMissionWith(const Submarine* other) const {
    for (const auto* p : partners_)
        if (p == other) return true;
    return false;
}

const CentralComputer& CombatSubmarine::centralComputer() const { return centralComputer_; }

void CombatSubmarine::clearMissionDetails() {
    missionDescription_.clear();
    commanderName_.clear();
    combatPersonnel_ = 0;
    // Detach symmetrically from each partner before clearing.
    for (auto* p : partners_) {
        auto& pp = p->partners_;
        pp.erase(std::remove(pp.begin(), pp.end(), this), pp.end());
    }
    partners_.clear();
}

}
