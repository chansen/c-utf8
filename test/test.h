/*
 * test.h -- minimal test harness for c-utf8.
 *
 * Provides TestCount / TestFailed counters, a CHECK macro,
 * RUN() for timed test groups, and report_results().
 * Every test file includes this header.
 */
#ifndef TEST_H
#define TEST_H
#include <stddef.h>
#include <stdio.h>
#include <time.h>

static size_t TestCount  = 0;
static size_t TestFailed = 0;
static size_t TestGroups = 0;

static inline double test_now(void) {
#if defined(CLOCK_MONOTONIC)
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
#else
  return (double)clock() / CLOCKS_PER_SEC;
#endif
}

#define CHECK(cond, msg) do { \
  TestCount++; \
  if (!(cond)) { \
    printf("FAIL: %s (line %d)\n", msg, __LINE__); \
    TestFailed++; \
  } \
} while (0)

/*
 * SUITE(name) -- print a header before the first RUN() in main().
 * Appends the DFA width tag when a 64-bit variant is compiled.
 */
#if defined(UTF8_DFA_64) || defined(UTF8_RDFA_64)
#  define SUITE(name) printf("\n--- %s [64] ---\n", (name))
#elif defined(UTF8_DFA_32) || defined(UTF8_RDFA_32)
#  define SUITE(name) printf("\n--- %s [32] ---\n", (name))
#else
#  define SUITE(name) printf("\n--- %s ---\n", (name))
#endif

/*
 * RUN(fn) -- run a void test function, print status with assertion
 * count and elapsed time:
 *
 *   ok test_name                              1234  (0.001s)
 */
#define RUN(fn) do { \
  size_t _before_count = TestCount; \
  size_t _before_fail  = TestFailed; \
  double _t0 = test_now(); \
  fn(); \
  double _t1 = test_now(); \
  size_t _assertions = TestCount - _before_count; \
  double _elapsed = (double)(_t1 - _t0); \
  const char *_status = (TestFailed == _before_fail) ? "ok" : "FAIL"; \
  printf("  %-4s %-40s %8zu  (%.3fs)\n", _status, #fn, _assertions, _elapsed); \
  TestGroups++; \
} while (0)

static inline int
report_results(void) {
  const char *gs = (TestGroups == 1) ? "group" : "groups";
  if (TestFailed) {
    printf("FAILED %zu of %zu assertions in %zu %s\n",
           TestFailed, TestCount, TestGroups, gs);
  } else {
    printf("passed %zu assertions in %zu %s\n",
           TestCount, TestGroups, gs);
  }
  return TestFailed ? 1 : 0;
}

#endif /* TEST_H */
