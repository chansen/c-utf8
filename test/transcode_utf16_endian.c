#include <string.h>
#include <stdint.h>

#include "utf8_dfa64.h"
#include "utf8_transcode.h"
#include "utf8_transcode_endian.h"

#include "test.h"

/*
 * Byte-layout expectations below are written as literal constants rather
 * than derived from utf8_transcode_utf16's native output, so these checks
 * hold regardless of the host's own endianness (a native-output comparison
 * would silently assume a little-endian build host). Logic that doesn't
 * depend on byte order -- illformed detection, truncation, advance,
 * resume -- is exercised once via the LE entry point rather than doubled
 * for BE, since both are generated from the same macro body and differ
 * only in which store function they call; see utf8_transcode_endian.h.
 */

static void test_empty(void) {
  uint8_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf16le("", 0, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "empty: status OK");
  CHECK(r.consumed == 0,                 "empty: consumed 0");
  CHECK(r.decoded  == 0,                 "empty: decoded 0");
  CHECK(r.written  == 0,                 "empty: written 0");
  CHECK(r.advance  == 0,                 "empty: advance 0");
}

static void test_ascii_le(void) {
  // 'A' = U+0041 -> LE bytes 41 00
  uint8_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf16le("A", 1, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "ascii le: status OK");
  CHECK(r.consumed == 1,                 "ascii le: consumed 1");
  CHECK(r.decoded  == 1,                 "ascii le: decoded 1");
  CHECK(r.written  == 2,                 "ascii le: written 2 bytes");
  CHECK(dst[0] == 0x41,                  "ascii le: low byte");
  CHECK(dst[1] == 0x00,                  "ascii le: high byte");
}

static void test_ascii_be(void) {
  // 'A' = U+0041 -> BE bytes 00 41
  uint8_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf16be("A", 1, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "ascii be: status OK");
  CHECK(r.consumed == 1,                 "ascii be: consumed 1");
  CHECK(r.decoded  == 1,                 "ascii be: decoded 1");
  CHECK(r.written  == 2,                 "ascii be: written 2 bytes");
  CHECK(dst[0] == 0x00,                  "ascii be: high byte");
  CHECK(dst[1] == 0x41,                  "ascii be: low byte");
}

static void test_bmp_le(void) {
  // "é€" = U+00E9 U+20AC -> LE bytes E9 00 AC 20
  const char *src = "\xC3\xA9\xE2\x82\xAC";
  uint8_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf16le(src, 5, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "bmp le: status OK");
  CHECK(r.consumed == 5,                 "bmp le: consumed 5");
  CHECK(r.decoded  == 2,                 "bmp le: decoded 2");
  CHECK(r.written  == 4,                 "bmp le: written 4 bytes");
  uint8_t want[4] = {0xE9, 0x00, 0xAC, 0x20};
  CHECK(memcmp(dst, want, 4) == 0,       "bmp le: byte layout");
}

static void test_bmp_be(void) {
  // "é€" = U+00E9 U+20AC -> BE bytes 00 E9 20 AC
  const char *src = "\xC3\xA9\xE2\x82\xAC";
  uint8_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf16be(src, 5, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "bmp be: status OK");
  CHECK(r.consumed == 5,                 "bmp be: consumed 5");
  CHECK(r.decoded  == 2,                 "bmp be: decoded 2");
  CHECK(r.written  == 4,                 "bmp be: written 4 bytes");
  uint8_t want[4] = {0x00, 0xE9, 0x20, 0xAC};
  CHECK(memcmp(dst, want, 4) == 0,       "bmp be: byte layout");
}

static void test_surrogate_pair_le(void) {
  // U+10348 GOTHIC LETTER HWAIR: F0 90 8D 88 -> units D800 DF48
  // -> LE bytes 00 D8 48 DF
  const char *src = "\xF0\x90\x8D\x88";
  uint8_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf16le(src, 4, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "surrogate le: status OK");
  CHECK(r.consumed == 4,                 "surrogate le: consumed 4");
  CHECK(r.decoded  == 1,                 "surrogate le: decoded 1");
  CHECK(r.written  == 4,                 "surrogate le: written 4 bytes");
  uint8_t want[4] = {0x00, 0xD8, 0x48, 0xDF};
  CHECK(memcmp(dst, want, 4) == 0,       "surrogate le: byte layout");
}

