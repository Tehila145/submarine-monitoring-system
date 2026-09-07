#ifndef TEST_UTIL_H
#define TEST_UTIL_H
#include <cstdio>
static int g_fail = 0;
#define EXPECT(cond) do { if (!(cond)) { \
    std::printf("  FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__); ++g_fail; } } while (0)
#define REPORT() do { \
    if (g_fail) { std::printf("%d CHECK(S) FAILED\n", g_fail); return 1; } \
    std::printf("OK\n"); return 0; } while (0)
#endif
