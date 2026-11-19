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
#ifndef UTF8_TRANSCODE_ENDIAN_H
#define UTF8_TRANSCODE_ENDIAN_H
#include <stddef.h>
#include <stdint.h>

#include "utf8_transcode_common.h"

#ifndef UTF8_DFA64_H
#  error "include utf8_dfa64.h before utf8_transcode_endian.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Transcode UTF-8 to UTF-16LE, UTF-16BE, UTF-32LE or UTF-32BE.
 *
 * utf8_transcode.h writes native uint16_t/uint32_t code units. The functions
 * here write bytes in an explicit byte order, independent of the host. dst
 * is a byte buffer that needs no alignment; dst_len and written are byte
 * counts. No byte order mark is written.
 *
 * Each function body is a copy of its counterpart in utf8_transcode.h in
 * which only the dst bound check and the store differ; keep the two in
 * sync. The bodies are defined once as macros and instantiated per byte
 * order. The macros are not part of the public interface and are undefined
 * below.
 */

#define UTF8_DEFINE_TRANSCODE_UTF16(SUF, STORE16)                            \
static inline utf8_transcode_result_t                                        \
utf8_transcode_utf16##SUF(const char *src,                                   \
                          size_t src_len,                                    \
                          uint8_t *dst,                                      \
                          size_t dst_len) {                                  \
  const uint8_t *bytes = (const uint8_t *)src;                               \
  size_t pos = 0;                                                            \
  size_t decoded = 0;                                                        \
  size_t written = 0;                                                        \
                                                                             \
  while (pos < src_len && written + 2 <= dst_len) {                          \
    utf8_dfa_state_t state = UTF8_DFA_ACCEPT;                                \
    uint32_t cp = 0;                                                         \
    size_t start = pos;                                                      \
                                                                             \
    do {                                                                     \
      state = utf8_dfa_step_decode(state, bytes[pos++], &cp);                \
      if (state == UTF8_DFA_ACCEPT) {                                        \
        if (cp <= 0xFFFFu) {                                                 \
          STORE16(dst + written, (uint16_t)cp);                              \
          written += 2;                                                      \
        } else {                                                             \
          /* Surrogate pair: need two code units (4 bytes) */                \
          if (written + 4 > dst_len) {                                       \
            pos = start;                                                     \
            goto exhausted;                                                  \
          }                                                                  \
          cp -= 0x10000u;                                                    \
          STORE16(dst + written, (uint16_t)(0xD800u + (cp >> 10)));          \
          written += 2;                                                      \
          STORE16(dst + written, (uint16_t)(0xDC00u + (cp & 0x3FFu)));       \
          written += 2;                                                      \
        }                                                                    \
        decoded++;                                                           \
        goto next;                                                           \
      }                                                                      \
      if (state == UTF8_DFA_REJECT) {                                        \
        size_t advance = pos > start + 1 ? pos - start - 1 : 1;              \
        return (utf8_transcode_result_t){.status = UTF8_TRANSCODE_ILLFORMED, \
                                         .consumed = start,                  \
                                         .decoded = decoded,                 \
                                         .written = written,                 \
                                         .advance = advance};                \
      }                                                                      \
    } while (pos < src_len);                                                 \
                                                                             \
    return (utf8_transcode_result_t){.status = UTF8_TRANSCODE_TRUNCATED,     \
                                     .consumed = start,                      \
                                     .decoded = decoded,                     \
                                     .written = written,                     \
                                     .advance = pos - start};                \
  next:;                                                                     \
  }                                                                          \
                                                                             \
exhausted:;                                                                  \
  utf8_transcode_status_t status =                                           \
      pos < src_len ? UTF8_TRANSCODE_EXHAUSTED : UTF8_TRANSCODE_OK;          \
  return (utf8_transcode_result_t){.status = status,                         \
                                   .consumed = pos,                          \
                                   .decoded = decoded,                       \
                                   .written = written,                       \
                                   .advance = 0};                            \
}

