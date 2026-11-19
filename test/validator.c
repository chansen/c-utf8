#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef UTF8_DFA_64
#  include "utf8_dfa64.h"
#else
#  include "utf8_dfa32.h"
#endif

#include "utf8_valid.h"
#include "utf8_validator.h"
#include "test.h"
#include "test_util.h"

/* Where the data of a result is expected to live. */
typedef enum {
  IN_SRC,   /* data == src */
  IN_BUF,   /* data == validator buffer */
} where_t;

static void
result_check(const char* func,
             const char* desc,
             const utf8_validator_t* v,
             const char* src,
             utf8_validator_status_t exp_status,
             const char* exp_data,
             size_t exp_size,
             where_t exp_where,
             size_t exp_consumed,
             size_t exp_pending,
             size_t exp_advance,
             size_t exp_carried,
             utf8_validator_result_t got,
             unsigned line) {
  TestCount++;
  if (got.status != exp_status) {
    printf("%s [%s] status == %d, expected %d at line %u\n",
           func, desc, (int)got.status, (int)exp_status, line);
    TestFailed++;
  }

  TestCount++;
  if (got.size != exp_size) {
    printf("%s [%s] size == %zu, expected %zu at line %u\n",
           func, desc, got.size, exp_size, line);
    TestFailed++;
  }
  else if (exp_size > 0 && memcmp(got.data, exp_data, exp_size) != 0) {
    char got_esc[4 * 64 + 1], exp_esc[4 * 64 + 1];
    printf("%s [%s] data == \"%s\", expected \"%s\" at line %u\n",
           func, desc,
           escape_str(got.data, got.size, got_esc),
           escape_str(exp_data, exp_size, exp_esc), line);
    TestFailed++;
  }

  TestCount++;
  if ((exp_where == IN_SRC && got.data != src) ||
      (exp_where == IN_BUF && got.data != v->buffer)) {
    printf("%s [%s] data is not in the %s at line %u\n",
           func, desc, exp_where == IN_SRC ? "source" : "buffer", line);
    TestFailed++;
  }

  TestCount++;
  if (got.consumed != exp_consumed) {
    printf("%s [%s] consumed == %zu, expected %zu at line %u\n",
           func, desc, got.consumed, exp_consumed, line);
    TestFailed++;
  }

  TestCount++;
  if (got.pending != exp_pending) {
    printf("%s [%s] pending == %zu, expected %zu at line %u\n",
           func, desc, got.pending, exp_pending, line);
    TestFailed++;
  }

  TestCount++;
  if (got.advance != exp_advance) {
    printf("%s [%s] advance == %zu, expected %zu at line %u\n",
           func, desc, got.advance, exp_advance, line);
    TestFailed++;
  }

  TestCount++;
  if (got.carried != exp_carried) {
    printf("%s [%s] carried == %zu, expected %zu at line %u\n",
           func, desc, got.carried, exp_carried, line);
    TestFailed++;
  }
}

/* Result of the most recent FEED or FINISH, inspected by SUBPART. */
static utf8_validator_result_t Last;

/* The ill-formed subpart follows the well-formed data: data + size. */
static void
subpart_check(const char* desc,
              const char* exp_subpart,
              size_t exp_len,
              unsigned line) {
  size_t got_len = Last.carried + Last.advance;

  TestCount++;
  if (got_len != exp_len) {
    printf("subpart [%s] length == %zu, expected %zu at line %u\n",
           desc, got_len, exp_len, line);
    TestFailed++;
  }
  else if (memcmp(Last.data + Last.size, exp_subpart, exp_len) != 0) {
    char got_esc[4 * 4 + 1], exp_esc[4 * 4 + 1];
    printf("subpart [%s] == \"%s\", expected \"%s\" at line %u\n",
           desc,
           escape_str(Last.data + Last.size, got_len, got_esc),
           escape_str(exp_subpart, exp_len, exp_esc), line);
    TestFailed++;
  }
}

#define SUBPART(desc, exp_subpart, exp_len) \
  subpart_check(desc, exp_subpart, exp_len, __LINE__)

