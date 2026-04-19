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
#ifndef UTF8_DISTANCE_H
#define UTF8_DISTANCE_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(UTF8_DFA32_H) && defined(UTF8_DFA64_H)
#  error "utf8_dfa32.h and utf8_dfa64.h are mutually exclusive"
#elif !defined(UTF8_DFA32_H) && !defined(UTF8_DFA64_H)
#  error "include utf8_dfa32.h or utf8_dfa64.h before utf8_distance.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * utf8_distance -- count the number of codepoints in src[0..len).
 *
 * Returns the number of codepoints in src[0..len), or (size_t)-1 if
 * src[0..len) contains ill-formed UTF-8.
 */
static inline size_t utf8_distance(const char *src, size_t len) {
  const uint8_t *bytes = (const uint8_t *)src;
  utf8_dfa_state_t state = UTF8_DFA_ACCEPT;
  size_t count = 0;

  for (;len >= 4; len -= 4, bytes += 4) {
    utf8_dfa_state_t s0 = utf8_dfa_step(state, bytes[0]);
    utf8_dfa_state_t s1 = utf8_dfa_step(s0,    bytes[1]);
    utf8_dfa_state_t s2 = utf8_dfa_step(s1,    bytes[2]);
    utf8_dfa_state_t s3 = utf8_dfa_step(s2,    bytes[3]);
    count += (s0 == UTF8_DFA_ACCEPT) + (s1 == UTF8_DFA_ACCEPT)
           + (s2 == UTF8_DFA_ACCEPT) + (s3 == UTF8_DFA_ACCEPT);
    state = s3;
  }

  for (size_t i = 0; i < len; i++) {
    state = utf8_dfa_step(state, bytes[i]);
    if (state == UTF8_DFA_ACCEPT)
      count++;
  }

  return state == UTF8_DFA_ACCEPT ? count : (size_t)-1;
}

/*
 * utf8_distance_ascii -- count the number of codepoints in src[0..len)
 * with an ASCII fast path.
 *
 * Processes 8 bytes at a time; when all 8 are ASCII and the DFA is in the 
 * accept state, increments count by 8 without entering the DFA.  Falls back 
 * to byte-wise DFA stepping otherwise.
 *
 * Returns the number of codepoints in src[0..len), or (size_t)-1 if
 * src[0..len) contains ill-formed UTF-8.
 */
static inline size_t utf8_distance_ascii(const char *src, size_t len) {
  const uint8_t *bytes = (const uint8_t *)src;
  utf8_dfa_state_t state = UTF8_DFA_ACCEPT;
  size_t count = 0;

  for (; len >= 8; len -= 8, bytes += 8) {
    if (state == UTF8_DFA_ACCEPT) {
      uint64_t v;
      memcpy(&v, bytes, sizeof(v));
      if ((v & UINT64_C(0x8080808080808080)) == 0) {
        count += 8;
        continue;
      }
    }
    {
      utf8_dfa_state_t s0 = utf8_dfa_step(state, bytes[0]);
      utf8_dfa_state_t s1 = utf8_dfa_step(s0,    bytes[1]);
      utf8_dfa_state_t s2 = utf8_dfa_step(s1,    bytes[2]);
      utf8_dfa_state_t s3 = utf8_dfa_step(s2,    bytes[3]);
      utf8_dfa_state_t s4 = utf8_dfa_step(s3,    bytes[4]);
      utf8_dfa_state_t s5 = utf8_dfa_step(s4,    bytes[5]);
      utf8_dfa_state_t s6 = utf8_dfa_step(s5,    bytes[6]);
      utf8_dfa_state_t s7 = utf8_dfa_step(s6,    bytes[7]);
      count += (s0 == UTF8_DFA_ACCEPT) + (s1 == UTF8_DFA_ACCEPT)
             + (s2 == UTF8_DFA_ACCEPT) + (s3 == UTF8_DFA_ACCEPT)
             + (s4 == UTF8_DFA_ACCEPT) + (s5 == UTF8_DFA_ACCEPT)
             + (s6 == UTF8_DFA_ACCEPT) + (s7 == UTF8_DFA_ACCEPT);
      state = s7;
    }
  }

  for (size_t i = 0; i < len; i++) {
    state = utf8_dfa_step(state, bytes[i]);
    if (state == UTF8_DFA_ACCEPT)
      count++;
  }

  return state == UTF8_DFA_ACCEPT ? count : (size_t)-1;
}

#ifdef __cplusplus
}
#endif
#endif /* UTF8_DISTANCE_H */