#define UTF8_DEFINE_TRANSCODE_UTF16_REPLACE(SUF, STORE16)                    \
static inline utf8_transcode_result_t                                        \
utf8_transcode_utf16##SUF##_replace(const char *src,                         \
                                    size_t src_len,                          \
                                    uint8_t *dst,                            \
                                    size_t dst_len) {                        \
  const uint8_t *bytes = (const uint8_t *)src;                               \
  size_t pos = 0;                                                            \
  size_t decoded = 0;                                                        \
  size_t written = 0;                                                        \
                                                                             \
  while (pos < src_len && written + 2 <= dst_len) {                          \
    utf8_dfa_state_t state = UTF8_DFA_ACCEPT;                                \
    uint32_t cp = 0;                                                         \
    size_t start = pos;                                                      \
                                                                             \
    do {                                                                     \
      state = utf8_dfa_step_decode(state, bytes[pos++], &cp);                \
      if (state == UTF8_DFA_ACCEPT) {                                        \
        if (cp <= 0xFFFFu) {                                                 \
          STORE16(dst + written, (uint16_t)cp);                              \
          written += 2;                                                      \
        } else {                                                             \
          /* Surrogate pair: need two code units (4 bytes) */                \
          if (written + 4 > dst_len) {                                       \
            pos = start;                                                     \
            goto exhausted;                                                  \
          }                                                                  \
          cp -= 0x10000u;                                                    \
          STORE16(dst + written, (uint16_t)(0xD800u + (cp >> 10)));          \
          written += 2;                                                      \
          STORE16(dst + written, (uint16_t)(0xDC00u + (cp & 0x3FFu)));       \
          written += 2;                                                      \
        }                                                                    \
        decoded++;                                                           \
        goto next;                                                           \
      }                                                                      \
      if (state == UTF8_DFA_REJECT) {                                        \
        /* Back up: the rejecting byte belongs to the next sequence */       \
        if (pos > start + 1)                                                 \
          pos--;                                                             \
        STORE16(dst + written, (uint16_t)0xFFFDu);                           \
        written += 2;                                                        \
        decoded++;                                                           \
        goto next;                                                           \
      }                                                                      \
    } while (pos < src_len);                                                 \
                                                                             \
    /* Truncated sequence at end of src: emit one U+FFFD */                  \
    STORE16(dst + written, (uint16_t)0xFFFDu);                               \
    written += 2;                                                            \
    decoded++;                                                               \
    break;                                                                   \
  next:;                                                                     \
  }                                                                          \
                                                                             \
exhausted:;                                                                  \
  utf8_transcode_status_t status =                                           \
      pos < src_len ? UTF8_TRANSCODE_EXHAUSTED : UTF8_TRANSCODE_OK;          \
  return (utf8_transcode_result_t){.status = status,                         \
                                   .consumed = pos,                          \
                                   .decoded = decoded,                       \
                                   .written = written,                       \
                                   .advance = 0};                            \
}

