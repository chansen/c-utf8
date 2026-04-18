/*
 * Copyright (c) 2026 Christian Hansen <chansen@cpan.org>
 * <https://github.com/chansen/c-utf8>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef UTF8_TRANSCODE_UNSAFE_H
#define UTF8_TRANSCODE_UNSAFE_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "utf8_transcode_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * utf8_transcode_utf32_unsafe -- transcode well-formed UTF-8 to UTF-32.
 *
 * src[0..src_len) MUST contain well-formed UTF-8. No validation is performed.
 * dst[0..dst_len) receives UTF-32 code units. Both src and dst are fully
 * caller-owned.
 *
 * Processes 8 bytes at a time; when all 8 are ASCII, widens to dst without
 * decoding. Batches consecutive sequences of the same length class to avoid
 * re-entering the lead-byte classifier.
 *
 * Returns a utf8_transcode_result_t describing the outcome:
 *
 *   status:
 *     UTF8_TRANSCODE_OK         src fully consumed.
 *     UTF8_TRANSCODE_EXHAUSTED  dst full before src was consumed.
 *
 *   consumed:  bytes read from src.
 *   decoded:   codepoints decoded from src.
 *   written:   UTF-32 code units written to dst (equals decoded for UTF-32).
 *   advance:   always 0.
 */
static inline utf8_transcode_result_t utf8_transcode_utf32_unsafe(const char *src,
                                                                  size_t src_len,
                                                                  uint32_t *dst,
                                                                  size_t dst_len) {
  const unsigned char *s = (const unsigned char *)src;
  size_t r = 0, w = 0;

  while (r < src_len && w < dst_len) {
    unsigned int c = s[r];
    if (c < 0x80u) {
      while (r + 8 <= src_len && w + 8 <= dst_len) {
        uint64_t v;
        memcpy(&v, s + r, sizeof(v));
        if (v & UINT64_C(0x8080808080808080))
          break;
        for (size_t i = 0; i < 8; i++)
          dst[w + i] = s[r + i];
        r += 8;
        w += 8;
      }
      while (r < src_len && w < dst_len && s[r] < 0x80u)
        dst[w++] = s[r++];
    } else if (c < 0xE0u) {
      do {
        dst[w++] = ((uint32_t)(s[r + 0] & 0x1Fu) << 6)
                 |  (uint32_t)(s[r + 1] & 0x3Fu);
        r += 2;
      } while (r < src_len && w < dst_len && (unsigned)s[r] - 0xC0u < 0x20u);
    } else if (c < 0xF0u) {
      do {
        dst[w++] = ((uint32_t)(s[r + 0] & 0x0Fu) << 12)
                 | ((uint32_t)(s[r + 1] & 0x3Fu) << 6)
                 |  (uint32_t)(s[r + 2] & 0x3Fu);
        r += 3;
      } while (r < src_len && w < dst_len && (unsigned)s[r] - 0xE0u < 0x10u);
    } else {
      do {
        dst[w++] = ((uint32_t)(s[r + 0] & 0x07u) << 18)
                 | ((uint32_t)(s[r + 1] & 0x3Fu) << 12)
                 | ((uint32_t)(s[r + 2] & 0x3Fu) << 6)
                 |  (uint32_t)(s[r + 3] & 0x3Fu);
        r += 4;
      } while (r < src_len && w < dst_len && (unsigned)s[r] >= 0xF0u);
    }
  }

  utf8_transcode_status_t status = r < src_len ? UTF8_TRANSCODE_EXHAUSTED
                                               : UTF8_TRANSCODE_OK;
  return (utf8_transcode_result_t){.status   = status,
                                   .consumed = r,
                                   .decoded  = w,
                                   .written  = w,
                                   .advance  = 0};
}

/*
 * utf8_transcode_utf16_unsafe -- transcode well-formed UTF-8 to UTF-16.
 *
 * src[0..src_len) MUST contain well-formed UTF-8. No validation is performed.
 * dst[0..dst_len) receives UTF-16 code units. Codepoints above U+FFFF are
 * encoded as surrogate pairs and consume two units. Both src and dst are fully
 * caller-owned.
 *
 * Processes 8 bytes at a time; when all 8 are ASCII, widens to dst without
 * decoding. Batches consecutive sequences of the same length class to avoid
 * re-entering the lead-byte classifier.
 *
 * Returns a utf8_transcode_result_t describing the outcome:
 *
 *   status:
 *     UTF8_TRANSCODE_OK         src fully consumed.
 *     UTF8_TRANSCODE_EXHAUSTED  dst full before src was consumed.
 *
 *   consumed:  bytes read from src.
 *   decoded:   codepoints decoded from src.
 *   written:   UTF-16 code units written to dst.
 *   advance:   always 0.
 */
static inline utf8_transcode_result_t utf8_transcode_utf16_unsafe(const char *src,
                                                                  size_t src_len,
                                                                  uint16_t *dst,
                                                                  size_t dst_len) {
  const unsigned char *s = (const unsigned char *)src;
  size_t r = 0, w = 0, n = 0;

  while (r < src_len && w < dst_len) {
    unsigned int c = s[r];
    if (c < 0x80u) {
      while (r + 8 <= src_len && w + 8 <= dst_len) {
        uint64_t v;
        memcpy(&v, s + r, sizeof(v));
        if (v & UINT64_C(0x8080808080808080))
          break;
        for (size_t i = 0; i < 8; i++)
          dst[w + i] = s[r + i];
        r += 8;
        w += 8;
        n += 8;
      }
      while (r < src_len && w < dst_len && s[r] < 0x80u) {
        dst[w++] = s[r++];
        n++;
      }
    } else if (c < 0xE0u) {
      do {
        dst[w++] = (uint16_t)(((uint32_t)(s[r + 0] & 0x1Fu) << 6)
                             | (uint32_t)(s[r + 1] & 0x3Fu));
        r += 2;
        n++;
      } while (r < src_len && w < dst_len && (unsigned)s[r] - 0xC0u < 0x20u);
    } else if (c < 0xF0u) {
      do {
        dst[w++] = (uint16_t)(((uint32_t)(s[r + 0] & 0x0Fu) << 12)
                            | ((uint32_t)(s[r + 1] & 0x3Fu) << 6)
                            |  (uint32_t)(s[r + 2] & 0x3Fu));
        r += 3;
        n++;
      } while (r < src_len && w < dst_len && (unsigned)s[r] - 0xE0u < 0x10u);
    } else {
      do {
        if (w + 1 >= dst_len)
          goto exhausted;
        uint32_t cp = ((uint32_t)(s[r + 0] & 0x07u) << 18)
                    | ((uint32_t)(s[r + 1] & 0x3Fu) << 12)
                    | ((uint32_t)(s[r + 2] & 0x3Fu) << 6)
                    |  (uint32_t)(s[r + 3] & 0x3Fu);
        cp -= 0x10000u;
        dst[w++] = (uint16_t)(0xD800u + (cp >> 10));
        dst[w++] = (uint16_t)(0xDC00u + (cp & 0x3FFu));
        r += 4;
        n++;
      } while (r < src_len && (unsigned)s[r] >= 0xF0u);
    }
  }

exhausted:;
  utf8_transcode_status_t status = r < src_len ? UTF8_TRANSCODE_EXHAUSTED
                                               : UTF8_TRANSCODE_OK;
  return (utf8_transcode_result_t){.status   = status,
                                   .consumed = r,
                                   .decoded  = n,
                                   .written  = w,
                                   .advance  = 0};
}

#ifdef __cplusplus
}
#endif
#endif /* UTF8_TRANSCODE_UNSAFE_H */
