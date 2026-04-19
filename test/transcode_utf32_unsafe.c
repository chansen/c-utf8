#include <string.h>

#include "utf8_dfa64.h"
#include "utf8_transcode.h"
#include "utf8_transcode_unsafe.h"

#include "test.h"

static void test_empty(void) {
  uint32_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf32_unsafe("", 0, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "empty: status OK");
  CHECK(r.consumed == 0,                 "empty: consumed 0");
  CHECK(r.decoded  == 0,                 "empty: decoded 0");
  CHECK(r.written  == 0,                 "empty: written 0");
  CHECK(r.advance  == 0,                 "empty: advance 0");
}

static void test_ascii_short(void) {
  uint32_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf32_unsafe("ABC", 3, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "ascii short: status OK");
  CHECK(r.consumed == 3,                 "ascii short: consumed 3");
  CHECK(r.decoded  == 3,                 "ascii short: decoded 3");
  CHECK(r.written  == 3,                 "ascii short: written 3");
  CHECK(dst[0] == 'A',                   "ascii short: dst[0]");
  CHECK(dst[1] == 'B',                   "ascii short: dst[1]");
  CHECK(dst[2] == 'C',                   "ascii short: dst[2]");
}

static void test_ascii_one_stride(void) {
  // exactly 8 ASCII -> hits fast path once
  uint32_t dst[16];
  utf8_transcode_result_t r = utf8_transcode_utf32_unsafe("ABCDEFGH", 8, dst, 16);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "ascii stride: status OK");
  CHECK(r.consumed == 8,                 "ascii stride: consumed 8");
  CHECK(r.decoded  == 8,                 "ascii stride: decoded 8");
  CHECK(dst[0] == 'A',                   "ascii stride: dst[0]");
  CHECK(dst[7] == 'H',                   "ascii stride: dst[7]");
}

static void test_ascii_two_strides(void) {
  uint32_t dst[16];
  utf8_transcode_result_t r = utf8_transcode_utf32_unsafe("ABCDEFGHIJKLMNOP", 16, dst, 16);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "ascii 2 strides: status OK");
  CHECK(r.consumed == 16,                "ascii 2 strides: consumed 16");
  CHECK(r.decoded  == 16,                "ascii 2 strides: decoded 16");
  CHECK(dst[0]  == 'A',                  "ascii 2 strides: dst[0]");
  CHECK(dst[15] == 'P',                  "ascii 2 strides: dst[15]");
}

static void test_ascii_stride_plus_tail(void) {
  // 8 fast-path + 3 scalar tail
  uint32_t dst[16];
  utf8_transcode_result_t r = utf8_transcode_utf32_unsafe("ABCDEFGHIJK", 11, dst, 16);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "stride+tail: status OK");
  CHECK(r.consumed == 11,                "stride+tail: consumed 11");
  CHECK(r.decoded  == 11,                "stride+tail: decoded 11");
  CHECK(dst[10] == 'K',                  "stride+tail: dst[10]");
}

static void test_2byte(void) {
  // "éé" = 4 bytes, 2 codepoints
  const char *src = "\xC3\xA9\xC3\xA9";
  uint32_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf32_unsafe(src, 4, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "2byte: status OK");
  CHECK(r.consumed == 4,                 "2byte: consumed 4");
  CHECK(r.decoded  == 2,                 "2byte: decoded 2");
  CHECK(dst[0] == 0x00E9,                "2byte: dst[0] U+00E9");
  CHECK(dst[1] == 0x00E9,                "2byte: dst[1] U+00E9");
}

static void test_3byte(void) {
  // "€€" = 6 bytes, 2 codepoints
  const char *src = "\xE2\x82\xAC\xE2\x82\xAC";
  uint32_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf32_unsafe(src, 6, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "3byte: status OK");
  CHECK(r.consumed == 6,                 "3byte: consumed 6");
  CHECK(r.decoded  == 2,                 "3byte: decoded 2");
  CHECK(dst[0] == 0x20AC,                "3byte: dst[0] U+20AC");
  CHECK(dst[1] == 0x20AC,                "3byte: dst[1] U+20AC");
}