#define UTF8_DEFINE_TRANSCODE_UTF32(SUF, STORE32)                            \
static inline utf8_transcode_result_t                                        \
utf8_transcode_utf32##SUF(const char *src,                                   \
                          size_t src_len,                                    \
                          uint8_t *dst,                                      \
                          size_t dst_len) {                                  \
  const uint8_t *bytes = (const uint8_t *)src;                               \
  size_t pos = 0;                                                            \
  size_t decoded = 0;                                                        \
  size_t dst_units = dst_len / 4;                                            \
                                                                             \
  while (pos < src_len && decoded < dst_units) {                             \
    utf8_dfa_state_t state = UTF8_DFA_ACCEPT;                                \
    uint32_t cp = 0;                                                         \
    size_t start = pos;                                                      \
                                                                             \
    do {                                                                     \
      state = utf8_dfa_step_decode(state, bytes[pos++], &cp);                \
      if (state == UTF8_DFA_ACCEPT) {                                        \
        STORE32(dst + decoded * 4, cp);                                      \
        decoded++;                                                           \
        goto next;                                                           \
      }                                                                      \
      if (state == UTF8_DFA_REJECT) {                                        \
        size_t advance = pos > start + 1 ? pos - start - 1 : 1;              \
        return (utf8_transcode_result_t){.status = UTF8_TRANSCODE_ILLFORMED, \
                                         .consumed = start,                  \
                                         .decoded = decoded,                 \
                                         .written = decoded * 4,             \
                                         .advance = advance};                \
      }                                                                      \
    } while (pos < src_len);                                                 \
                                                                             \
    return (utf8_transcode_result_t){.status = UTF8_TRANSCODE_TRUNCATED,     \
                                     .consumed = start,                      \
                                     .decoded = decoded,                     \
                                     .written = decoded * 4,                 \
                                     .advance = pos - start};                \
  next:;                                                                     \
  }                                                                          \
                                                                             \
  utf8_transcode_status_t status =                                           \
      pos < src_len ? UTF8_TRANSCODE_EXHAUSTED : UTF8_TRANSCODE_OK;          \
  return (utf8_transcode_result_t){.status = status,                         \
                                   .consumed = pos,                          \
                                   .decoded = decoded,                       \
                                   .written = decoded * 4,                   \
                                   .advance = 0};                            \
}

#define UTF8_DEFINE_TRANSCODE_UTF32_REPLACE(SUF, STORE32)                    \
static inline utf8_transcode_result_t                                        \
utf8_transcode_utf32##SUF##_replace(const char *src,                         \
                                    size_t src_len,                          \
                                    uint8_t *dst,                            \
                                    size_t dst_len) {                        \
  const uint8_t *bytes = (const uint8_t *)src;                               \
  size_t pos = 0;                                                            \
  size_t decoded = 0;                                                        \
  size_t dst_units = dst_len / 4;                                            \
                                                                             \
  while (pos < src_len && decoded < dst_units) {                             \
    utf8_dfa_state_t state = UTF8_DFA_ACCEPT;                                \
    uint32_t cp = 0;                                                         \
    size_t start = pos;                                                      \
                                                                             \
    do {                                                                     \
      state = utf8_dfa_step_decode(state, bytes[pos++], &cp);                \
      if (state == UTF8_DFA_ACCEPT) {                                        \
        STORE32(dst + decoded * 4, cp);                                      \
        decoded++;                                                           \
        goto next;                                                           \
      }                                                                      \
      if (state == UTF8_DFA_REJECT) {                                        \
        /* Back up: the rejecting byte belongs to the next sequence */       \
        if (pos > start + 1)                                                 \
          pos--;                                                             \
        STORE32(dst + decoded * 4, 0xFFFDu);                                 \
        decoded++;                                                           \
        goto next;                                                           \
      }                                                                      \
    } while (pos < src_len);                                                 \
                                                                             \
    /* Truncated sequence at end of src: emit one U+FFFD */                  \
    STORE32(dst + decoded * 4, 0xFFFDu);                                     \
    decoded++;                                                               \
    break;                                                                   \
  next:;                                                                     \
  }                                                                          \
                                                                             \
  utf8_transcode_status_t status =                                           \
      pos < src_len ? UTF8_TRANSCODE_EXHAUSTED : UTF8_TRANSCODE_OK;          \
  return (utf8_transcode_result_t){.status = status,                         \
                                   .consumed = pos,                          \
                                   .decoded = decoded,                       \
                                   .written = decoded * 4,                   \
                                   .advance = 0};                            \
}

