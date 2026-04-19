#include "utf8_rdfa64.h"
#include "utf8_decode_prev.h"
#include "utf8_decode_prev_unsafe.h"

#include "test.h"

static void test_end(void) {
  uint32_t cp = 0x1234;
  CHECK(utf8_decode_prev_unsafe("", 0, &cp) == 0, "end: returns 0");
  CHECK(cp == 0x1234,                              "end: cp unchanged");
}

static void test_ascii(void) {
  uint32_t cp;
  CHECK(utf8_decode_prev_unsafe("A",    1, &cp) == 1 && cp == 'A',   "ascii: A");
  CHECK(utf8_decode_prev_unsafe("\x00", 1, &cp) == 1 && cp == 0,     "ascii: U+0000");
  CHECK(utf8_decode_prev_unsafe("\x7F", 1, &cp) == 1 && cp == 0x7F,  "ascii: U+007F");
}

static void test_2byte(void) {
  uint32_t cp;
  CHECK(utf8_decode_prev_unsafe("\xC2\x80", 2, &cp) == 2 && cp == 0x0080, "2byte: U+0080");
  CHECK(utf8_decode_prev_unsafe("\xC3\xA9", 2, &cp) == 2 && cp == 0x00E9, "2byte: U+00E9");
  CHECK(utf8_decode_prev_unsafe("\xDF\xBF", 2, &cp) == 2 && cp == 0x07FF, "2byte: U+07FF");
}

static void test_3byte(void) {
  uint32_t cp;
  CHECK(utf8_decode_prev_unsafe("\xE0\xA0\x80", 3, &cp) == 3 && cp == 0x0800,  "3byte: U+0800");
  CHECK(utf8_decode_prev_unsafe("\xE2\x82\xAC", 3, &cp) == 3 && cp == 0x20AC,  "3byte: U+20AC");
  CHECK(utf8_decode_prev_unsafe("\xED\x9F\xBF", 3, &cp) == 3 && cp == 0xD7FF,  "3byte: U+D7FF");
  CHECK(utf8_decode_prev_unsafe("\xEE\x80\x80", 3, &cp) == 3 && cp == 0xE000,  "3byte: U+E000");
  CHECK(utf8_decode_prev_unsafe("\xEF\xBF\xBF", 3, &cp) == 3 && cp == 0xFFFF,  "3byte: U+FFFF");
}

static void test_4byte(void) {
  uint32_t cp;
  CHECK(utf8_decode_prev_unsafe("\xF0\x90\x80\x80", 4, &cp) == 4 && cp == 0x10000,  "4byte: U+10000");
  CHECK(utf8_decode_prev_unsafe("\xF0\x9F\x98\x80", 4, &cp) == 4 && cp == 0x1F600,  "4byte: U+1F600");
  CHECK(utf8_decode_prev_unsafe("\xF4\x8F\xBF\xBF", 4, &cp) == 4 && cp == 0x10FFFF, "4byte: U+10FFFF");
}

static void test_sequence(void) {
  // Walk "Aé€𐍈" backwards = 1+2+3+4 = 10 bytes
  const char *src = "A\xC3\xA9\xE2\x82\xAC\xF0\x90\x8D\x88";
  size_t len = 10;
  uint32_t cp;
  int n;

  n = utf8_decode_prev_unsafe(src, len, &cp);
  CHECK(n == 4 && cp == 0x10348, "seq: U+10348");
  len -= (size_t)n;

  n = utf8_decode_prev_unsafe(src, len, &cp);
  CHECK(n == 3 && cp == 0x20AC, "seq: U+20AC");
  len -= (size_t)n;

  n = utf8_decode_prev_unsafe(src, len, &cp);
  CHECK(n == 2 && cp == 0x00E9, "seq: U+00E9");
  len -= (size_t)n;

  n = utf8_decode_prev_unsafe(src, len, &cp);
  CHECK(n == 1 && cp == 'A', "seq: A");
  len -= (size_t)n;

  CHECK(utf8_decode_prev_unsafe(src, len, &cp) == 0, "seq: end");
}

static void test_extra_len(void) {
  // Passing more len than the last codepoint needs
  uint32_t cp;
  CHECK(utf8_decode_prev_unsafe("ABC",               3, &cp) == 1 && cp == 'C',     "extra: ascii");
  CHECK(utf8_decode_prev_unsafe("X\xC3\xA9",         3, &cp) == 2 && cp == 0x00E9,  "extra: 2byte");
  CHECK(utf8_decode_prev_unsafe("X\xE2\x82\xAC",     4, &cp) == 3 && cp == 0x20AC,  "extra: 3byte");
  CHECK(utf8_decode_prev_unsafe("X\xF0\x90\x8D\x88", 5, &cp) == 4 && cp == 0x10348, "extra: 4byte");
}

static void test_agrees_with_safe(void) {
  static const struct {
    const char *s;
    size_t len;
  } cases[] = {
    {"",                 0},
    {"A",                1},
    {"\x00",             1},
    {"\x7F",             1},
    {"\xC2\x80",         2},
    {"\xC3\xA9",         2},
    {"\xDF\xBF",         2},
    {"\xE0\xA0\x80",     3},
    {"\xE2\x82\xAC",     3},
    {"\xED\x9F\xBF",     3},
    {"\xEE\x80\x80",     3},
    {"\xEF\xBF\xBF",     3},
    {"\xF0\x90\x80\x80", 4},
    {"\xF0\x9F\x98\x80", 4},
    {"\xF4\x8F\xBF\xBF", 4},
  };
  size_t n = sizeof(cases) / sizeof(cases[0]);
  for (size_t i = 0; i < n; i++) {
    uint32_t cp_safe = 0, cp_unsafe = 0;
    int r_safe   = utf8_decode_prev(cases[i].s, cases[i].len, &cp_safe);
    int r_unsafe = utf8_decode_prev_unsafe(cases[i].s, cases[i].len, &cp_unsafe);
    CHECK(r_unsafe == r_safe,     "agrees: return value");
    CHECK(cp_unsafe == cp_safe,   "agrees: codepoint");
  }
}

int main(void) {
  test_end();
  test_ascii();
  test_2byte();
  test_3byte();
  test_4byte();
  test_sequence();
  test_extra_len();
  test_agrees_with_safe();
  return report_results();
}
