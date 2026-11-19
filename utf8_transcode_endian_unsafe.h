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
#ifndef UTF8_TRANSCODE_ENDIAN_UNSAFE_H
#define UTF8_TRANSCODE_ENDIAN_UNSAFE_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "utf8_transcode_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Transcode well-formed UTF-8 to UTF-16LE, UTF-16BE, UTF-32LE or UTF-32BE,
 * without validation.
 *
 * Unsafe counterpart to utf8_transcode_endian.h: src[0..src_len) MUST
 * contain well-formed UTF-8; behaviour is undefined otherwise. dst is a
 * byte buffer that needs no alignment; dst_len and written are byte counts.
 * No byte order mark is written.
 *
 * Each function body is a copy of its counterpart in
 * utf8_transcode_unsafe.h, with w counting code units and each store
 * addressing dst by byte offset; keep the two in sync. LOW is the offset of
 * the least significant byte within a code unit: the ASCII fast path zeroes
 * eight units and writes each byte at that offset. The bodies are defined
 * once as macros and instantiated per byte order. The macros are not part
 * of the public interface and are undefined below.
 */

#define UTF8_DEFINE_TRANSCODE_UTF16_UNSAFE(SUF, STORE16, LOW)                \
static inline utf8_transcode_result_t                                        \
utf8_transcode_utf16##SUF##_unsafe(const char *src,                          \
                                   size_t src_len,                           \
                                   uint8_t *dst,                             \
                                   size_t dst_len) {                         \
  const uint8_t *bytes = (const uint8_t *)src;                               \
  size_t r = 0, w = 0, n = 0;                                                \
  size_t dst_units = dst_len / 2;                                            \
                                                                             \
  while (r < src_len && w < dst_units) {                                     \
    unsigned int lead = bytes[r];                                            \
    if (lead < 0x80u) {                                                      \
      while (r + 8 <= src_len && w + 8 <= dst_units) {                       \
        uint64_t v;                                                          \
        memcpy(&v, bytes + r, sizeof(v));                                    \
        if (v & UINT64_C(0x8080808080808080))                                \
          break;                                                             \
        memset(dst + w * 2, 0, 16);                                          \
        for (size_t i = 0; i < 8; i++)                                       \
          dst[(w + i) * 2 + LOW] = bytes[r + i];                             \
        r += 8;                                                              \
        w += 8;                                                              \
        n += 8;                                                              \
      }                                                                      \
      while (r < src_len && w < dst_units && bytes[r] < 0x80u) {             \
        STORE16(dst + w * 2, (uint16_t)bytes[r]);                            \
        w++;                                                                 \
        r++;                                                                 \
        n++;                                                                 \
      }                                                                      \
    } else if (lead < 0xE0u) {                                               \
      do {                                                                   \
        STORE16(dst + w * 2,                                                 \
                (uint16_t)(((uint32_t)(bytes[r + 0] & 0x1Fu) << 6)           \
                          |  (uint32_t)(bytes[r + 1] & 0x3Fu)));             \
        w++;                                                                 \
        r += 2;                                                              \
        n++;                                                                 \
      } while (r < src_len && w < dst_units &&                               \
               (unsigned)bytes[r] - 0xC0u < 0x20u);                          \
    } else if (lead < 0xF0u) {                                               \
      do {                                                                   \
        STORE16(dst + w * 2,                                                 \
                (uint16_t)(((uint32_t)(bytes[r + 0] & 0x0Fu) << 12)          \
                          | ((uint32_t)(bytes[r + 1] & 0x3Fu) << 6)          \
                          |  (uint32_t)(bytes[r + 2] & 0x3Fu)));             \
        w++;                                                                 \
        r += 3;                                                              \
        n++;                                                                 \
      } while (r < src_len && w < dst_units &&                               \
               (unsigned)bytes[r] - 0xE0u < 0x10u);                          \
    } else {                                                                 \
      do {                                                                   \
        if (w + 1 >= dst_units)                                              \
          goto exhausted;                                                    \
        uint32_t cp = ((uint32_t)(bytes[r + 0] & 0x07u) << 18)               \
                    | ((uint32_t)(bytes[r + 1] & 0x3Fu) << 12)               \
                    | ((uint32_t)(bytes[r + 2] & 0x3Fu) << 6)                \
                    |  (uint32_t)(bytes[r + 3] & 0x3Fu);                     \
        cp -= 0x10000u;                                                      \
        STORE16(dst + w * 2, (uint16_t)(0xD800u + (cp >> 10)));              \
        w++;                                                                 \
        STORE16(dst + w * 2, (uint16_t)(0xDC00u + (cp & 0x3FFu)));           \
        w++;                                                                 \
        r += 4;                                                              \
        n++;                                                                 \
      } while (r < src_len && (unsigned)bytes[r] >= 0xF0u);                  \
    }                                                                        \
  }                                                                          \
                                                                             \
exhausted:;                                                                  \
  utf8_transcode_status_t status = r < src_len ? UTF8_TRANSCODE_EXHAUSTED    \
                                               : UTF8_TRANSCODE_OK;          \
  return (utf8_transcode_result_t){.status   = status,                       \
                                   .consumed = r,                            \
                                   .decoded  = n,                            \
                                   .written  = w * 2,                        \
                                   .advance  = 0};                           \
}

