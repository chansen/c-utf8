#include <string.h>
#include <stdint.h>

#include "utf8_dfa64.h"
#include "utf8_transcode_endian.h"
#include "utf8_transcode_endian_unsafe.h"

#include "test.h"

static void test_empty(void) {
  uint8_t dst[16];
  utf8_transcode_result_t r = utf8_transcode_utf32le_unsafe("", 0, dst, 16);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "empty: status OK");
  CHECK(r.consumed == 0,                 "empty: consumed 0");
  CHECK(r.decoded  == 0,                 "empty: decoded 0");
  CHECK(r.written  == 0,                 "empty: written 0");
}

static void test_ascii_short_le(void) {
  uint8_t dst[32];
  utf8_transcode_result_t r = utf8_transcode_utf32le_unsafe("ABC", 3, dst, 32);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "ascii short le: status OK");
  CHECK(r.written  == 12,                "ascii short le: written 12 bytes");
  uint8_t want[12] = {0x41, 0, 0, 0,  0x42, 0, 0, 0,  0x43, 0, 0, 0};
  CHECK(memcmp(dst, want, 12) == 0,      "ascii short le: byte layout");
}

static void test_ascii_short_be(void) {
  uint8_t dst[32];
  utf8_transcode_result_t r = utf8_transcode_utf32be_unsafe("ABC", 3, dst, 32);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "ascii short be: status OK");
  CHECK(r.written  == 12,                "ascii short be: written 12 bytes");
  uint8_t want[12] = {0, 0, 0, 0x41,  0, 0, 0, 0x42,  0, 0, 0, 0x43};
  CHECK(memcmp(dst, want, 12) == 0,      "ascii short be: byte layout");
}

static void test_ascii_one_stride(void) {
  // exactly 8 ASCII -> hits fast path once
  uint8_t dst[64];
  utf8_transcode_result_t r = utf8_transcode_utf32le_unsafe("ABCDEFGH", 8, dst, 64);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "ascii stride: status OK");
  CHECK(r.consumed == 8,                 "ascii stride: consumed 8");
  CHECK(r.written  == 32,                "ascii stride: written 32 bytes");
  CHECK(dst[0] == 'A' && dst[28] == 'H', "ascii stride: first/last");
}

static void test_ascii_two_strides(void) {
  uint8_t dst[80];
  utf8_transcode_result_t r = utf8_transcode_utf32le_unsafe("ABCDEFGHIJKLMNOP", 16, dst, 80);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "ascii 2 strides: status OK");
  CHECK(r.written  == 64,                "ascii 2 strides: written 64 bytes");
  CHECK(dst[0] == 'A' && dst[60] == 'P', "ascii 2 strides: first/last");
}

static void test_ascii_stride_plus_tail(void) {
  // 8 fast-path + 3 scalar tail
  uint8_t dst[64];
  utf8_transcode_result_t r = utf8_transcode_utf32le_unsafe("ABCDEFGHIJK", 11, dst, 64);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "stride+tail: status OK");
  CHECK(r.consumed == 11,                "stride+tail: consumed 11");
  CHECK(r.written  == 44,                "stride+tail: written 44 bytes");
  CHECK(dst[40] == 'K',                  "stride+tail: dst byte 40");
}

static void test_2byte(void) {
  // "éé" = 4 bytes, 2 codepoints -> LE E9 00 00 00 E9 00 00 00
  const char *src = "\xC3\xA9\xC3\xA9";
  uint8_t dst[16];
  utf8_transcode_result_t r = utf8_transcode_utf32le_unsafe(src, 4, dst, 16);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "2byte: status OK");
  CHECK(r.decoded  == 2,                 "2byte: decoded 2");
  uint8_t want[8] = {0xE9, 0, 0, 0,  0xE9, 0, 0, 0};
  CHECK(memcmp(dst, want, 8) == 0,       "2byte: byte layout");
}

