CXX      ?= c++
CXXFLAGS := -std=c++17 -Wall -Wextra -Iinclude -I../central/include -I../lnc/src/core
# Reuse the LNC's C codec and the shared Ground<->Central protocol.
CORE_C   := ../lnc/src/core/tlv.c ../lnc/src/core/bytes.c
SHARED   := ../central/src/ground_protocol.cpp
ALLSRC   := $(wildcard src/*.cpp)
LIBSRC   := $(filter-out src/main.cpp,$(ALLSRC))
TESTS    := $(wildcard test/test_*.cpp)

groundstation:
	$(CXX) $(CXXFLAGS) $(ALLSRC) $(SHARED) -x c++ $(CORE_C) -o groundstation

test:
	@set -e; for t in $(TESTS); do \
	  echo "== $$t =="; \
	  $(CXX) $(CXXFLAGS) $$t $(LIBSRC) $(SHARED) -x c++ $(CORE_C) -o /tmp/gs_test; \
	  /tmp/gs_test; \
	done; \
	echo "ALL GROUND STATION TESTS PASSED"

.PHONY: groundstation test
