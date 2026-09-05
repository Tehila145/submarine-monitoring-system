#ifndef TEST_UTIL_H
#define TEST_UTIL_H
#include <stdio.h>
static int g_fail = 0;
#define EXPECT(cond) do { if (!(cond)) { \
    printf("  FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__); g_fail++; } } while (0)
#define REPORT() do { \
    if (g_fail) { printf("%d CHECK(S) FAILED\n", g_fail); return 1; } \
    printf("OK\n"); return 0; } while (0)
#endif
