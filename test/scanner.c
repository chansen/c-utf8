#include <stdbool.h>

#ifdef UTF8_DFA_64
#  include "utf8_dfa64.h"
#else
#  include "utf8_dfa32.h"
#endif

#include "utf8_scanner.h"
#include "test.h"

static void
stream_check(const char* func,
             const char* desc,
             utf8_scanner_status_t exp_status,
             size_t exp_consumed,
             size_t exp_pending,
             size_t exp_advance,
             size_t exp_carried,
             utf8_scanner_result_t got,
             unsigned line) {
  TestCount++;
  if (got.status != exp_status) {
    printf("%s [%s] status == %d, expected %d at line %u\n",
           func, desc, (int)got.status, (int)exp_status, line);
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

#define STREAM(desc, st, src, len, exp_status, exp_consumed, exp_pending, exp_advance, exp_carried) do { \
  utf8_scanner_result_t _r = utf8_scanner_feed(st, src, len); \
  stream_check("utf8_scanner_feed", desc, exp_status, exp_consumed, exp_pending, exp_advance, exp_carried, _r, __LINE__); \
} while (0)

#define FINISH(desc, st, exp_status, exp_consumed, exp_pending, exp_advance, exp_carried) do { \
  utf8_scanner_result_t _r = utf8_scanner_finish(st); \
  stream_check("utf8_scanner_finish", desc, exp_status, exp_consumed, exp_pending, exp_advance, exp_carried, _r, __LINE__); \
} while (0)

