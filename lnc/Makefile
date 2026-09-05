CC      ?= cc
CFLAGS  := -std=c11 -Wall -Wextra -Isrc/core -Isrc/app -Itest
# Pure, host-buildable sources: all of core plus the app scheduler helper.
# main.c is intentionally excluded (it pulls in STM32 HAL and only builds on target).
CORE    := $(wildcard src/core/*.c) src/app/sched.c
TESTS   := $(wildcard test/test_*.c)

test:
	@set -e; for t in $(TESTS); do \
	  echo "== $$t =="; \
	  $(CC) $(CFLAGS) $$t $(CORE) -o /tmp/lnc_test; \
	  /tmp/lnc_test; \
	done; \
	echo "ALL HOST TESTS PASSED"

.PHONY: test
