#include <string.h>

#include "utf8_advance_backward_unsafe.h"

#ifdef UTF8_RDFA_64
#  include "utf8_rdfa64.h"
#else
#  include "utf8_rdfa32.h"
#endif
#include "utf8_advance_backward.h"

#include "test.h"

// "Aé€𐍈" = 1+2+3+4 = 10 bytes, 4 codepoints
static const char   *mixed     = "A\xC3\xA9\xE2\x82\xAC\xF0\x90\x8D\x88";
static const size_t  mixed_len = 10;

static void test_zero(void) {
  size_t advanced;
  CHECK(utf8_advance_backward_unsafe(mixed, mixed_len, 0, &advanced) == 10,
        "zero: offset 10");
  CHECK(advanced == 0, "zero: advanced 0");
}

static void test_one(void) {
  size_t advanced;
  CHECK(utf8_advance_backward_unsafe(mixed, mixed_len, 1, &advanced) == 6,
        "one: offset 6");
  CHECK(advanced == 1, "one: advanced 1");
}

static void test_two(void) {
  size_t advanced;
  CHECK(utf8_advance_backward_unsafe(mixed, mixed_len, 2, &advanced) == 3,
        "two: offset 3");
  CHECK(advanced == 2, "two: advanced 2");
}

static void test_three(void) {
  size_t advanced;
  CHECK(utf8_advance_backward_unsafe(mixed, mixed_len, 3, &advanced) == 1,
        "three: offset 1");
  CHECK(advanced == 3, "three: advanced 3");
}

static void test_exact(void) {
  size_t advanced;
  CHECK(utf8_advance_backward_unsafe(mixed, mixed_len, 4, &advanced) == 0,
        "exact: offset 0");
  CHECK(advanced == 4, "exact: advanced 4");
}

static void test_exceed(void) {
  size_t advanced;
  CHECK(utf8_advance_backward_unsafe(mixed, mixed_len, 5, &advanced) == 0,
        "exceed: offset 0");
  CHECK(advanced == 4, "exceed: advanced 4");

  CHECK(utf8_advance_backward_unsafe(mixed, mixed_len, 100, &advanced) == 0,
        "exceed 100: offset 0");
  CHECK(advanced == 4, "exceed 100: advanced 4");
}

static void test_empty(void) {
  size_t advanced;
  CHECK(utf8_advance_backward_unsafe("", 0, 0, &advanced) == 0,
        "empty d=0: offset 0");
  CHECK(advanced == 0, "empty d=0: advanced 0");

  CHECK(utf8_advance_backward_unsafe("", 0, 1, &advanced) == 0,
        "empty d=1: offset 0");
  CHECK(advanced == 0, "empty d=1: advanced 0");
}

static void test_ascii(void) {
  size_t advanced;
  CHECK(utf8_advance_backward_unsafe("ABCDEFGH", 8, 3, &advanced) == 5,
        "ascii: d=3 offset 5");
  CHECK(advanced == 3, "ascii: d=3 advanced 3");

  CHECK(utf8_advance_backward_unsafe("ABCDEFGH", 8, 8, &advanced) == 0,
        "ascii: d=8 offset 0");
  CHECK(advanced == 8, "ascii: d=8 advanced 8");

  CHECK(utf8_advance_backward_unsafe("ABCDEFGHIJKLMNOP", 16, 12, &advanced) == 4,
        "ascii: d=12 offset 4");
  CHECK(advanced == 12, "ascii: d=12 advanced 12");
}

static void test_2byte(void) {
  // 4x é = 8 bytes, 4 codepoints
  const char *src = "\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9";
  size_t advanced;
  CHECK(utf8_advance_backward_unsafe(src, 8, 2, &advanced) == 4,
        "2byte: d=2 offset 4");
  CHECK(advanced == 2, "2byte: d=2 advanced 2");
}

