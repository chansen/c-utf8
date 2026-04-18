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
 *   written:   code units written to dst.
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

#endif /* UTF8_TRANSCODE_COMMON_H */
