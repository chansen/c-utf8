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
#ifndef UTF8_VALIDATOR_H
#define UTF8_VALIDATOR_H
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#if defined(UTF8_DFA32_H) && defined(UTF8_DFA64_H)
#  error "utf8_dfa32.h and utf8_dfa64.h are mutually exclusive"
#elif !defined(UTF8_DFA32_H) && !defined(UTF8_DFA64_H)
#  error "include utf8_dfa32.h or utf8_dfa64.h before utf8_validator.h"
#endif

#include "utf8_scanner.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * utf8_validator_status_t -- outcome of a validation step.
 *
 *   UTF8_VALIDATOR_OK         no errors, data ends on a sequence boundary.
 *   UTF8_VALIDATOR_PARTIAL    src fully consumed, ends in the middle of a
 *                             sequence; the incomplete sequence is buffered.
 *   UTF8_VALIDATOR_ILLFORMED  stopped at an ill-formed sequence.
 *   UTF8_VALIDATOR_TRUNCATED  stream ended in the middle of a sequence
 *                             (utf8_validator_finish only).
 */
typedef enum {
  UTF8_VALIDATOR_OK,
  UTF8_VALIDATOR_PARTIAL,
  UTF8_VALIDATOR_ILLFORMED,
  UTF8_VALIDATOR_TRUNCATED,
} utf8_validator_status_t;

/*
 * utf8_validator_result_t -- result of a validation step.
 *
 *   status:    outcome of the operation (see utf8_validator_status_t).
 *   data:      well-formed bytes ending on a sequence boundary. Points into
 *              src, or into the validator's buffer for a sequence that spans
 *              chunks. Valid until the next call on the validator.
 *   size:      length of data, may be 0.
 *   consumed:  bytes read from src.
 *   pending:   bytes of an incomplete trailing sequence held in the
 *              validator's buffer on PARTIAL, else 0.
 *   advance:   bytes to skip on ILLFORMED, else 0.
 *              Resume at src[consumed + advance].
 *   carried:   bytes from previous chunks that belong to the same subpart
 *              on ILLFORMED or TRUNCATED, else 0.
 *
 * On ILLFORMED or TRUNCATED the bytes of the ill-formed subpart follow the
 * well-formed data:
 *
 *   subpart bytes  = data + size
 *   subpart length = carried + advance
 *
 * and are valid until the next call on the validator.
 */
typedef struct {
  utf8_validator_status_t status;
  const char* data;
  size_t size;
  size_t consumed;
  size_t pending;
  size_t advance;
  size_t carried;
} utf8_validator_result_t;

typedef struct {
  utf8_scanner_t scanner;
  char buffer[4];
  size_t buffered;
} utf8_validator_t;

static inline void
utf8_validator_set_window(utf8_validator_t *v, size_t window) {
  utf8_scanner_set_window(&v->scanner, window);
}

static inline void
utf8_validator_set_ascii(utf8_validator_t *v, bool ascii) {
  utf8_scanner_set_ascii(&v->scanner, ascii);
}

static inline void
utf8_validator_init(utf8_validator_t *v) {
  utf8_scanner_init(&v->scanner);
  v->buffered = 0;
}

/*
 * utf8_validator_feed -- validate the next chunk of a UTF-8 stream.
 *
 * src[0..len) is the next chunk. A sequence that spans chunks is assembled
 * in the validator's buffer, so every result describes one contiguous run
 * of well-formed bytes. Call utf8_validator_finish after the final chunk.
 *
 * A call does not always consume all of src: when the validator holds an
 * incomplete sequence from a previous chunk, the call only reads the bytes
 * that complete it and returns. Call again with the rest of src:
 *
 *   while (len > 0) {
 *     r = utf8_validator_feed(v, src, len);
 *     ...
 *     src += r.consumed + r.advance;
 *     len -= r.consumed + r.advance;
 *   }
 *
 * Returns a utf8_validator_result_t describing the outcome:
 *
 *   status:
 *     UTF8_VALIDATOR_OK         no errors, data ends on a sequence boundary.
 *     UTF8_VALIDATOR_PARTIAL    src fully consumed, ends in the middle of a
 *                               sequence; the incomplete sequence is buffered.
 *     UTF8_VALIDATOR_ILLFORMED  stopped at an ill-formed sequence.
 *
 *   data:      well-formed bytes ending on a sequence boundary. Points into
 *              src, or into the validator's buffer for a sequence that spans
 *              chunks. Valid until the next call on the validator.
 *   size:      length of data, may be 0.
 *   consumed:  bytes read from src.
 *   pending:   bytes of an incomplete trailing sequence held in the
 *              validator's buffer on PARTIAL, else 0.
 *   advance:   bytes to skip on ILLFORMED, else 0.
 *              Resume at src[consumed + advance].
 *   carried:   bytes from previous chunks that belong to the same subpart
 *              on ILLFORMED, else 0.
 *
 * On ILLFORMED:
 *
 *   subpart length = carried + advance
 *   subpart start  = src + consumed - carried   (chunk-relative)
 *   subpart bytes  = data + size
 *
 * and the validator state is reset. When carried is non-zero the subpart
 * began in a previous chunk: consumed + advance may then be 0, and the
 * subpart bytes are in the validator's buffer.
 */
