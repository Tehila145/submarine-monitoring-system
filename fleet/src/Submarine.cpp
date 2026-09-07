#include "fleet/Submarine.h"
#include <ostream>
#include <istream>

namespace fleet {

Submarine::Submarine(std::string serial, std::string name)
    : serial_(std::move(serial)), name_(std::move(name)) {}
Submarine::~Submarine() = default;

const std::string& Submarine::serial() const { return serial_; }
const std::string& Submarine::name() const { return name_; }
bool Submarine::isAvailable() const { return !onMission_; }

void Submarine::assignMission(std::istream& in, std::ostream& out) {
    onMission_ = true;
    updateMissionDetails(in, out);
}

void Submarine::endMission() {
    onMission_ = false;
    clearMissionDetails();
}

void Submarine::receiveMessage(const Message& msg) { inbox_.push_back(msg); }

void Submarine::displayMessages(std::ostream& out) const {
    if (inbox_.empty()) {
        out << "Messages for " << serial_ << ": none\n";
        return;
    }
    out << "Messages for " << serial_ << " (" << inbox_.size() << "):\n";
    for (const auto& m : inbox_) {
        const std::string from = m.sender() ? m.sender()->serial() : "unknown";
        out << "  from " << from << ": " << m.content() << "\n";
    }
}

void Submarine::printHeader(std::ostream& out) const {
    out << "[" << typeName() << "] Serial: " << serial_
        << " | Name: " << name_
        << " | Status: " << (onMission_ ? "On mission" : "Available");
}

}