static void test_4byte(void) {
  // U+10348 GOTHIC LETTER HWAIR -> LE 48 03 01 00
  const char *src = "\xF0\x90\x8D\x88";
  uint8_t dst[16];
  utf8_transcode_result_t r = utf8_transcode_utf32le_unsafe(src, 4, dst, 16);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "4byte: status OK");
  CHECK(r.decoded  == 1,                 "4byte: decoded 1");
  uint8_t want[4] = {0x48, 0x03, 0x01, 0x00};
  CHECK(memcmp(dst, want, 4) == 0,       "4byte: byte layout");
}

static void test_mixed(void) {
  // "Aé€𐍈" = 1+2+3+4 = 10 bytes, 4 codepoints
  const char *src = "A\xC3\xA9\xE2\x82\xAC\xF0\x90\x8D\x88";
  uint8_t dst[32];
  utf8_transcode_result_t r = utf8_transcode_utf32le_unsafe(src, 10, dst, 32);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "mixed: status OK");
  CHECK(r.decoded  == 4,                 "mixed: decoded 4");
  uint8_t want[16] = {0x41, 0, 0, 0,  0xE9, 0, 0, 0,  0xAC, 0x20, 0, 0,  0x48, 0x03, 0x01, 0};
  CHECK(memcmp(dst, want, 16) == 0,      "mixed: byte layout");
}

static void test_ascii_then_multibyte(void) {
  const char *src = "ABCDEFGH\xC3\xA9";
  uint8_t dst[64];
  utf8_transcode_result_t r = utf8_transcode_utf32le_unsafe(src, 10, dst, 64);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "ascii then multi: status OK");
  CHECK(r.decoded  == 9,                 "ascii then multi: decoded 9");
  uint8_t want_tail[4] = {0xE9, 0, 0, 0};
  CHECK(memcmp(dst + 32, want_tail, 4) == 0, "ascii then multi: tail byte layout");
}

static void test_multibyte_then_ascii(void) {
  const char *src = "\xC3\xA9" "ABCDEFGH";
  uint8_t dst[64];
  utf8_transcode_result_t r = utf8_transcode_utf32le_unsafe(src, 10, dst, 64);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "multi then ascii: status OK");
  CHECK(r.decoded  == 9,                 "multi then ascii: decoded 9");
  uint8_t want_head[8] = {0xE9, 0, 0, 0,  0x41, 0, 0, 0};
  CHECK(memcmp(dst, want_head, 8) == 0,  "multi then ascii: head byte layout");
}

static void test_batch_2byte(void) {
  // 4x é = 8 bytes, 4 codepoints -- exercises do/while batching of 2-byte class
  const char *src = "\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9";
  uint8_t dst[32];
  utf8_transcode_result_t r = utf8_transcode_utf32le_unsafe(src, 8, dst, 32);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "batch 2byte: status OK");
  CHECK(r.decoded  == 4,                 "batch 2byte: decoded 4");
  for (int i = 0; i < 4; i++) {
    uint8_t want[4] = {0xE9, 0, 0, 0};
    CHECK(memcmp(dst + i * 4, want, 4) == 0, "batch 2byte: unit byte layout");
  }
}

static void test_batch_3byte(void) {
  // 4x € = 12 bytes, 4 codepoints -- exercises do/while batching of 3-byte class
  const char *src = "\xE2\x82\xAC\xE2\x82\xAC\xE2\x82\xAC\xE2\x82\xAC";
  uint8_t dst[32];
  utf8_transcode_result_t r = utf8_transcode_utf32le_unsafe(src, 12, dst, 32);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "batch 3byte: status OK");
  CHECK(r.decoded  == 4,                 "batch 3byte: decoded 4");
  for (int i = 0; i < 4; i++) {
    uint8_t want[4] = {0xAC, 0x20, 0, 0};
    CHECK(memcmp(dst + i * 4, want, 4) == 0, "batch 3byte: unit byte layout");
  }
}

static void test_batch_4byte(void) {
  // 2x 𐍈 = 8 bytes, 2 codepoints -- exercises do/while batching of 4-byte class
  const char *src = "\xF0\x90\x8D\x88\xF0\x90\x8D\x88";
  uint8_t dst[16];
  utf8_transcode_result_t r = utf8_transcode_utf32le_unsafe(src, 8, dst, 16);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "batch 4byte: status OK");
  CHECK(r.decoded  == 2,                 "batch 4byte: decoded 2");
  uint8_t want[8] = {0x48, 0x03, 0x01, 0,  0x48, 0x03, 0x01, 0};
  CHECK(memcmp(dst, want, 8) == 0,       "batch 4byte: byte layout");
}