static inline utf8_validator_result_t
utf8_validator_feed(utf8_validator_t* v,
                    const char* src,
                    size_t len) {
  utf8_scanner_result_t r;

  if (v->buffered > 0) {
    // Complete the buffered sequence, one byte at a time.
    size_t carried = v->buffered;
    size_t pos = 0;

    while (pos < len) {
      r = utf8_scanner_feed(&v->scanner, src + pos, 1);
      if (r.status == UTF8_SCANNER_ILLFORMED) {
        // src[pos] is not part of the subpart and is left unread. The
        // buffer holds the whole subpart: carried + pos bytes.
        v->buffered = 0;
        return (utf8_validator_result_t){
          .status   = UTF8_VALIDATOR_ILLFORMED,
          .data     = v->buffer,
          .size     = 0,
          .consumed = 0,
          .pending  = 0,
          .advance  = pos,
          .carried  = carried,
        };
      }

      v->buffer[v->buffered++] = src[pos++];

      if (r.status == UTF8_SCANNER_OK) {
        size_t size = v->buffered;
        v->buffered = 0;
        return (utf8_validator_result_t){
          .status   = UTF8_VALIDATOR_OK,
          .data     = v->buffer,
          .size     = size,
          .consumed = pos,
          .pending  = 0,
          .advance  = 0,
          .carried  = 0,
        };
      }
    }

    return (utf8_validator_result_t){
      .status   = UTF8_VALIDATOR_PARTIAL,
      .data     = v->buffer,
      .size     = 0,
      .consumed = len,
      .pending  = v->buffered,
      .advance  = 0,
      .carried  = 0,
    };
  }

  r = utf8_scanner_feed(&v->scanner, src, len);

  if (r.status == UTF8_SCANNER_PARTIAL) {
    // Keep the incomplete trailing sequence for the next chunk.
    memcpy(v->buffer, src + r.consumed, r.pending);
    v->buffered = r.pending;
    return (utf8_validator_result_t){
      .status   = UTF8_VALIDATOR_PARTIAL,
      .data     = src,
      .size     = r.consumed,
      .consumed = len,
      .pending  = r.pending,
      .advance  = 0,
      .carried  = 0,
    };
  }

  if (r.status == UTF8_SCANNER_ILLFORMED) {
    return (utf8_validator_result_t){
      .status   = UTF8_VALIDATOR_ILLFORMED,
      .data     = src,
      .size     = r.consumed,
      .consumed = r.consumed,
      .pending  = 0,
      .advance  = r.advance,
      .carried  = 0,
    };
  }

  return (utf8_validator_result_t){
    .status   = UTF8_VALIDATOR_OK,
    .data     = src,
    .size     = len,
    .consumed = len,
    .pending  = 0,
    .advance  = 0,
    .carried  = 0,
  };
}

/*
 * utf8_validator_finish -- signal the end of a UTF-8 stream.
 *
 * Call after the final chunk has been passed to utf8_validator_feed.
 *
 * Returns a utf8_validator_result_t describing the outcome:
 *
 *   status:
 *     UTF8_VALIDATOR_OK         stream ended on a sequence boundary.
 *     UTF8_VALIDATOR_TRUNCATED  stream ended in the middle of a sequence.
 *
 *   data:      bytes of the incomplete trailing sequence on TRUNCATED.
 *   size:      always 0.
 *   consumed:  always 0.
 *   pending:   always 0.
 *   advance:   always 0.
 *   carried:   bytes of the incomplete trailing sequence on TRUNCATED, else 0.
 *
 * On TRUNCATED the subpart is the last carried bytes of the stream:
 *
 *   subpart length = carried
 *   subpart start  = end of stream - carried
 *   subpart bytes  = data
 *
 * and the validator state is reset.
 */
static inline utf8_validator_result_t
utf8_validator_finish(utf8_validator_t* v) {
  utf8_scanner_result_t r = utf8_scanner_finish(&v->scanner);

  v->buffered = 0;

  return (utf8_validator_result_t){
    .status   = r.status == UTF8_SCANNER_OK ? UTF8_VALIDATOR_OK
                                            : UTF8_VALIDATOR_TRUNCATED,
    .data     = v->buffer,
    .size     = 0,
    .consumed = 0,
    .pending  = 0,
    .advance  = 0,
    .carried  = r.carried,
  };
}

#ifdef __cplusplus
}
#endif
#endif