static void
test_streaming(void) {
  utf8_scanner_t st;

  utf8_scanner_init(&st);
  STREAM("clean ASCII chunk", &st, "hello", 5,
         UTF8_SCANNER_OK, 5, 0, 0, 0);
  STREAM("clean ASCII chunk 2", &st, "world", 5,
         UTF8_SCANNER_OK, 5, 0, 0, 0);
  FINISH("clean ASCII chunk 2, finish", &st,
         UTF8_SCANNER_OK, 0, 0, 0, 0);

  utf8_scanner_init(&st);
  STREAM("clean 2-byte chunk", &st, "\xC3\xA9\xC3\xA9", 4,
         UTF8_SCANNER_OK, 4, 0, 0, 0);
  FINISH("clean 2-byte chunk, finish", &st,
         UTF8_SCANNER_OK, 0, 0, 0, 0);

  utf8_scanner_init(&st);
  STREAM("2-byte split chunk 1", &st, "\xC3", 1,
         UTF8_SCANNER_PARTIAL, 0, 1, 0, 0);
  STREAM("2-byte split chunk 2", &st, "\xA9", 1,
         UTF8_SCANNER_OK, 1, 0, 0, 0);

  utf8_scanner_init(&st);
  STREAM("3-byte split 1/3", &st, "\xE2", 1,
         UTF8_SCANNER_PARTIAL, 0, 1, 0, 0);
  STREAM("3-byte split 2/3", &st, "\x82", 1,
         UTF8_SCANNER_PARTIAL, 0, 2, 0, 0);
  STREAM("3-byte split 3/3", &st, "\xAC", 1,
         UTF8_SCANNER_OK, 1, 0, 0, 0);
  FINISH("3-byte split 3/3, finish", &st,
         UTF8_SCANNER_OK, 0, 0, 0, 0);

  utf8_scanner_init(&st);
  STREAM("4-byte split 1/4", &st, "\xF0", 1,
         UTF8_SCANNER_PARTIAL, 0, 1, 0, 0);
  STREAM("4-byte split 2/4", &st, "\x9F", 1,
         UTF8_SCANNER_PARTIAL, 0, 2, 0, 0);
  STREAM("4-byte split 3/4", &st, "\x98", 1,
         UTF8_SCANNER_PARTIAL, 0, 3, 0, 0);
  STREAM("4-byte split 4/4", &st, "\x80", 1,
         UTF8_SCANNER_OK, 1, 0, 0, 0);
  FINISH("4-byte split 4/4, finish", &st,
         UTF8_SCANNER_OK, 0, 0, 0, 0);

  utf8_scanner_init(&st);
  STREAM("multi-seq chunk", &st,
         "\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80", 9,
         UTF8_SCANNER_OK, 9, 0, 0, 0);
  FINISH("multi-seq chunk, finish", &st,
         UTF8_SCANNER_OK, 0, 0, 0, 0);

  utf8_scanner_init(&st);
  STREAM("16-byte ASCII block", &st, "abcdefghijklmnop", 16,
         UTF8_SCANNER_OK, 16, 0, 0, 0);
  FINISH("16-byte ASCII block, finish", &st,
         UTF8_SCANNER_OK, 0, 0, 0, 0);

  utf8_scanner_init(&st);
  STREAM("32-byte ASCII blocks", &st,
         "abcdefghijklmnopqrstuvwxyz012345", 32,
         UTF8_SCANNER_OK, 32, 0, 0, 0);
  FINISH("32-byte ASCII blocks, finish", &st,
         UTF8_SCANNER_OK, 0, 0, 0, 0);

  utf8_scanner_init(&st);
  STREAM("block then partial lead", &st, "abcdefghijklmnop\xC3", 17,
         UTF8_SCANNER_PARTIAL, 16, 1, 0, 0);
  STREAM("complete lead after block", &st, "\xA9", 1,
         UTF8_SCANNER_OK, 1, 0, 0, 0);
  FINISH("complete lead after block, finish", &st,
         UTF8_SCANNER_OK, 0, 0, 0, 0);

  utf8_scanner_init(&st);
  STREAM("reject at block end", &st, "abcdefghijklmno\x80", 16,
         UTF8_SCANNER_ILLFORMED, 15, 0, 1, 0);

  utf8_scanner_init(&st);
  STREAM("reject after clean block", &st, "abcdefghijklmnop\x80", 17,
         UTF8_SCANNER_ILLFORMED, 16, 0, 1, 0);

  utf8_scanner_init(&st);
  STREAM("invalid mid-chunk", &st, "ab\x80""cd", 5,
         UTF8_SCANNER_ILLFORMED, 2, 0, 1, 0);

  utf8_scanner_init(&st);
  STREAM("invalid at start", &st, "\x80""hello", 6,
         UTF8_SCANNER_ILLFORMED, 0, 0, 1, 0);

  utf8_scanner_init(&st);
  STREAM("invalid at end", &st, "hello\x80", 6,
         UTF8_SCANNER_ILLFORMED, 5, 0, 1, 0);

  utf8_scanner_init(&st);
  STREAM("truncated 2-byte", &st, "\xC3", 1,
         UTF8_SCANNER_PARTIAL, 0, 1, 0, 0);
  FINISH("truncated 2-byte, finish", &st,
         UTF8_SCANNER_TRUNCATED, 0, 0, 0, 1);

  utf8_scanner_init(&st);
  STREAM("truncated 3-byte", &st, "\xE2\x82", 2,
         UTF8_SCANNER_PARTIAL, 0, 2, 0, 0);
  FINISH("truncated 3-byte, finish", &st,
         UTF8_SCANNER_TRUNCATED, 0, 0, 0, 2);

  utf8_scanner_init(&st);
  STREAM("truncated 4-byte", &st, "\xF0\x9F\x98", 3,
         UTF8_SCANNER_PARTIAL, 0, 3, 0, 0);
  FINISH("truncated 4-byte, finish", &st,
         UTF8_SCANNER_TRUNCATED, 0, 0, 0, 3);

  utf8_scanner_init(&st);
  STREAM("invalid before resume", &st, "ab\x80", 3,
         UTF8_SCANNER_ILLFORMED, 2, 0, 1, 0);
  STREAM("resume after invalid", &st, "cd", 2,
         UTF8_SCANNER_OK, 2, 0, 0, 0);
  FINISH("resume after invalid, finish", &st,
         UTF8_SCANNER_OK, 0, 0, 0, 0);

  utf8_scanner_init(&st);
  STREAM("truncated before resume", &st, "\xC3", 1,
         UTF8_SCANNER_PARTIAL, 0, 1, 0, 0);
  FINISH("truncated before resume, finish", &st,
         UTF8_SCANNER_TRUNCATED, 0, 0, 0, 1);
  STREAM("resume after truncated", &st, "ok", 2,
         UTF8_SCANNER_OK, 2, 0, 0, 0);
  FINISH("resume after truncated, finish", &st,
         UTF8_SCANNER_OK, 0, 0, 0, 0);

  utf8_scanner_init(&st);
  STREAM("empty chunk", &st, "", 0,
         UTF8_SCANNER_OK, 0, 0, 0, 0);
  FINISH("finish after empty chunk", &st,
         UTF8_SCANNER_OK, 0, 0, 0, 0);

  utf8_scanner_init(&st);
  STREAM("single-byte 1", &st, "h", 1,
         UTF8_SCANNER_OK, 1, 0, 0, 0);
  STREAM("single-byte 2", &st, "i", 1,
         UTF8_SCANNER_OK, 1, 0, 0, 0);
  STREAM("single-byte 3", &st, "!", 1,
         UTF8_SCANNER_OK, 1, 0, 0, 0);
  FINISH("single-byte 3, finish", &st,
         UTF8_SCANNER_OK, 0, 0, 0, 0);

  utf8_scanner_init(&st);
  STREAM("split then more 1", &st, "abc\xC3", 4,
         UTF8_SCANNER_PARTIAL, 3, 1, 0, 0);
  STREAM("split then more 2", &st, "\xA9""def", 4,
         UTF8_SCANNER_OK, 4, 0, 0, 0);
  FINISH("split then more 2, finish", &st,
         UTF8_SCANNER_OK, 0, 0, 0, 0);

  utf8_scanner_init(&st);
  STREAM("carried lead", &st, "\xC3", 1,
         UTF8_SCANNER_PARTIAL, 0, 1, 0, 0);
  STREAM("carried lead then ASCII", &st, "A", 1,
         UTF8_SCANNER_ILLFORMED, 0, 0, 0, 1);
  STREAM("retry ASCII after carried lead", &st, "A", 1,
         UTF8_SCANNER_OK, 1, 0, 0, 0);
  FINISH("retry ASCII after carried lead, finish", &st,
         UTF8_SCANNER_OK, 0, 0, 0, 0);

  utf8_scanner_init(&st);
  STREAM("carried lead + valid cont", &st, "\xE2", 1,
         UTF8_SCANNER_PARTIAL, 0, 1, 0, 0);
  STREAM("carried lead + cont then ASCII", &st, "\x82""A", 2,
         UTF8_SCANNER_ILLFORMED, 0, 0, 1, 1);
  STREAM("retry ASCII after carried lead + cont", &st, "A", 1,
         UTF8_SCANNER_OK, 1, 0, 0, 0);
  FINISH("retry ASCII after carried lead + cont, finish", &st,
         UTF8_SCANNER_OK, 0, 0, 0, 0);

  utf8_scanner_init(&st);
  STREAM("truncated from previous chunk", &st, "\xC3", 1,
         UTF8_SCANNER_PARTIAL, 0, 1, 0, 0);
  FINISH("finish after carried lead", &st,
         UTF8_SCANNER_TRUNCATED, 0, 0, 0, 1);

  utf8_scanner_init(&st);
  STREAM("carried 4-byte lead 1/4", &st, "\xF0", 1,
         UTF8_SCANNER_PARTIAL, 0, 1, 0, 0);
  STREAM("carried 4-byte lead 2/4", &st, "\x9F", 1,
         UTF8_SCANNER_PARTIAL, 0, 2, 0, 0);
  STREAM("carried 4-byte lead then ASCII", &st, "A", 1,
         UTF8_SCANNER_ILLFORMED, 0, 0, 0, 2);

  utf8_scanner_init(&st);
  STREAM("carried 4-byte lead + 2 cont 1/4", &st, "\xF0", 1,
         UTF8_SCANNER_PARTIAL, 0, 1, 0, 0);
  STREAM("carried 4-byte lead + 2 cont 2/4", &st, "\x9F", 1,
         UTF8_SCANNER_PARTIAL, 0, 2, 0, 0);
  STREAM("carried 4-byte lead + 2 cont then ASCII", &st, "\x98""A", 2,
         UTF8_SCANNER_ILLFORMED, 0, 0, 1, 2);

  utf8_scanner_init(&st);
  STREAM("carried 4-byte trunc 1/4", &st, "\xF0", 1,
         UTF8_SCANNER_PARTIAL, 0, 1, 0, 0);
  STREAM("carried 4-byte trunc 2/4", &st, "\x9F", 1,
         UTF8_SCANNER_PARTIAL, 0, 2, 0, 0);
  STREAM("carried 4-byte trunc 3/4", &st, "\x98", 1,
         UTF8_SCANNER_PARTIAL, 0, 3, 0, 0);
  FINISH("finish after carried 4-byte prefix", &st,
         UTF8_SCANNER_TRUNCATED, 0, 0, 0, 3);

  utf8_scanner_init(&st);
  FINISH("finish without input", &st,
         UTF8_SCANNER_OK, 0, 0, 0, 0);

  utf8_scanner_init(&st);
  STREAM("truncated after clean prefix", &st, "abc\xC3", 4,
         UTF8_SCANNER_PARTIAL, 3, 1, 0, 0);
  FINISH("truncated after clean prefix, finish", &st,
         UTF8_SCANNER_TRUNCATED, 0, 0, 0, 1);
  FINISH("finish after truncated finish", &st,
         UTF8_SCANNER_OK, 0, 0, 0, 0);
}