static void test_3byte(void) {
  // 3x € = 9 bytes, 3 codepoints
  const char *src = "\xE2\x82\xAC\xE2\x82\xAC\xE2\x82\xAC";
  size_t advanced;
  CHECK(utf8_advance_backward_unsafe(src, 9, 2, &advanced) == 3,
        "3byte: d=2 offset 3");
  CHECK(advanced == 2, "3byte: d=2 advanced 2");
}

static void test_4byte(void) {
  // 2x 𐍈 = 8 bytes, 2 codepoints
  const char *src = "\xF0\x90\x8D\x88\xF0\x90\x8D\x88";
  size_t advanced;
  CHECK(utf8_advance_backward_unsafe(src, 8, 1, &advanced) == 4,
        "4byte: d=1 offset 4");
  CHECK(advanced == 1, "4byte: d=1 advanced 1");
}

static void test_null_advanced(void) {
  CHECK(utf8_advance_backward_unsafe(mixed, mixed_len, 2, NULL) == 3,
        "null advanced: offset 3");
}

static void test_large_ascii(void) {
  char buf[2048];
  memset(buf, 'A', sizeof buf);
  size_t advanced;
  CHECK(utf8_advance_backward_unsafe(buf, sizeof buf, 1000, &advanced) == 1048,
        "large ascii: d=1000 offset 1048");
  CHECK(advanced == 1000, "large ascii: d=1000 advanced 1000");
}

static void test_agrees_with_safe(void) {
  static const struct {
    const char *s;
    size_t len;
    size_t distance;
  } cases[] = {
    {"",                                              0,  0},
    {"",                                              0,  1},
    {"A",                                             1,  0},
    {"A",                                             1,  1},
    {"A",                                             1,  2},
    {"ABC",                                           3,  2},
    {"ABCDEFGH",                                      8,  4},
    {"ABCDEFGH",                                      8,  8},
    {"ABCDEFGH",                                      8, 10},
    {"ABCDEFGHIJKLMNOP",                             16,  9},
    {"\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9",              8,  2},
    {"\xE2\x82\xAC\xE2\x82\xAC\xE2\x82\xAC",          9,  2},
    {"\xF0\x90\x8D\x88\xF0\x90\x8D\x88",              8,  1},
    {"A\xC3\xA9\xE2\x82\xAC\xF0\x90\x8D\x88",        10,  0},
    {"A\xC3\xA9\xE2\x82\xAC\xF0\x90\x8D\x88",        10,  1},
    {"A\xC3\xA9\xE2\x82\xAC\xF0\x90\x8D\x88",        10,  2},
    {"A\xC3\xA9\xE2\x82\xAC\xF0\x90\x8D\x88",        10,  3},
    {"A\xC3\xA9\xE2\x82\xAC\xF0\x90\x8D\x88",        10,  4},
    {"A\xC3\xA9\xE2\x82\xAC\xF0\x90\x8D\x88",        10,  5},
    {"ABCDEFGH\xC3\xA9",                             10,  9},
    {"\xC3\xA9" "ABCDEFGH",                          10,  5},
  };
  size_t n = sizeof(cases) / sizeof(cases[0]);
  for (size_t i = 0; i < n; i++) {
    size_t adv_safe = 0, adv_unsafe = 0;
    size_t r_safe   = utf8_advance_backward(cases[i].s, cases[i].len,
                                            cases[i].distance, &adv_safe);
    size_t r_unsafe = utf8_advance_backward_unsafe(cases[i].s, cases[i].len,
                                                   cases[i].distance, &adv_unsafe);
    CHECK(r_unsafe   == r_safe,   "agrees: offset");
    CHECK(adv_unsafe == adv_safe, "agrees: advanced");
  }
}

int main(void) {
  SUITE(__FILE__);
  RUN(test_zero);
  RUN(test_one);
  RUN(test_two);
  RUN(test_three);
  RUN(test_exact);
  RUN(test_exceed);
  RUN(test_empty);
  RUN(test_ascii);
  RUN(test_2byte);
  RUN(test_3byte);
  RUN(test_4byte);
  RUN(test_null_advanced);
  RUN(test_large_ascii);
  RUN(test_agrees_with_safe);
  return report_results();
}
