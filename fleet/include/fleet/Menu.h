#pragma once
#include <iosfwd>

namespace fleet {
class Fleet;

class Menu {
public:
    Menu(Fleet& fleet, std::istream& in, std::ostream& out);
    void run();
    bool step();   // returns false only when Exit (10) was chosen

private:
    void doAdd();
    void doDisplayAll();
    void doSearch();
    void doAssign();
    void doUpdate();
    void doEndMission();
    void doAssociate();
    void doSendMessage();
    void doDisplayMessages();

    Fleet& fleet_;
    std::istream& in_;
    std::ostream& out_;
};
}
