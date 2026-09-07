#pragma once
#include <string>
#include <vector>
#include "fleet/Submarine.h"
#include "fleet/CentralComputer.h"

namespace fleet {

class CombatSubmarine : public Submarine {
public:
    CombatSubmarine(std::string serial, std::string name);
    std::string typeName() const override;
    void display(std::ostream& out) const override;
    void updateMissionDetails(std::istream& in, std::ostream& out) override;

    void addPartner(CombatSubmarine* other);
    const std::vector<CombatSubmarine*>& partners() const;
    bool sharesMissionWith(const Submarine* other) const;
    const CentralComputer& centralComputer() const;

protected:
    void clearMissionDetails() override;

private:
    std::string missionDescription_;
    std::string commanderName_;
    int combatPersonnel_ = 0;
    std::vector<CombatSubmarine*> partners_;
    CentralComputer centralComputer_;
};

}
