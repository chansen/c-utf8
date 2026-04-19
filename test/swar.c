#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "utf8_swar.h"
#include "test_common.h"

#define CHECK(cond, msg) do { \
  TestCount++; \
  if (!(cond)) { \
    printf("FAIL: %s (line %d)\n", msg, __LINE__); \
    TestFailed++; \
  } \
} while (0)

#define CHECK_WORD(got, expected, msg) do { \
  TestCount++; \
  if ((got) != (expected)) { \
    printf("FAIL: %s (line %d): got %016llx expected %016llx\n", \
           msg, __LINE__, \
           (unsigned long long)(got), (unsigned long long)(expected)); \
    TestFailed++; \
  } \
} while (0)

#define CHECK_COUNT(got, expected, msg) do { \
  TestCount++; \
  if ((got) != (size_t)(expected)) { \
    printf("FAIL: %s (line %d): got %zu expected %zu\n", \
           msg, __LINE__, (size_t)(got), (size_t)(expected)); \
    TestFailed++; \
  } \
} while (0)

static uint64_t
make_expected(const unsigned char bytes[8], int (*pred)(unsigned char)) {
  uint64_t r = 0;
  for (int i = 0; i < 8; i++)
    r |= (uint64_t)(pred(bytes[i]) ? 1 : 0) << (i * 8);
  return r;
}

static int is_ascii(unsigned char b)    { return b < 0x80; }
static int is_cont(unsigned char b)     { return (b & 0xC0) == 0x80; }
static int is_non_cont(unsigned char b) { return (b & 0xC0) != 0x80; }

// scalar reference count for a buffer of any length
static size_t scalar_count_codepoints(const unsigned char *src, size_t len) {
  size_t count = 0;
  for (size_t i = 0; i < len; i++)
    count += is_non_cont(src[i]);
  return count;
}

static void test_hsum_bits8(void) {
  CHECK(utf8_swar_hsum_bits8(UINT64_C(0x0000000000000000)) == 0,
        "hsum_bits8: all zero");
  CHECK(utf8_swar_hsum_bits8(UINT64_C(0x0101010101010101)) == 8,
        "hsum_bits8: all one");
  CHECK(utf8_swar_hsum_bits8(UINT64_C(0x0000000000000001)) == 1,
        "hsum_bits8: low byte");
  CHECK(utf8_swar_hsum_bits8(UINT64_C(0x0100000000000000)) == 1,
        "hsum_bits8: high byte");
  CHECK(utf8_swar_hsum_bits8(UINT64_C(0x0001000100010001)) == 4,
        "hsum_bits8: alternating");

  for (int n = 0; n <= 8; n++) {
    uint64_t w = 0;
    for (int i = 0; i < n; i++)
      w |= (uint64_t)1 << (i * 8);
    char msg[64];
    snprintf(msg, sizeof msg, "hsum_bits8: count %d", n);
    CHECK(utf8_swar_hsum_bits8(w) == (size_t)n, msg);
  }
}

static void test_hsum_bytes8(void) {
  CHECK(utf8_swar_hsum_bytes8(UINT64_C(0x0000000000000000)) == 0,
        "hsum_bytes8: all zero");
  CHECK(utf8_swar_hsum_bytes8(UINT64_C(0x0101010101010101)) == 8,
        "hsum_bytes8: all one");
  CHECK(utf8_swar_hsum_bytes8(UINT64_C(0x0202020202020202)) == 16,
        "hsum_bytes8: all two");
  CHECK(utf8_swar_hsum_bytes8(UINT64_C(0x0001000200030004)) == 10,
        "hsum_bytes8: mixed low lanes");
  CHECK(utf8_swar_hsum_bytes8(UINT64_C(0xFF00000000000000)) == 255,
        "hsum_bytes8: high lane 255");
  CHECK(utf8_swar_hsum_bytes8(UINT64_C(0x00000000000000FF)) == 255,
        "hsum_bytes8: low lane 255");
  CHECK(utf8_swar_hsum_bytes8(UINT64_C(0xFFFFFFFFFFFFFFFF)) == 2040,
        "hsum_bytes8: all lanes 255");
  CHECK(utf8_swar_hsum_bytes8(UINT64_C(0x7F7F7F7F7F7F7F7F)) == 1016,
        "hsum_bytes8: all lanes 127");
}

