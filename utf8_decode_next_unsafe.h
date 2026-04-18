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
#ifndef UTF8_DECODE_NEXT_UNSAFE_H
#define UTF8_DECODE_NEXT_UNSAFE_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * utf8_decode_next_unsafe -- decode one codepoint from well-formed
 * UTF-8 at src[0..len).
 *
 * src MUST point to well-formed UTF-8. No validation is performed.
 *
 * Returns bytes consumed (1-4) and writes the codepoint to *codepoint.
 * Returns 0 when len is 0.
 */
static inline int utf8_decode_next_unsafe(const char *src,
                                          size_t len,
                                          uint32_t *codepoint) {
  if (len == 0)
    return 0;

  const unsigned char *s = (const unsigned char *)src;
  unsigned int c = s[0];

  if (c < 0x80u) {
    *codepoint = c;
    return 1;
  }
  else if (c < 0xE0u) {
    *codepoint = ((uint32_t)(c    & 0x1Fu) << 6)
               |  (uint32_t)(s[1] & 0x3Fu);
    return 2;
  }
  else if (c < 0xF0u) {
    *codepoint = ((uint32_t)(c    & 0x0Fu) << 12)
               | ((uint32_t)(s[1] & 0x3Fu) << 6)
               |  (uint32_t)(s[2] & 0x3Fu);
    return 3;
  }
  else {
    *codepoint = ((uint32_t)(c    & 0x07u) << 18)
               | ((uint32_t)(s[1] & 0x3Fu) << 12)
               | ((uint32_t)(s[2] & 0x3Fu) << 6)
               |  (uint32_t)(s[3] & 0x3Fu);
  }
  return 4;
}

#ifdef __cplusplus
}
#endif
#endif /* UTF8_DECODE_NEXT_UNSAFE_H */