static void test_exhausted(void) {
  uint8_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf32le_unsafe("ABCDEFGH", 8, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_EXHAUSTED, "exhausted: status");
  CHECK(r.consumed == 2,                        "exhausted: consumed 2");
  CHECK(r.written  == 8,                        "exhausted: written 8 bytes");
}

static void test_exhausted_resume(void) {
  const char *src = "ABCD";
  size_t len = 4;
  uint8_t dst[8];
  utf8_transcode_result_t r;

  r = utf8_transcode_utf32le_unsafe(src, len, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_EXHAUSTED, "resume: first status");
  CHECK(r.consumed == 2,                        "resume: first consumed");
  src += r.consumed;
  len -= r.consumed;

  r = utf8_transcode_utf32le_unsafe(src, len, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "resume: second status");
  CHECK(dst[0] == 'C' && dst[4] == 'D',  "resume: second dst layout");
}

static void test_dst_exact(void) {
  uint8_t dst[12];
  utf8_transcode_result_t r = utf8_transcode_utf32le_unsafe("ABC", 3, dst, 12);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "dst exact: status OK");
  CHECK(r.written  == 12,                "dst exact: written 12 bytes");
}

static void test_dst_len_not_multiple_of_4(void) {
  uint8_t dst[7];
  memset(dst, 0xAA, sizeof dst);
  utf8_transcode_result_t r = utf8_transcode_utf32le_unsafe("AB", 2, dst, 7);
  CHECK(r.status   == UTF8_TRANSCODE_EXHAUSTED, "dst_len=7: status");
  CHECK(r.written  == 4,                        "dst_len=7: written 4 (not 7)");
  CHECK(dst[4] == 0xAA,                         "dst_len=7: trailing bytes untouched");
}

static void test_agrees_with_safe(void) {
  static const struct { const char *s; size_t len; } cases[] = {
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
    uint8_t d_safe_le[256], d_unsafe_le[256], d_safe_be[256], d_unsafe_be[256];
    utf8_transcode_result_t rsl = utf8_transcode_utf32le(cases[i].s, cases[i].len, d_safe_le, 256);
    utf8_transcode_result_t rul = utf8_transcode_utf32le_unsafe(cases[i].s, cases[i].len, d_unsafe_le, 256);
    utf8_transcode_result_t rsb = utf8_transcode_utf32be(cases[i].s, cases[i].len, d_safe_be, 256);
    utf8_transcode_result_t rub = utf8_transcode_utf32be_unsafe(cases[i].s, cases[i].len, d_unsafe_be, 256);

    CHECK(rul.status   == rsl.status,   "agrees le: status");
    CHECK(rul.written  == rsl.written,  "agrees le: written");
    CHECK(memcmp(d_safe_le, d_unsafe_le, rsl.written) == 0, "agrees le: dst contents");

    CHECK(rub.status   == rsb.status,   "agrees be: status");
    CHECK(rub.written  == rsb.written,  "agrees be: written");
    CHECK(memcmp(d_safe_be, d_unsafe_be, rsb.written) == 0, "agrees be: dst contents");
  }
}

int main(void) {
  SUITE(__FILE__);
  RUN(test_empty);
  RUN(test_ascii_short_le);
  RUN(test_ascii_short_be);
  RUN(test_ascii_one_stride);
  RUN(test_ascii_two_strides);
  RUN(test_ascii_stride_plus_tail);
  RUN(test_2byte);
  RUN(test_4byte);
  RUN(test_mixed);
  RUN(test_ascii_then_multibyte);
  RUN(test_multibyte_then_ascii);
  RUN(test_batch_2byte);
  RUN(test_batch_3byte);
  RUN(test_batch_4byte);
  RUN(test_exhausted);
  RUN(test_exhausted_resume);
  RUN(test_dst_exact);
  RUN(test_dst_len_not_multiple_of_4);
  RUN(test_agrees_with_safe);
  return report_results();
}
