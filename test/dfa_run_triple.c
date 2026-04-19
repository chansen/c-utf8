#include <string.h>

#ifdef UTF8_DFA_64
#  include "utf8_dfa64.h"
#else
#  include "utf8_dfa32.h"
#endif

#include "test_util.h"

static utf8_dfa_state_t triple_run(const char *src, size_t len) {
  return utf8_dfa_run_triple(UTF8_DFA_ACCEPT, (const unsigned char *)src, len);
}

static utf8_dfa_state_t ref_run(const char *src, size_t len) {
  return utf8_dfa_run(UTF8_DFA_ACCEPT, (const unsigned char *)src, len);
}

static bool triple_accepts(const char *src, size_t len) {
  return triple_run(src, len) == UTF8_DFA_ACCEPT;
}

#define EXPECT_ACCEPT(src, len) do { \
  char _esc[255*4+1]; \
  TestCount++; \
  if (!triple_accepts((src), (len))) { \
    escape_str((src), (len), _esc); \
    printf("FAIL line %u: expected ACCEPT for \"%s\" (len=%zu)\n", \
           __LINE__, _esc, (size_t)(len)); \
    TestFailed++; \
  } \
} while (0)

#define EXPECT_REJECT(src, len) do { \
  char _esc[255*4+1]; \
  TestCount++; \
  if (triple_accepts((src), (len))) { \
    escape_str((src), (len), _esc); \
    printf("FAIL line %u: expected REJECT for \"%s\" (len=%zu)\n", \
           __LINE__, _esc, (size_t)(len)); \
    TestFailed++; \
  } \
} while (0)

