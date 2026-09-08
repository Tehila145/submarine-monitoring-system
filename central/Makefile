CXX      ?= c++
CXXFLAGS := -std=c++17 -Wall -Wextra -Iinclude -I../lnc/src/core -pthread
# Reuse the LNC's C codec directly (compiled as C, bridged via extern "C").
CORE_C   := ../lnc/src/core/tlv.c ../lnc/src/core/bytes.c
ALLSRC   := $(wildcard src/*.cpp)
LIBSRC   := $(filter-out src/main.cpp,$(ALLSRC))
TESTS    := $(wildcard test/test_*.cpp)

test:
	@set -e; for t in $(TESTS); do \
	  echo "== $$t =="; \
	  $(CXX) $(CXXFLAGS) $$t $(LIBSRC) -x c++ $(CORE_C) -o /tmp/central_test; \
	  /tmp/central_test; \
	done; \
	echo "ALL CENTRAL TESTS PASSED"

central:
	$(CXX) $(CXXFLAGS) $(ALLSRC) -x c++ $(CORE_C) -o central

.PHONY: test central