#define FEED(desc, v, src, len, exp_status, exp_data, exp_size, exp_where, exp_consumed, exp_pending, exp_advance, exp_carried) do { \
  const char* _src = (src); \
  utf8_validator_result_t _r = utf8_validator_feed(v, _src, len); \
  Last = _r; \
  result_check("utf8_validator_feed", desc, v, _src, exp_status, exp_data, exp_size, exp_where, \
               exp_consumed, exp_pending, exp_advance, exp_carried, _r, __LINE__); \
} while (0)

#define FINISH(desc, v, exp_status, exp_carried) do { \
  utf8_validator_result_t _r = utf8_validator_finish(v); \
  Last = _r; \
  result_check("utf8_validator_finish", desc, v, NULL, exp_status, "", 0, IN_BUF, \
               0, 0, 0, exp_carried, _r, __LINE__); \
} while (0)

static void
test_feed(void) {
  utf8_validator_t v;

  utf8_validator_init(&v);
  FINISH("finish without input", &v, UTF8_VALIDATOR_OK, 0);

  utf8_validator_init(&v);
  FEED("clean ASCII chunk 1", &v, "hello", 5,
       UTF8_VALIDATOR_OK, "hello", 5, IN_SRC, 5, 0, 0, 0);
  FEED("clean ASCII chunk 2", &v, "world", 5,
       UTF8_VALIDATOR_OK, "world", 5, IN_SRC, 5, 0, 0, 0);
  FINISH("clean ASCII, finish", &v, UTF8_VALIDATOR_OK, 0);

  utf8_validator_init(&v);
  FEED("multi-seq chunk", &v, "\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80", 9,
       UTF8_VALIDATOR_OK, "\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80", 9, IN_SRC, 9, 0, 0, 0);
  FINISH("multi-seq chunk, finish", &v, UTF8_VALIDATOR_OK, 0);

  utf8_validator_init(&v);
  FEED("empty chunk", &v, "", 0,
       UTF8_VALIDATOR_OK, "", 0, IN_SRC, 0, 0, 0, 0);
  FINISH("empty chunk, finish", &v, UTF8_VALIDATOR_OK, 0);

  /* A sequence that spans chunks is returned from the buffer. */
  utf8_validator_init(&v);
  FEED("2-byte split 1/2", &v, "ab\xC3", 3,
       UTF8_VALIDATOR_PARTIAL, "ab", 2, IN_SRC, 3, 1, 0, 0);
  FEED("2-byte split 2/2", &v, "\xA9""cd", 3,
       UTF8_VALIDATOR_OK, "\xC3\xA9", 2, IN_BUF, 1, 0, 0, 0);
  FEED("2-byte split, rest of chunk", &v, "cd", 2,
       UTF8_VALIDATOR_OK, "cd", 2, IN_SRC, 2, 0, 0, 0);
  FINISH("2-byte split, finish", &v, UTF8_VALIDATOR_OK, 0);

  utf8_validator_init(&v);
  FEED("lone lead", &v, "\xC3", 1,
       UTF8_VALIDATOR_PARTIAL, "", 0, IN_SRC, 1, 1, 0, 0);
  FEED("lone lead completed at chunk end", &v, "\xA9", 1,
       UTF8_VALIDATOR_OK, "\xC3\xA9", 2, IN_BUF, 1, 0, 0, 0);
  FINISH("lone lead, finish", &v, UTF8_VALIDATOR_OK, 0);

  utf8_validator_init(&v);
  FEED("3-byte split 1/3", &v, "\xE2", 1,
       UTF8_VALIDATOR_PARTIAL, "", 0, IN_SRC, 1, 1, 0, 0);
  FEED("3-byte split 2/3", &v, "\x82", 1,
       UTF8_VALIDATOR_PARTIAL, "", 0, IN_BUF, 1, 2, 0, 0);
  FEED("3-byte split 3/3", &v, "\xAC", 1,
       UTF8_VALIDATOR_OK, "\xE2\x82\xAC", 3, IN_BUF, 1, 0, 0, 0);
  FINISH("3-byte split, finish", &v, UTF8_VALIDATOR_OK, 0);

  utf8_validator_init(&v);
  FEED("4-byte split 1/4", &v, "\xF0", 1,
       UTF8_VALIDATOR_PARTIAL, "", 0, IN_SRC, 1, 1, 0, 0);
  FEED("4-byte split 2/4", &v, "\x9F", 1,
       UTF8_VALIDATOR_PARTIAL, "", 0, IN_BUF, 1, 2, 0, 0);
  FEED("4-byte split 3/4", &v, "\x98", 1,
       UTF8_VALIDATOR_PARTIAL, "", 0, IN_BUF, 1, 3, 0, 0);
  FEED("4-byte split 4/4", &v, "\x80", 1,
       UTF8_VALIDATOR_OK, "\xF0\x9F\x98\x80", 4, IN_BUF, 1, 0, 0, 0);
  FINISH("4-byte split, finish", &v, UTF8_VALIDATOR_OK, 0);

  utf8_validator_init(&v);
  FEED("4-byte split 2+2 1/2", &v, "\xF0\x9F", 2,
       UTF8_VALIDATOR_PARTIAL, "", 0, IN_SRC, 2, 2, 0, 0);
  FEED("4-byte split 2+2 2/2", &v, "\x98\x80!", 3,
       UTF8_VALIDATOR_OK, "\xF0\x9F\x98\x80", 4, IN_BUF, 2, 0, 0, 0);
  FEED("4-byte split 2+2, rest of chunk", &v, "!", 1,
       UTF8_VALIDATOR_OK, "!", 1, IN_SRC, 1, 0, 0, 0);

  /* The buffer is reused for the next incomplete sequence. */
  utf8_validator_init(&v);
  FEED("back-to-back split 1", &v, "\xC3", 1,
       UTF8_VALIDATOR_PARTIAL, "", 0, IN_SRC, 1, 1, 0, 0);
  FEED("back-to-back split 2", &v, "\xA9\xE2", 2,
       UTF8_VALIDATOR_OK, "\xC3\xA9", 2, IN_BUF, 1, 0, 0, 0);
  FEED("back-to-back split 3", &v, "\xE2", 1,
       UTF8_VALIDATOR_PARTIAL, "", 0, IN_SRC, 1, 1, 0, 0);
  FEED("back-to-back split 4", &v, "\x82\xAC", 2,
       UTF8_VALIDATOR_OK, "\xE2\x82\xAC", 3, IN_BUF, 2, 0, 0, 0);
  FINISH("back-to-back split, finish", &v, UTF8_VALIDATOR_OK, 0);

  utf8_validator_init(&v);
  FEED("pending lead", &v, "\xC3", 1,
       UTF8_VALIDATOR_PARTIAL, "", 0, IN_SRC, 1, 1, 0, 0);
  FEED("empty chunk while pending", &v, "", 0,
       UTF8_VALIDATOR_PARTIAL, "", 0, IN_BUF, 0, 1, 0, 0);
  FEED("completed after empty chunk", &v, "\xA9", 1,
       UTF8_VALIDATOR_OK, "\xC3\xA9", 2, IN_BUF, 1, 0, 0, 0);

  utf8_validator_init(&v);
  FEED("block then partial lead", &v, "abcdefghijklmnop\xC3", 17,
       UTF8_VALIDATOR_PARTIAL, "abcdefghijklmnop", 16, IN_SRC, 17, 1, 0, 0);
  FEED("complete lead after block", &v, "\xA9", 1,
       UTF8_VALIDATOR_OK, "\xC3\xA9", 2, IN_BUF, 1, 0, 0, 0);

  /* Ill-formed sequences within a chunk. */
  utf8_validator_init(&v);
  FEED("invalid mid-chunk", &v, "ab\x80""cd", 5,
       UTF8_VALIDATOR_ILLFORMED, "ab", 2, IN_SRC, 2, 0, 1, 0);
  SUBPART("invalid mid-chunk", "\x80", 1);
  FEED("resume after invalid", &v, "cd", 2,
       UTF8_VALIDATOR_OK, "cd", 2, IN_SRC, 2, 0, 0, 0);
  FINISH("resume after invalid, finish", &v, UTF8_VALIDATOR_OK, 0);

  utf8_validator_init(&v);
  FEED("invalid at start", &v, "\x80""hello", 6,
       UTF8_VALIDATOR_ILLFORMED, "", 0, IN_SRC, 0, 0, 1, 0);
  SUBPART("invalid at start", "\x80", 1);

  utf8_validator_init(&v);
  FEED("invalid at end", &v, "hello\x80", 6,
       UTF8_VALIDATOR_ILLFORMED, "hello", 5, IN_SRC, 5, 0, 1, 0);
  SUBPART("invalid at end", "\x80", 1);

  utf8_validator_init(&v);
  FEED("truncated 3-byte mid-chunk", &v, "a\xE2\x82""b", 4,
       UTF8_VALIDATOR_ILLFORMED, "a", 1, IN_SRC, 1, 0, 2, 0);
  SUBPART("truncated 3-byte mid-chunk", "\xE2\x82", 2);

  /* Ill-formed sequences that began in a previous chunk. */
  utf8_validator_init(&v);
  FEED("carried lead", &v, "\xC3", 1,
       UTF8_VALIDATOR_PARTIAL, "", 0, IN_SRC, 1, 1, 0, 0);
  FEED("carried lead then ASCII", &v, "A", 1,
       UTF8_VALIDATOR_ILLFORMED, "", 0, IN_BUF, 0, 0, 0, 1);
  SUBPART("carried lead then ASCII", "\xC3", 1);
  FEED("retry ASCII after carried lead", &v, "A", 1,
       UTF8_VALIDATOR_OK, "A", 1, IN_SRC, 1, 0, 0, 0);
  FINISH("retry ASCII after carried lead, finish", &v, UTF8_VALIDATOR_OK, 0);

  utf8_validator_init(&v);
  FEED("carried lead + valid cont", &v, "\xE2", 1,
       UTF8_VALIDATOR_PARTIAL, "", 0, IN_SRC, 1, 1, 0, 0);
  FEED("carried lead + cont then ASCII", &v, "\x82""A", 2,
       UTF8_VALIDATOR_ILLFORMED, "", 0, IN_BUF, 0, 0, 1, 1);
  SUBPART("carried lead + cont then ASCII", "\xE2\x82", 2);
  FEED("retry ASCII after carried lead + cont", &v, "A", 1,
       UTF8_VALIDATOR_OK, "A", 1, IN_SRC, 1, 0, 0, 0);

  utf8_validator_init(&v);
  FEED("carried 4-byte lead 1/4", &v, "\xF0", 1,
       UTF8_VALIDATOR_PARTIAL, "", 0, IN_SRC, 1, 1, 0, 0);
  FEED("carried 4-byte lead 2/4", &v, "\x9F", 1,
       UTF8_VALIDATOR_PARTIAL, "", 0, IN_BUF, 1, 2, 0, 0);
  FEED("carried 4-byte lead then ASCII", &v, "A", 1,
       UTF8_VALIDATOR_ILLFORMED, "", 0, IN_BUF, 0, 0, 0, 2);
  SUBPART("carried 4-byte lead then ASCII", "\xF0\x9F", 2);

  utf8_validator_init(&v);
  FEED("carried 4-byte lead + 2 cont 1/4", &v, "\xF0", 1,
       UTF8_VALIDATOR_PARTIAL, "", 0, IN_SRC, 1, 1, 0, 0);
  FEED("carried 4-byte lead + 2 cont 2/4", &v, "\x9F", 1,
       UTF8_VALIDATOR_PARTIAL, "", 0, IN_BUF, 1, 2, 0, 0);
  FEED("carried 4-byte lead + 2 cont then ASCII", &v, "\x98""A", 2,
       UTF8_VALIDATOR_ILLFORMED, "", 0, IN_BUF, 0, 0, 1, 2);
  SUBPART("carried 4-byte lead + 2 cont then ASCII", "\xF0\x9F\x98", 3);
  FEED("retry ASCII after carried 4-byte lead", &v, "A", 1,
       UTF8_VALIDATOR_OK, "A", 1, IN_SRC, 1, 0, 0, 0);

  /* End of stream. */
  utf8_validator_init(&v);
  FEED("truncated after clean prefix", &v, "abc\xC3", 4,
       UTF8_VALIDATOR_PARTIAL, "abc", 3, IN_SRC, 4, 1, 0, 0);
  FINISH("truncated after clean prefix, finish", &v, UTF8_VALIDATOR_TRUNCATED, 1);
  SUBPART("truncated after clean prefix, finish", "\xC3", 1);
  FINISH("finish after truncated finish", &v, UTF8_VALIDATOR_OK, 0);
  FEED("resume after truncated finish", &v, "ok", 2,
       UTF8_VALIDATOR_OK, "ok", 2, IN_SRC, 2, 0, 0, 0);
  FINISH("resume after truncated finish, finish", &v, UTF8_VALIDATOR_OK, 0);

  utf8_validator_init(&v);
  FEED("carried 4-byte trunc 1/4", &v, "\xF0", 1,
       UTF8_VALIDATOR_PARTIAL, "", 0, IN_SRC, 1, 1, 0, 0);
  FEED("carried 4-byte trunc 2/4", &v, "\x9F", 1,
       UTF8_VALIDATOR_PARTIAL, "", 0, IN_BUF, 1, 2, 0, 0);
  FEED("carried 4-byte trunc 3/4", &v, "\x98", 1,
       UTF8_VALIDATOR_PARTIAL, "", 0, IN_BUF, 1, 3, 0, 0);
  FINISH("finish after carried 4-byte prefix", &v, UTF8_VALIDATOR_TRUNCATED, 3);
  SUBPART("finish after carried 4-byte prefix", "\xF0\x9F\x98", 3);
}