static void test_4byte(void) {
  // U+10348 GOTHIC LETTER HWAIR
  const char *src = "\xF0\x90\x8D\x88";
  uint32_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf32_unsafe(src, 4, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "4byte: status OK");
  CHECK(r.consumed == 4,                 "4byte: consumed 4");
  CHECK(r.decoded  == 1,                 "4byte: decoded 1");
  CHECK(dst[0] == 0x10348,               "4byte: dst[0] U+10348");
}

static void test_mixed(void) {
  // "Aé€𐍈" = 1+2+3+4 = 10 bytes, 4 codepoints
  const char *src = "A\xC3\xA9\xE2\x82\xAC\xF0\x90\x8D\x88";
  uint32_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf32_unsafe(src, 10, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "mixed: status OK");
  CHECK(r.consumed == 10,                "mixed: consumed 10");
  CHECK(r.decoded  == 4,                 "mixed: decoded 4");
  CHECK(dst[0] == 'A',                   "mixed: dst[0] A");
  CHECK(dst[1] == 0x00E9,                "mixed: dst[1] U+00E9");
  CHECK(dst[2] == 0x20AC,                "mixed: dst[2] U+20AC");
  CHECK(dst[3] == 0x10348,               "mixed: dst[3] U+10348");
}

static void test_ascii_then_multibyte(void) {
  // 8 ASCII (fast path) + é (2-byte) = 10 bytes, 9 codepoints
  const char *src = "ABCDEFGH\xC3\xA9";
  uint32_t dst[16];
  utf8_transcode_result_t r = utf8_transcode_utf32_unsafe(src, 10, dst, 16);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "ascii then multi: status OK");
  CHECK(r.consumed == 10,                "ascii then multi: consumed 10");
  CHECK(r.decoded  == 9,                 "ascii then multi: decoded 9");
  CHECK(dst[8] == 0x00E9,                "ascii then multi: dst[8] U+00E9");
}

static void test_multibyte_then_ascii(void) {
  // é (2-byte) + 8 ASCII = 10 bytes, 9 codepoints
  const char *src = "\xC3\xA9" "ABCDEFGH";
  uint32_t dst[16];
  utf8_transcode_result_t r = utf8_transcode_utf32_unsafe(src, 10, dst, 16);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "multi then ascii: status OK");
  CHECK(r.consumed == 10,                "multi then ascii: consumed 10");
  CHECK(r.decoded  == 9,                 "multi then ascii: decoded 9");
  CHECK(dst[0] == 0x00E9,                "multi then ascii: dst[0] U+00E9");
  CHECK(dst[1] == 'A',                   "multi then ascii: dst[1] A");
}

static void test_batch_2byte(void) {
  // 4x é = 8 bytes, 4 codepoints — tests do/while batching of 2-byte class
  const char *src = "\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9";
  uint32_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf32_unsafe(src, 8, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "batch 2byte: status OK");
  CHECK(r.consumed == 8,                 "batch 2byte: consumed 8");
  CHECK(r.decoded  == 4,                 "batch 2byte: decoded 4");
  for (int i = 0; i < 4; i++)
    CHECK(dst[i] == 0x00E9,              "batch 2byte: dst value");
}

static void test_batch_3byte(void) {
  // 4x € = 12 bytes, 4 codepoints — tests do/while batching of 3-byte class
  const char *src = "\xE2\x82\xAC\xE2\x82\xAC\xE2\x82\xAC\xE2\x82\xAC";
  uint32_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf32_unsafe(src, 12, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "batch 3byte: status OK");
  CHECK(r.consumed == 12,                "batch 3byte: consumed 12");
  CHECK(r.decoded  == 4,                 "batch 3byte: decoded 4");
  for (int i = 0; i < 4; i++)
    CHECK(dst[i] == 0x20AC,              "batch 3byte: dst value");
}

