#pragma once
#include <string>
#include <vector>
#include "fleet/Submarine.h"

namespace fleet {

class ResearchSubmarine : public Submarine {
public:
    ResearchSubmarine(std::string serial, std::string name);
    std::string typeName() const override;
    void display(std::ostream& out) const override;
    void updateMissionDetails(std::istream& in, std::ostream& out) override;

    const std::string& topic() const;
    const std::vector<std::string>& researchers() const;

protected:
    void clearMissionDetails() override;

private:
    std::string topic_;
    std::vector<std::string> researchers_;
};

}
