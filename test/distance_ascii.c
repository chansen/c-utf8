#ifdef UTF8_DFA_64
#  include "utf8_dfa64.h"
#else
#  include "utf8_dfa32.h"
#endif
#include "utf8_distance.h"

#include "test.h"

static void test_one_stride(void) {
  // exactly 8 ASCII bytes -> 1 fast-path iteration, no tail
  CHECK(utf8_distance_ascii("ABCDEFGH", 8) == 8,
        "one stride: 8 ASCII");
}

static void test_two_strides(void) {
  // 16 ASCII bytes -> 2 fast-path iterations
  CHECK(utf8_distance_ascii("ABCDEFGHIJKLMNOP", 16) == 16,
        "two strides: 16 ASCII");
}

static void test_three_strides(void) {
  // 24 ASCII bytes -> 3 fast-path iterations
  CHECK(utf8_distance_ascii("ABCDEFGHIJKLMNOPQRSTUVWX", 24) == 24,
        "three strides: 24 ASCII");
}

static void test_stride_plus_tail(void) {
  // 8 fast-path + 3 scalar tail
  CHECK(utf8_distance_ascii("ABCDEFGHIJK", 11) == 11,
        "stride + tail: 11 ASCII");
  // 16 fast-path + 1 scalar tail
  CHECK(utf8_distance_ascii("ABCDEFGHIJKLMNOPQ", 17) == 17,
        "stride + tail: 17 ASCII");
}

static void test_ascii_then_multibyte(void) {
  // 8 ASCII (fast) + é (2-byte, DFA fallback in tail) = 10 bytes, 9 cp
  CHECK(utf8_distance_ascii("ABCDEFGH\xC3\xA9", 10) == 9,
        "fast then DFA: 8 ASCII + 2-byte");
  // 8 ASCII (fast) + € (3-byte, DFA) + 5 ASCII (tail) = 16 bytes, 14 cp
  CHECK(utf8_distance_ascii("ABCDEFGH\xE2\x82\xAC" "KLMNO", 16) == 14,
        "fast then DFA: 8 ASCII + 3-byte + 5 ASCII tail");
}

static void test_multibyte_then_ascii(void) {
  // é (2-byte DFA) + 8 ASCII (fast) = 10 bytes, 9 cp
  CHECK(utf8_distance_ascii("\xC3\xA9" "ABCDEFGH", 10) == 9,
        "DFA then fast: 2-byte + 8 ASCII");
  // 𐍈 (4-byte DFA) + 4 ASCII (DFA, same stride) + 8 ASCII (fast) = 16 bytes, 13 cp
  CHECK(utf8_distance_ascii("\xF0\x90\x8D\x88" "ABCDIJKLMNOP", 16) == 13,
        "DFA then fast: 4-byte + 12 ASCII");
}

static void test_straddle_boundary(void) {
  /* 7 ASCII + é (bytes 7-8) + 7 ASCII = 16 bytes
   * stride 0: 7 ASCII + first byte of é -> DFA fallback
   * stride 1: second byte of é + 7 ASCII -> DFA fallback
   * result: 15 codepoints */
  CHECK(utf8_distance_ascii("ABCDEFG\xC3\xA9HIJKLMN", 16) == 15,
        "straddle: 2-byte across stride boundary");
}

static void test_state_guard(void) {
  /* \xC3 puts DFA in TAIL1 state, then 8 ASCII bytes follow.
   * Without state guard, fast path would skip the DFA and miss the error.
   * With state guard, falls through to DFA which detects ill-formed. */
  CHECK(utf8_distance_ascii("\xC3" "ABCDEFGH", 9) == (size_t)-1,
        "state guard: truncated lead + 8 ASCII -> -1");
  // \xE2\x82 puts DFA mid-sequence, then 8 ASCII
  CHECK(utf8_distance_ascii("\xE2\x82" "ABCDEFGH", 10) == (size_t)-1,
        "state guard: truncated 3-byte + 8 ASCII -> -1");
  // \xF0\x90\x8D puts DFA mid-sequence, then 8 ASCII
  CHECK(utf8_distance_ascii("\xF0\x90\x8D" "ABCDEFGH", 11) == (size_t)-1,
        "state guard: truncated 4-byte + 8 ASCII -> -1");
}

static void test_pure_multibyte_stride(void) {
  // 4x é = 8 bytes, 4 cp -> DFA fallback for full stride
  CHECK(utf8_distance_ascii("\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9", 8) == 4,
        "pure multibyte stride: 4x 2-byte");
  // 2x € + 2 ASCII = 8 bytes -> DFA fallback
  CHECK(utf8_distance_ascii("\xE2\x82\xAC\xE2\x82\xAC" "AB", 8) == 4,
        "pure multibyte stride: 2x 3-byte + 2 ASCII");
}

static void test_below_stride(void) {
  CHECK(utf8_distance_ascii("", 0) == 0,
        "empty");
  CHECK(utf8_distance_ascii("ABCDEFG", 7) == 7,
        "below stride: 7 ASCII (scalar only)");
}

static void test_agrees_with_base(void) {
  static const struct {
    const char *s;
    size_t len;
  } cases[] = {
    {"ABCDEFGH",                                         8},
    {"ABCDEFGHIJKLMNOP",                                16},
    {"ABCDEFGHIJKLMNOPQRSTUVWX",                        24},
    {"ABCDEFGHIJK",                                     11},
    {"ABCDEFGH\xC3\xA9",                                10},
    {"\xC3\xA9" "ABCDEFGH",                             10},
    {"\xF0\x90\x8D\x88" "ABCDIJKLMNOP",                 16},
    {"ABCDEFG\xC3\xA9HIJKLMN",                          16},
    {"\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9",                8},
    {"ABCDEFGH\xE2\x82\xAC" "ABCDEFGH" "\xC3\xA9" "AB", 22},
  };
  size_t n = sizeof(cases) / sizeof(cases[0]);
  for (size_t i = 0; i < n; i++) {
    size_t expected = utf8_distance(cases[i].s, cases[i].len);
    size_t got      = utf8_distance_ascii(cases[i].s, cases[i].len);
    CHECK(got == expected, "agrees with base");
  }
}

int main(void) {
  SUITE(__FILE__);
  RUN(test_one_stride);
  RUN(test_two_strides);
  RUN(test_three_strides);
  RUN(test_stride_plus_tail);
  RUN(test_ascii_then_multibyte);
  RUN(test_multibyte_then_ascii);
  RUN(test_straddle_boundary);
  RUN(test_state_guard);
  RUN(test_pure_multibyte_stride);
  RUN(test_below_stride);
  RUN(test_agrees_with_base);
  return report_results();
}
