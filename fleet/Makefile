CXX      ?= c++
CXXFLAGS := -std=c++17 -Wall -Wextra -Iinclude -Itest
ALLSRC   := $(wildcard src/*.cpp)
LIBSRC   := $(filter-out src/main.cpp,$(ALLSRC))
TESTS    := $(wildcard test/test_*.cpp)

test:
	@set -e; for t in $(TESTS); do \
	  echo "== $$t =="; \
	  $(CXX) $(CXXFLAGS) $$t $(LIBSRC) -o /tmp/fleet_test; \
	  /tmp/fleet_test; \
	done; \
	echo "ALL FLEET TESTS PASSED"

fleet:
	$(CXX) $(CXXFLAGS) $(ALLSRC) -o fleet

.PHONY: test fleet
