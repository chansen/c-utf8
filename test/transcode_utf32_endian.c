#include <string.h>
#include <stdint.h>

#include "utf8_dfa64.h"
#include "utf8_transcode.h"
#include "utf8_transcode_endian.h"

#include "test.h"

/*
 * See transcode_utf16_endian.c for the general approach: byte-layout
 * checks use literal constants (host-endianness-independent), and
 * byte-order-independent logic (illformed/truncated/advance/resume) is
 * exercised once via LE rather than doubled for BE.
 */

static void test_decode_empty(void) {
  uint8_t dst[16];
  utf8_transcode_result_t r = utf8_transcode_utf32le("", 0, dst, 16);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "decode empty: status OK");
  CHECK(r.consumed == 0,                 "decode empty: consumed 0");
  CHECK(r.decoded  == 0,                 "decode empty: decoded 0");
  CHECK(r.written  == 0,                 "decode empty: written 0");
  CHECK(r.advance  == 0,                 "decode empty: advance 0");
}

static void test_decode_ascii_le(void) {
  // 'A' = U+0041 -> LE bytes 41 00 00 00
  uint8_t dst[16];
  utf8_transcode_result_t r = utf8_transcode_utf32le("A", 1, dst, 16);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "decode ascii le: status OK");
  CHECK(r.written  == 4,                 "decode ascii le: written 4 bytes");
  uint8_t want[4] = {0x41, 0x00, 0x00, 0x00};
  CHECK(memcmp(dst, want, 4) == 0,       "decode ascii le: byte layout");
}

static void test_decode_ascii_be(void) {
  // 'A' = U+0041 -> BE bytes 00 00 00 41
  uint8_t dst[16];
  utf8_transcode_result_t r = utf8_transcode_utf32be("A", 1, dst, 16);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "decode ascii be: status OK");
  CHECK(r.written  == 4,                 "decode ascii be: written 4 bytes");
  uint8_t want[4] = {0x00, 0x00, 0x00, 0x41};
  CHECK(memcmp(dst, want, 4) == 0,       "decode ascii be: byte layout");
}

static void test_decode_multibyte_le(void) {
  // "é€𐍈" = U+00E9 U+20AC U+10348
  const char *src = "\xC3\xA9\xE2\x82\xAC\xF0\x90\x8D\x88";
  uint8_t dst[16];
  utf8_transcode_result_t r = utf8_transcode_utf32le(src, 9, dst, 16);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "decode multibyte le: status OK");
  CHECK(r.consumed == 9,                 "decode multibyte le: consumed 9");
  CHECK(r.decoded  == 3,                 "decode multibyte le: decoded 3");
  CHECK(r.written  == 12,                "decode multibyte le: written 12 bytes");
  uint8_t want[12] = {0xE9, 0x00, 0x00, 0x00,
                       0xAC, 0x20, 0x00, 0x00,
                       0x48, 0x03, 0x01, 0x00};
  CHECK(memcmp(dst, want, 12) == 0,      "decode multibyte le: byte layout");
}

static void test_decode_multibyte_be(void) {
  // "é€𐍈" = U+00E9 U+20AC U+10348
  const char *src = "\xC3\xA9\xE2\x82\xAC\xF0\x90\x8D\x88";
  uint8_t dst[16];
  utf8_transcode_result_t r = utf8_transcode_utf32be(src, 9, dst, 16);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "decode multibyte be: status OK");
  CHECK(r.written  == 12,                "decode multibyte be: written 12 bytes");
  uint8_t want[12] = {0x00, 0x00, 0x00, 0xE9,
                       0x00, 0x00, 0x20, 0xAC,
                       0x00, 0x01, 0x03, 0x48};
  CHECK(memcmp(dst, want, 12) == 0,      "decode multibyte be: byte layout");
}

static void test_decode_exhausted(void) {
  uint8_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf32le("ABC", 3, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_EXHAUSTED, "decode exhausted: status");
  CHECK(r.consumed == 2,                        "decode exhausted: consumed 2");
  CHECK(r.decoded  == 2,                        "decode exhausted: decoded 2");
  CHECK(r.written  == 8,                        "decode exhausted: written 8 bytes");
}

