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
#ifndef UTF8_ADVANCE_BACKWARD_H
#define UTF8_ADVANCE_BACKWARD_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(UTF8_RDFA32_H) && defined(UTF8_RDFA64_H)
#  error "utf8_rdfa32.h and utf8_rdfa64.h are mutually exclusive"
#elif !defined(UTF8_RDFA32_H) && !defined(UTF8_RDFA64_H)
#  error "include utf8_rdfa32.h or utf8_rdfa64.h before utf8_advance_backward.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * utf8_advance_backward -- advance backward by distance codepoints.
 *
 * Returns the byte offset of the start of the codepoint distance positions
 * before the end of src[0..len), or 0 if distance exceeds the number of
 * codepoints in the buffer. Returns (size_t)-1 if src[0..len) contains
 * ill-formed UTF-8. If advanced is non-NULL, writes the number of codepoints
 * actually advanced before stopping.
 */
static inline size_t utf8_advance_backward(const char *src,
                                           size_t len,
                                           size_t distance,
                                           size_t *advanced) {
  const unsigned char *s = (const unsigned char *)src;
  utf8_rdfa_state_t state = UTF8_RDFA_ACCEPT;
  size_t pos = len;
  size_t count = 0;

  while (distance - count >= 4 && pos >= 4) {
    utf8_rdfa_state_t s0 = utf8_rdfa_step(state, s[pos - 1]);
    utf8_rdfa_state_t s1 = utf8_rdfa_step(s0,    s[pos - 2]);
    utf8_rdfa_state_t s2 = utf8_rdfa_step(s1,    s[pos - 3]);
    utf8_rdfa_state_t s3 = utf8_rdfa_step(s2,    s[pos - 4]);
    count += (s0 == UTF8_RDFA_ACCEPT) + (s1 == UTF8_RDFA_ACCEPT)
           + (s2 == UTF8_RDFA_ACCEPT) + (s3 == UTF8_RDFA_ACCEPT);
    state = s3;
    pos -= 4;
  }

  while (pos > 0 && count < distance) {
    state = utf8_rdfa_step(state, s[--pos]);
    if (state == UTF8_RDFA_ACCEPT)
      count++;
  }

  if (advanced)
    *advanced = count;
  return state == UTF8_RDFA_ACCEPT ? pos : (size_t)-1;
}

/*
 * utf8_advance_backward_ascii -- advance backward by distance codepoints
 * with an ASCII fast path.
 *
 * Processes 8 bytes at a time; when all 8 are ASCII and the reverse DFA is 
 * in the accept state, increments count by 8 without entering the DFA.  
 * Falls back to byte-wise DFA stepping otherwise.
 *
 * Returns the byte offset of the start of the codepoint distance positions
 * before the end of src[0..len), or 0 if distance exceeds the number of
 * codepoints in the buffer. Returns (size_t)-1 if src[0..len) contains
 * ill-formed UTF-8. If advanced is non-NULL, writes the number of codepoints
 * actually advanced before stopping.
 */
static inline size_t utf8_advance_backward_ascii(const char *src,
                                                 size_t len,
                                                 size_t distance,
                                                 size_t *advanced) {
  const unsigned char *s = (const unsigned char *)src;
  utf8_rdfa_state_t state = UTF8_RDFA_ACCEPT;
  size_t pos = len;
  size_t count = 0;

  while (distance - count >= 8 && pos >= 8) {
    if (state == UTF8_RDFA_ACCEPT) {
      uint64_t v;
      memcpy(&v, s + pos - 8, sizeof(v));
      if ((v & UINT64_C(0x8080808080808080)) == 0) {
        count += 8;
        pos -= 8;
        continue;
      }
    }
    {
      utf8_rdfa_state_t s0 = utf8_rdfa_step(state, s[pos - 1]);
      utf8_rdfa_state_t s1 = utf8_rdfa_step(s0,    s[pos - 2]);
      utf8_rdfa_state_t s2 = utf8_rdfa_step(s1,    s[pos - 3]);
      utf8_rdfa_state_t s3 = utf8_rdfa_step(s2,    s[pos - 4]);
      utf8_rdfa_state_t s4 = utf8_rdfa_step(s3,    s[pos - 5]);
      utf8_rdfa_state_t s5 = utf8_rdfa_step(s4,    s[pos - 6]);
      utf8_rdfa_state_t s6 = utf8_rdfa_step(s5,    s[pos - 7]);
      utf8_rdfa_state_t s7 = utf8_rdfa_step(s6,    s[pos - 8]);
      count += (s0 == UTF8_RDFA_ACCEPT) + (s1 == UTF8_RDFA_ACCEPT)
             + (s2 == UTF8_RDFA_ACCEPT) + (s3 == UTF8_RDFA_ACCEPT)
             + (s4 == UTF8_RDFA_ACCEPT) + (s5 == UTF8_RDFA_ACCEPT)
             + (s6 == UTF8_RDFA_ACCEPT) + (s7 == UTF8_RDFA_ACCEPT);
      state = s7;
    }
    pos -= 8;
  }

  while (pos > 0 && count < distance) {
    state = utf8_rdfa_step(state, s[--pos]);
    if (state == UTF8_RDFA_ACCEPT)
      count++;
  }

  if (advanced)
    *advanced = count;
  return state == UTF8_RDFA_ACCEPT ? pos : (size_t)-1;
}

#ifdef __cplusplus
}
#endif
#endif /* UTF8_ADVANCE_BACKWARD_H */