/*
 * utf8_transcode_utf16le, utf8_transcode_utf16be -- transcode UTF-8 to a
 * UTF-16LE or UTF-16BE byte stream.
 *
 * src[0..src_len) is the input buffer. dst[0..dst_len) receives bytes in
 * the named byte order; both bounds are byte counts. Codepoints above
 * U+FFFF are encoded as surrogate pairs and consume 4 bytes. Both src and
 * dst are fully caller-owned.
 *
 * Returns a utf8_transcode_result_t (see utf8_transcode_common.h):
 *
 *   status:
 *     UTF8_TRANSCODE_OK         src fully consumed, no errors.
 *     UTF8_TRANSCODE_EXHAUSTED  dst full before src was consumed.
 *     UTF8_TRANSCODE_ILLFORMED  stopped at an ill-formed sequence.
 *     UTF8_TRANSCODE_TRUNCATED  src ends in the middle of a sequence.
 *
 *   consumed:  bytes read from src.
 *   decoded:   codepoints decoded from src.
 *   written:   bytes written to dst (a multiple of 2).
 *   advance:   bytes to skip past the ill-formed sequence (ILLFORMED or
 *              TRUNCATED, else 0). Resume at src[consumed+advance].
 *
 * utf8_transcode_utf16le_replace() / utf8_transcode_utf16be_replace() are
 * identical but replace each ill-formed sequence with U+FFFD and continue.
 * Status is always OK or EXHAUSTED; advance is always 0.
 */
UTF8_DEFINE_TRANSCODE_UTF16(le, utf8_transcode_store_le16)
UTF8_DEFINE_TRANSCODE_UTF16(be, utf8_transcode_store_be16)
UTF8_DEFINE_TRANSCODE_UTF16_REPLACE(le, utf8_transcode_store_le16)
UTF8_DEFINE_TRANSCODE_UTF16_REPLACE(be, utf8_transcode_store_be16)

/*
 * utf8_transcode_utf32le, utf8_transcode_utf32be -- transcode UTF-8 to a
 * UTF-32LE or UTF-32BE byte stream.
 *
 * src[0..src_len) is the input buffer. dst[0..dst_len) receives bytes in
 * the named byte order; both bounds are byte counts. Both src and dst are
 * fully caller-owned.
 *
 * Returns a utf8_transcode_result_t (see utf8_transcode_common.h):
 *
 *   status:
 *     UTF8_TRANSCODE_OK         src fully consumed, no errors.
 *     UTF8_TRANSCODE_EXHAUSTED  dst full before src was consumed.
 *     UTF8_TRANSCODE_ILLFORMED  stopped at an ill-formed sequence.
 *     UTF8_TRANSCODE_TRUNCATED  src ends in the middle of a sequence.
 *
 *   consumed:  bytes read from src.
 *   decoded:   codepoints decoded from src.
 *   written:   bytes written to dst (equals decoded * 4).
 *   advance:   bytes to skip past the ill-formed sequence (ILLFORMED or
 *              TRUNCATED, else 0). Resume at src[consumed+advance].
 *
 * utf8_transcode_utf32le_replace() / utf8_transcode_utf32be_replace() are
 * identical but replace each ill-formed sequence with U+FFFD and continue.
 * Status is always OK or EXHAUSTED; advance is always 0.
 */
UTF8_DEFINE_TRANSCODE_UTF32(le, utf8_transcode_store_le32)
UTF8_DEFINE_TRANSCODE_UTF32(be, utf8_transcode_store_be32)
UTF8_DEFINE_TRANSCODE_UTF32_REPLACE(le, utf8_transcode_store_le32)
UTF8_DEFINE_TRANSCODE_UTF32_REPLACE(be, utf8_transcode_store_be32)

#undef UTF8_DEFINE_TRANSCODE_UTF16
#undef UTF8_DEFINE_TRANSCODE_UTF16_REPLACE
#undef UTF8_DEFINE_TRANSCODE_UTF32
#undef UTF8_DEFINE_TRANSCODE_UTF32_REPLACE

#ifdef __cplusplus
}
#endif
#endif /* UTF8_TRANSCODE_ENDIAN_H */