#define EXPECT_AGREES(src, len) do { \
  char _esc[255*4+1]; \
  utf8_dfa_state_t t = triple_run((src), (len)); \
  utf8_dfa_state_t r = ref_run((src), (len)); \
  bool t_ok = (t == UTF8_DFA_ACCEPT); \
  bool r_ok = (r == UTF8_DFA_ACCEPT); \
  TestCount++; \
  if (t_ok != r_ok) { \
    escape_str((src), (len), _esc); \
    printf("FAIL line %u: triple %s but ref %s for \"%s\" (len=%zu)\n", \
           __LINE__, t_ok ? "ACCEPT" : "REJECT", \
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
  EXPECT_ACCEPT("A", 1);
  EXPECT_REJECT("\x80", 1);
  EXPECT_REJECT("\xC3", 1);
}

static void
test_two_bytes(void) {
  EXPECT_ACCEPT("AB", 2);
  EXPECT_ACCEPT("\xC3\xA9", 2);
  EXPECT_REJECT("\xC3\xC3", 2);
  EXPECT_REJECT("\x80\x80", 2);
}

static void
test_three_bytes(void) {
  // Exactly 3 bytes: one byte per segment (ideal split) 
  EXPECT_ACCEPT("ABC", 3);
  EXPECT_ACCEPT("\xE2\x82\xAC", 3);  // € 
  EXPECT_REJECT("\xED\xA0\x80", 3);  // surrogate 
}

// --- Split-point boundary alignment --- 

static void
test_split_on_continuation(void) {
  /*
   * "AB\xC3\xA9CD" (6 bytes):
   *   m0 = 2, m1 = 4.  m1 lands on 'C' (ASCII, fine).
   *   If m0 landed on a continuation it would back up.
   */
  EXPECT_ACCEPT("AB\xC3\xA9" "CD", 6);
  EXPECT_AGREES("AB\xC3\xA9" "CD", 6);

  /*
   * "A\xE2\x82\xAC" "BC" (6 bytes):
   *   m0 = 2 lands on \x82 (continuation), backs up to 1.
   *   m1 = 4 lands on 'B' (ASCII, fine).
   */
  EXPECT_ACCEPT("A\xE2\x82\xAC" "BC", 6);
  EXPECT_AGREES("A\xE2\x82\xAC" "BC", 6);

  /*
   * 4-byte sequence spanning a split point:
   * "AB\xF0\x9F\x98\x80" "CD\xC3\xA9" (10 bytes)
   *   m0 = 3 lands on \x9F (cont), backs up to 2.
   *   m1 = 6 lands on 'C' (ASCII, fine).
   */
  EXPECT_ACCEPT("AB\xF0\x9F\x98\x80" "CD\xC3\xA9", 10);
  EXPECT_AGREES("AB\xF0\x9F\x98\x80" "CD\xC3\xA9", 10);

  /*
   * Both split points land on continuations:
   * "\xE2\x82\xAC\xE2\x82\xAC\xE2\x82\xAC" (9 bytes, 3 × €)
   *   m0 = 3 lands on \xE2 (lead, fine).
   *   m1 = 6 lands on \xE2 (lead, fine).
   */
  EXPECT_ACCEPT("\xE2\x82\xAC\xE2\x82\xAC\xE2\x82\xAC", 9);
  EXPECT_AGREES("\xE2\x82\xAC\xE2\x82\xAC\xE2\x82\xAC", 9);

  // 3 × 😀 = 12 bytes. m0=4, m1=8 — lands exactly on sequence boundaries.
  EXPECT_ACCEPT(
    "\xF0\x9F\x98\x80\xF0\x9F\x98\x80\xF0\x9F\x98\x80", 12);
  EXPECT_AGREES(
    "\xF0\x9F\x98\x80\xF0\x9F\x98\x80\xF0\x9F\x98\x80", 12);
}

static void
test_unicode_scalar_values(void) {
  char buf[8];

  for (uint32_t ord = 0x0000; ord <= 0x007F; ord++) {
    encode_ord(ord, 1, buf);
    EXPECT_AGREES(buf, 1);
  }

  for (uint32_t ord = 0x0080; ord <= 0x07FF; ord++) {
    encode_ord(ord, 2, buf);
    EXPECT_AGREES(buf, 2);
  }

  for (uint32_t ord = 0x0800; ord <= 0xFFFF; ord++) {
    if (ord >= 0xD800 && ord <= 0xDFFF)
      continue;
    encode_ord(ord, 3, buf);
    EXPECT_AGREES(buf, 3);
  }

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
test_invalid_in_first_third(void) {
  // surrogate in first third, valid rest 
  EXPECT_REJECT("\xED\xA0\x80" "abcdefghi", 12);
  EXPECT_AGREES("\xED\xA0\x80" "abcdefghi", 12);
}

static void
test_invalid_in_second_third(void) {
  // valid first, surrogate in middle, valid last 
  EXPECT_REJECT("abcd" "\xED\xA0\x80" "efgh", 11);
  EXPECT_AGREES("abcd" "\xED\xA0\x80" "efgh", 11);
}

static void
test_invalid_in_third_third(void) {
  // valid first two thirds, surrogate at end 
  EXPECT_REJECT("abcdefghi" "\xED\xA0\x80", 12);
  EXPECT_AGREES("abcdefghi" "\xED\xA0\x80", 12);
}

static void
test_invalid_in_all_thirds(void) {
  EXPECT_REJECT("\xED\xA0\x80" "\xED\xA0\x80" "\xED\xA0\x80", 9);
  EXPECT_AGREES("\xED\xA0\x80" "\xED\xA0\x80" "\xED\xA0\x80", 9);
}

static void
test_mixed_valid_sequences(void) {
  // é + € + 😀 = 9 bytes 
  EXPECT_ACCEPT("\xC3\xA9" "\xE2\x82\xAC" "\xF0\x9F\x98\x80", 9);
  EXPECT_AGREES("\xC3\xA9" "\xE2\x82\xAC" "\xF0\x9F\x98\x80", 9);

  // 😀 + € + é (reversed) 
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
  // 128 × é = 256 bytes 
  char buf[256];
  for (size_t i = 0; i < 256; i += 2) {
    buf[i]     = (char)0xC3;
    buf[i + 1] = (char)0xA9;
  }

  for (size_t len = 0; len <= 256; len += 2)
    EXPECT_AGREES(buf, len);
}

static void
test_large_3byte(void) {
  // 85 × € (255 bytes) + 1 ASCII = 256 bytes 
  char buf[256];
  for (size_t i = 0; i + 2 < 256; i += 3) {
    buf[i]     = (char)0xE2;
    buf[i + 1] = (char)0x82;
    buf[i + 2] = (char)0xAC;
  }
  buf[255] = 'Z';

  EXPECT_AGREES(buf, 256);

  // Test at sequence-aligned lengths 
  for (size_t len = 0; len <= 255; len += 3)
    EXPECT_AGREES(buf, len);
}

static void
test_large_with_error_at_every_position(void) {
  char buf[48];
  memset(buf, 'A', sizeof(buf));

  for (size_t pos = 0; pos < 48; pos++) {
    buf[pos] = (char)0x80;
    EXPECT_REJECT(buf, 48);
    EXPECT_AGREES(buf, 48);
    buf[pos] = 'A';
  }
}

// --- Uneven segment lengths due to boundary alignment --- 

static void
test_uneven_segments(void) {
  /*
   * 4-byte seq at the very start forces m0 to back up to 0,
   * making the first segment empty and the other two large.
   * "\xF0\x9F\x98\x80" + 8 ASCII = 12 bytes
   *   m0 = 4, m1 = 8, both on ASCII boundaries — even split.
   */
  EXPECT_ACCEPT("AAAAA\xF0\x9F\x98\x80""AAA", 12);
  EXPECT_AGREES("AAAAA\xF0\x9F\x98\x80""AAA", 12);

  // All 4-byte sequences: forces heavy boundary adjustment 
  EXPECT_ACCEPT(
    "\xF0\x9F\x98\x80" "\xF0\x9F\x98\x81" "\xF0\x9F\x98\x82"
    "\xF0\x9F\x98\x83" "\xF0\x9F\x98\x84", 20);
  EXPECT_AGREES(
    "\xF0\x9F\x98\x80" "\xF0\x9F\x98\x81" "\xF0\x9F\x98\x82"
    "\xF0\x9F\x98\x83" "\xF0\x9F\x98\x84", 20);
}

// --- Propagation of incoming state --- 

static void
test_incoming_state(void) {
  utf8_dfa_state_t st;

  /*
   * Need len >= 3 so all three segments get bytes and the
   * continuation lands in the first segment.
   */
  st = utf8_dfa_step(UTF8_DFA_ACCEPT, 0xC3);
  TestCount++;
  if (utf8_dfa_run_triple(st, (const unsigned char *)"\xA9XY", 3) != UTF8_DFA_ACCEPT) {
    printf("FAIL line %u: incoming state with valid continuation\n", __LINE__);
    TestFailed++;
  }

  st = utf8_dfa_step(UTF8_DFA_ACCEPT, 0xC3);
  TestCount++;
  if (utf8_dfa_run_triple(st, (const unsigned char *)"\x00XY", 3) == UTF8_DFA_ACCEPT) {
    printf("FAIL line %u: incoming state with invalid continuation should reject\n", __LINE__);
    TestFailed++;
  }

  /*
   * With len=1, m0=0: first segment is empty, incoming mid-sequence
   * state can't complete — must reject.
   */
  st = utf8_dfa_step(UTF8_DFA_ACCEPT, 0xC3);
  TestCount++;
  if (utf8_dfa_run_triple(st, (const unsigned char *)"\xA9", 1) == UTF8_DFA_ACCEPT) {
    printf("FAIL line %u: incoming mid-sequence state with len=1 should reject "
           "(first segment is empty)\n", __LINE__);
    TestFailed++;
  }
}

// --- Exhaustive 2-byte agreement --- 

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
  test_three_bytes();
  test_split_on_continuation();
  test_unicode_scalar_values();
  test_surrogates();
  test_non_shortest_form();
  test_non_unicode();
  test_bare_continuations();
  test_invalid_in_first_third();
  test_invalid_in_second_third();
  test_invalid_in_third_third();
  test_invalid_in_all_thirds();
  test_mixed_valid_sequences();
  test_large_ascii();
  test_large_multibyte();
  test_large_3byte();
  test_large_with_error_at_every_position();
  test_uneven_segments();
  test_incoming_state();
  test_exhaustive_2byte();
  return report_results();
}
