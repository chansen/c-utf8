#include <string.h>

#ifdef UTF8_DFA_64
#  include "utf8_dfa64.h"
#else
#  include "utf8_dfa32.h"
#endif

#include "test_util.h"

static utf8_dfa_state_t dual_run(const char *src, size_t len) {
  return utf8_dfa_run_dual(UTF8_DFA_ACCEPT, (const unsigned char *)src, len);
}

static utf8_dfa_state_t ref_run(const char *src, size_t len) {
  return utf8_dfa_run(UTF8_DFA_ACCEPT, (const unsigned char *)src, len);
}

static bool dual_accepts(const char *src, size_t len) {
  return dual_run(src, len) == UTF8_DFA_ACCEPT;
}

#define EXPECT_ACCEPT(src, len) do { \
  char _esc[255*4+1]; \
  TestCount++; \
  if (!dual_accepts((src), (len))) { \
    escape_str((src), (len), _esc); \
    printf("FAIL line %u: expected ACCEPT for \"%s\" (len=%zu)\n", \
           __LINE__, _esc, (size_t)(len)); \
    TestFailed++; \
  } \
} while (0)

#define EXPECT_REJECT(src, len) do { \
  char _esc[255*4+1]; \
  TestCount++; \
  if (dual_accepts((src), (len))) { \
    escape_str((src), (len), _esc); \
    printf("FAIL line %u: expected REJECT for \"%s\" (len=%zu)\n", \
           __LINE__, _esc, (size_t)(len)); \
    TestFailed++; \
  } \
} while (0)

#define EXPECT_AGREES(src, len) do { \
  char _esc[255*4+1]; \
  utf8_dfa_state_t d = dual_run((src), (len)); \
  utf8_dfa_state_t r = ref_run((src), (len)); \
  bool d_ok = (d == UTF8_DFA_ACCEPT); \
  bool r_ok = (r == UTF8_DFA_ACCEPT); \
  TestCount++; \
  if (d_ok != r_ok) { \
    escape_str((src), (len), _esc); \
    printf("FAIL line %u: dual %s but ref %s for \"%s\" (len=%zu)\n", \
           __LINE__, d_ok ? "ACCEPT" : "REJECT", \
           r_ok ? "ACCEPT" : "REJECT", _esc, (size_t)(len)); \
    TestFailed++; \
  } \
} while (0)

static void
test_empty(void) {
  EXPECT_ACCEPT("", 0);
}

static void
test_single_byte(void) {
  // len=1: mid=0, first half empty, second half is the single byte
  EXPECT_ACCEPT("A", 1);
  EXPECT_REJECT("\x80", 1);
  EXPECT_REJECT("\xC3", 1);
}

static void
test_two_bytes(void) {
  // len=2: mid=1, split right down the middle
  EXPECT_ACCEPT("AB", 2);
  EXPECT_ACCEPT("\xC3\xA9", 2);  // é
  EXPECT_REJECT("\xC3\xC3", 2);  // truncated + lead
  EXPECT_REJECT("\x80\x80", 2);  // bare continuations
}

static void
test_split_on_continuation(void) {
  /*
   * "AB\xC3\xA9CD" (6 bytes) — mid=3 lands on \xA9 (continuation).
   * The split must back up to index 2 (\xC3) so the 2-byte sequence
   * stays in the second half.
   */
  EXPECT_ACCEPT("AB\xC3\xA9" "CD", 6);
  EXPECT_AGREES("AB\xC3\xA9" "CD", 6);

  // 3-byte sequence spanning the midpoint
  // "A\xE2\x82\xAC" (4 bytes) — mid=2 lands on \x82, backs up to 1
  EXPECT_ACCEPT("A\xE2\x82\xAC", 4);
  EXPECT_AGREES("A\xE2\x82\xAC", 4);

  // 4-byte sequence near the midpoint
  // "AB\xF0\x9F\x98\x80CD" (8 bytes) — mid=4 lands on \x98, backs up to 2
  EXPECT_ACCEPT("AB\xF0\x9F\x98\x80" "CD", 8);
  EXPECT_AGREES("AB\xF0\x9F\x98\x80" "CD", 8);
}

