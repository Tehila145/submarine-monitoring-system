#pragma once
#include <memory>
#include <string>
#include <vector>
#include "fleet/Submarine.h"

namespace fleet {
class CombatSubmarine;

class Fleet {
public:
    Submarine* add(std::unique_ptr<Submarine> sub);
    Submarine* findBySerial(const std::string& serial) const;
    const std::vector<std::unique_ptr<Submarine>>& all() const;

    bool associate(CombatSubmarine* a, CombatSubmarine* b);
    bool sendMessage(CombatSubmarine* from, Submarine* to, const std::string& text);

private:
    std::vector<std::unique_ptr<Submarine>> subs_;
};
}