static void test_dst_len_not_multiple_of_4(void) {
  // dst_len values that aren't multiples of 4: only whole units are ever
  // written, trailing bytes are left untouched and not counted.
  uint8_t dst[7];
  memset(dst, 0xAA, sizeof dst);
  utf8_transcode_result_t r = utf8_transcode_utf32le("AB", 2, dst, 7);
  CHECK(r.status   == UTF8_TRANSCODE_EXHAUSTED, "dst_len=7: status");
  CHECK(r.consumed == 1,                        "dst_len=7: consumed 1 (only 1 unit fits)");
  CHECK(r.written  == 4,                        "dst_len=7: written 4 (not 7)");
  CHECK(dst[4] == 0xAA,                         "dst_len=7: trailing bytes untouched");

  uint8_t tiny[3] = {0xAA, 0xAA, 0xAA};
  utf8_transcode_result_t r1 = utf8_transcode_utf32le("A", 1, tiny, 3);
  CHECK(r1.status  == UTF8_TRANSCODE_EXHAUSTED, "dst_len=3: status");
  CHECK(r1.written == 0,                        "dst_len=3: written 0");
  CHECK(tiny[0] == 0xAA,                        "dst_len=3: untouched");
}

static void test_decode_exhausted_resume(void) {
  const char *src = "ABCD";
  size_t len = 4;
  uint8_t dst[8];
  utf8_transcode_result_t r;

  r = utf8_transcode_utf32le(src, len, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_EXHAUSTED, "decode resume: first status");
  CHECK(r.consumed == 2,                        "decode resume: first consumed");
  CHECK(r.written  == 8,                        "decode resume: first written");
  src += r.consumed;
  len -= r.consumed;

  r = utf8_transcode_utf32le(src, len, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "decode resume: second status");
  CHECK(r.consumed == 2,                 "decode resume: second consumed");
  CHECK(dst[0] == 'C' && dst[4] == 'D',  "decode resume: second dst layout");
}

static void test_decode_illformed(void) {
  uint8_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf32le("A\x80" "B", 3, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_ILLFORMED, "decode illformed: status");
  CHECK(r.consumed == 1,                        "decode illformed: consumed 1 (A)");
  CHECK(r.advance  == 1,                        "decode illformed: advance 1");
  CHECK(dst[0] == 'A',                          "decode illformed: dst[0]");
}

static void test_decode_illformed_resume(void) {
  const char *src = "A\x80" "B";
  size_t len = 3;
  uint8_t dst[8];
  utf8_transcode_result_t r;

  r = utf8_transcode_utf32le(src, len, dst, 8);
  CHECK(r.status == UTF8_TRANSCODE_ILLFORMED, "illformed resume: first call");
  src += r.consumed + r.advance;
  len -= r.consumed + r.advance;

  r = utf8_transcode_utf32le(src, len, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "illformed resume: second call");
  CHECK(dst[0]     == 'B',               "illformed resume: dst[0]");
}

static void test_decode_illformed_lead_then_bad_cont(void) {
  uint8_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf32le("\xE2\x41", 2, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_ILLFORMED, "illformed lead+bad: status");
  CHECK(r.advance  == 1,                        "illformed lead+bad: advance 1");

  r = utf8_transcode_utf32le("\xF0\x9F\x41", 3, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_ILLFORMED, "illformed 2valid+bad: status");
  CHECK(r.advance  == 2,                        "illformed 2valid+bad: advance 2");
}

static void test_decode_truncated(void) {
  uint8_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf32le("\xE2\x82", 2, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_TRUNCATED, "decode truncated: status");
  CHECK(r.written  == 0,                        "decode truncated: written 0");
  CHECK(r.advance  == 2,                        "decode truncated: advance 2");
}

static void test_decode_truncated_after_valid(void) {
  uint8_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf32le("A\xC3", 2, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_TRUNCATED, "truncated after valid: status");
  CHECK(r.written  == 4,                        "truncated after valid: written 4 bytes");
  CHECK(r.advance  == 1,                        "truncated after valid: advance 1");
}

static void test_replace_clean(void) {
  uint8_t dst[16];
  utf8_transcode_result_t r = utf8_transcode_utf32le_replace("ABC", 3, dst, 16);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace clean: status OK");
  CHECK(r.written  == 12,                "replace clean: written 12 bytes");
}