/*
 * Chunking tests: feed the same input split in different ways and compare
 * the well-formed data and the ill-formed subparts reported by the
 * validator against a reference computed in one piece with utf8_check and
 * utf8_maximal_subpart.
 */

#define MAX_INPUT 512

typedef struct {
  size_t pos;
  size_t len;
} subpart_t;

typedef struct {
  char out[3 * MAX_INPUT];
  size_t out_len;
  subpart_t subparts[MAX_INPUT];
  size_t count;
  bool ok;
} trace_t;

static void
trace_init(trace_t* t) {
  t->out_len = 0;
  t->count = 0;
  t->ok = true;
}

static void
trace_data(trace_t* t, const char* data, size_t size) {
  memcpy(t->out + t->out_len, data, size);
  t->out_len += size;
}

static void
trace_subpart(trace_t* t, size_t pos, size_t len) {
  t->subparts[t->count].pos = pos;
  t->subparts[t->count].len = len;
  t->count++;
  trace_data(t, "\xEF\xBF\xBD", 3);
}

static bool
trace_equal(const trace_t* a, const trace_t* b) {
  if (!a->ok || !b->ok)
    return false;
  if (a->out_len != b->out_len || memcmp(a->out, b->out, a->out_len) != 0)
    return false;
  if (a->count != b->count)
    return false;
  for (size_t i = 0; i < a->count; i++) {
    if (a->subparts[i].pos != b->subparts[i].pos ||
        a->subparts[i].len != b->subparts[i].len)
      return false;
  }
  return true;
}