static void test_surrogate_pair_be(void) {
  // U+10348 -> units D800 DF48 -> BE bytes D8 00 DF 48
  const char *src = "\xF0\x90\x8D\x88";
  uint8_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf16be(src, 4, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "surrogate be: status OK");
  CHECK(r.consumed == 4,                 "surrogate be: consumed 4");
  CHECK(r.decoded  == 1,                 "surrogate be: decoded 1");
  CHECK(r.written  == 4,                 "surrogate be: written 4 bytes");
  uint8_t want[4] = {0xD8, 0x00, 0xDF, 0x48};
  CHECK(memcmp(dst, want, 4) == 0,       "surrogate be: byte layout");
}

static void test_mixed_le(void) {
  // "Aé𐍈" = U+0041 U+00E9 U+10348 -> 1+1+2 = 4 units, 8 bytes
  const char *src = "A\xC3\xA9\xF0\x90\x8D\x88";
  uint8_t dst[12];
  utf8_transcode_result_t r = utf8_transcode_utf16le(src, 7, dst, 12);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "mixed le: status OK");
  CHECK(r.consumed == 7,                 "mixed le: consumed 7");
  CHECK(r.decoded  == 3,                 "mixed le: decoded 3");
  CHECK(r.written  == 8,                 "mixed le: written 8 bytes");
  uint8_t want[8] = {0x41, 0x00, 0xE9, 0x00, 0x00, 0xD8, 0x48, 0xDF};
  CHECK(memcmp(dst, want, 8) == 0,       "mixed le: byte layout");
}

static void test_exhausted(void) {
  // dst too small for BMP characters (bytes, not units)
  uint8_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf16le("ABC", 3, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_EXHAUSTED, "exhausted: status");
  CHECK(r.consumed == 2,                        "exhausted: consumed 2");
  CHECK(r.decoded  == 2,                        "exhausted: decoded 2");
  CHECK(r.written  == 4,                        "exhausted: written 4 bytes");
  CHECK(dst[0] == 'A' && dst[2] == 'B',         "exhausted: dst layout");
}

static void test_exhausted_surrogate(void) {
  // dst has 2 bytes (1 unit) left but next codepoint needs a surrogate
  // pair (4 bytes / 2 units)
  const char *src = "A\xF0\x90\x8D\x88";
  uint8_t dst[4];
  utf8_transcode_result_t r = utf8_transcode_utf16le(src, 5, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_EXHAUSTED, "exhausted surrogate: status");
  CHECK(r.consumed == 1,                        "exhausted surrogate: consumed 1");
  CHECK(r.decoded  == 1,                        "exhausted surrogate: decoded 1");
  CHECK(r.written  == 2,                        "exhausted surrogate: written 2 bytes");
  CHECK(dst[0] == 'A',                          "exhausted surrogate: dst[0]");
}

static void test_dst_len_not_multiple_of_2(void) {
  // Odd dst_len: the trailing byte must be left unused, never touched,
  // and never counted -- only whole 2-byte units are ever written.
  uint8_t dst[5];
  memset(dst, 0xAA, sizeof dst);
  utf8_transcode_result_t r = utf8_transcode_utf16le("ABC", 3, dst, 5);
  CHECK(r.status   == UTF8_TRANSCODE_EXHAUSTED, "odd dst_len: status");
  CHECK(r.consumed == 2,                        "odd dst_len: consumed 2");
  CHECK(r.written  == 4,                        "odd dst_len: written 4 (not 5)");
  CHECK(dst[4] == 0xAA,                         "odd dst_len: trailing byte untouched");

  // A single leftover byte (dst_len=1) must report EXHAUSTED with
  // written=0, never a partial unit.
  uint8_t tiny[1] = {0xAA};
  utf8_transcode_result_t r1 = utf8_transcode_utf16le("A", 1, tiny, 1);
  CHECK(r1.status  == UTF8_TRANSCODE_EXHAUSTED, "dst_len=1: status");
  CHECK(r1.written == 0,                        "dst_len=1: written 0");
  CHECK(tiny[0] == 0xAA,                        "dst_len=1: untouched");
}

static void test_exhausted_resume(void) {
  const char *src = "ABCD";
  size_t len = 4;
  uint8_t dst[4];
  utf8_transcode_result_t r;

  r = utf8_transcode_utf16le(src, len, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_EXHAUSTED, "resume: first status");
  CHECK(r.consumed == 2,                        "resume: first consumed");
  CHECK(r.written  == 4,                        "resume: first written");
  src += r.consumed;
  len -= r.consumed;

  r = utf8_transcode_utf16le(src, len, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "resume: second status");
  CHECK(r.consumed == 2,                 "resume: second consumed");
  CHECK(r.written  == 4,                 "resume: second written");
  CHECK(dst[0] == 'C' && dst[2] == 'D',  "resume: second dst layout");
}