static void test_replace_bare_cont_le(void) {
  // \x80 -> U+FFFD -> LE bytes FD FF 00 00
  uint8_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf32le_replace("\x80", 1, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace bare cont le: status OK");
  uint8_t want[4] = {0xFD, 0xFF, 0x00, 0x00};
  CHECK(memcmp(dst, want, 4) == 0,       "replace bare cont le: byte layout");
}

static void test_replace_bare_cont_be(void) {
  // \x80 -> U+FFFD -> BE bytes 00 00 FF FD
  uint8_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf32be_replace("\x80", 1, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace bare cont be: status OK");
  uint8_t want[4] = {0x00, 0x00, 0xFF, 0xFD};
  CHECK(memcmp(dst, want, 4) == 0,       "replace bare cont be: byte layout");
}

static void test_replace_mixed(void) {
  uint8_t dst[16];
  utf8_transcode_result_t r = utf8_transcode_utf32le_replace("A\x80" "B", 3, dst, 16);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace mixed: status OK");
  CHECK(r.decoded  == 3,                 "replace mixed: decoded 3");
  uint8_t want[12] = {0x41, 0x00, 0x00, 0x00,
                       0xFD, 0xFF, 0x00, 0x00,
                       0x42, 0x00, 0x00, 0x00};
  CHECK(memcmp(dst, want, 12) == 0,      "replace mixed: byte layout");
}

static void test_replace_truncated(void) {
  uint8_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf32le_replace("\xE2\x82", 2, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace truncated: status OK");
  CHECK(r.decoded  == 1,                 "replace truncated: decoded 1");
  uint8_t want[4] = {0xFD, 0xFF, 0x00, 0x00};
  CHECK(memcmp(dst, want, 4) == 0,       "replace truncated: byte layout");
}

static void test_replace_exhausted(void) {
  uint8_t dst[16];
  utf8_transcode_result_t r = utf8_transcode_utf32le_replace("ABCD", 4, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_EXHAUSTED, "replace exhausted: status");
  CHECK(r.written  == 8,                        "replace exhausted: written 8 bytes");

  r = utf8_transcode_utf32le_replace("ABCD", 4, dst, 16);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace exact: status OK");
  CHECK(r.written  == 16,                "replace exact: written 16 bytes");
}

static void test_replace_surrogate(void) {
  // ED A0 80 (U+D800 encoded as surrogate): maximal subparts 1+1+1 ->
  // three U+FFFD
  uint8_t dst[16];
  utf8_transcode_result_t r = utf8_transcode_utf32le_replace("\xED\xA0\x80", 3, dst, 16);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace surrogate: status OK");
  CHECK(r.decoded  == 3,                 "replace surrogate: decoded 3");
  uint8_t want[12] = {0xFD, 0xFF, 0x00, 0x00,
                       0xFD, 0xFF, 0x00, 0x00,
                       0xFD, 0xFF, 0x00, 0x00};
  CHECK(memcmp(dst, want, 12) == 0,      "replace surrogate: byte layout");
}

static void test_agrees_with_native(void) {
  // See transcode_utf16_endian.c: compares countable fields only, never
  // raw bytes, since raw-byte comparison against native would assume this
  // build's host endianness.
  static const struct { const char *s; size_t len; } cases[] = {
    {"",                                       0},
    {"ABC",                                    3},
    {"\xC3\xA9\xE2\x82\xAC\xF0\x90\x8D\x88",   9},
    {"A\x80" "B",                              3},
  };
  size_t n = sizeof(cases) / sizeof(cases[0]);
  for (size_t i = 0; i < n; i++) {
    uint32_t d_native[64];
    uint8_t d_le[256];
    utf8_transcode_result_t rn = utf8_transcode_utf32(cases[i].s, cases[i].len, d_native, 64);
    utf8_transcode_result_t rl = utf8_transcode_utf32le(cases[i].s, cases[i].len, d_le, 256);
    CHECK(rl.status   == rn.status,       "agrees: status");
    CHECK(rl.consumed == rn.consumed,     "agrees: consumed");
    CHECK(rl.decoded  == rn.decoded,      "agrees: decoded");
    CHECK(rl.written  == rn.written * 4,  "agrees: written bytes = units*4");
  }
}

int main(void) {
  SUITE(__FILE__);
  RUN(test_decode_empty);
  RUN(test_decode_ascii_le);
  RUN(test_decode_ascii_be);
  RUN(test_decode_multibyte_le);
  RUN(test_decode_multibyte_be);
  RUN(test_decode_exhausted);
  RUN(test_dst_len_not_multiple_of_4);
  RUN(test_decode_exhausted_resume);
  RUN(test_decode_illformed);
  RUN(test_decode_illformed_resume);
  RUN(test_decode_illformed_lead_then_bad_cont);
  RUN(test_decode_truncated);
  RUN(test_decode_truncated_after_valid);
  RUN(test_replace_clean);
  RUN(test_replace_bare_cont_le);
  RUN(test_replace_bare_cont_be);
  RUN(test_replace_mixed);
  RUN(test_replace_truncated);
  RUN(test_replace_exhausted);
  RUN(test_replace_surrogate);
  RUN(test_agrees_with_native);
  return report_results();
}
