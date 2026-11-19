#include <string.h>
#include <stdint.h>

#include "utf8_dfa64.h"
#include "utf8_transcode_endian.h"
#include "utf8_transcode_endian_unsafe.h"

#include "test.h"

/*
 * utf8_transcode_endian_unsafe.h itself needs no DFA header -- it's
 * self-contained, matching this project's convention that an _unsafe.h
 * header never depends on its validating sibling (see
 * utf8_transcode_unsafe.h). utf8_dfa64.h and utf8_transcode_endian.h are
 * pulled in only so test_agrees_with_safe below has something to compare
 * against -- unlike transcode_utf16_endian.c's native comparison, this one
 * checks raw bytes directly, since both sides already write the same
 * explicit byte order and neither depends on host endianness.
 */

static void test_empty(void) {
  uint8_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf16le_unsafe("", 0, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "empty: status OK");
  CHECK(r.consumed == 0,                 "empty: consumed 0");
  CHECK(r.decoded  == 0,                 "empty: decoded 0");
  CHECK(r.written  == 0,                 "empty: written 0");
}

static void test_ascii_short_le(void) {
  uint8_t dst[16];
  utf8_transcode_result_t r = utf8_transcode_utf16le_unsafe("ABC", 3, dst, 16);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "ascii short le: status OK");
  CHECK(r.written  == 6,                 "ascii short le: written 6 bytes");
  uint8_t want[6] = {0x41, 0x00, 0x42, 0x00, 0x43, 0x00};
  CHECK(memcmp(dst, want, 6) == 0,       "ascii short le: byte layout");
}

static void test_ascii_short_be(void) {
  uint8_t dst[16];
  utf8_transcode_result_t r = utf8_transcode_utf16be_unsafe("ABC", 3, dst, 16);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "ascii short be: status OK");
  CHECK(r.written  == 6,                 "ascii short be: written 6 bytes");
  uint8_t want[6] = {0x00, 0x41, 0x00, 0x42, 0x00, 0x43};
  CHECK(memcmp(dst, want, 6) == 0,       "ascii short be: byte layout");
}

static void test_ascii_one_stride(void) {
  // Exactly 8 bytes: one full pass of the ASCII fast path, no scalar tail
  uint8_t dst[32];
  utf8_transcode_result_t r = utf8_transcode_utf16le_unsafe("ABCDEFGH", 8, dst, 32);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "ascii stride: status OK");
  CHECK(r.consumed == 8,                 "ascii stride: consumed 8");
  CHECK(r.written  == 16,                "ascii stride: written 16 bytes");
  CHECK(dst[0] == 'A' && dst[14] == 'H', "ascii stride: first/last byte");
}

static void test_ascii_two_strides(void) {
  uint8_t dst[64];
  utf8_transcode_result_t r = utf8_transcode_utf16le_unsafe("ABCDEFGHIJKLMNOP", 16, dst, 64);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "ascii 2 strides: status OK");
  CHECK(r.written  == 32,                "ascii 2 strides: written 32 bytes");
  CHECK(dst[0] == 'A' && dst[30] == 'P', "ascii 2 strides: first/last byte");
}

static void test_bmp(void) {
  const char *src = "\xC3\xA9\xE2\x82\xAC"; // U+00E9 U+20AC
  uint8_t dst[16];
  utf8_transcode_result_t r = utf8_transcode_utf16le_unsafe(src, 5, dst, 16);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "bmp: status OK");
  CHECK(r.written  == 4,                 "bmp: written 4 bytes");
  uint8_t want[4] = {0xE9, 0x00, 0xAC, 0x20};
  CHECK(memcmp(dst, want, 4) == 0,       "bmp: byte layout");
}

static void test_surrogate_pair_le(void) {
  const char *src = "\xF0\x90\x8D\x88"; // U+10348 -> D800 DF48
  uint8_t dst[16];
  utf8_transcode_result_t r = utf8_transcode_utf16le_unsafe(src, 4, dst, 16);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "surrogate le: status OK");
  CHECK(r.decoded  == 1,                 "surrogate le: decoded 1");
  CHECK(r.written  == 4,                 "surrogate le: written 4 bytes");
  uint8_t want[4] = {0x00, 0xD8, 0x48, 0xDF};
  CHECK(memcmp(dst, want, 4) == 0,       "surrogate le: byte layout");
}

static void test_surrogate_pair_be(void) {
  const char *src = "\xF0\x90\x8D\x88";
  uint8_t dst[16];
  utf8_transcode_result_t r = utf8_transcode_utf16be_unsafe(src, 4, dst, 16);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "surrogate be: status OK");
  uint8_t want[4] = {0xD8, 0x00, 0xDF, 0x48};
  CHECK(memcmp(dst, want, 4) == 0,       "surrogate be: byte layout");
}

static void test_ascii_then_multibyte(void) {
  // 8 ASCII (fast path) + é (2-byte) = 10 bytes, 9 codepoints
  const char *src = "ABCDEFGH\xC3\xA9";
  uint8_t dst[32];
  utf8_transcode_result_t r = utf8_transcode_utf16le_unsafe(src, 10, dst, 32);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "ascii then multi: status OK");
  CHECK(r.consumed == 10,                "ascii then multi: consumed 10");
  CHECK(r.decoded  == 9,                 "ascii then multi: decoded 9");
  CHECK(r.written  == 18,                "ascii then multi: written 18 bytes");
  uint8_t want_tail[2] = {0xE9, 0x00};
  CHECK(memcmp(dst + 16, want_tail, 2) == 0, "ascii then multi: tail byte layout");
}

