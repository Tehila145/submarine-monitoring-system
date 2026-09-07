#pragma once
#include <string>
#include <vector>
#include <iosfwd>
#include "fleet/Message.h"

namespace fleet {

class Submarine {
public:
    Submarine(std::string serial, std::string name);
    virtual ~Submarine();

    const std::string& serial() const;
    const std::string& name() const;
    bool isAvailable() const;

    void assignMission(std::istream& in, std::ostream& out);
    void endMission();

    void receiveMessage(const Message& msg);
    void displayMessages(std::ostream& out) const;

    virtual std::string typeName() const = 0;
    virtual void display(std::ostream& out) const = 0;
    virtual void updateMissionDetails(std::istream& in, std::ostream& out) = 0;

protected:
    virtual void clearMissionDetails() = 0;
    void printHeader(std::ostream& out) const;

    std::string serial_;
    std::string name_;
    bool onMission_ = false;
    std::vector<Message> inbox_;
};

}