static void test_illformed(void) {
  // "A\x80B": bare continuation in the middle
  uint8_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf16le("A\x80" "B", 3, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_ILLFORMED, "illformed: status");
  CHECK(r.consumed == 1,                        "illformed: consumed 1");
  CHECK(r.decoded  == 1,                        "illformed: decoded 1");
  CHECK(r.written  == 2,                        "illformed: written 2 bytes");
  CHECK(r.advance  == 1,                        "illformed: advance 1");
  CHECK(dst[0] == 'A',                          "illformed: dst[0]");
}

static void test_illformed_resume(void) {
  const char *src = "A\x80" "B";
  size_t len = 3;
  uint8_t dst[8];
  utf8_transcode_result_t r;

  r = utf8_transcode_utf16le(src, len, dst, 8);
  CHECK(r.status == UTF8_TRANSCODE_ILLFORMED, "illformed resume: first call");
  src += r.consumed + r.advance;
  len -= r.consumed + r.advance;

  r = utf8_transcode_utf16le(src, len, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "illformed resume: second call");
  CHECK(r.decoded  == 1,                 "illformed resume: decoded 1");
  CHECK(dst[0]     == 'B',               "illformed resume: dst[0]");
}

static void test_illformed_lead_then_bad_cont(void) {
  uint8_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf16le("\xE2\x41", 2, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_ILLFORMED, "illformed lead+bad: status");
  CHECK(r.consumed == 0,                        "illformed lead+bad: consumed 0");
  CHECK(r.advance  == 1,                        "illformed lead+bad: advance 1");

  r = utf8_transcode_utf16le("\xF0\x9F\x41", 3, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_ILLFORMED, "illformed 2valid+bad: status");
  CHECK(r.consumed == 0,                        "illformed 2valid+bad: consumed 0");
  CHECK(r.advance  == 2,                        "illformed 2valid+bad: advance 2");
}

static void test_truncated(void) {
  uint8_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf16le("\xE2\x82", 2, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_TRUNCATED, "truncated: status");
  CHECK(r.consumed == 0,                        "truncated: consumed 0");
  CHECK(r.written  == 0,                        "truncated: written 0");
  CHECK(r.advance  == 2,                        "truncated: advance 2");
}

static void test_truncated_after_valid(void) {
  uint8_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf16le("A\xC3", 2, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_TRUNCATED, "truncated after valid: status");
  CHECK(r.consumed == 1,                        "truncated after valid: consumed 1");
  CHECK(r.written  == 2,                        "truncated after valid: written 2 bytes");
  CHECK(r.advance  == 1,                        "truncated after valid: advance 1");
  CHECK(dst[0]     == 'A',                      "truncated after valid: dst[0]");
}