#define UTF8_DEFINE_TRANSCODE_UTF32_UNSAFE(SUF, STORE32, LOW)                \
static inline utf8_transcode_result_t                                        \
utf8_transcode_utf32##SUF##_unsafe(const char *src,                          \
                                   size_t src_len,                           \
                                   uint8_t *dst,                             \
                                   size_t dst_len) {                         \
  const uint8_t *bytes = (const uint8_t *)src;                               \
  size_t r = 0, w = 0;                                                       \
  size_t dst_units = dst_len / 4;                                            \
                                                                             \
  while (r < src_len && w < dst_units) {                                     \
    unsigned int lead = bytes[r];                                            \
    if (lead < 0x80u) {                                                      \
      while (r + 8 <= src_len && w + 8 <= dst_units) {                       \
        uint64_t v;                                                          \
        memcpy(&v, bytes + r, sizeof(v));                                    \
        if (v & UINT64_C(0x8080808080808080))                                \
          break;                                                             \
        memset(dst + w * 4, 0, 32);                                          \
        for (size_t i = 0; i < 8; i++)                                       \
          dst[(w + i) * 4 + LOW] = bytes[r + i];                             \
        r += 8;                                                              \
        w += 8;                                                              \
      }                                                                      \
      while (r < src_len && w < dst_units && bytes[r] < 0x80u) {             \
        STORE32(dst + w * 4, bytes[r]);                                      \
        w++;                                                                 \
        r++;                                                                 \
      }                                                                      \
    } else if (lead < 0xE0u) {                                               \
      do {                                                                   \
        STORE32(dst + w * 4, ((uint32_t)(bytes[r + 0] & 0x1Fu) << 6)         \
                            |  (uint32_t)(bytes[r + 1] & 0x3Fu));            \
        w++;                                                                 \
        r += 2;                                                              \
      } while (r < src_len && w < dst_units &&                               \
               (unsigned)bytes[r] - 0xC0u < 0x20u);                          \
    } else if (lead < 0xF0u) {                                               \
      do {                                                                   \
        STORE32(dst + w * 4, ((uint32_t)(bytes[r + 0] & 0x0Fu) << 12)        \
                            | ((uint32_t)(bytes[r + 1] & 0x3Fu) << 6)        \
                            |  (uint32_t)(bytes[r + 2] & 0x3Fu));            \
        w++;                                                                 \
        r += 3;                                                              \
      } while (r < src_len && w < dst_units &&                               \
               (unsigned)bytes[r] - 0xE0u < 0x10u);                          \
    } else {                                                                 \
      do {                                                                   \
        STORE32(dst + w * 4, ((uint32_t)(bytes[r + 0] & 0x07u) << 18)        \
                            | ((uint32_t)(bytes[r + 1] & 0x3Fu) << 12)       \
                            | ((uint32_t)(bytes[r + 2] & 0x3Fu) << 6)        \
                            |  (uint32_t)(bytes[r + 3] & 0x3Fu));            \
        w++;                                                                 \
        r += 4;                                                              \
      } while (r < src_len && w < dst_units && (unsigned)bytes[r] >= 0xF0u); \
    }                                                                        \
  }                                                                          \
                                                                             \
  utf8_transcode_status_t status = r < src_len ? UTF8_TRANSCODE_EXHAUSTED    \
                                               : UTF8_TRANSCODE_OK;          \
  return (utf8_transcode_result_t){.status   = status,                       \
                                   .consumed = r,                            \
                                   .decoded  = w,                            \
                                   .written  = w * 4,                        \
                                   .advance  = 0};                           \
}

/*
 * utf8_transcode_utf16le_unsafe, utf8_transcode_utf16be_unsafe --
 * transcode well-formed UTF-8 to a UTF-16LE or UTF-16BE byte stream, with
 * no validation. src[0..src_len) MUST already be well-formed UTF-8.
 *
 * dst[0..dst_len) receives bytes in the named byte order; both bounds are
 * byte counts. Codepoints above U+FFFF are encoded as surrogate pairs and
 * consume 4 bytes. Both src and dst are fully caller-owned.
 *
 * Returns a utf8_transcode_result_t (see utf8_transcode_common.h):
 *
 *   status:
 *     UTF8_TRANSCODE_OK         src fully consumed.
 *     UTF8_TRANSCODE_EXHAUSTED  dst full before src was consumed.
 *
 *   consumed:  bytes read from src.
 *   decoded:   codepoints decoded from src.
 *   written:   bytes written to dst (a multiple of 2).
 *   advance:   always 0.
 */
UTF8_DEFINE_TRANSCODE_UTF16_UNSAFE(le, utf8_transcode_store_le16, 0)
UTF8_DEFINE_TRANSCODE_UTF16_UNSAFE(be, utf8_transcode_store_be16, 1)

/*
 * utf8_transcode_utf32le_unsafe, utf8_transcode_utf32be_unsafe --
 * transcode well-formed UTF-8 to a UTF-32LE or UTF-32BE byte stream, with
 * no validation. src[0..src_len) MUST already be well-formed UTF-8.
 *
 * dst[0..dst_len) receives bytes in the named byte order; both bounds are
 * byte counts. Both src and dst are fully caller-owned.
 *
 * Returns a utf8_transcode_result_t (see utf8_transcode_common.h):
 *
 *   status:
 *     UTF8_TRANSCODE_OK         src fully consumed.
 *     UTF8_TRANSCODE_EXHAUSTED  dst full before src was consumed.
 *
 *   consumed:  bytes read from src.
 *   decoded:   codepoints decoded from src.
 *   written:   bytes written to dst (equals decoded * 4).
 *   advance:   always 0.
 */
UTF8_DEFINE_TRANSCODE_UTF32_UNSAFE(le, utf8_transcode_store_le32, 0)
UTF8_DEFINE_TRANSCODE_UTF32_UNSAFE(be, utf8_transcode_store_be32, 3)

#undef UTF8_DEFINE_TRANSCODE_UTF16_UNSAFE
#undef UTF8_DEFINE_TRANSCODE_UTF32_UNSAFE

#ifdef __cplusplus
}
#endif
#endif /* UTF8_TRANSCODE_ENDIAN_UNSAFE_H */