static void
test_unicode_scalar_values(void) {
  char buf[8];

  // 1-byte: U+0000..U+007F
  for (uint32_t ord = 0x0000; ord <= 0x007F; ord++) {
    encode_ord(ord, 1, buf);
    EXPECT_AGREES(buf, 1);
  }

  // 2-byte: U+0080..U+07FF
  for (uint32_t ord = 0x0080; ord <= 0x07FF; ord++) {
    encode_ord(ord, 2, buf);
    EXPECT_AGREES(buf, 2);
  }

  // 3-byte: U+0800..U+FFFF (skip surrogates)
  for (uint32_t ord = 0x0800; ord <= 0xFFFF; ord++) {
    if (ord >= 0xD800 && ord <= 0xDFFF)
      continue;
    encode_ord(ord, 3, buf);
    EXPECT_AGREES(buf, 3);
  }

  // 4-byte: U+10000..U+10FFFF
  for (uint32_t ord = 0x10000; ord <= 0x10FFFF; ord++) {
    encode_ord(ord, 4, buf);
    EXPECT_AGREES(buf, 4);
  }
}

static void
test_surrogates(void) {
  char buf[3];
  for (uint32_t ord = 0xD800; ord <= 0xDFFF; ord++) {
    encode_ord(ord, 3, buf);
    EXPECT_REJECT(buf, 3);
  }
}

static void
test_non_shortest_form(void) {
  char buf[4];

  for (uint32_t ord = 0x0000; ord <= 0x007F; ord++) {
    encode_ord(ord, 2, buf);
    EXPECT_REJECT(buf, 2);
  }
  for (uint32_t ord = 0x0000; ord <= 0x07FF; ord++) {
    encode_ord(ord, 3, buf);
    EXPECT_REJECT(buf, 3);
  }
  for (uint32_t ord = 0x0000; ord <= 0xFFFF; ord++) {
    encode_ord(ord, 4, buf);
    EXPECT_REJECT(buf, 4);
  }
}

static void
test_non_unicode(void) {
  char buf[4];
  for (uint32_t ord = 0x110000; ord <= 0x1FFFFF; ord++) {
    encode_ord(ord, 4, buf);
    EXPECT_REJECT(buf, 4);
  }
}

static void
test_bare_continuations(void) {
  char buf[1];
  for (unsigned b = 0x80; b <= 0xBF; b++) {
    buf[0] = (char)b;
    EXPECT_REJECT(buf, 1);
  }
}

static void
test_invalid_in_first_half(void) {
  // surrogate in first half, valid ASCII in second
  EXPECT_REJECT("\xED\xA0\x80" "abcdef", 9);
  EXPECT_AGREES("\xED\xA0\x80" "abcdef", 9);
}

static void
test_invalid_in_second_half(void) {
  // valid ASCII in first half, surrogate in second
  EXPECT_REJECT("abcdef" "\xED\xA0\x80", 9);
  EXPECT_AGREES("abcdef" "\xED\xA0\x80", 9);
}

static void
test_invalid_in_both_halves(void) {
  EXPECT_REJECT("\xED\xA0\x80" "\xED\xA0\x80", 6);
  EXPECT_AGREES("\xED\xA0\x80" "\xED\xA0\x80", 6);
}

static void
test_mixed_valid_sequences(void) {
  // é (2B) + € (3B) + 😀 (4B) = 9 bytes
  EXPECT_ACCEPT("\xC3\xA9" "\xE2\x82\xAC" "\xF0\x9F\x98\x80", 9);
  EXPECT_AGREES("\xC3\xA9" "\xE2\x82\xAC" "\xF0\x9F\x98\x80", 9);

  // Reversed order: 😀 + € + é
  EXPECT_ACCEPT("\xF0\x9F\x98\x80" "\xE2\x82\xAC" "\xC3\xA9", 9);
  EXPECT_AGREES("\xF0\x9F\x98\x80" "\xE2\x82\xAC" "\xC3\xA9", 9);

  // ASCII padding around multibyte
  EXPECT_ACCEPT("hello\xC3\xA9world\xE2\x82\xAC!", 16);
  EXPECT_AGREES("hello\xC3\xA9world\xE2\x82\xAC!", 16);
}

