#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "utf8_simd.h"
#include "utf8_swar.h"

#include "test_common.h"

#define CHECK(cond, msg)          \
  do {                            \
    TestCount++;                  \
    if (!(cond)) {                \
      printf("FAIL: %s (line %d)\n", msg, __LINE__); \
      TestFailed++;               \
    }                             \
  } while (0)

#define CHECK_COUNT(got, expected, msg) do { \
  TestCount++; \
  if ((got) != (size_t)(expected)) { \
    printf("FAIL: %s (line %d): got %zu expected %zu\n", \
           msg, __LINE__, (size_t)(got), (size_t)(expected)); \
    TestFailed++; \
  } \
} while (0)

#ifdef UTF8_SIMD_AVAILABLE

static size_t scalar_count_codepoints(const unsigned char *src, size_t len) {
  size_t count = 0;
  for (size_t i = 0; i < len; i++)
    count += ((int8_t)src[i] > -65);
  return count;
}

static void test_zero_blocks(void) {
  unsigned char buf[32];
  memset(buf, 'A', sizeof buf);
  CHECK_COUNT(utf8_simd_count_codepoints_Nx32(buf, 0), 0,
              "n=0");
}

static void test_ascii(void) {
  unsigned char buf[256];
  memset(buf, 'A', sizeof buf);
  CHECK_COUNT(utf8_simd_count_codepoints_Nx32(buf, 1), 32,
              "ascii: 1 block");
  CHECK_COUNT(utf8_simd_count_codepoints_Nx32(buf, 4), 128,
              "ascii: 4 blocks");
  CHECK_COUNT(utf8_simd_count_codepoints_Nx32(buf, 8), 256,
              "ascii: 8 blocks");
}

static void test_continuation(void) {
  unsigned char buf[256];
  memset(buf, 0x80, sizeof buf);
  CHECK_COUNT(utf8_simd_count_codepoints_Nx32(buf, 1), 0,
              "continuation: 1 block");
  CHECK_COUNT(utf8_simd_count_codepoints_Nx32(buf, 4), 0,
              "continuation: 4 blocks");
}

static void test_2byte(void) {
  // 16x é per 32-byte block
  unsigned char buf[64];
  for (size_t i = 0; i < sizeof buf; i += 2) {
    buf[i]     = 0xC3;
    buf[i + 1] = 0xA9;
  }
  CHECK_COUNT(utf8_simd_count_codepoints_Nx32(buf, 1), 16,
              "2byte: 1 block");
  CHECK_COUNT(utf8_simd_count_codepoints_Nx32(buf, 2), 32,
              "2byte: 2 blocks");
}

static void test_3byte(void) {
  // 10x € + 2 pad bytes per 32-byte block
  // Use full 32 bytes: 10x3 = 30 + 2 ASCII
  unsigned char buf[32];
  size_t j = 0;
  for (int k = 0; k < 10; k++) {
    buf[j++] = 0xE2;
    buf[j++] = 0x82;
    buf[j++] = 0xAC;
  }
  buf[j++] = 'A';
  buf[j++] = 'B';
  CHECK_COUNT(utf8_simd_count_codepoints_Nx32(buf, 1), 12,
              "3byte: 10x U+20AC + 2 ASCII");
}

static void test_4byte(void) {
  // 8x 𐍈 = 32 bytes, 8 codepoints
  unsigned char buf[32];
  for (size_t i = 0; i < 32; i += 4) {
    buf[i]     = 0xF0;
    buf[i + 1] = 0x90;
    buf[i + 2] = 0x8D;
    buf[i + 3] = 0x88;
  }
  CHECK_COUNT(utf8_simd_count_codepoints_Nx32(buf, 1), 8,
              "4byte: 8x U+10348");
}

static void test_nul(void) {
  unsigned char buf[32] = {0};
  CHECK_COUNT(utf8_simd_count_codepoints_Nx32(buf, 1), 32,
              "nul: 32 NUL bytes = 32 codepoints");
}

static void test_agrees_with_swar(void) {
  // Rotating pattern covering all byte classes
  unsigned char buf[256 * 32];
  for (size_t i = 0; i < sizeof buf; i++)
    buf[i] = (unsigned char)(i & 0xFF);
  for (size_t n = 1; n <= 256; n++) {
    size_t expected = scalar_count_codepoints(buf, n * 32);
    size_t simd     = utf8_simd_count_codepoints_Nx32(buf, n);
    size_t swar     = utf8_swar_count_codepoints_Nx32(buf, n);
    char msg[64];
    snprintf(msg, sizeof msg, "agrees swar: n=%zu", n);
    CHECK_COUNT(simd, expected, msg);
    snprintf(msg, sizeof msg, "agrees scalar: n=%zu", n);
    CHECK_COUNT(swar, expected, msg);
  }
}

static void test_large(void) {
  // Exercise batch flushing (>127 blocks for SSE2/NEON, >255 for AVX2)
  unsigned char buf[512 * 32];
  for (size_t i = 0; i < sizeof buf; i += 2) {
    buf[i]     = 0xC3;
    buf[i + 1] = 0xA9;
  }
  size_t expected = scalar_count_codepoints(buf, sizeof buf);
  CHECK_COUNT(utf8_simd_count_codepoints_Nx32(buf, 512), expected,
              "large: 512 blocks 2-byte");
}

static void test_mixed(void) {
  // "Aé€𐍈" repeated to fill 32 bytes: 10 bytes per unit, 3 units + 2 ASCII pad
  unsigned char unit[10] = {
    0x41,                         // A
    0xC3, 0xA9,                   // é
    0xE2, 0x82, 0xAC,             // €
    0xF0, 0x90, 0x8D, 0x88        // 𐍈
  };
  unsigned char buf[32];
  memcpy(buf +  0, unit, 10);
  memcpy(buf + 10, unit, 10);
  memcpy(buf + 20, unit, 10);
  buf[30] = 'X';
  buf[31] = 'Y';
  // 3 * 4 codepoints + 2 ASCII = 14
  CHECK_COUNT(utf8_simd_count_codepoints_Nx32(buf, 1), 14,
              "mixed: 3x(Aé€𐍈) + XY");
}

#endif /* UTF8_SIMD_AVAILABLE */

int main(void) {
#ifdef UTF8_SIMD_AVAILABLE
  test_zero_blocks();
  test_ascii();
  test_continuation();
  test_2byte();
  test_3byte();
  test_4byte();
  test_nul();
  test_agrees_with_swar();
  test_large();
  test_mixed();
#else
  printf("SIMD not available, skipping tests.\n");
#endif
  return report_results();
}
