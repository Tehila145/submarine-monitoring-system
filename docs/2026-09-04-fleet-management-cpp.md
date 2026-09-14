# Fleet Management System (C++) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the C++ Submarine Fleet Management System — an object model of research/combat submarines with mission assignment, combat-group association, inter-submarine messaging, and a 10-option console menu.

**Architecture:** One abstract `Submarine` base carries shared state (serial, name, mission status, message inbox) and two pure-virtual hooks (`display`, `updateMissionDetails`); `ResearchSubmarine` and `CombatSubmarine` override them. `CombatSubmarine` owns a `CentralComputer` by value (composition) and holds non-owning partner pointers. A `Fleet` owns all submarines via `unique_ptr` and hosts the cross-object operations (associate, sendMessage); a thin `Menu` reads/writes injected streams so it is testable. All I/O flows through `std::istream&`/`std::ostream&` parameters so every behaviour can be driven from a `std::stringstream` in a test.

**Tech Stack:** C++17, CMake ≥ 3.14, GoogleTest (pulled via CMake `FetchContent`).

**Spec:** `SUBMARINE_MONITORING_SYSTEM/final project.pdf` (OOP Part) and the derived design doc `SUBMARINE_MONITORING_SYSTEM/architecture.html` (§11–14) and `SUBMARINE_MONITORING_SYSTEM/class-diagram.html`.

## Global Constraints