static void
trace_reference(trace_t* t, const char* src, size_t len) {
  size_t pos = 0;

  trace_init(t);
  while (pos < len) {
    size_t cursor, n;

    if (utf8_check(src + pos, len - pos, &cursor)) {
      trace_data(t, src + pos, len - pos);
      break;
    }
    trace_data(t, src + pos, cursor);
    pos += cursor;
    n = utf8_maximal_subpart(src + pos, len - pos);
    trace_subpart(t, pos, n);
    pos += n;
  }
}

/*
 * Feeds src[0..len) split at cuts[0..ncuts). Each chunk lives in its own
 * allocation that is released as soon as the validator has consumed it, so
 * the sanitizers catch any read outside or after the chunk.
 */
static void
trace_validator(trace_t* t,
                utf8_validator_t* v,
                const char* src,
                size_t len,
                const size_t* cuts,
                size_t ncuts) {
  utf8_validator_result_t r;
  size_t start = 0;

  trace_init(t);
  for (size_t i = 0; i <= ncuts; i++) {
    size_t end = i < ncuts ? cuts[i] : len;
    size_t n = end - start;
    size_t offset = start;
    char* chunk = malloc(n > 0 ? n : 1);
    const char* p = chunk;
    bool stalled = false;

    memcpy(chunk, src + start, n);

    if (n == 0) {
      r = utf8_validator_feed(v, p, 0);
      if (r.status == UTF8_VALIDATOR_ILLFORMED || r.size != 0 ||
          r.consumed != 0 || r.advance != 0)
        t->ok = false;
    }

    while (n > 0) {
      size_t step;

      r = utf8_validator_feed(v, p, n);
      step = r.consumed + r.advance;
      if (step > n || (step == 0 && stalled)) {
        t->ok = false;
        break;
      }
      stalled = step == 0;

      trace_data(t, r.data, r.size);

      switch (r.status) {
        case UTF8_VALIDATOR_OK:
          break;
        case UTF8_VALIDATOR_PARTIAL:
          if (step != n)
            t->ok = false;
          break;
        case UTF8_VALIDATOR_ILLFORMED: {
          size_t sub_pos = offset + r.consumed - r.carried;
          size_t sub_len = r.carried + r.advance;

          if (sub_pos + sub_len > len ||
              memcmp(r.data + r.size, src + sub_pos, sub_len) != 0)
            t->ok = false;
          trace_subpart(t, sub_pos, sub_len);
          break;
        }
        case UTF8_VALIDATOR_TRUNCATED:
          t->ok = false;
          break;
      }

      p += step;
      n -= step;
      offset += step;
    }

    free(chunk);
    start = end;
  }

  r = utf8_validator_finish(v);
  if (r.status == UTF8_VALIDATOR_TRUNCATED) {
    if (r.carried > len ||
        memcmp(r.data + r.size, src + len - r.carried, r.carried) != 0)
      t->ok = false;
    trace_subpart(t, len - r.carried, r.carried);
  }
  else if (r.status != UTF8_VALIDATOR_OK)
    t->ok = false;
}