static void
test_large_ascii(void) {
  char buf[256];
  memset(buf, 'x', sizeof(buf));

  for (size_t len = 0; len <= 256; len++)
    EXPECT_AGREES(buf, len);
}

static void
test_large_multibyte(void) {
  // 128 × é = 256 bytes of 2-byte sequences
  char buf[256];
  for (size_t i = 0; i < 256; i += 2) {
    buf[i]     = (char)0xC3;
    buf[i + 1] = (char)0xA9;
  }

  for (size_t len = 0; len <= 256; len += 2)
    EXPECT_AGREES(buf, len);
}

static void
test_large_with_error_at_every_position(void) {
  /*
   * 32 bytes of valid ASCII with a single bare continuation injected
   * at each position in turn. Exercises the split landing before,
   * after, and on the error byte.
   */
  char buf[32];
  memset(buf, 'A', sizeof(buf));

  for (size_t pos = 0; pos < 32; pos++) {
    buf[pos] = (char)0x80;
    EXPECT_REJECT(buf, 32);
    EXPECT_AGREES(buf, 32);
    buf[pos] = 'A';
  }
}

static void
test_incoming_state(void) {
  utf8_dfa_state_t st;

  /*
   * Feed the lead byte of a 2-byte sequence into step,
   * then hand the continuation + extra bytes to run_dual.
   * Need len >= 2 so that mid >= 1 and the continuation
   * lands in the first half where the incoming state applies.
   */
  st = utf8_dfa_step(UTF8_DFA_ACCEPT, 0xC3);
  TestCount++;
  if (utf8_dfa_run_dual(st, (const unsigned char *)"\xA9X", 2) != UTF8_DFA_ACCEPT) {
    printf("FAIL line %u: incoming state with valid continuation\n", __LINE__);
    TestFailed++;
  }

  // Same lead byte, but continuation is wrong (0x00 is ASCII, not 0x80-0xBF).
  st = utf8_dfa_step(UTF8_DFA_ACCEPT, 0xC3);
  TestCount++;
  if (utf8_dfa_run_dual(st, (const unsigned char *)"\x00X", 2) == UTF8_DFA_ACCEPT) {
    printf("FAIL line %u: incoming state with invalid continuation should reject\n", __LINE__);
    TestFailed++;
  }

  // With len=1, mid=0: the first half is empty, so a non-ACCEPT
  // incoming state has no bytes to complete the sequence — must reject.
  st = utf8_dfa_step(UTF8_DFA_ACCEPT, 0xC3);
  TestCount++;
  if (utf8_dfa_run_dual(st, (const unsigned char *)"\xA9", 1) == UTF8_DFA_ACCEPT) {
    printf("FAIL line %u: incoming mid-sequence state with len=1 should reject "
           "(first half is empty)\n", __LINE__);
    TestFailed++;
  }
}


static void
test_exhaustive_2byte(void) {
  char buf[2];
  for (unsigned b0 = 0; b0 < 256; b0++) {
    for (unsigned b1 = 0; b1 < 256; b1++) {
      buf[0] = (char)b0;
      buf[1] = (char)b1;
      EXPECT_AGREES(buf, 2);
    }
  }
}

int
main(void) {
  test_empty();
  test_single_byte();
  test_two_bytes();
  test_split_on_continuation();
  test_unicode_scalar_values();
  test_surrogates();
  test_non_shortest_form();
  test_non_unicode();
  test_bare_continuations();
  test_invalid_in_first_half();
  test_invalid_in_second_half();
  test_invalid_in_both_halves();
  test_mixed_valid_sequences();
  test_large_ascii();
  test_large_multibyte();
  test_large_with_error_at_every_position();
  test_incoming_state();
  test_exhaustive_2byte();
  return report_results();
}
