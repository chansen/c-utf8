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
#ifndef UTF8_TRANSCODE_COMMON_H
#define UTF8_TRANSCODE_COMMON_H
#include <stddef.h>
#include <stdint.h>

/*
 * utf8_transcode_status_t -- outcome of a transcoding operation.
 *
 *   UTF8_TRANSCODE_OK         src fully consumed, no errors.
 *   UTF8_TRANSCODE_EXHAUSTED  dst full before src was consumed.
 *   UTF8_TRANSCODE_ILLFORMED  stopped at an ill-formed sequence.
 *   UTF8_TRANSCODE_TRUNCATED  src ends in the middle of a sequence.
 */
typedef enum {
  UTF8_TRANSCODE_OK,
  UTF8_TRANSCODE_EXHAUSTED,
  UTF8_TRANSCODE_ILLFORMED,
  UTF8_TRANSCODE_TRUNCATED,
} utf8_transcode_status_t;

/*
 * utf8_transcode_result_t -- result of a transcoding operation.
 *
 *   status:    outcome of the operation (see utf8_transcode_status_t).
 *   consumed:  bytes read from src.
 *   decoded:   codepoints decoded from src.
 *   written:   code units written to dst; bytes for the explicit byte
 *              order variants (utf8_transcode_endian.h).
 *   advance:   bytes to skip past the ill-formed sequence on ILLFORMED or
 *              TRUNCATED, else 0. Resume at src[consumed+advance].
 */
typedef struct {
  utf8_transcode_status_t status;
  size_t consumed;
  size_t decoded;
  size_t written;
  size_t advance;
} utf8_transcode_result_t;

/*
 * Byte order stores used by utf8_transcode_endian.h and
 * utf8_transcode_endian_unsafe.h. Written as shifts, so they are correct on
 * any host and need no alignment.
 */
static inline void utf8_transcode_store_be16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)(v >> 8);
  p[1] = (uint8_t)(v);
}

static inline void utf8_transcode_store_le16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)(v);
  p[1] = (uint8_t)(v >> 8);
}

static inline void utf8_transcode_store_be32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)(v);
}

static inline void utf8_transcode_store_le32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v);
  p[1] = (uint8_t)(v >> 8);
  p[2] = (uint8_t)(v >> 16);
  p[3] = (uint8_t)(v >> 24);
}

#endif /* UTF8_TRANSCODE_COMMON_H */