static void test_multibyte_then_ascii(void) {
  // é (2-byte) + 8 ASCII = 10 bytes, 9 codepoints
  const char *src = "\xC3\xA9" "ABCDEFGH";
  uint8_t dst[32];
  utf8_transcode_result_t r = utf8_transcode_utf16le_unsafe(src, 10, dst, 32);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "multi then ascii: status OK");
  CHECK(r.written  == 18,                "multi then ascii: written 18 bytes");
  uint8_t want_head[4] = {0xE9, 0x00, 0x41, 0x00};
  CHECK(memcmp(dst, want_head, 4) == 0,  "multi then ascii: head byte layout");
}

static void test_batch_4byte(void) {
  // 2x 𐍈 = 8 bytes, 2 codepoints, 4 units (2 surrogate pairs) -- exercises
  // the same-length-class batching path for 4-byte sequences
  const char *src = "\xF0\x90\x8D\x88\xF0\x90\x8D\x88";
  uint8_t dst[32];
  utf8_transcode_result_t r = utf8_transcode_utf16le_unsafe(src, 8, dst, 32);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "batch 4byte: status OK");
  CHECK(r.decoded  == 2,                 "batch 4byte: decoded 2");
  CHECK(r.written  == 8,                 "batch 4byte: written 8 bytes");
  uint8_t want[8] = {0x00, 0xD8, 0x48, 0xDF, 0x00, 0xD8, 0x48, 0xDF};
  CHECK(memcmp(dst, want, 8) == 0,       "batch 4byte: byte layout");
}

static void test_exhausted(void) {
  uint8_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf16le_unsafe("ABCDEFGH", 8, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_EXHAUSTED, "exhausted: status");
  CHECK(r.consumed == 2,                        "exhausted: consumed 2");
  CHECK(r.written  == 4,                        "exhausted: written 4 bytes");
}

static void test_exhausted_surrogate(void) {
  const char *src = "A\xF0\x90\x8D\x88";
  uint8_t dst[2];
  utf8_transcode_result_t r = utf8_transcode_utf16le_unsafe(src, 5, dst, 2);
  CHECK(r.status   == UTF8_TRANSCODE_EXHAUSTED, "exhausted surrogate: status");
  CHECK(r.consumed == 1,                        "exhausted surrogate: consumed 1");
  CHECK(r.written  == 2,                        "exhausted surrogate: written 2 bytes");
}

static void test_dst_len_not_multiple_of_2(void) {
  uint8_t dst[5];
  memset(dst, 0xAA, sizeof dst);
  utf8_transcode_result_t r = utf8_transcode_utf16le_unsafe("ABC", 3, dst, 5);
  CHECK(r.status   == UTF8_TRANSCODE_EXHAUSTED, "odd dst_len: status");
  CHECK(r.written  == 4,                        "odd dst_len: written 4 (not 5)");
  CHECK(dst[4] == 0xAA,                         "odd dst_len: trailing byte untouched");
}

static void test_exhausted_resume(void) {
  const char *src = "ABCD";
  size_t len = 4;
  uint8_t dst[4];
  utf8_transcode_result_t r;

  r = utf8_transcode_utf16le_unsafe(src, len, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_EXHAUSTED, "resume: first status");
  CHECK(r.consumed == 2,                        "resume: first consumed");
  src += r.consumed;
  len -= r.consumed;

  r = utf8_transcode_utf16le_unsafe(src, len, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "resume: second status");
  CHECK(dst[0] == 'C' && dst[2] == 'D',  "resume: second dst layout");
}

static void test_decoded_vs_written(void) {
  // 2x 𐍈 = 2 decoded, 4 units = 8 bytes written
  const char *src = "\xF0\x90\x8D\x88\xF0\x90\x8D\x88";
  uint8_t dst[32];
  utf8_transcode_result_t r = utf8_transcode_utf16le_unsafe(src, 8, dst, 32);
  CHECK(r.decoded == 2,                  "decoded vs written: decoded 2");
  CHECK(r.written == 8,                  "decoded vs written: written 8 bytes");
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
    {"A\xF0\x90\x8D\x88" "B",                               6},
    {"ABCDEFGH\xE2\x82\xAC" "ABCDEFGH" "\xC3\xA9" "AB",    22},
  };
  size_t n = sizeof(cases) / sizeof(cases[0]);
  for (size_t i = 0; i < n; i++) {
    uint8_t d_safe_le[128], d_unsafe_le[128], d_safe_be[128], d_unsafe_be[128];
    utf8_transcode_result_t rsl = utf8_transcode_utf16le(cases[i].s, cases[i].len, d_safe_le, 128);
    utf8_transcode_result_t rul = utf8_transcode_utf16le_unsafe(cases[i].s, cases[i].len, d_unsafe_le, 128);
    utf8_transcode_result_t rsb = utf8_transcode_utf16be(cases[i].s, cases[i].len, d_safe_be, 128);
    utf8_transcode_result_t rub = utf8_transcode_utf16be_unsafe(cases[i].s, cases[i].len, d_unsafe_be, 128);

    CHECK(rul.status   == rsl.status,   "agrees le: status");
    CHECK(rul.consumed == rsl.consumed, "agrees le: consumed");
    CHECK(rul.decoded  == rsl.decoded,  "agrees le: decoded");
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
  RUN(test_bmp);
  RUN(test_surrogate_pair_le);
  RUN(test_surrogate_pair_be);
  RUN(test_ascii_then_multibyte);
  RUN(test_multibyte_then_ascii);
  RUN(test_batch_4byte);
  RUN(test_exhausted);
  RUN(test_exhausted_surrogate);
  RUN(test_dst_len_not_multiple_of_2);
  RUN(test_exhausted_resume);
  RUN(test_decoded_vs_written);
  RUN(test_agrees_with_safe);
  return report_results();
}
