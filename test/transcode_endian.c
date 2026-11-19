#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "utf8_dfa64.h"
#include "utf8_transcode.h"
#include "utf8_transcode_unsafe.h"
#include "utf8_transcode_endian.h"
#include "utf8_transcode_endian_unsafe.h"

#include "test.h"
#include "test_util.h"

/*
 * Compares every explicit byte order function against its native
 * counterpart in utf8_transcode.h / utf8_transcode_unsafe.h: same status,
 * consumed, decoded and advance, written scaled from units to bytes, and
 * the same code unit values. The output bytes are decoded back to values
 * before comparing, so the checks do not depend on the byte order of the
 * host. Nothing may be written past the reported length.
 */

#define MAX_SRC   128
#define MAX_UNITS (MAX_SRC + 8)
#define GUARD     8

typedef utf8_transcode_result_t (*native16_fn)(const char *, size_t, uint16_t *, size_t);
typedef utf8_transcode_result_t (*native32_fn)(const char *, size_t, uint32_t *, size_t);
typedef utf8_transcode_result_t (*endian_fn)(const char *, size_t, uint8_t *, size_t);

static uint32_t
load(const uint8_t *p, size_t size, bool big) {
  uint32_t v = 0;
  for (size_t i = 0; i < size; i++)
    v |= (uint32_t)p[i] << (8 * (big ? size - 1 - i : i));
  return v;
}

static bool
same_result(utf8_transcode_result_t native,
            utf8_transcode_result_t endian,
            size_t unit_size) {
  return endian.status   == native.status &&
         endian.consumed == native.consumed &&
         endian.decoded  == native.decoded &&
         endian.advance  == native.advance &&
         endian.written  == native.written * unit_size;
}

static bool
same16(native16_fn native, endian_fn endian, bool big,
       const char *src, size_t len, size_t dst_len) {
  uint16_t units[MAX_UNITS];
  uint8_t bytes[2 * MAX_UNITS + GUARD];
  utf8_transcode_result_t rn, re;

  memset(bytes, 0xA5, sizeof bytes);
  rn = native(src, len, units, dst_len / 2);
  re = endian(src, len, bytes, dst_len);

  if (!same_result(rn, re, 2))
    return false;
  for (size_t i = 0; i < rn.written; i++) {
    if (load(bytes + 2 * i, 2, big) != units[i])
      return false;
  }
  for (size_t i = 0; i < GUARD; i++) {
    if (bytes[re.written + i] != 0xA5)
      return false;
  }
  return true;
}

static bool
same32(native32_fn native, endian_fn endian, bool big,
       const char *src, size_t len, size_t dst_len) {
  uint32_t units[MAX_UNITS];
  uint8_t bytes[4 * MAX_UNITS + GUARD];
  utf8_transcode_result_t rn, re;

  memset(bytes, 0xA5, sizeof bytes);
  rn = native(src, len, units, dst_len / 4);
  re = endian(src, len, bytes, dst_len);

  if (!same_result(rn, re, 4))
    return false;
  for (size_t i = 0; i < rn.written; i++) {
    if (load(bytes + 4 * i, 4, big) != units[i])
      return false;
  }
  for (size_t i = 0; i < GUARD; i++) {
    if (bytes[re.written + i] != 0xA5)
      return false;
  }
  return true;
}

static void
report(const char *name, const char *src, size_t len, size_t dst_len) {
  char esc[4 * MAX_SRC + 1];
  printf("FAIL: %s differs from native for \"%s\" with dst_len %zu\n",
         name, escape_str(src, len, esc), dst_len);
}

#define CHECK16(name, native, endian, big, src, len, dst_len) do { \
  TestCount++; \
  if (!same16(native, endian, big, src, len, dst_len)) { \
    if (TestFailed++ < 10) \
      report(name, src, len, dst_len); \
  } \
} while (0)

#define CHECK32(name, native, endian, big, src, len, dst_len) do { \
  TestCount++; \
  if (!same32(native, endian, big, src, len, dst_len)) { \
    if (TestFailed++ < 10) \
      report(name, src, len, dst_len); \
  } \
} while (0)

/* Strict, _replace and _unsafe variants on well-formed input. */
static void
check_wellformed(const char *src, size_t len, size_t dst_len) {
  CHECK16("utf16le", utf8_transcode_utf16, utf8_transcode_utf16le, false, src, len, dst_len);
  CHECK16("utf16be", utf8_transcode_utf16, utf8_transcode_utf16be, true, src, len, dst_len);
  CHECK16("utf16le_unsafe", utf8_transcode_utf16_unsafe, utf8_transcode_utf16le_unsafe, false, src, len, dst_len);
  CHECK16("utf16be_unsafe", utf8_transcode_utf16_unsafe, utf8_transcode_utf16be_unsafe, true, src, len, dst_len);
  CHECK32("utf32le", utf8_transcode_utf32, utf8_transcode_utf32le, false, src, len, dst_len);
  CHECK32("utf32be", utf8_transcode_utf32, utf8_transcode_utf32be, true, src, len, dst_len);
  CHECK32("utf32le_unsafe", utf8_transcode_utf32_unsafe, utf8_transcode_utf32le_unsafe, false, src, len, dst_len);
  CHECK32("utf32be_unsafe", utf8_transcode_utf32_unsafe, utf8_transcode_utf32be_unsafe, true, src, len, dst_len);
}

