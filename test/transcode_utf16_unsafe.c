#include <string.h>

#include "utf8_dfa64.h"
#include "utf8_transcode.h"
#include "utf8_transcode_unsafe.h"

#include "test.h"

static void test_empty(void) {
  uint16_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf16_unsafe("", 0, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "empty: status OK");
  CHECK(r.consumed == 0,                 "empty: consumed 0");
  CHECK(r.decoded  == 0,                 "empty: decoded 0");
  CHECK(r.written  == 0,                 "empty: written 0");
  CHECK(r.advance  == 0,                 "empty: advance 0");
}

static void test_ascii_short(void) {
  uint16_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf16_unsafe("ABC", 3, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "ascii short: status OK");
  CHECK(r.consumed == 3,                 "ascii short: consumed 3");
  CHECK(r.decoded  == 3,                 "ascii short: decoded 3");
  CHECK(r.written  == 3,                 "ascii short: written 3");
  CHECK(dst[0] == 'A',                   "ascii short: dst[0]");
  CHECK(dst[1] == 'B',                   "ascii short: dst[1]");
  CHECK(dst[2] == 'C',                   "ascii short: dst[2]");
}

static void test_ascii_one_stride(void) {
  uint16_t dst[16];
  utf8_transcode_result_t r = utf8_transcode_utf16_unsafe("ABCDEFGH", 8, dst, 16);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "ascii stride: status OK");
  CHECK(r.consumed == 8,                 "ascii stride: consumed 8");
  CHECK(r.decoded  == 8,                 "ascii stride: decoded 8");
  CHECK(r.written  == 8,                 "ascii stride: written 8");
  CHECK(dst[0] == 'A',                   "ascii stride: dst[0]");
  CHECK(dst[7] == 'H',                   "ascii stride: dst[7]");
}

static void test_ascii_two_strides(void) {
  uint16_t dst[16];
  utf8_transcode_result_t r = utf8_transcode_utf16_unsafe("ABCDEFGHIJKLMNOP", 16, dst, 16);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "ascii 2 strides: status OK");
  CHECK(r.consumed == 16,                "ascii 2 strides: consumed 16");
  CHECK(r.decoded  == 16,                "ascii 2 strides: decoded 16");
  CHECK(r.written  == 16,                "ascii 2 strides: written 16");
  CHECK(dst[0]  == 'A',                  "ascii 2 strides: dst[0]");
  CHECK(dst[15] == 'P',                  "ascii 2 strides: dst[15]");
}

static void test_bmp(void) {
  // "é€" = U+00E9 U+20AC — both in BMP, one unit each
  const char *src = "\xC3\xA9\xE2\x82\xAC";
  uint16_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf16_unsafe(src, 5, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "bmp: status OK");
  CHECK(r.consumed == 5,                 "bmp: consumed 5");
  CHECK(r.decoded  == 2,                 "bmp: decoded 2");
  CHECK(r.written  == 2,                 "bmp: written 2");
  CHECK(dst[0] == 0x00E9,                "bmp: U+00E9");
  CHECK(dst[1] == 0x20AC,                "bmp: U+20AC");
}

static void test_surrogate_pair(void) {
  // U+10348 GOTHIC LETTER HWAIR: F0 90 8D 88 -> D800 DF48
  const char *src = "\xF0\x90\x8D\x88";
  uint16_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf16_unsafe(src, 4, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "surrogate pair: status OK");
  CHECK(r.consumed == 4,                 "surrogate pair: consumed 4");
  CHECK(r.decoded  == 1,                 "surrogate pair: decoded 1");
  CHECK(r.written  == 2,                 "surrogate pair: written 2");
  CHECK(dst[0] == 0xD800,                "surrogate pair: high surrogate");
  CHECK(dst[1] == 0xDF48,                "surrogate pair: low surrogate");
}

static void test_mixed(void) {
  // "Aé𐍈" = U+0041 U+00E9 U+10348 -> 1+1+2 = 4 units
  const char *src = "A\xC3\xA9\xF0\x90\x8D\x88";
  uint16_t dst[6];
  utf8_transcode_result_t r = utf8_transcode_utf16_unsafe(src, 7, dst, 6);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "mixed: status OK");
  CHECK(r.consumed == 7,                 "mixed: consumed 7");
  CHECK(r.decoded  == 3,                 "mixed: decoded 3");
  CHECK(r.written  == 4,                 "mixed: written 4");
  CHECK(dst[0] == 'A',                   "mixed: dst[0] A");
  CHECK(dst[1] == 0x00E9,                "mixed: dst[1] U+00E9");
  CHECK(dst[2] == 0xD800,                "mixed: dst[2] high surrogate");
  CHECK(dst[3] == 0xDF48,                "mixed: dst[3] low surrogate");
}

