#include <iostream>
#include "fleet/Fleet.h"
#include "fleet/Menu.h"

int main() {
    fleet::Fleet fleet;
    fleet::Menu menu(fleet, std::cin, std::cout);
    menu.run();
    return 0;
}