static void test_replace_clean(void) {
  uint8_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf16le_replace("ABC", 3, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace clean: status OK");
  CHECK(r.written  == 6,                 "replace clean: written 6 bytes");
}

static void test_replace_bare_cont_le(void) {
  // \x80 replaced with U+FFFD -> LE bytes FD FF
  uint8_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf16le_replace("\x80", 1, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace bare cont le: status OK");
  CHECK(r.written  == 2,                 "replace bare cont le: written 2 bytes");
  uint8_t want[2] = {0xFD, 0xFF};
  CHECK(memcmp(dst, want, 2) == 0,       "replace bare cont le: U+FFFD byte layout");
}

static void test_replace_bare_cont_be(void) {
  // \x80 replaced with U+FFFD -> BE bytes FF FD
  uint8_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf16be_replace("\x80", 1, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace bare cont be: status OK");
  CHECK(r.written  == 2,                 "replace bare cont be: written 2 bytes");
  uint8_t want[2] = {0xFF, 0xFD};
  CHECK(memcmp(dst, want, 2) == 0,       "replace bare cont be: U+FFFD byte layout");
}

static void test_replace_mixed(void) {
  uint8_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf16le_replace("A\x80" "B", 3, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace mixed: status OK");
  CHECK(r.decoded  == 3,                 "replace mixed: decoded 3");
  CHECK(r.written  == 6,                 "replace mixed: written 6 bytes");
  uint8_t want[6] = {0x41, 0x00, 0xFD, 0xFF, 0x42, 0x00};
  CHECK(memcmp(dst, want, 6) == 0,       "replace mixed: byte layout");
}

static void test_replace_truncated(void) {
  uint8_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf16le_replace("\xE2\x82", 2, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace truncated: status OK");
  CHECK(r.decoded  == 1,                 "replace truncated: decoded 1");
  uint8_t want[2] = {0xFD, 0xFF};
  CHECK(memcmp(dst, want, 2) == 0,       "replace truncated: U+FFFD byte layout");
}

static void test_replace_exhausted(void) {
  uint8_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf16le_replace("ABCD", 4, dst, 4);
  CHECK(r.status   == UTF8_TRANSCODE_EXHAUSTED, "replace exhausted: status");
  CHECK(r.written  == 4,                        "replace exhausted: written 4 bytes");

  r = utf8_transcode_utf16le_replace("ABCD", 4, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace exact: status OK");
  CHECK(r.written  == 8,                 "replace exact: written 8 bytes");
}

static void test_replace_surrogate_pair(void) {
  const char *src = "\xF0\x90\x8D\x88"; // U+10348
  uint8_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf16le_replace(src, 4, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace surrogate pair: status OK");
  CHECK(r.decoded  == 1,                 "replace surrogate pair: decoded 1");
  uint8_t want[4] = {0x00, 0xD8, 0x48, 0xDF};
  CHECK(memcmp(dst, want, 4) == 0,       "replace surrogate pair: byte layout");
}

static void test_replace_surrogate_encoding(void) {
  // ED A0 80 (U+D800 encoded as a UTF-8 surrogate): maximal subparts are
  // 1+1+1, producing three U+FFFD per the Unicode replacement algorithm
  uint8_t dst[8];
  utf8_transcode_result_t r = utf8_transcode_utf16le_replace("\xED\xA0\x80", 3, dst, 8);
  CHECK(r.status   == UTF8_TRANSCODE_OK, "replace surrogate encoding: status OK");
  CHECK(r.decoded  == 3,                 "replace surrogate encoding: decoded 3");
  CHECK(r.written  == 6,                 "replace surrogate encoding: written 6 bytes");
  uint8_t want[6] = {0xFD, 0xFF, 0xFD, 0xFF, 0xFD, 0xFF};
  CHECK(memcmp(dst, want, 6) == 0,       "replace surrogate encoding: byte layout");
}

static void test_agrees_with_native(void) {
  // Endian functions must agree with utf8_transcode_utf16 on everything
  // that doesn't depend on byte order: status, consumed, decoded, and
  // written (in units*2, since native counts units and endian counts
  // bytes). Raw bytes are not compared here, since that would assume this
  // build's host endianness; transcode_endian.c compares the decoded code
  // unit values against native for both byte orders.
  static const struct { const char *s; size_t len; } cases[] = {
    {"",                                       0},
    {"ABC",                                    3},
    {"\xC3\xA9\xE2\x82\xAC",                   5},
    {"\xF0\x90\x8D\x88",                       4},
    {"A\xC3\xA9\xF0\x90\x8D\x88",              7},
    {"A\x80" "B",                              3},
  };
  size_t n = sizeof(cases) / sizeof(cases[0]);
  for (size_t i = 0; i < n; i++) {
    uint16_t d_native[64];
    uint8_t d_le[128];
    utf8_transcode_result_t rn = utf8_transcode_utf16(cases[i].s, cases[i].len, d_native, 64);
    utf8_transcode_result_t rl = utf8_transcode_utf16le(cases[i].s, cases[i].len, d_le, 128);
    CHECK(rl.status   == rn.status,       "agrees: status");
    CHECK(rl.consumed == rn.consumed,     "agrees: consumed");
    CHECK(rl.decoded  == rn.decoded,      "agrees: decoded");
    CHECK(rl.written  == rn.written * 2,  "agrees: written bytes = units*2");
  }
}

int main(void) {
  SUITE(__FILE__);
  RUN(test_empty);
  RUN(test_ascii_le);
  RUN(test_ascii_be);
  RUN(test_bmp_le);
  RUN(test_bmp_be);
  RUN(test_surrogate_pair_le);
  RUN(test_surrogate_pair_be);
  RUN(test_mixed_le);
  RUN(test_exhausted);
  RUN(test_exhausted_surrogate);
  RUN(test_dst_len_not_multiple_of_2);
  RUN(test_exhausted_resume);
  RUN(test_illformed);
  RUN(test_illformed_resume);
  RUN(test_illformed_lead_then_bad_cont);
  RUN(test_truncated);
  RUN(test_truncated_after_valid);
  RUN(test_replace_clean);
  RUN(test_replace_bare_cont_le);
  RUN(test_replace_bare_cont_be);
  RUN(test_replace_mixed);
  RUN(test_replace_truncated);
  RUN(test_replace_exhausted);
  RUN(test_replace_surrogate_pair);
  RUN(test_replace_surrogate_encoding);
  RUN(test_agrees_with_native);
  return report_results();
}