static trace_t Expected;
static trace_t Got;

static void
check_chunking(utf8_validator_t* v,
               const char* src,
               size_t len,
               const size_t* cuts,
               size_t ncuts,
               bool* failed) {
  trace_validator(&Got, v, src, len, cuts, ncuts);
  if (!trace_equal(&Expected, &Got) && !*failed) {
    char esc[4 * 64 + 1];
    printf("FAIL: chunking of \"%s\"%s (%zu bytes) at", 
           escape_str(src, len < 64 ? len : 64, esc),
           len < 64 ? "" : "...", len);
    for (size_t i = 0; i < ncuts && i < 16; i++)
      printf(" %zu", cuts[i]);
    printf("\n");
    *failed = true;
  }
}

/* One chunk, every split into two and three chunks, and one byte at a time. */
static void
check_all_splits(const char* src, size_t len) {
  utf8_validator_t v;
  size_t cuts[MAX_INPUT];
  bool failed = false;

  trace_reference(&Expected, src, len);
  utf8_validator_init(&v);

  check_chunking(&v, src, len, cuts, 0, &failed);

  for (size_t i = 0; i <= len; i++) {
    cuts[0] = i;
    check_chunking(&v, src, len, cuts, 1, &failed);
    for (size_t j = i; j <= len; j++) {
      cuts[1] = j;
      check_chunking(&v, src, len, cuts, 2, &failed);
    }
  }

  for (size_t i = 0; i + 1 < len; i++)
    cuts[i] = i + 1;
  if (len > 1)
    check_chunking(&v, src, len, cuts, len - 1, &failed);

  TestCount++;
  if (failed)
    TestFailed++;
}

