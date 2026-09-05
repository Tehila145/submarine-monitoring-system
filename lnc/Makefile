CC      ?= cc
CFLAGS  := -std=c11 -Wall -Wextra -Isrc/core -Itest
CORE    := $(wildcard src/core/*.c)
TESTS   := $(wildcard test/test_*.c)

test:
	@set -e; for t in $(TESTS); do \
	  echo "== $$t =="; \
	  $(CC) $(CFLAGS) $$t $(CORE) -o /tmp/lnc_test; \
	  /tmp/lnc_test; \
	done; \
	echo "ALL HOST TESTS PASSED"

.PHONY: test