static void test_ascii_then_multibyte(void) {
  // 8 ASCII (fast path) + é (2-byte) = 10 bytes, 9 codepoints, 9 units
  const char *src = "ABCDEFGH\xC3\xA9";
  uint16_t dst[16];
  utf8_transcode_result_t r = utf8_transcode_utf16_unsafe(src, 10, dst, 16);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "ascii then multi: status OK");
  CHECK(r.consumed == 10,                "ascii then multi: consumed 10");
  CHECK(r.decoded  == 9,                 "ascii then multi: decoded 9");
  CHECK(r.written  == 9,                 "ascii then multi: written 9");
  CHECK(dst[8] == 0x00E9,                "ascii then multi: dst[8] U+00E9");
}

static void test_multibyte_then_ascii(void) {
  // é (2-byte) + 8 ASCII = 10 bytes, 9 codepoints, 9 units
  const char *src = "\xC3\xA9" "ABCDEFGH";
  uint16_t dst[16];
  utf8_transcode_result_t r = utf8_transcode_utf16_unsafe(src, 10, dst, 16);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "multi then ascii: status OK");
  CHECK(r.consumed == 10,                "multi then ascii: consumed 10");
  CHECK(r.decoded  == 9,                 "multi then ascii: decoded 9");
  CHECK(r.written  == 9,                 "multi then ascii: written 9");
  CHECK(dst[0] == 0x00E9,                "multi then ascii: dst[0] U+00E9");
  CHECK(dst[1] == 'A',                   "multi then ascii: dst[1] A");
}

static void test_batch_2byte(void) {
  // 4x é = 8 bytes, 4 codepoints
  const char *src = "\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9";
  uint16_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf16_unsafe(src, 8, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "batch 2byte: status OK");
  CHECK(r.consumed == 8,                 "batch 2byte: consumed 8");
  CHECK(r.decoded  == 4,                 "batch 2byte: decoded 4");
  CHECK(r.written  == 4,                 "batch 2byte: written 4");
  for (int i = 0; i < 4; i++)
    CHECK(dst[i] == 0x00E9,              "batch 2byte: dst value");
}

static void test_batch_3byte(void) {
  // 4x € = 12 bytes, 4 codepoints
  const char *src = "\xE2\x82\xAC\xE2\x82\xAC\xE2\x82\xAC\xE2\x82\xAC";
  uint16_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf16_unsafe(src, 12, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "batch 3byte: status OK");
  CHECK(r.consumed == 12,                "batch 3byte: consumed 12");
  CHECK(r.decoded  == 4,                 "batch 3byte: decoded 4");
  CHECK(r.written  == 4,                 "batch 3byte: written 4");
  for (int i = 0; i < 4; i++)
    CHECK(dst[i] == 0x20AC,              "batch 3byte: dst value");
}

static void test_batch_4byte(void) {
  // 2x 𐍈 = 8 bytes, 2 codepoints, 4 units (2 surrogate pairs)
  const char *src = "\xF0\x90\x8D\x88\xF0\x90\x8D\x88";
  uint16_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf16_unsafe(src, 8, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "batch 4byte: status OK");
  CHECK(r.consumed == 8,                 "batch 4byte: consumed 8");
  CHECK(r.decoded  == 2,                 "batch 4byte: decoded 2");
  CHECK(r.written  == 4,                 "batch 4byte: written 4");
  CHECK(dst[0] == 0xD800,                "batch 4byte: dst[0] high");
  CHECK(dst[1] == 0xDF48,                "batch 4byte: dst[1] low");
  CHECK(dst[2] == 0xD800,                "batch 4byte: dst[2] high");
  CHECK(dst[3] == 0xDF48,                "batch 4byte: dst[3] low");
}

static void test_exhausted(void) {
  uint16_t dst[2];
  utf8_transcode_result_t r = utf8_transcode_utf16_unsafe("ABCDEFGH", 8, dst, 2);
  CHECK(r.status   == UTF8_TRANSCODE_EXHAUSTED, "exhausted: status");
  CHECK(r.consumed == 2,                        "exhausted: consumed 2");
  CHECK(r.decoded  == 2,                        "exhausted: decoded 2");
  CHECK(r.written  == 2,                        "exhausted: written 2");
  CHECK(dst[0] == 'A',                          "exhausted: dst[0]");
  CHECK(dst[1] == 'B',                          "exhausted: dst[1]");
}

static void test_exhausted_surrogate(void) {
  // dst has only one unit left but next codepoint needs a surrogate pair
  const char *src = "A\xF0\x90\x8D\x88";
  uint16_t dst[2];
  utf8_transcode_result_t r = utf8_transcode_utf16_unsafe(src, 5, dst, 2);
  CHECK(r.status   == UTF8_TRANSCODE_EXHAUSTED, "exhausted surrogate: status");
  CHECK(r.consumed == 1,                        "exhausted surrogate: consumed 1");
  CHECK(r.decoded  == 1,                        "exhausted surrogate: decoded 1");
  CHECK(r.written  == 1,                        "exhausted surrogate: written 1");
  CHECK(dst[0] == 'A',                          "exhausted surrogate: dst[0]");
}