static void
test_splits(void) {
  static const struct {
    const char* src;
    size_t len;
  } tests[] = {
    { "", 0 },
    { "hello", 5 },
    { "a\xC3\xA9z", 4 },
    { "a\xE2\x82\xACz", 5 },
    { "a\xF0\x9F\x98\x80z", 6 },
    { "\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80", 9 },
    { "\xF0\x9F\x98\x80\xF0\x9F\x98\x80", 8 },
    { "a\x80z", 3 },
    { "a\xC3z", 3 },
    { "a\xE2\x82z", 4 },
    { "a\xF0\x9F\x98z", 5 },
    { "a\xC3\xC3\xA9z", 5 },
    { "a\xE2\xE2\x82\xACz", 6 },
    { "a\xF0\x9F\xF0\x9F\x98\x80z", 8 },
    { "a\xC0\xAFz", 4 },
    { "a\xE0\x80\xAFz", 5 },
    { "a\xED\xA0\x80z", 5 },
    { "a\xF0\x80\x80\x80z", 6 },
    { "a\xF4\x90\x80\x80z", 6 },
    { "a\xF5\x80\x80\x80z", 6 },
    { "\x80\x80\x80\x80", 4 },
    { "\xF0\xF0\xF0\xF0", 4 },
    { "abc\xC3", 4 },
    { "abc\xE2\x82", 5 },
    { "abc\xF0\x9F\x98", 6 },
    { "\xC3", 1 },
    { "\xE2\x82", 2 },
    { "\xF0\x9F\x98", 3 },
    { "\xE2\x82\xF0\x9F\x98", 5 },
  };

  for (size_t i = 0; i < sizeof tests / sizeof tests[0]; i++)
    check_all_splits(tests[i].src, tests[i].len);
}

