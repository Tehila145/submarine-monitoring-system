#include "fleet/Fleet.h"
#include "fleet/CombatSubmarine.h"
#include "fleet/Message.h"

namespace fleet {

Submarine* Fleet::add(std::unique_ptr<Submarine> sub) {
    if (!sub) return nullptr;
    if (findBySerial(sub->serial())) return nullptr;   // duplicate serial
    Submarine* observer = sub.get();
    subs_.push_back(std::move(sub));
    return observer;
}

Submarine* Fleet::findBySerial(const std::string& serial) const {
    for (const auto& s : subs_)
        if (s->serial() == serial) return s.get();
    return nullptr;
}

const std::vector<std::unique_ptr<Submarine>>& Fleet::all() const { return subs_; }

bool Fleet::associate(CombatSubmarine* a, CombatSubmarine* b) {
    if (!a || !b || a == b) return false;
    if (a->isAvailable() || b->isAvailable()) return false;
    // Link b to a and to every current member of a's group (full mesh).
    std::vector<CombatSubmarine*> group = a->partners();
    group.push_back(a);
    for (auto* member : group) member->addPartner(b);
    return true;
}

bool Fleet::sendMessage(CombatSubmarine* from, Submarine* to, const std::string& text) {
    if (!from || !to || from == to) return false;
    if (from->isAvailable() || to->isAvailable()) return false;
    if (!from->sharesMissionWith(to)) return false;
    to->receiveMessage(Message(text, from));
    return true;
}

}
