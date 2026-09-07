#include "fleet/Menu.h"
#include "fleet/Fleet.h"
#include "fleet/ResearchSubmarine.h"
#include "fleet/CombatSubmarine.h"
#include "fleet/text.h"
#include <istream>
#include <ostream>
#include <string>
#include <memory>

namespace fleet {

Menu::Menu(Fleet& fleet, std::istream& in, std::ostream& out)
    : fleet_(fleet), in_(in), out_(out) {}

void Menu::run() { while (step()) {} }

static std::string readLine(std::istream& in) {
    std::string line;
    std::getline(in, line);
    return trim(line);
}

// Resolve a combat submarine by prompting for a serial; nullptr if missing/not combat.
static CombatSubmarine* readCombat(Fleet& f, std::istream& in, std::ostream& out,
                                   const char* prompt) {
    out << prompt;
    Submarine* s = f.findBySerial(readLine(in));
    return dynamic_cast<CombatSubmarine*>(s);
}

bool Menu::step() {
    out_ << "\n=== Fleet Menu ===\n"
            "1 Add  2 Display all  3 Search  4 Assign mission  5 Update mission\n"
            "6 End mission  7 Associate combat  8 Send message  9 Messages  10 Exit\n"
            "Choice: ";
    std::string choice = readLine(in_);
    if (choice.empty() && !in_) return false;   // EOF safety
    if (choice == "1") doAdd();
    else if (choice == "2") doDisplayAll();
    else if (choice == "3") doSearch();
    else if (choice == "4") doAssign();
    else if (choice == "5") doUpdate();
    else if (choice == "6") doEndMission();
    else if (choice == "7") doAssociate();
    else if (choice == "8") doSendMessage();
    else if (choice == "9") doDisplayMessages();
    else if (choice == "10") { out_ << "Goodbye.\n"; return false; }
    else out_ << "Unknown option.\n";
    return true;
}

void Menu::doAdd() {
    out_ << "Type (research/combat): ";
    std::string type = readLine(in_);
    out_ << "Serial: ";
    std::string serial = readLine(in_);
    out_ << "Name: ";
    std::string name = readLine(in_);
    std::unique_ptr<Submarine> sub;
    if (type == "research") sub = std::make_unique<ResearchSubmarine>(serial, name);
    else if (type == "combat") sub = std::make_unique<CombatSubmarine>(serial, name);
    else { out_ << "Unknown type.\n"; return; }
    if (fleet_.add(std::move(sub))) out_ << "Added " << serial << ".\n";
    else out_ << "Serial already exists.\n";
}

void Menu::doDisplayAll() {
    if (fleet_.all().empty()) { out_ << "Fleet is empty.\n"; return; }
    for (const auto& s : fleet_.all()) s->display(out_);
}

void Menu::doSearch() {
    out_ << "Serial: ";
    Submarine* s = fleet_.findBySerial(readLine(in_));
    if (s) s->display(out_);
    else out_ << "Submarine not found.\n";
}

void Menu::doAssign() {
    out_ << "Serial: ";
    Submarine* s = fleet_.findBySerial(readLine(in_));
    if (!s) { out_ << "Submarine not found.\n"; return; }
    s->assignMission(in_, out_);
    out_ << "Mission assigned.\n";
}

void Menu::doUpdate() {
    out_ << "Serial: ";
    Submarine* s = fleet_.findBySerial(readLine(in_));
    if (!s) { out_ << "Submarine not found.\n"; return; }
    if (s->isAvailable()) { out_ << "Submarine has no active mission.\n"; return; }
    s->updateMissionDetails(in_, out_);
    out_ << "Mission updated.\n";
}

void Menu::doEndMission() {
    out_ << "Serial: ";
    Submarine* s = fleet_.findBySerial(readLine(in_));
    if (!s) { out_ << "Submarine not found.\n"; return; }
    s->endMission();
    out_ << "Mission ended; submarine available.\n";
}

void Menu::doAssociate() {
    CombatSubmarine* a = readCombat(fleet_, in_, out_, "First combat serial: ");
    CombatSubmarine* b = readCombat(fleet_, in_, out_, "Second combat serial: ");
    if (fleet_.associate(a, b)) out_ << "Associated.\n";
    else out_ << "Cannot associate (need two distinct combat subs, both on a mission).\n";
}

void Menu::doSendMessage() {
    CombatSubmarine* from = readCombat(fleet_, in_, out_, "From combat serial: ");
    out_ << "To serial: ";
    Submarine* to = fleet_.findBySerial(readLine(in_));
    out_ << "Message: ";
    std::string text = readLine(in_);
    if (fleet_.sendMessage(from, to, text)) out_ << "Message sent.\n";
    else out_ << "Cannot send (sender must be combat; both on the same mission).\n";
}

void Menu::doDisplayMessages() {
    out_ << "Serial: ";
    Submarine* s = fleet_.findBySerial(readLine(in_));
    if (!s) { out_ << "Submarine not found.\n"; return; }
    s->displayMessages(out_);
}

}