/* Strict and _replace variants; the input may be ill-formed. */
static void
check_any(const char *src, size_t len, size_t dst_len) {
  CHECK16("utf16le", utf8_transcode_utf16, utf8_transcode_utf16le, false, src, len, dst_len);
  CHECK16("utf16be", utf8_transcode_utf16, utf8_transcode_utf16be, true, src, len, dst_len);
  CHECK16("utf16le_replace", utf8_transcode_utf16_replace, utf8_transcode_utf16le_replace, false, src, len, dst_len);
  CHECK16("utf16be_replace", utf8_transcode_utf16_replace, utf8_transcode_utf16be_replace, true, src, len, dst_len);
  CHECK32("utf32le", utf8_transcode_utf32, utf8_transcode_utf32le, false, src, len, dst_len);
  CHECK32("utf32be", utf8_transcode_utf32, utf8_transcode_utf32be, true, src, len, dst_len);
  CHECK32("utf32le_replace", utf8_transcode_utf32_replace, utf8_transcode_utf32le_replace, false, src, len, dst_len);
  CHECK32("utf32be_replace", utf8_transcode_utf32_replace, utf8_transcode_utf32be_replace, true, src, len, dst_len);
}

/* Every Unicode scalar value, alone, into a destination that fits it. */
static void
test_scalar_values(void) {
  char src[4];

  for (uint32_t cp = 0; cp <= 0x10FFFF; cp++) {
    size_t len;

    if (cp >= 0xD800 && cp <= 0xDFFF)
      continue;
    len = cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
    encode_ord(cp, len, src);
    check_wellformed(src, len, 4);
  }
}

static uint32_t RandomState = 0x9E3779B9u;

static uint32_t
random_next(void) {
  uint32_t x = RandomState;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return RandomState = x;
}

static uint32_t
random_range(uint32_t lo, uint32_t hi) {
  return lo + random_next() % (hi - lo + 1);
}

/* ASCII runs long enough to reach the 8-byte fast path, and sequences of
   every length; ill-formed and truncated sequences unless wellformed. */
static size_t
random_input(char *dst, size_t max, bool wellformed) {
  size_t len = 0;

  while (len + 24 <= max) {
    switch (random_next() % (wellformed ? 12 : 16)) {
      case 0: case 1: case 2: case 3: {
        size_t n = random_range(1, 24);
        for (size_t i = 0; i < n; i++)
          dst[len++] = (char)random_range(0x20, 0x7E);
        break;
      }
      case 4: case 5:
        encode_ord(random_range(0x80, 0x7FF), 2, dst + len);
        len += 2;
        break;
      case 6: case 7: case 8:
        encode_ord(random_range(0x800, 0xD7FF), 3, dst + len);
        len += 3;
        break;
      case 9:
        encode_ord(random_range(0xE000, 0xFFFF), 3, dst + len);
        len += 3;
        break;
      case 10: case 11:
        encode_ord(random_range(0x10000, 0x10FFFF), 4, dst + len);
        len += 4;
        break;
      case 12: case 13:
        dst[len++] = (char)random_range(0x80, 0xFF);
        break;
      default: {
        size_t n = random_range(1, 3);
        encode_ord(random_range(0x10000, 0x10FFFF), 4, dst + len);
        len += n;
        break;
      }
    }
    if (random_next() % 12 == 0)
      break;
  }
  return len;
}

/* Random strings into every destination size, in bytes, from empty to
   more than enough: exercises EXHAUSTED at each position, a destination
   that is not a whole number of code units, and resuming mid-string. */
static void
test_random_strings(void) {
  char src[MAX_SRC];

  for (size_t iter = 0; iter < 400; iter++) {
    size_t len = random_input(src, sizeof src, false);
    for (size_t dst_len = 0; dst_len <= 4 * len + 5; dst_len++)
      check_any(src, len, dst_len);

    len = random_input(src, sizeof src, true);
    for (size_t dst_len = 0; dst_len <= 4 * len + 5; dst_len++)
      check_wellformed(src, len, dst_len);
  }
}

int
main(int argc, char **argv) {
  SUITE(__FILE__);
  RUN(test_scalar_values);
  RUN(test_random_strings);
  return report_results();
}