static void
test_reconstruction(void) {
  utf8_scanner_t st;
  utf8_scanner_result_t r;
  size_t chunk_start;
  size_t error_pos;
  size_t resume_pos;
  size_t subpart_len;

  utf8_scanner_init(&st);
  chunk_start = 100;

  r = utf8_scanner_feed(&st, "\xF0", 1);
  stream_check("utf8_scanner_feed", "reconstruct partial 1", UTF8_SCANNER_PARTIAL,
               0, 1, 0, 0, r, __LINE__);
  chunk_start += 1;

  r = utf8_scanner_feed(&st, "\x9F\x98""A", 3);
  stream_check("utf8_scanner_feed", "reconstruct illformed", UTF8_SCANNER_ILLFORMED,
               0, 0, 2, 1, r, __LINE__);

  error_pos = chunk_start + r.consumed - r.carried;
  resume_pos = chunk_start + r.consumed + r.advance;
  subpart_len = r.carried + r.advance;

  CHECK(error_pos == 100, "reconstruct: error_pos");
  CHECK(resume_pos == 103, "reconstruct: resume_pos");
  CHECK(subpart_len == 3, "reconstruct: subpart_len");

  utf8_scanner_init(&st);
  chunk_start = 100;

  r = utf8_scanner_feed(&st, "ab\xE2\x82", 4);
  stream_check("utf8_scanner_feed", "reconstruct partial 2", UTF8_SCANNER_PARTIAL,
               2, 2, 0, 0, r, __LINE__);
  chunk_start += 4;

  r = utf8_scanner_finish(&st);
  stream_check("utf8_scanner_finish", "reconstruct truncated", UTF8_SCANNER_TRUNCATED,
               0, 0, 0, 2, r, __LINE__);

  error_pos = chunk_start + r.consumed - r.carried;
  resume_pos = chunk_start + r.consumed + r.advance;
  subpart_len = r.carried + r.advance;

  CHECK(error_pos == 102, "reconstruct truncated: error_pos");
  CHECK(resume_pos == 104, "reconstruct truncated: resume_pos");
  CHECK(subpart_len == 2, "reconstruct truncated: subpart_len");
}

int
main(int argc, char **argv) {
  SUITE(__FILE__);
  RUN(test_streaming);
  RUN(test_reconstruction);
  return report_results();
}
