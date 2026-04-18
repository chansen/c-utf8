#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef UTF8_RDFA_64
#  include "utf8_rdfa64.h"
#else
#  include "utf8_rdfa32.h"
#endif

#include "utf8_advance_backward.h"

#include "test_common.h"

#define CHECK(cond, msg)          \
  do {                            \
    TestCount++;                  \
    if (!(cond)) {                \
      printf("FAIL: %s (line %d)\n", msg, __LINE__); \
      TestFailed++;               \
    }                             \
  } while (0)

static void test_stop_at_stride(void) {
  size_t advanced;
  // 16 ASCII, retreat 8 -> offset 8 (consumed last 8 bytes)
  CHECK(utf8_advance_backward_ascii("ABCDEFGHIJKLMNOP", 16, 8, &advanced) == 8,
        "stop at stride: distance 8 in 16 ASCII");
  CHECK(advanced == 8, "stop at stride: advanced 8");

  // 16 ASCII, retreat 16 -> offset 0
  CHECK(utf8_advance_backward_ascii("ABCDEFGHIJKLMNOP", 16, 16, &advanced) == 0,
        "stop at stride: distance 16 in 16 ASCII");
  CHECK(advanced == 16, "stop at stride: advanced 16");
}

static void test_stop_mid_stride(void) {
  size_t advanced;
  // 16 ASCII, retreat 3 -> offset 13
  CHECK(utf8_advance_backward_ascii("ABCDEFGHIJKLMNOP", 16, 3, &advanced) == 13,
        "stop mid-stride: distance 3 in 16 ASCII");
  CHECK(advanced == 3, "stop mid-stride: advanced 3");

  // 24 ASCII, retreat 10 -> 1 fast-path stride (count=8), then scalar 2
  CHECK(utf8_advance_backward_ascii("ABCDEFGHIJKLMNOPQRSTUVWX", 24, 10, &advanced) == 14,
        "stop mid-stride: distance 10 in 24 ASCII");
  CHECK(advanced == 10, "stop mid-stride: advanced 10");
}

static void test_fast_then_dfa(void) {
  size_t advanced;
  // é + 8 ASCII = 10 bytes, 9 cp; retreat 9 -> offset 0
  CHECK(utf8_advance_backward_ascii("\xC3\xA9" "ABCDEFGH", 10, 9, &advanced) == 0,
        "fast then DFA: retreat 9 past 8 ASCII + é");
  CHECK(advanced == 9, "fast then DFA: advanced 9");

  // retreat 8 -> stops at byte 2 (just before é)
  CHECK(utf8_advance_backward_ascii("\xC3\xA9" "ABCDEFGH", 10, 8, &advanced) == 2,
        "fast then DFA: retreat 8, stop after é");
  CHECK(advanced == 8, "fast then DFA: advanced 8");
}

static void test_dfa_then_fast(void) {
  size_t advanced;
  // 8 ASCII + é = 10 bytes, 9 cp; retreat 9 -> offset 0
  CHECK(utf8_advance_backward_ascii("ABCDEFGH\xC3\xA9", 10, 9, &advanced) == 0,
        "DFA then fast: retreat 9 past é + 8 ASCII");
  CHECK(advanced == 9, "DFA then fast: advanced 9");

  // 12 ASCII + 𐍈 = 16 bytes, 13 cp; retreat 5
  CHECK(utf8_advance_backward_ascii("ABCDIJKLMNOP\xF0\x90\x8D\x88", 16, 5, &advanced) == 8,
        "DFA then fast: retreat 5 past 4-byte + ASCII");
  CHECK(advanced == 5, "DFA then fast: advanced 5");
}

static void test_straddle(void) {
  size_t advanced;
  // 7 ASCII + é (bytes 7-8) + 7 ASCII = 16 bytes, 15 cp; retreat 10
  CHECK(utf8_advance_backward_ascii("ABCDEFG\xC3\xA9HIJKLMN", 16, 10, &advanced) == 5,
        "straddle: retreat 10 across boundary");
  CHECK(advanced == 10, "straddle: advanced 10");
}

static void test_state_guard(void) {
  size_t advanced;
  // 8 ASCII + \xA9 (bare continuation) -> ill-formed
  CHECK(utf8_advance_backward_ascii("ABCDEFGH\xA9", 9, 5, &advanced) == (size_t)-1,
        "state guard: 8 ASCII + bare continuation -> -1");
  CHECK(advanced == 0, "state guard: advanced 0");

  /* 8 ASCII + \x80\x82\xE2 (reverse: \xE2\x82\x80 is valid \xE2\x82\x80,
   * but let's use an actually-ill-formed reverse sequence)
   * 8 ASCII + \xC3 (truncated) -> ill-formed */
  CHECK(utf8_advance_backward_ascii("ABCDEFGH\xC3", 9, 5, &advanced) == (size_t)-1,
        "state guard: 8 ASCII + truncated lead -> -1");
  CHECK(advanced == 0, "state guard: advanced 0 (truncated)");
}

static void test_exceed(void) {
  size_t advanced;
  CHECK(utf8_advance_backward_ascii("ABCDEFGH", 8, 100, &advanced) == 0,
        "exceed: distance 100 in 8 ASCII");
  CHECK(advanced == 8, "exceed: advanced 8");
}

static void test_empty(void) {
  size_t advanced;
  CHECK(utf8_advance_backward_ascii("", 0, 0, &advanced) == 0,
        "empty: distance 0");
  CHECK(advanced == 0, "empty: advanced 0");
}

static void test_null_advanced(void) {
  CHECK(utf8_advance_backward_ascii("ABCDEFGHIJKLMNOP", 16, 8, NULL) == 8,
        "null advanced: offset 8");
}

static void test_agrees_with_base(void) {
  static const struct {
    const char *s;
    size_t len;
    size_t dist;
  } cases[] = {
    {"ABCDEFGH",                          8,  8},
    {"ABCDEFGHIJKLMNOP",                 16,  3},
    {"ABCDEFGHIJKLMNOP",                 16, 16},
    {"ABCDEFGHIJKLMNOP",                 16, 20},
    {"\xC3\xA9" "ABCDEFGH",              10,  9},
    {"ABCDEFGH\xC3\xA9",                 10,  5},
    {"ABCDIJKLMNOP\xF0\x90\x8D\x88",     16, 13},
    {"ABCDEFG\xC3\xA9HIJKLMN",           16, 10},
    {"ABCDEFGH\xE2\x82\xAC" "ABCDEFGH",  19, 15},
  };
  size_t n = sizeof(cases) / sizeof(cases[0]);
  for (size_t i = 0; i < n; i++) {
    size_t adv_base, adv_ascii;
    size_t off_base  = utf8_advance_backward(cases[i].s, cases[i].len,
                                             cases[i].dist, &adv_base);
    size_t off_ascii = utf8_advance_backward_ascii(cases[i].s, cases[i].len,
                                                   cases[i].dist, &adv_ascii);
    CHECK(off_ascii == off_base, "agrees: offset");
    CHECK(adv_ascii == adv_base, "agrees: advanced");
  }
}

int main(void) {
  test_stop_at_stride();
  test_stop_mid_stride();
  test_fast_then_dfa();
  test_dfa_then_fast();
  test_straddle();
  test_state_guard();
  test_exceed();
  test_empty();
  test_null_advanced();
  test_agrees_with_base();
  return report_results();
}
