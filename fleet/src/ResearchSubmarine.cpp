#include "fleet/ResearchSubmarine.h"
#include "fleet/text.h"
#include <istream>
#include <ostream>

namespace fleet {

ResearchSubmarine::ResearchSubmarine(std::string serial, std::string name)
    : Submarine(std::move(serial), std::move(name)) {}

std::string ResearchSubmarine::typeName() const { return "Research"; }

void ResearchSubmarine::updateMissionDetails(std::istream& in, std::ostream& out) {
    out << "Research topic: ";
    std::string line;
    std::getline(in, line);
    topic_ = trim(line);
    out << "Researchers (comma-separated): ";
    std::getline(in, line);
    researchers_ = split(line, ',');
}

void ResearchSubmarine::display(std::ostream& out) const {
    printHeader(out);
    out << "\n  Topic: " << topic_
        << "\n  Researchers: " << join(researchers_, ", ") << "\n";
}

const std::string& ResearchSubmarine::topic() const { return topic_; }
const std::vector<std::string>& ResearchSubmarine::researchers() const { return researchers_; }

void ResearchSubmarine::clearMissionDetails() {
    topic_.clear();
    researchers_.clear();
}

}