static void test_exhausted_resume(void) {
  const char *src = "ABCD";
  size_t len = 4;
  uint16_t dst[2];
  utf8_transcode_result_t r;

  r = utf8_transcode_utf16_unsafe(src, len, dst, 2);
  CHECK(r.status   == UTF8_TRANSCODE_EXHAUSTED, "resume: first status");
  CHECK(r.consumed == 2,                        "resume: first consumed");
  CHECK(r.decoded  == 2,                        "resume: first decoded");
  CHECK(r.written  == 2,                        "resume: first written");
  src += r.consumed;
  len -= r.consumed;

  r = utf8_transcode_utf16_unsafe(src, len, dst, 2);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "resume: second status");
  CHECK(r.consumed == 2,                 "resume: second consumed");
  CHECK(r.decoded  == 2,                 "resume: second decoded");
  CHECK(r.written  == 2,                 "resume: second written");
  CHECK(dst[0] == 'C',                   "resume: dst[0]");
  CHECK(dst[1] == 'D',                   "resume: dst[1]");
}

static void test_dst_exact(void) {
  uint16_t dst[3];
  utf8_transcode_result_t r = utf8_transcode_utf16_unsafe("ABC", 3, dst, 3);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "dst exact: status OK");
  CHECK(r.consumed == 3,                 "dst exact: consumed 3");
  CHECK(r.decoded  == 3,                 "dst exact: decoded 3");
  CHECK(r.written  == 3,                 "dst exact: written 3");
}

static void test_decoded_vs_written(void) {
  // 2x 𐍈 = 2 decoded, 4 written
  const char *src = "\xF0\x90\x8D\x88\xF0\x90\x8D\x88";
  uint16_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf16_unsafe(src, 8, dst, 8);
  CHECK(r.decoded == 2,                  "decoded vs written: decoded 2");
  CHECK(r.written == 4,                  "decoded vs written: written 4");

  // "A𐍈B" = 3 decoded, 4 written
  src = "A\xF0\x90\x8D\x88" "B";
  r = utf8_transcode_utf16_unsafe(src, 6, dst, 8);
  CHECK(r.decoded == 3,                  "decoded vs written: mixed decoded 3");
  CHECK(r.written == 4,                  "decoded vs written: mixed written 4");
}

static void test_agrees_with_safe(void) {
  static const struct {
    const char *s;
    size_t len;
  } cases[] = {
    {"",                                                    0},
    {"ABCDEFGH",                                            8},
    {"ABCDEFGHIJKLMNOP",                                   16},
    {"ABCDEFGHIJK",                                        11},
    {"\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9",                    8},
    {"\xE2\x82\xAC\xE2\x82\xAC",                            6},
    {"\xF0\x90\x8D\x88\xF0\x90\x8D\x88",                    8},
    {"A\xC3\xA9\xE2\x82\xAC\xF0\x90\x8D\x88",              10},
    {"ABCDEFGH\xC3\xA9",                                   10},
    {"\xC3\xA9" "ABCDEFGH",                                10},
    {"A\xF0\x90\x8D\x88" "B",                               6},
    {"ABCDEFGH\xE2\x82\xAC" "ABCDEFGH" "\xC3\xA9" "AB",    22},
  };
  size_t n = sizeof(cases) / sizeof(cases[0]);
  for (size_t i = 0; i < n; i++) {
    uint16_t d_safe[64], d_unsafe[64];
    utf8_transcode_result_t r_safe   = utf8_transcode_utf16(cases[i].s, cases[i].len,
                                                             d_safe, 64);
    utf8_transcode_result_t r_unsafe = utf8_transcode_utf16_unsafe(cases[i].s, cases[i].len,
                                                                    d_unsafe, 64);
    CHECK(r_unsafe.status   == r_safe.status,   "agrees: status");
    CHECK(r_unsafe.consumed == r_safe.consumed, "agrees: consumed");
    CHECK(r_unsafe.decoded  == r_safe.decoded,  "agrees: decoded");
    CHECK(r_unsafe.written  == r_safe.written,  "agrees: written");
    CHECK(memcmp(d_safe, d_unsafe, r_safe.written * sizeof(uint16_t)) == 0,
          "agrees: dst contents");
  }
}

int main(void) {
  test_empty();
  test_ascii_short();
  test_ascii_one_stride();
  test_ascii_two_strides();
  test_bmp();
  test_surrogate_pair();
  test_mixed();
  test_ascii_then_multibyte();
  test_multibyte_then_ascii();
  test_batch_2byte();
  test_batch_3byte();
  test_batch_4byte();
  test_exhausted();
  test_exhausted_surrogate();
  test_exhausted_resume();
  test_dst_exact();
  test_decoded_vs_written();
  test_agrees_with_safe();
  return report_results();
}
