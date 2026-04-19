/*
 * test.h -- minimal test harness for c-utf8.
 *
 * Provides TestCount / TestFailed counters, a CHECK macro,
 * and report_results().  Every test file includes this header.
 */
#ifndef TEST_H
#define TEST_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

static size_t TestCount  = 0;
static size_t TestFailed = 0;

#define CHECK(cond, msg) do { \
  TestCount++; \
  if (!(cond)) { \
    printf("FAIL: %s (line %d)\n", msg, __LINE__); \
    TestFailed++; \
  } \
} while (0)

static inline int
report_results(void) {
  if (TestFailed)
    printf("FAILED %zu tests of %zu.\n", TestFailed, TestCount);
  else
    printf("Passed %zu tests.\n", TestCount);
  return TestFailed ? 1 : 0;
}

#endif /* TEST_H */