/*
 * Every string of up to four bytes over an alphabet with one byte from
 * each DFA byte class.
 */
static void
test_splits_exhaustive(void) {
  static const char alphabet[] = {
    '\x41', '\x80', '\x90', '\xA0', '\xC0', '\xC2', '\xE0',
    '\xE1', '\xED', '\xEE', '\xF0', '\xF1', '\xF4', '\xF5',
  };
  const size_t n = sizeof alphabet;
  char src[4];

  check_all_splits(src, 0);

  for (size_t a = 0; a < n; a++) {
    src[0] = alphabet[a];
    check_all_splits(src, 1);
    for (size_t b = 0; b < n; b++) {
      src[1] = alphabet[b];
      check_all_splits(src, 2);
      for (size_t c = 0; c < n; c++) {
        src[2] = alphabet[c];
        check_all_splits(src, 3);
        for (size_t d = 0; d < n; d++) {
          src[3] = alphabet[d];
          check_all_splits(src, 4);
        }
      }
    }
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

static size_t
random_range(size_t lo, size_t hi) {
  return lo + random_next() % (hi - lo + 1);
}

/* Mostly well-formed text with occasional ill-formed and truncated sequences. */
static size_t
random_input(char* dst, size_t max) {
  size_t len = 0;

  while (len + 40 <= max) {
    switch (random_next() % 16) {
      case 0: case 1: case 2: case 3: case 4: case 5: {
        size_t n = random_range(1, 40);
        for (size_t i = 0; i < n; i++)
          dst[len++] = (char)random_range(0x20, 0x7E);
        break;
      }
      case 6: case 7: case 8:
        encode_ord((uint32_t)random_range(0x80, 0x7FF), 2, dst + len);
        len += 2;
        break;
      case 9: case 10: case 11:
        encode_ord((uint32_t)random_range(0x800, 0xD7FF), 3, dst + len);
        len += 3;
        break;
      case 12: case 13:
        encode_ord((uint32_t)random_range(0x10000, 0x10FFFF), 4, dst + len);
        len += 4;
        break;
      case 14:
        dst[len++] = (char)random_range(0x80, 0xFF);
        break;
      case 15: {
        size_t n = random_range(1, 3);
        encode_ord((uint32_t)random_range(0x10000, 0x10FFFF), 4, dst + len);
        len += n;
        break;
      }
    }
    if (random_next() % 24 == 0)
      break;
  }
  return len;
}

static void
test_random_chunks(void) {
  static const size_t windows[] = { 0, 64, 256, 4096 };
  char src[MAX_INPUT];
  size_t cuts[MAX_INPUT];

  for (size_t iter = 0; iter < 4000; iter++) {
    utf8_validator_t v;
    size_t len = random_input(src, sizeof src);
    size_t ncuts = 0;
    size_t pos = 0;
    bool failed = false;

    for (;;) {
      switch (random_next() % 4) {
        case 0: pos += random_range(0, 3);    break;
        case 1: pos += random_range(1, 16);   break;
        case 2: pos += random_range(60, 130); break;
        case 3: pos += random_range(1, 400);  break;
      }
      if (pos >= len)
        break;
      cuts[ncuts++] = pos;
    }

    utf8_validator_init(&v);
    utf8_validator_set_window(&v, windows[iter % 4]);
    utf8_validator_set_ascii(&v, (iter / 4) % 2 == 1);

    trace_reference(&Expected, src, len);
    check_chunking(&v, src, len, cuts, ncuts, &failed);

    TestCount++;
    if (failed)
      TestFailed++;
  }
}

int
main(int argc, char **argv) {
  SUITE(__FILE__);
  RUN(test_feed);
  RUN(test_splits);
  RUN(test_splits_exhaustive);
  RUN(test_random_chunks);
  return report_results();
}
