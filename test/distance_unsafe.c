#include <string.h>

#include "utf8_distance_unsafe.h"

#ifdef UTF8_DFA_64
#  include "utf8_dfa64.h"
#else
#  include "utf8_dfa32.h"
#endif
#include "utf8_distance.h"

#include "test.h"

static void test_empty(void) {
  CHECK(utf8_distance_unsafe("", 0) == 0, "empty: 0");
}

static void test_ascii(void) {
  CHECK(utf8_distance_unsafe("A", 1) == 1,       "ascii: 1");
  CHECK(utf8_distance_unsafe("ABC", 3) == 3,     "ascii: 3");
  CHECK(utf8_distance_unsafe("ABCDEFGH", 8) == 8, "ascii: 8 (one block)");
  CHECK(utf8_distance_unsafe("ABCDEFGHIJKLMNOP", 16) == 16, "ascii: 16 (two blocks)");
}

static void test_ascii_tail(void) {
  // 8 block + 1-7 tail
  CHECK(utf8_distance_unsafe("ABCDEFGHI",      9) == 9,  "ascii tail: 9");
  CHECK(utf8_distance_unsafe("ABCDEFGHIJ",    10) == 10, "ascii tail: 10");
  CHECK(utf8_distance_unsafe("ABCDEFGHIJKLMNO", 15) == 15, "ascii tail: 15");
}

static void test_2byte(void) {
  // 4x é = 8 bytes, 4 codepoints
  const char *src = "\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9";
  CHECK(utf8_distance_unsafe(src, 8) == 4, "2byte: 4x U+00E9");
}

static void test_3byte(void) {
  // 4x € = 12 bytes, 4 codepoints
  const char *src = "\xE2\x82\xAC\xE2\x82\xAC\xE2\x82\xAC\xE2\x82\xAC";
  CHECK(utf8_distance_unsafe(src, 12) == 4, "3byte: 4x U+20AC");
}

static void test_4byte(void) {
  // 2x 𐍈 = 8 bytes, 2 codepoints
  const char *src = "\xF0\x90\x8D\x88\xF0\x90\x8D\x88";
  CHECK(utf8_distance_unsafe(src, 8) == 2, "4byte: 2x U+10348");
}

static void test_mixed(void) {
  // "Aé€𐍈" = 1+2+3+4 = 10 bytes, 4 codepoints
  const char *src = "A\xC3\xA9\xE2\x82\xAC\xF0\x90\x8D\x88";
  CHECK(utf8_distance_unsafe(src, 10) == 4, "mixed: 4 codepoints");
}

static void test_partial(void) {
  // "Aé" = 3 bytes, 2 codepoints
  const char *src = "A\xC3\xA9\xE2\x82\xAC\xF0\x90\x8D\x88";
  CHECK(utf8_distance_unsafe(src, 3) == 2, "partial: 2 codepoints");
}

static void test_nul(void) {
  // NUL bytes are valid ASCII codepoints
  const char src[8] = {0};
  CHECK(utf8_distance_unsafe(src, 8) == 8, "nul: 8 NUL bytes = 8 codepoints");
}

static void test_large(void) {
  // 256 blocks of 8 ASCII bytes = 2048 codepoints
  char buf[2048];
  memset(buf, 'A', sizeof buf);
  CHECK(utf8_distance_unsafe(buf, sizeof buf) == 2048, "large: 2048 ASCII");

  // 512 x 2-byte = 1024 bytes, 512 codepoints
  char buf2[1024];
  for (size_t i = 0; i < sizeof buf2; i += 2) {
    buf2[i]     = (char)0xC3;
    buf2[i + 1] = (char)0xA9;
  }
  CHECK(utf8_distance_unsafe(buf2, sizeof buf2) == 512, "large: 512x 2-byte");
}

static void test_agrees_with_safe(void) {
  static const struct {
    const char *s;
    size_t len;
  } cases[] = {
    {"",                                                     0},
    {"A",                                                    1},
    {"ABC",                                                  3},
    {"ABCDEFGH",                                             8},
    {"ABCDEFGHIJKLMNOP",                                     6},
    {"ABCDEFGHIJK",                                         11},
    {"\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9",                     8},
    {"\xE2\x82\xAC\xE2\x82\xAC\xE2\x82\xAC\xE2\x82\xAC",    12},
    {"\xF0\x90\x8D\x88\xF0\x90\x8D\x88",                     8},
    {"A\xC3\xA9\xE2\x82\xAC\xF0\x90\x8D\x88",               10},
    {"ABCDEFGH\xC3\xA9",                                    10},
    {"\xC3\xA9" "ABCDEFGH",                                 10},
    {"ABCDEFGH\xE2\x82\xAC" "ABCDEFGH" "\xC3\xA9" "AB",     22},
  };
  size_t n = sizeof(cases) / sizeof(cases[0]);
  for (size_t i = 0; i < n; i++) {
    size_t r_safe   = utf8_distance(cases[i].s, cases[i].len);
    size_t r_unsafe = utf8_distance_unsafe(cases[i].s, cases[i].len);
    CHECK(r_unsafe == r_safe, "agrees: distance");
  }
}

int main(void) {
  SUITE(__FILE__);
  RUN(test_empty);
  RUN(test_ascii);
  RUN(test_ascii_tail);
  RUN(test_2byte);
  RUN(test_3byte);
  RUN(test_4byte);
  RUN(test_mixed);
  RUN(test_partial);
  RUN(test_nul);
  RUN(test_large);
  RUN(test_agrees_with_safe);
  return report_results();
}