static void test_mark_ascii8(void) {
  uint64_t all = UINT64_C(0x0101010101010101);

  for (int b = 0; b < 256; b++) {
    uint64_t w = 0;
    memset(&w, b, sizeof w);
    uint64_t expected = is_ascii((unsigned char)b) ? all : 0;
    CHECK_WORD(utf8_swar_mark_ascii8(w), expected, "mark_ascii8: uniform byte");
  }

  {
    uint64_t w = 0;
    memset(&w, 0x7F, sizeof w);
    CHECK_WORD(utf8_swar_mark_ascii8(w), all, "mark_ascii8: 0x7F is ASCII");
  }
  {
    uint64_t w = 0;
    memset(&w, 0x80, sizeof w);
    CHECK_WORD(utf8_swar_mark_ascii8(w), 0,   "mark_ascii8: 0x80 is not ASCII");
  }

  unsigned char mixed[8] = {0x00, 0x41, 0x7F, 0x80, 0xBF, 0xC2, 0xE0, 0xFF};
  uint64_t w;
  memcpy(&w, mixed, 8);
  CHECK_WORD(utf8_swar_mark_ascii8(w), make_expected(mixed, is_ascii),
             "mark_ascii8: mixed");
}

static void test_mark_non_continuations8(void) {
  uint64_t all = UINT64_C(0x0101010101010101);

  for (int b = 0; b < 256; b++) {
    uint64_t w = 0;
    memset(&w, b, sizeof w);
    uint64_t expected = is_non_cont((unsigned char)b) ? all : 0;
    CHECK_WORD(utf8_swar_mark_non_continuations8(w), expected,
               "mark_non_continuations8: uniform byte");
  }

  {
    uint64_t w = 0;
    memset(&w, 0x7F, sizeof w);
    CHECK_WORD(utf8_swar_mark_non_continuations8(w), all, "mark_non_continuations8: 0x7F");
  }
  {
    uint64_t w = 0;
    memset(&w, 0x80, sizeof w);
    CHECK_WORD(utf8_swar_mark_non_continuations8(w), 0,   "mark_non_continuations8: 0x80");
  }
  {
    uint64_t w = 0;
    memset(&w, 0xBF, sizeof w);
    CHECK_WORD(utf8_swar_mark_non_continuations8(w), 0,   "mark_non_continuations8: 0xBF");
  }
  {
    uint64_t w = 0;
    memset(&w, 0xC0, sizeof w);
    CHECK_WORD(utf8_swar_mark_non_continuations8(w), all, "mark_non_continuations8: 0xC0");
  }

  unsigned char mixed[8] = {0x00, 0x41, 0x7F, 0x80, 0xBF, 0xC2, 0xE0, 0xFF};
  uint64_t w;
  memcpy(&w, mixed, 8);
  CHECK_WORD(utf8_swar_mark_non_continuations8(w), make_expected(mixed, is_non_cont),
             "mark_non_continuations8: mixed");
}

static void test_mark_continuations8(void) {
  uint64_t all = UINT64_C(0x0101010101010101);

  for (int b = 0; b < 256; b++) {
    uint64_t w = 0;
    memset(&w, b, sizeof w);
    uint64_t expected = is_cont((unsigned char)b) ? all : 0;
    CHECK_WORD(utf8_swar_mark_continuations8(w), expected,
               "mark_continuations8: uniform byte");
  }

  {
    uint64_t w = 0;
    memset(&w, 0x7F, sizeof w);
    CHECK_WORD(utf8_swar_mark_continuations8(w), 0,   "mark_continuations8: 0x7F");
  }
  {
    uint64_t w = 0;
    memset(&w, 0x80, sizeof w);
    CHECK_WORD(utf8_swar_mark_continuations8(w), all, "mark_continuations8: 0x80");
  }
  {
    uint64_t w = 0;
    memset(&w, 0xBF, sizeof w);
    CHECK_WORD(utf8_swar_mark_continuations8(w), all, "mark_continuations8: 0xBF");
  }
  {
    uint64_t w = 0;
    memset(&w, 0xC0, sizeof w);
    CHECK_WORD(utf8_swar_mark_continuations8(w), 0,   "mark_continuations8: 0xC0");
  }

  unsigned char mixed[8] = {0x00, 0x41, 0x7F, 0x80, 0xBF, 0xC2, 0xE0, 0xFF};
  uint64_t w;
  memcpy(&w, mixed, 8);
  CHECK_WORD(utf8_swar_mark_continuations8(w), make_expected(mixed, is_cont),
             "mark_continuations8: mixed");
}