static void test_batch_4byte(void) {
  // 2x 𐍈 = 8 bytes, 2 codepoints — tests do/while batching of 4-byte class
  const char *src = "\xF0\x90\x8D\x88\xF0\x90\x8D\x88";
  uint32_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf32_unsafe(src, 8, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "batch 4byte: status OK");
  CHECK(r.consumed == 8,                 "batch 4byte: consumed 8");
  CHECK(r.decoded  == 2,                 "batch 4byte: decoded 2");
  CHECK(dst[0] == 0x10348,               "batch 4byte: dst[0]");
  CHECK(dst[1] == 0x10348,               "batch 4byte: dst[1]");
}

static void test_exhausted(void) {
  uint32_t dst[2];
  utf8_transcode_result_t r = utf8_transcode_utf32_unsafe("ABCDEFGH", 8, dst, 2);
  CHECK(r.status   == UTF8_TRANSCODE_EXHAUSTED, "exhausted: status");
  CHECK(r.consumed == 2,                        "exhausted: consumed 2");
  CHECK(r.decoded  == 2,                        "exhausted: decoded 2");
  CHECK(dst[0] == 'A',                          "exhausted: dst[0]");
  CHECK(dst[1] == 'B',                          "exhausted: dst[1]");
}

static void test_exhausted_resume(void) {
  const char *src = "ABCD";
  size_t len = 4;
  uint32_t dst[2];
  utf8_transcode_result_t r;

  r = utf8_transcode_utf32_unsafe(src, len, dst, 2);
  CHECK(r.status   == UTF8_TRANSCODE_EXHAUSTED, "resume: first status");
  CHECK(r.consumed == 2,                        "resume: first consumed");
  src += r.consumed;
  len -= r.consumed;

  r = utf8_transcode_utf32_unsafe(src, len, dst, 2);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "resume: second status");
  CHECK(r.consumed == 2,                 "resume: second consumed");
  CHECK(dst[0] == 'C',                   "resume: dst[0]");
  CHECK(dst[1] == 'D',                   "resume: dst[1]");
}

static void test_dst_exact(void) {
  uint32_t dst[3];
  utf8_transcode_result_t r = utf8_transcode_utf32_unsafe("ABC", 3, dst, 3);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "dst exact: status OK");
  CHECK(r.consumed == 3,                 "dst exact: consumed 3");
  CHECK(r.decoded  == 3,                 "dst exact: decoded 3");
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
    {"ABCDEFGH\xE2\x82\xAC" "ABCDEFGH" "\xC3\xA9" "AB",    22},
  };
  size_t n = sizeof(cases) / sizeof(cases[0]);
  for (size_t i = 0; i < n; i++) {
    uint32_t d_safe[64], d_unsafe[64];
    utf8_transcode_result_t r_safe   = utf8_transcode_utf32(cases[i].s, cases[i].len,
                                                             d_safe, 64);
    utf8_transcode_result_t r_unsafe = utf8_transcode_utf32_unsafe(cases[i].s, cases[i].len,
                                                                    d_unsafe, 64);
    CHECK(r_unsafe.status   == r_safe.status,   "agrees: status");
    CHECK(r_unsafe.consumed == r_safe.consumed, "agrees: consumed");
    CHECK(r_unsafe.decoded  == r_safe.decoded,  "agrees: decoded");
    CHECK(r_unsafe.written  == r_safe.written,  "agrees: written");
    CHECK(memcmp(d_safe, d_unsafe, r_safe.written * sizeof(uint32_t)) == 0,
          "agrees: dst contents");
  }
}

int main(void) {
  test_empty();
  test_ascii_short();
  test_ascii_one_stride();
  test_ascii_two_strides();
  test_ascii_stride_plus_tail();
  test_2byte();
  test_3byte();
  test_4byte();
  test_mixed();
  test_ascii_then_multibyte();
  test_multibyte_then_ascii();
  test_batch_2byte();
  test_batch_3byte();
  test_batch_4byte();
  test_exhausted();
  test_exhausted_resume();
  test_dst_exact();
  test_agrees_with_safe();
  return report_results();
}