- **Language:** C++17, no compiler extensions (`CMAKE_CXX_STANDARD 17`, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF`).
- **Ownership rule:** `Fleet` is the *sole owner* of submarines (`std::vector<std::unique_ptr<Submarine>>`). Every other submarine reference in the system (`partners_`, `Message::sender_`, `Menu`'s current selection) is a **non-owning raw pointer**. Never `delete` through a non-owning pointer.
- **Polymorphism rule:** No `if (type == ...)` / `dynamic_cast` branching in menu or fleet logic to choose *behaviour*. Type-specific behaviour goes through the virtual methods. (`dynamic_cast` is permitted only where an operation is defined as combat-only — associate/sendMessage — to obtain a `CombatSubmarine*`.)
- **Base class:** `Submarine` MUST declare a `virtual ~Submarine()` so deletion through `Submarine*` is safe.
- **I/O rule:** Domain classes never touch `std::cin`/`std::cout` directly. They take `std::istream&`/`std::ostream&` parameters. Only `main.cpp` binds those to the real console.
- **Namespace:** all library code lives in `namespace fleet`.
- **Every task builds and tests green before commit.**

**Design note (refines the design doc):** the diagram's thin `Mission` handle is realised as a `bool onMission_` status flag plus the type-specific detail fields on each subclass — there is no separate heap `Mission` object. This is simpler (YAGNI) and matches the spec, where "mission details" *are* the type-specific fields. Open questions #1 (CentralComputer on combat only) and #2 ("missions the fleet manages") from the design doc are resolved here as: CentralComputer on `CombatSubmarine` only; "participating together" is modelled by the `partners_` full-mesh group, with no separate fleet-wide mission registry.

---

## File Structure

```
fleet/
  CMakeLists.txt                     # library + executable + test wiring
  include/fleet/
    text.h            # trim / split / join string helpers
    Message.h         # value type: content + non-owning sender pointer
    Submarine.h       # abstract base
    ResearchSubmarine.h
    CentralComputer.h # tiny composed object (Part-1 stand-in)
    CombatSubmarine.h
    Fleet.h           # owns submarines; associate + sendMessage
    Menu.h            # stream-driven dispatch over a Fleet
  src/
    text.cpp
    Message.cpp
    Submarine.cpp
    ResearchSubmarine.cpp
    CentralComputer.cpp
    CombatSubmarine.cpp
    Fleet.cpp
    Menu.cpp
    main.cpp          # binds std::cin/std::cout to Menu; the only I/O site
  tests/
    CMakeLists.txt
    test_text.cpp
    test_message.cpp
    test_submarine.cpp
    test_research_submarine.cpp
    test_combat_submarine.cpp
    test_fleet.cpp
    test_menu.cpp
```

Responsibilities: each header/source pair is one class or one cohesive helper set. `Fleet` holds cross-object logic that doesn't belong to a single submarine. `Menu` holds presentation/dispatch only and delegates all state changes to `Fleet`.

---

### Task 1: Project scaffold + text helpers

Sets up CMake + GoogleTest and delivers the first tested unit (string helpers that later parsing depends on).

**Files:**
- Create: `fleet/CMakeLists.txt`
- Create: `fleet/include/fleet/text.h`
- Create: `fleet/src/text.cpp`
- Create: `fleet/tests/CMakeLists.txt`
- Test: `fleet/tests/test_text.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `std::string fleet::trim(const std::string& s);` — strips leading/trailing ASCII whitespace.
  - `std::vector<std::string> fleet::split(const std::string& s, char delim);` — splits on `delim`, trims each piece, drops empty pieces.
  - `std::string fleet::join(const std::vector<std::string>& parts, const std::string& sep);`

- [ ] **Step 1: Write the failing test**

`fleet/tests/test_text.cpp`:
```cpp
#include <gtest/gtest.h>
#include "fleet/text.h"

using namespace fleet;

TEST(Text, TrimStripsBothEnds) {
    EXPECT_EQ(trim("  hi  "), "hi");
    EXPECT_EQ(trim("nospace"), "nospace");
    EXPECT_EQ(trim("   "), "");
}

TEST(Text, SplitTrimsAndDropsEmpties) {
    std::vector<std::string> expected{"Dana Levi", "Omer Katz"};
    EXPECT_EQ(split(" Dana Levi , Omer Katz ,", ','), expected);
}

TEST(Text, JoinInsertsSeparator) {
    EXPECT_EQ(join({"a", "b", "c"}, ", "), "a, b, c");
    EXPECT_EQ(join({}, ", "), "");
}
```

- [ ] **Step 2: Create the build files**

`fleet/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.14)
project(fleet CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(fleet_lib
    src/text.cpp
)
target_include_directories(fleet_lib PUBLIC include)

add_executable(fleet src/main.cpp)
target_link_libraries(fleet PRIVATE fleet_lib)

enable_testing()
add_subdirectory(tests)
```
> As later tasks create `src/*.cpp`, append each new file to the `fleet_lib` source list. `main.cpp` is created in Task 8; until then, comment out the `add_executable`/`target_link_libraries` lines or create an empty `int main(){}` placeholder.

For now create a placeholder `fleet/src/main.cpp`:
```cpp
int main() { return 0; }
```

`fleet/tests/CMakeLists.txt`:
```cmake
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
FetchContent_MakeAvailable(googletest)

add_executable(fleet_tests
    test_text.cpp
)
target_link_libraries(fleet_tests PRIVATE fleet_lib GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(fleet_tests)
```
> As later tasks add `test_*.cpp`, append each to the `fleet_tests` source list.

- [ ] **Step 3: Run test to verify it fails**

Run:
```bash
cmake -S fleet -B fleet/build && cmake --build fleet/build --target fleet_tests
```
Expected: FAIL — `fatal error: fleet/text.h: No such file or directory`.

- [ ] **Step 4: Write minimal implementation**

`fleet/include/fleet/text.h`:
```cpp
#pragma once
#include <string>
#include <vector>

namespace fleet {
std::string trim(const std::string& s);
std::vector<std::string> split(const std::string& s, char delim);
std::string join(const std::vector<std::string>& parts, const std::string& sep);
}
```

`fleet/src/text.cpp`:
```cpp
#include "fleet/text.h"
#include <cctype>
#include <sstream>

namespace fleet {

std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        std::string t = trim(item);
        if (!t.empty()) out.push_back(t);
    }
    return out;
}

std::string join(const std::vector<std::string>& parts, const std::string& sep) {
    std::string out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) out += sep;
        out += parts[i];
    }
    return out;
}

}
```

- [ ] **Step 5: Run test to verify it passes**

Run: `cmake --build fleet/build --target fleet_tests && ctest --test-dir fleet/build --output-on-failure`
Expected: PASS (3 tests).

- [ ] **Step 6: Commit**

```bash
git add fleet/CMakeLists.txt fleet/tests/CMakeLists.txt fleet/include/fleet/text.h fleet/src/text.cpp fleet/tests/test_text.cpp fleet/src/main.cpp
git commit -m "feat: project scaffold + text helpers"
```

---

### Task 2: Message value type

**Files:**
- Create: `fleet/include/fleet/Message.h`, `fleet/src/Message.cpp`
- Test: `fleet/tests/test_message.cpp`
- Modify: `fleet/CMakeLists.txt` (add `src/Message.cpp`), `fleet/tests/CMakeLists.txt` (add `test_message.cpp`)

**Interfaces:**
- Consumes: nothing (uses a forward-declared `Submarine`).
- Produces:
  - `fleet::Message(std::string content, const Submarine* sender);`
  - `const std::string& Message::content() const;`
  - `const Submarine* Message::sender() const;`

- [ ] **Step 1: Write the failing test**

`fleet/tests/test_message.cpp`:
```cpp
#include <gtest/gtest.h>
#include "fleet/Message.h"

using namespace fleet;

TEST(Message, StoresContentAndSenderPointer) {
    const Submarine* fake = reinterpret_cast<const Submarine*>(0x1234);
    Message m("dive to 200m", fake);
    EXPECT_EQ(m.content(), "dive to 200m");
    EXPECT_EQ(m.sender(), fake);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build fleet/build --target fleet_tests`
Expected: FAIL — `fleet/Message.h: No such file or directory`.

- [ ] **Step 3: Write minimal implementation**

`fleet/include/fleet/Message.h`:
```cpp
#pragma once
#include <string>

namespace fleet {
class Submarine;  // non-owning reference only

class Message {
public:
    Message(std::string content, const Submarine* sender);
    const std::string& content() const;
    const Submarine* sender() const;
private:
    std::string content_;
    const Submarine* sender_;
};
}
```

`fleet/src/Message.cpp`:
```cpp
#include "fleet/Message.h"

namespace fleet {
Message::Message(std::string content, const Submarine* sender)
    : content_(std::move(content)), sender_(sender) {}
const std::string& Message::content() const { return content_; }
const Submarine* Message::sender() const { return sender_; }
}
```

Add `src/Message.cpp` to `fleet_lib` and `test_message.cpp` to `fleet_tests`.

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build fleet/build --target fleet_tests && ctest --test-dir fleet/build --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add fleet/include/fleet/Message.h fleet/src/Message.cpp fleet/tests/test_message.cpp fleet/CMakeLists.txt fleet/tests/CMakeLists.txt
git commit -m "feat: Message value type"
```

---

### Task 3: Submarine abstract base

Delivers shared state + the mission/inbox lifecycle. Tested through a minimal concrete test double defined inside the test file (the base is abstract).

**Files:**
- Create: `fleet/include/fleet/Submarine.h`, `fleet/src/Submarine.cpp`
- Test: `fleet/tests/test_submarine.cpp`
- Modify: both `CMakeLists.txt` files.

**Interfaces:**
- Consumes: `fleet::Message`.
- Produces (base class API relied on by every later task):
  - `Submarine(std::string serial, std::string name);`
  - `virtual ~Submarine();`
  - `const std::string& serial() const;` / `const std::string& name() const;`
  - `bool isAvailable() const;` — true iff `!onMission_`.
  - `void assignMission(std::istream& in, std::ostream& out);` — sets `onMission_ = true`, then calls `updateMissionDetails(in, out)`.
  - `void endMission();` — sets `onMission_ = false`, calls `clearMissionDetails()`.
  - `void receiveMessage(const Message& msg);`
  - `void displayMessages(std::ostream& out) const;`
  - `virtual std::string typeName() const = 0;`
  - `virtual void display(std::ostream& out) const = 0;`
  - `virtual void updateMissionDetails(std::istream& in, std::ostream& out) = 0;`
  - protected `virtual void clearMissionDetails() = 0;`
  - protected `void printHeader(std::ostream& out) const;` — prints `[<typeName>] Serial: <serial> | Name: <name> | Status: <Available|On mission>`.

- [ ] **Step 1: Write the failing test**

`fleet/tests/test_submarine.cpp`:
```cpp
#include <gtest/gtest.h>
#include <sstream>
#include "fleet/Submarine.h"
#include "fleet/Message.h"

using namespace fleet;

namespace {
// Minimal concrete double to exercise the abstract base.
class StubSub : public Submarine {
public:
    using Submarine::Submarine;
    std::string typeName() const override { return "Stub"; }
    void display(std::ostream& out) const override { printHeader(out); out << "\n"; }
    void updateMissionDetails(std::istream& in, std::ostream&) override {
        std::getline(in, detail);
    }
    std::string detail;
protected:
    void clearMissionDetails() override { detail.clear(); }
};
}

TEST(Submarine, StartsAvailable) {
    StubSub s("R-001", "Poseidon");
    EXPECT_EQ(s.serial(), "R-001");
    EXPECT_EQ(s.name(), "Poseidon");
    EXPECT_TRUE(s.isAvailable());
}

TEST(Submarine, AssignThenEndTogglesAvailabilityAndDetails) {
    StubSub s("R-001", "Poseidon");
    std::istringstream in("survey the trench\n");
    std::ostringstream out;
    s.assignMission(in, out);
    EXPECT_FALSE(s.isAvailable());
    EXPECT_EQ(s.detail, "survey the trench");
    s.endMission();
    EXPECT_TRUE(s.isAvailable());
    EXPECT_EQ(s.detail, "");
}

TEST(Submarine, InboxCollectsMessagesInOrder) {
    StubSub a("C-001", "Nautilus");
    StubSub b("C-002", "Triton");
    a.receiveMessage(Message("first", &b));
    a.receiveMessage(Message("second", &b));
    std::ostringstream out;
    a.displayMessages(out);
    EXPECT_EQ(out.str(),
        "Messages for C-001 (2):\n"
        "  from C-002: first\n"
        "  from C-002: second\n");
}

TEST(Submarine, DisplayMessagesEmpty) {
    StubSub a("C-001", "Nautilus");
    std::ostringstream out;
    a.displayMessages(out);
    EXPECT_EQ(out.str(), "Messages for C-001: none\n");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build fleet/build --target fleet_tests`
Expected: FAIL — `fleet/Submarine.h: No such file or directory`.

- [ ] **Step 3: Write minimal implementation**

`fleet/include/fleet/Submarine.h`:
```cpp
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
```

`fleet/src/Submarine.cpp`:
```cpp
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
```
> Note: `Message::sender()->serial()` requires the full `Submarine` definition here — `Submarine.cpp` includes `Submarine.h`, so it is complete at that point.

Add `src/Submarine.cpp` and `test_submarine.cpp` to the CMake lists.

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build fleet/build --target fleet_tests && ctest --test-dir fleet/build --output-on-failure`
Expected: PASS (4 new tests).

- [ ] **Step 5: Commit**

```bash
git add fleet/include/fleet/Submarine.h fleet/src/Submarine.cpp fleet/tests/test_submarine.cpp fleet/CMakeLists.txt fleet/tests/CMakeLists.txt
git commit -m "feat: Submarine abstract base with mission + inbox lifecycle"
```

---

### Task 4: ResearchSubmarine

**Files:**
- Create: `fleet/include/fleet/ResearchSubmarine.h`, `fleet/src/ResearchSubmarine.cpp`
- Test: `fleet/tests/test_research_submarine.cpp`
- Modify: both `CMakeLists.txt` files.

**Interfaces:**
- Consumes: `fleet::Submarine`, `fleet::text` helpers.
- Produces:
  - `ResearchSubmarine(std::string serial, std::string name);`
  - overrides `typeName()` → `"Research"`, `display()`, `updateMissionDetails()`, `clearMissionDetails()`.
  - `const std::string& topic() const;` / `const std::vector<std::string>& researchers() const;`
  - Input format for `updateMissionDetails`: line 1 = research topic; line 2 = researcher names, comma-separated.

- [ ] **Step 1: Write the failing test**

`fleet/tests/test_research_submarine.cpp`:
```cpp
#include <gtest/gtest.h>
#include <sstream>
#include "fleet/ResearchSubmarine.h"

using namespace fleet;

TEST(Research, UpdateMissionDetailsParsesTopicAndResearchers) {
    ResearchSubmarine r("R-001", "Poseidon");
    std::istringstream in("Deep-sea vents\nDana Levi, Omer Katz\n");
    std::ostringstream out;
    r.assignMission(in, out);
    EXPECT_EQ(r.topic(), "Deep-sea vents");
    ASSERT_EQ(r.researchers().size(), 2u);
    EXPECT_EQ(r.researchers()[0], "Dana Levi");
    EXPECT_EQ(r.researchers()[1], "Omer Katz");
}

TEST(Research, DisplayShowsAllFields) {
    ResearchSubmarine r("R-001", "Poseidon");
    std::istringstream in("Deep-sea vents\nDana Levi, Omer Katz\n");
    std::ostringstream sink;
    r.assignMission(in, sink);
    std::ostringstream out;
    r.display(out);
    EXPECT_EQ(out.str(),
        "[Research] Serial: R-001 | Name: Poseidon | Status: On mission\n"
        "  Topic: Deep-sea vents\n"
        "  Researchers: Dana Levi, Omer Katz\n");
}

TEST(Research, EndMissionClearsDetails) {
    ResearchSubmarine r("R-001", "Poseidon");
    std::istringstream in("Deep-sea vents\nDana Levi\n");
    std::ostringstream sink;
    r.assignMission(in, sink);
    r.endMission();
    EXPECT_TRUE(r.isAvailable());
    EXPECT_EQ(r.topic(), "");
    EXPECT_TRUE(r.researchers().empty());
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build fleet/build --target fleet_tests`
Expected: FAIL — `fleet/ResearchSubmarine.h: No such file or directory`.

- [ ] **Step 3: Write minimal implementation**

`fleet/include/fleet/ResearchSubmarine.h`:
```cpp
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
```

`fleet/src/ResearchSubmarine.cpp`:
```cpp
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
```
> The prompt strings (`"Research topic: "` etc.) go to `out`; tests send a throwaway `ostringstream` when they only care about parsed state, and assert on `out` only in the display test (which does not call `assignMission` into the same stream).

Add `src/ResearchSubmarine.cpp` and `test_research_submarine.cpp` to CMake.

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build fleet/build --target fleet_tests && ctest --test-dir fleet/build --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add fleet/include/fleet/ResearchSubmarine.h fleet/src/ResearchSubmarine.cpp fleet/tests/test_research_submarine.cpp fleet/CMakeLists.txt fleet/tests/CMakeLists.txt
git commit -m "feat: ResearchSubmarine with research-topic mission details"
```

---

### Task 5: CentralComputer

Tiny composed object so `CombatSubmarine` can own one by value.

**Files:**
- Create: `fleet/include/fleet/CentralComputer.h`, `fleet/src/CentralComputer.cpp`
- Test: fold into `test_combat_submarine.cpp` in Task 6 (no separate test file — a one-method value object doesn't warrant its own reviewer gate).
- Modify: `fleet/CMakeLists.txt` (add `src/CentralComputer.cpp`).

**Interfaces:**
- Produces:
  - `CentralComputer();`
  - `std::string status() const;` → `"operational"`.

- [ ] **Step 1: Write the implementation** (trivial value object; verified via CombatSubmarine's composition test in Task 6)

`fleet/include/fleet/CentralComputer.h`:
```cpp
#pragma once
#include <string>

namespace fleet {
// Stand-in for the Part-1 Central Computer; owned by each CombatSubmarine.
class CentralComputer {
public:
    CentralComputer() = default;
    std::string status() const { return "operational"; }
};
}
```

`fleet/src/CentralComputer.cpp`:
```cpp
#include "fleet/CentralComputer.h"
// Header-only behaviour today; .cpp kept for a stable translation unit
// as Part-1 wiring grows.
namespace fleet {}
```

Add `src/CentralComputer.cpp` to `fleet_lib`.

- [ ] **Step 2: Build to verify it compiles**

Run: `cmake --build fleet/build --target fleet_lib`
Expected: PASS (compiles).

- [ ] **Step 3: Commit**

```bash
git add fleet/include/fleet/CentralComputer.h fleet/src/CentralComputer.cpp fleet/CMakeLists.txt
git commit -m "feat: CentralComputer composed object"
```

---

### Task 6: CombatSubmarine

**Files:**
- Create: `fleet/include/fleet/CombatSubmarine.h`, `fleet/src/CombatSubmarine.cpp`
- Test: `fleet/tests/test_combat_submarine.cpp`
- Modify: both `CMakeLists.txt` files.

**Interfaces:**
- Consumes: `fleet::Submarine`, `fleet::CentralComputer`, `fleet::text`.
- Produces:
  - `CombatSubmarine(std::string serial, std::string name);`
  - overrides `typeName()` → `"Combat"`, `display()`, `updateMissionDetails()`, `clearMissionDetails()`.
  - `void addPartner(CombatSubmarine* other);` — bidirectional, ignores self and duplicates.
  - `const std::vector<CombatSubmarine*>& partners() const;`
  - `bool sharesMissionWith(const Submarine* other) const;` — true iff `other` is in `partners_`.
  - `const CentralComputer& centralComputer() const;`
  - Input format for `updateMissionDetails`: line 1 = mission description; line 2 = commander name; line 3 = combat personnel count (integer).

- [ ] **Step 1: Write the failing test**

`fleet/tests/test_combat_submarine.cpp`:
```cpp
#include <gtest/gtest.h>
#include <sstream>
#include "fleet/CombatSubmarine.h"

using namespace fleet;

TEST(Combat, UpdateMissionDetailsParsesThreeFields) {
    CombatSubmarine c("C-001", "Nautilus");
    std::istringstream in("Patrol sector 7\nCmdr Yael Bar\n42\n");
    std::ostringstream sink;
    c.assignMission(in, sink);
    std::ostringstream out;
    c.display(out);
    EXPECT_EQ(out.str(),
        "[Combat] Serial: C-001 | Name: Nautilus | Status: On mission\n"
        "  Mission: Patrol sector 7\n"
        "  Commander: Cmdr Yael Bar | Personnel: 42\n"
        "  Partners: \n");
}

TEST(Combat, OwnsAnOperationalCentralComputer) {
    CombatSubmarine c("C-001", "Nautilus");
    EXPECT_EQ(c.centralComputer().status(), "operational");
}

TEST(Combat, AddPartnerIsBidirectionalAndDeduped) {
    CombatSubmarine a("C-001", "Nautilus");
    CombatSubmarine b("C-002", "Triton");
    a.addPartner(&b);
    a.addPartner(&b);      // duplicate ignored
    a.addPartner(&a);      // self ignored
    ASSERT_EQ(a.partners().size(), 1u);
    ASSERT_EQ(b.partners().size(), 1u);
    EXPECT_EQ(a.partners()[0], &b);
    EXPECT_EQ(b.partners()[0], &a);
    EXPECT_TRUE(a.sharesMissionWith(&b));
    EXPECT_FALSE(a.sharesMissionWith(&a));
}

TEST(Combat, EndMissionClearsDetailsAndPartners) {
    CombatSubmarine a("C-001", "Nautilus");
    CombatSubmarine b("C-002", "Triton");
    a.addPartner(&b);
    std::istringstream in("Patrol\nCmdr X\n10\n");
    std::ostringstream sink;
    a.assignMission(in, sink);
    a.endMission();
    EXPECT_TRUE(a.isAvailable());
    EXPECT_TRUE(a.partners().empty());
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build fleet/build --target fleet_tests`
Expected: FAIL — `fleet/CombatSubmarine.h: No such file or directory`.

- [ ] **Step 3: Write minimal implementation**

`fleet/include/fleet/CombatSubmarine.h`:
```cpp
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
```

`fleet/src/CombatSubmarine.cpp`:
```cpp
#include "fleet/CombatSubmarine.h"
#include "fleet/text.h"
#include <istream>
#include <ostream>
#include <algorithm>
#include <string>

namespace fleet {

CombatSubmarine::CombatSubmarine(std::string serial, std::string name)
    : Submarine(std::move(serial), std::move(name)) {}

std::string CombatSubmarine::typeName() const { return "Combat"; }

void CombatSubmarine::updateMissionDetails(std::istream& in, std::ostream& out) {
    std::string line;
    out << "Mission description: ";
    std::getline(in, line);
    missionDescription_ = trim(line);
    out << "Commander name: ";
    std::getline(in, line);
    commanderName_ = trim(line);
    out << "Combat personnel: ";
    std::getline(in, line);
    combatPersonnel_ = std::stoi(trim(line.empty() ? "0" : line));
}

void CombatSubmarine::display(std::ostream& out) const {
    printHeader(out);
    std::vector<std::string> serials;
    for (const auto* p : partners_) serials.push_back(p->serial());
    out << "\n  Mission: " << missionDescription_
        << "\n  Commander: " << commanderName_
        << " | Personnel: " << combatPersonnel_
        << "\n  Partners: " << join(serials, ", ") << "\n";
}

void CombatSubmarine::addPartner(CombatSubmarine* other) {
    if (!other || other == this) return;
    auto here = std::find(partners_.begin(), partners_.end(), other);
    if (here == partners_.end()) partners_.push_back(other);
    auto there = std::find(other->partners_.begin(), other->partners_.end(), this);
    if (there == other->partners_.end()) other->partners_.push_back(this);
}

const std::vector<CombatSubmarine*>& CombatSubmarine::partners() const { return partners_; }

bool CombatSubmarine::sharesMissionWith(const Submarine* other) const {
    for (const auto* p : partners_)
        if (p == other) return true;
    return false;
}

const CentralComputer& CombatSubmarine::centralComputer() const { return centralComputer_; }

void CombatSubmarine::clearMissionDetails() {
    missionDescription_.clear();
    commanderName_.clear();
    combatPersonnel_ = 0;
    // Detach from partners symmetrically, then clear.
    for (auto* p : partners_) {
        auto& pp = const_cast<std::vector<CombatSubmarine*>&>(p->partners());
        pp.erase(std::remove(pp.begin(), pp.end(), this), pp.end());
    }
    partners_.clear();
}

}
```
> `clearMissionDetails` detaches this submarine from every partner's list before clearing its own, so ending a mission leaves no dangling back-references. `const_cast` is used only to reach the sibling's private vector through the public accessor; acceptable here because they are the same class.

Add `src/CombatSubmarine.cpp` and `test_combat_submarine.cpp` to CMake.

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build fleet/build --target fleet_tests && ctest --test-dir fleet/build --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add fleet/include/fleet/CombatSubmarine.h fleet/src/CombatSubmarine.cpp fleet/tests/test_combat_submarine.cpp fleet/CMakeLists.txt fleet/tests/CMakeLists.txt
git commit -m "feat: CombatSubmarine with partners, central computer, combat mission details"
```

---

### Task 7: Fleet — ownership, add, find, associate, sendMessage

**Files:**
- Create: `fleet/include/fleet/Fleet.h`, `fleet/src/Fleet.cpp`
- Test: `fleet/tests/test_fleet.cpp`
- Modify: both `CMakeLists.txt` files.

**Interfaces:**
- Consumes: all submarine types.
- Produces:
  - `Submarine* Fleet::add(std::unique_ptr<Submarine> sub);` — returns a non-owning observer pointer, or `nullptr` if a submarine with the same serial already exists (ownership then released/destroyed).
  - `Submarine* Fleet::findBySerial(const std::string& serial) const;` — `nullptr` if absent.
  - `const std::vector<std::unique_ptr<Submarine>>& Fleet::all() const;`
  - `bool Fleet::associate(CombatSubmarine* a, CombatSubmarine* b);` — full-mesh links `b` into `a`'s group; false if either null, equal, or either is available (not on a mission).
  - `bool Fleet::sendMessage(CombatSubmarine* from, Submarine* to, const std::string& text);` — false if null, equal, either available, or they do not share a mission; otherwise delivers `Message(text, from)` to `to` and returns true.

- [ ] **Step 1: Write the failing test**

`fleet/tests/test_fleet.cpp`:
```cpp
#include <gtest/gtest.h>
#include <sstream>
#include <memory>
#include "fleet/Fleet.h"
#include "fleet/ResearchSubmarine.h"
#include "fleet/CombatSubmarine.h"

using namespace fleet;

namespace {
CombatSubmarine* addCombat(Fleet& f, const std::string& serial, const std::string& name) {
    return static_cast<CombatSubmarine*>(
        f.add(std::make_unique<CombatSubmarine>(serial, name)));
}
void putOnMission(CombatSubmarine* c) {
    std::istringstream in("Patrol\nCmdr X\n5\n");
    std::ostringstream sink;
    c->assignMission(in, sink);
}
}

TEST(Fleet, AddRejectsDuplicateSerial) {
    Fleet f;
    EXPECT_NE(f.add(std::make_unique<ResearchSubmarine>("R-001", "A")), nullptr);
    EXPECT_EQ(f.add(std::make_unique<ResearchSubmarine>("R-001", "B")), nullptr);
    EXPECT_EQ(f.all().size(), 1u);
}

TEST(Fleet, FindBySerial) {
    Fleet f;
    f.add(std::make_unique<ResearchSubmarine>("R-001", "A"));
    EXPECT_NE(f.findBySerial("R-001"), nullptr);
    EXPECT_EQ(f.findBySerial("R-999"), nullptr);
}

TEST(Fleet, AssociateRequiresBothOnMission) {
    Fleet f;
    auto* a = addCombat(f, "C-001", "Nautilus");
    auto* b = addCombat(f, "C-002", "Triton");
    EXPECT_FALSE(f.associate(a, b));      // neither on mission
    putOnMission(a);
    putOnMission(b);
    EXPECT_TRUE(f.associate(a, b));
    EXPECT_TRUE(a->sharesMissionWith(b));
}

TEST(Fleet, AssociateFormsFullMeshGroup) {
    Fleet f;
    auto* a = addCombat(f, "C-001", "A");
    auto* b = addCombat(f, "C-002", "B");
    auto* c = addCombat(f, "C-003", "C");
    for (auto* x : {a, b, c}) putOnMission(x);
    ASSERT_TRUE(f.associate(a, b));
    ASSERT_TRUE(f.associate(a, c));   // c joins a's group -> also linked to b
    EXPECT_TRUE(b->sharesMissionWith(c));
    EXPECT_TRUE(c->sharesMissionWith(b));
}

TEST(Fleet, SendMessageDeliveredOnlyWithinSharedMission) {
    Fleet f;
    auto* a = addCombat(f, "C-001", "A");
    auto* b = addCombat(f, "C-002", "B");
    for (auto* x : {a, b}) putOnMission(x);
    EXPECT_FALSE(f.sendMessage(a, b, "hi"));   // not associated yet
    f.associate(a, b);
    EXPECT_TRUE(f.sendMessage(a, b, "dive"));
    std::ostringstream out;
    b->displayMessages(out);
    EXPECT_EQ(out.str(),
        "Messages for C-002 (1):\n"
        "  from C-001: dive\n");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build fleet/build --target fleet_tests`
Expected: FAIL — `fleet/Fleet.h: No such file or directory`.

- [ ] **Step 3: Write minimal implementation**

`fleet/include/fleet/Fleet.h`:
```cpp
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
```

`fleet/src/Fleet.cpp`:
```cpp
#include "fleet/Fleet.h"
#include "fleet/CombatSubmarine.h"
#include "fleet/Message.h"

namespace fleet {

Submarine* Fleet::add(std::unique_ptr<Submarine> sub) {
    if (!sub) return nullptr;
    if (findBySerial(sub->serial())) return nullptr;  // duplicate serial
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
```

Add `src/Fleet.cpp` and `test_fleet.cpp` to CMake.

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build fleet/build --target fleet_tests && ctest --test-dir fleet/build --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add fleet/include/fleet/Fleet.h fleet/src/Fleet.cpp fleet/tests/test_fleet.cpp fleet/CMakeLists.txt fleet/tests/CMakeLists.txt
git commit -m "feat: Fleet ownership, association groups, and mission-scoped messaging"
```

---

### Task 8: Menu dispatch + main entry point

Delivers the 10-option menu. Dispatch is a testable method over injected streams; `main` binds the console.

**Files:**
- Create: `fleet/include/fleet/Menu.h`, `fleet/src/Menu.cpp`
- Modify: `fleet/src/main.cpp` (replace placeholder), both `CMakeLists.txt` files.
- Test: `fleet/tests/test_menu.cpp`

**Interfaces:**
- Consumes: `fleet::Fleet` and all submarine types.
- Produces:
  - `Menu(Fleet& fleet, std::istream& in, std::ostream& out);`
  - `bool Menu::step();` — reads one menu choice, performs it, returns `false` only when the user chose `10` (Exit); `true` otherwise.
  - `void Menu::run();` — loops `step()` until it returns `false`.
  - Menu option numbers map exactly to the spec: 1 add, 2 display all, 3 search by serial, 4 assign mission, 5 update mission details, 6 end mission, 7 associate combat partners, 8 send message, 9 display received messages, 10 exit.

- [ ] **Step 1: Write the failing test**

`fleet/tests/test_menu.cpp`:
```cpp
#include <gtest/gtest.h>
#include <sstream>
#include "fleet/Menu.h"
#include "fleet/Fleet.h"

using namespace fleet;

// Add a research submarine (type 1), then display all (2), then exit (10).
TEST(Menu, AddResearchThenDisplayAll) {
    Fleet f;
    std::istringstream in(
        "1\n"              // add
        "research\n"       // type
        "R-001\n"          // serial
        "Poseidon\n"       // name
        "2\n"              // display all
        "10\n");           // exit
    std::ostringstream out;
    Menu menu(f, in, out);
    menu.run();
    EXPECT_NE(out.str().find("R-001"), std::string::npos);
    EXPECT_NE(out.str().find("[Research]"), std::string::npos);
    EXPECT_EQ(f.all().size(), 1u);
}

TEST(Menu, SearchBySerialReportsMissing) {
    Fleet f;
    std::istringstream in("3\nX-999\n10\n");   // search, serial, exit
    std::ostringstream out;
    Menu menu(f, in, out);
    menu.run();
    EXPECT_NE(out.str().find("not found"), std::string::npos);
}

TEST(Menu, StepReturnsFalseOnExit) {
    Fleet f;
    std::istringstream in("10\n");
    std::ostringstream out;
    Menu menu(f, in, out);
    EXPECT_FALSE(menu.step());
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build fleet/build --target fleet_tests`
Expected: FAIL — `fleet/Menu.h: No such file or directory`.

- [ ] **Step 3: Write minimal implementation**

`fleet/include/fleet/Menu.h`:
```cpp
#pragma once
#include <iosfwd>

namespace fleet {
class Fleet;

class Menu {
public:
    Menu(Fleet& fleet, std::istream& in, std::ostream& out);
    void run();
    bool step();   // false only when Exit (10) was chosen

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
```

`fleet/src/Menu.cpp`:
```cpp
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
```

`fleet/src/main.cpp` (replace placeholder):
```cpp
#include <iostream>
#include "fleet/Fleet.h"
#include "fleet/Menu.h"

int main() {
    fleet::Fleet fleet;
    fleet::Menu menu(fleet, std::cin, std::cout);
    menu.run();
    return 0;
}
```

Add `src/Menu.cpp` to `fleet_lib`, uncomment the `add_executable(fleet ...)` lines, and add `test_menu.cpp` to `fleet_tests`.

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build fleet/build && ctest --test-dir fleet/build --output-on-failure`
Expected: PASS (all suites, including the executable target building).

- [ ] **Step 5: Manual smoke of the real binary**

Run: `printf '1\nresearch\nR-001\nPoseidon\n2\n10\n' | ./fleet/build/fleet`
Expected: prints the menu, "Added R-001.", the research submarine details, then "Goodbye."

- [ ] **Step 6: Commit**

```bash
git add fleet/include/fleet/Menu.h fleet/src/Menu.cpp fleet/src/main.cpp fleet/tests/test_menu.cpp fleet/CMakeLists.txt fleet/tests/CMakeLists.txt
git commit -m "feat: console menu dispatch and main entry point"
```

---

## Self-Review

**Spec coverage** (OOP Part operations 1–10):
1. Add submarine → `Menu::doAdd` + `Fleet::add` (Task 7/8) ✓
2. Display all → `Menu::doDisplayAll` + virtual `display()` (Task 8, 4, 6) ✓
3. Search by serial → `Fleet::findBySerial` (Task 7) ✓
4. Assign mission → `Submarine::assignMission` (Task 3) ✓
5. Update mission details → virtual `updateMissionDetails` (Task 4, 6) ✓
6. End mission → `Submarine::endMission` (Task 3, 6) ✓
7. Associate combat subs → `Fleet::associate` (Task 7) ✓
8. Send message → `Fleet::sendMessage` (Task 7) ✓
9. Display received messages → `Submarine::displayMessages` (Task 3) ✓
10. Exit → `Menu::step` returns false on `10` (Task 8) ✓
Type-specific storage: research (researchers + topic, Task 4) ✓; combat (mission desc + commander + personnel + partners + CentralComputer, Task 5/6) ✓.

**Placeholder scan:** no TBD/TODO; every code step contains compilable code. ✓

**Type consistency:** `assignMission`/`updateMissionDetails` use `(std::istream&, std::ostream&)` everywhere; `sharesMissionWith(const Submarine*)`, `addPartner(CombatSubmarine*)`, `findBySerial`→`Submarine*`, `add`→`Submarine*` consistent across Tasks 3–8. ✓

**Open questions carried from the design doc** (settle with instructor; each is isolated to one method so a change is cheap):
- CentralComputer on combat only (Task 5/6). If it must be on all types, move the member to `Submarine`.
- "Missions the fleet manages" modelled as the `partners_` full mesh (Task 6/7); no separate registry. If a fleet-wide mission registry is required, add a `Mission` class owned by `Fleet` and have `associate` attach to it.