static void test_invariants(void) {
  uint64_t all = UINT64_C(0x0101010101010101);

  for (int b = 0; b < 256; b++) {
    uint64_t w = 0;
    memset(&w, b, sizeof w);
    uint64_t non_conts = utf8_swar_mark_non_continuations8(w);
    uint64_t conts     = utf8_swar_mark_continuations8(w);
    uint64_t ascii     = utf8_swar_mark_ascii8(w);

    CHECK((non_conts | conts)  == all, "invariant: non_conts | conts == all");
    CHECK((non_conts & conts)  == 0,   "invariant: non_conts & conts == 0");
    CHECK((ascii & ~non_conts) == 0,   "invariant: ascii subset of non_conts");

    size_t n_non_conts = utf8_swar_hsum_bits8(non_conts);
    size_t n_conts     = utf8_swar_hsum_bits8(conts);
    CHECK(n_non_conts + n_conts == 8, "invariant: non_conts + conts == 8");
  }
}

static void test_count_codepoints_1x8(void) {
  unsigned char ascii8[8] = {'A','B','C','D','E','F','G','H'};
  CHECK_COUNT(utf8_swar_count_codepoints_1x8(ascii8), 8,
              "count_codepoints_1x8: all ASCII");

  unsigned char cont8[8];
  memset(cont8, 0x80, 8);
  CHECK_COUNT(utf8_swar_count_codepoints_1x8(cont8), 0,
              "count_codepoints_1x8: all continuation");

  unsigned char two8[8] = {0xC3,0xA9, 0xC3,0xA9, 0xC3,0xA9, 0xC3,0xA9};
  CHECK_COUNT(utf8_swar_count_codepoints_1x8(two8), 4,
              "count_codepoints_1x8: 4x 2-byte");

  unsigned char nul8[8] = {0,0,0,0,0,0,0,0};
  CHECK_COUNT(utf8_swar_count_codepoints_1x8(nul8), 8,
              "count_codepoints_1x8: all NUL");

  unsigned char ff8[8];
  memset(ff8, 0xFF, 8);
  CHECK_COUNT(utf8_swar_count_codepoints_1x8(ff8), 8,
              "count_codepoints_1x8: all 0xFF");

  unsigned char mixed[8] = {0xC3, 0xA9, 0x41, 0x80, 0xE6, 0x97, 0xA5, 0x42};
  CHECK_COUNT(utf8_swar_count_codepoints_1x8(mixed), 4,
              "count_codepoints_1x8: mixed");

  for (int b = 0; b < 256; b++) {
    unsigned char buf[8];
    memset(buf, b, 8);
    size_t expected = is_non_cont((unsigned char)b) ? 8 : 0;
    char msg[64]; snprintf(msg, sizeof msg,
                           "count_codepoints_1x8: uniform 0x%02x", b);
    CHECK_COUNT(utf8_swar_count_codepoints_1x8(buf), expected, msg);
  }
}

static void test_count_codepoints_Nx32(void) {
  // n=0 returns 0
  unsigned char buf[512];
  memset(buf, 0x41, sizeof buf);
  CHECK_COUNT(utf8_swar_count_codepoints_Nx32(buf, 0), 0,
              "nx32: n=0");

  // all ASCII
  CHECK_COUNT(utf8_swar_count_codepoints_Nx32(buf, 1), 32,
              "nx32: 1 block all ASCII");
  CHECK_COUNT(utf8_swar_count_codepoints_Nx32(buf, 4), 128,
              "nx32: 4 blocks all ASCII");

  // all continuation
  memset(buf, 0x80, sizeof buf);
  CHECK_COUNT(utf8_swar_count_codepoints_Nx32(buf, 1),  0,
              "nx32: 1 block all continuation");
  CHECK_COUNT(utf8_swar_count_codepoints_Nx32(buf, 8),  0,
              "nx32: 8 blocks all continuation");

  // consistency: nx32(n) == scalar over n*32 bytes
  unsigned char pattern[256];
  for (int i = 0; i < 256; i++)
    pattern[i] = (unsigned char)i;
  for (size_t n = 1; n <= 8; n++) {
    size_t expected = scalar_count_codepoints(pattern, n * 32);
    char msg[64];
    snprintf(msg, sizeof msg, "nx32: n=%zu vs scalar", n);
    CHECK_COUNT(utf8_swar_count_codepoints_Nx32(pattern, n),
                expected, msg);
  }

  // large n to exercise the 63-block batching on non-POPCNT path
  unsigned char large[128 * 32];
  for (size_t i = 0; i < sizeof large; i++)
      large[i] = (unsigned char)(i % 4 == 0 ? 0x41 : 0x80);
  size_t expected = scalar_count_codepoints(large, sizeof large);
  CHECK_COUNT(utf8_swar_count_codepoints_Nx32(large, 128),
              expected, "nx32: n=128 mixed");
}

int main(void) {
  test_hsum_bits8();
  test_hsum_bytes8();
  test_mark_ascii8();
  test_mark_non_continuations8();
  test_mark_continuations8();
  test_invariants();
  test_count_codepoints_1x8();
  test_count_codepoints_Nx32();
  return report_results();
}
