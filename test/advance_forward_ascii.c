#ifdef UTF8_DFA_64
#  include "utf8_dfa64.h"
#else
#  include "utf8_dfa32.h"
#endif

#include "utf8_advance_forward.h"

#include "test.h"

static void test_stop_at_stride(void) {
  size_t advanced;
  // 16 ASCII, advance 8 -> lands on stride boundary
  CHECK(utf8_advance_forward_ascii("ABCDEFGHIJKLMNOP", 16, 8, &advanced) == 8,
        "stop at stride: distance 8 in 16 ASCII");
  CHECK(advanced == 8, "stop at stride: advanced 8");

  // 16 ASCII, advance 16 -> consumes both strides exactly
  CHECK(utf8_advance_forward_ascii("ABCDEFGHIJKLMNOP", 16, 16, &advanced) == 16,
        "stop at stride: distance 16 in 16 ASCII");
  CHECK(advanced == 16, "stop at stride: advanced 16");
}

static void test_stop_mid_stride(void) {
  size_t advanced;
  /* 16 ASCII, advance 3 -> fast path takes 8, overshoots;
   * but loop condition (distance - count >= 8) prevents entry,
   * scalar tail advances 3 */
  CHECK(utf8_advance_forward_ascii("ABCDEFGHIJKLMNOP", 16, 3, &advanced) == 3,
        "stop mid-stride: distance 3 in 16 ASCII");
  CHECK(advanced == 3, "stop mid-stride: advanced 3");

  // 24 ASCII, advance 10 -> 1 fast-path stride (count=8), then scalar 2
  CHECK(utf8_advance_forward_ascii("ABCDEFGHIJKLMNOPQRSTUVWX", 24, 10, &advanced) == 10,
        "stop mid-stride: distance 10 in 24 ASCII");
  CHECK(advanced == 10, "stop mid-stride: advanced 10");
}

static void test_fast_then_dfa(void) {
  size_t advanced;
  // 8 ASCII (fast) + é = 10 bytes, 9 cp; advance 9 -> offset 10
  CHECK(utf8_advance_forward_ascii("ABCDEFGH\xC3\xA9", 10, 9, &advanced) == 10,
        "fast then DFA: advance 9 past 8 ASCII + é");
  CHECK(advanced == 9, "fast then DFA: advanced 9");

  // advance 8 -> stops at byte 8 (just past the ASCII block)
  CHECK(utf8_advance_forward_ascii("ABCDEFGH\xC3\xA9", 10, 8, &advanced) == 8,
        "fast then DFA: advance 8, stop before é");
  CHECK(advanced == 8, "fast then DFA: advanced 8");
}

static void test_dfa_then_fast(void) {
  size_t advanced;
  // é + 8 ASCII = 10 bytes, 9 cp
  // DFA processes first 8 bytes (é + 6 ASCII = 7 cp), fast path takes stride 2
  CHECK(utf8_advance_forward_ascii("\xC3\xA9" "ABCDEFGH", 10, 9, &advanced) == 10,
        "DFA then fast: advance 9 past é + 8 ASCII");
  CHECK(advanced == 9, "DFA then fast: advanced 9");

  // 𐍈 + 12 ASCII = 16 bytes, 13 cp; advance 5
  CHECK(utf8_advance_forward_ascii("\xF0\x90\x8D\x88" "ABCDIJKLMNOP", 16, 5, &advanced) == 8,
        "DFA then fast: advance 5 past 4-byte + ASCII");
  CHECK(advanced == 5, "DFA then fast: advanced 5");
}

static void test_straddle(void) {
  size_t advanced;
  // 7 ASCII + é (bytes 7-8) + 7 ASCII = 16 bytes, 15 cp; advance 10
  CHECK(utf8_advance_forward_ascii("ABCDEFG\xC3\xA9HIJKLMN", 16, 10, &advanced) == 11,
        "straddle: advance 10 across boundary");
  CHECK(advanced == 10, "straddle: advanced 10");
}

static void test_state_guard(void) {
  size_t advanced;
  // \xC3 + 8 ASCII -> ill-formed
  CHECK(utf8_advance_forward_ascii("\xC3" "ABCDEFGH", 9, 5, &advanced) == (size_t)-1,
        "state guard: truncated lead + 8 ASCII -> -1");
  CHECK(advanced == 0, "state guard: advanced 0");

  // \xE2\x82 + 8 ASCII -> ill-formed
  CHECK(utf8_advance_forward_ascii("\xE2\x82" "ABCDEFGH", 10, 5, &advanced) == (size_t)-1,
        "state guard: truncated 3-byte + 8 ASCII -> -1");
  CHECK(advanced == 0, "state guard: advanced 0 (3-byte)");
}

static void test_exceed(void) {
  size_t advanced;
  CHECK(utf8_advance_forward_ascii("ABCDEFGH", 8, 100, &advanced) == 8,
        "exceed: distance 100 in 8 ASCII");
  CHECK(advanced == 8, "exceed: advanced 8");
}

static void test_empty(void) {
  size_t advanced;
  CHECK(utf8_advance_forward_ascii("", 0, 0, &advanced) == 0,
        "empty: distance 0");
  CHECK(advanced == 0, "empty: advanced 0");
}

static void test_null_advanced(void) {
  CHECK(utf8_advance_forward_ascii("ABCDEFGHIJKLMNOP", 16, 8, NULL) == 8,
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
    {"ABCDEFGH\xC3\xA9",                 10,  9},
    {"\xC3\xA9" "ABCDEFGH",              10,  5},
    {"\xF0\x90\x8D\x88" "ABCDIJKLMNOP",  16, 13},
    {"ABCDEFG\xC3\xA9HIJKLMN",           16, 10},
    {"ABCDEFGH\xE2\x82\xAC" "ABCDEFGH",  19, 15},
  };
  size_t n = sizeof(cases) / sizeof(cases[0]);
  for (size_t i = 0; i < n; i++) {
    size_t adv_base, adv_ascii;
    size_t off_base  = utf8_advance_forward(cases[i].s, cases[i].len,
                                            cases[i].dist, &adv_base);
    size_t off_ascii = utf8_advance_forward_ascii(cases[i].s, cases[i].len,
                                                  cases[i].dist, &adv_ascii);
    CHECK(off_ascii == off_base, "agrees: offset");
    CHECK(adv_ascii == adv_base, "agrees: advanced");
  }
}

int main(void) {
  SUITE(__FILE__);
  RUN(test_stop_at_stride);
  RUN(test_stop_mid_stride);
  RUN(test_fast_then_dfa);
  RUN(test_dfa_then_fast);
  RUN(test_straddle);
  RUN(test_state_guard);
  RUN(test_exceed);
  RUN(test_empty);
  RUN(test_null_advanced);
  RUN(test_agrees_with_base);
  return report_results();
}
