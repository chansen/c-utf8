CPPFLAGS  = -I.
CC       ?= cc

TEST_DIR        = test
TEST_DATA       = $(TEST_DIR)/data/utf8tests.txt
TEST_HARNESS    = $(TEST_DIR)/test.h $(TEST_DIR)/test_util.h
TEST_CFLAGS     = -std=c99 -O2 -Wall -fsanitize=address,undefined

BENCH_DIR       = benchmark
BENCH_SRC       = $(BENCH_DIR)/bench.c
BENCH_BIN       = bench
BENCH_OPTFLAGS ?= -O3 -march=native
BENCH_CFLAGS   ?= -std=c99 -Wall $(BENCH_OPTFLAGS)

TESTS = \
	dfa_step32 \
	dfa_step64 \
	dfa_step_decode \
	rdfa_step32 \
	rdfa_step64 \
	rdfa_step_decode \
	valid32 \
	valid64 \
	valid_stream32 \
	valid_stream64 \
	valid_file32 \
	valid_file64 \
	advance_forward32 \
	advance_forward64 \
	advance_forward_ascii32 \
	advance_forward_ascii64 \
	advance_forward_unsafe \
	advance_backward32 \
	advance_backward64 \
	advance_backward_ascii32 \
	advance_backward_ascii64 \
	advance_backward_unsafe \
	distance32 \
	distance64 \
	distance_ascii32 \
	distance_ascii64 \
	distance_unsafe \
	decode_next \
	decode_next_unsafe \
	decode_prev \
	decode_prev_unsafe \
	transcode_utf16 \
	transcode_utf16_unsafe \
	transcode_utf32 \
	transcode_utf32_unsafe \
	dfa_run_dual32 \
	dfa_run_dual64 \
	dfa_run_triple32 \
	dfa_run_triple64 \
	swar \
	simd

all: test

dfa_step32: $(TEST_DIR)/dfa_step.c utf8_dfa32.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/dfa_step.c

dfa_step64: $(TEST_DIR)/dfa_step.c utf8_dfa64.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_DFA_64 -o $@ $(TEST_DIR)/dfa_step.c

dfa_step_decode: $(TEST_DIR)/dfa_step_decode.c utf8_dfa64.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/dfa_step_decode.c

rdfa_step32: $(TEST_DIR)/rdfa_step.c utf8_rdfa32.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/rdfa_step.c

rdfa_step64: $(TEST_DIR)/rdfa_step.c utf8_rdfa64.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_RDFA_64 -o $@ $(TEST_DIR)/rdfa_step.c

rdfa_step_decode: $(TEST_DIR)/rdfa_step_decode.c utf8_rdfa64.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/rdfa_step_decode.c

valid32: $(TEST_DIR)/valid.c utf8_dfa32.h utf8_valid.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/valid.c

valid64: $(TEST_DIR)/valid.c utf8_dfa64.h utf8_valid.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_DFA_64 -o $@ $(TEST_DIR)/valid.c

valid_stream32: $(TEST_DIR)/valid_stream.c utf8_dfa32.h utf8_valid_stream.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/valid_stream.c

valid_stream64: $(TEST_DIR)/valid_stream.c utf8_dfa64.h utf8_valid_stream.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_DFA_64 -o $@ $(TEST_DIR)/valid_stream.c

valid_file32: $(TEST_DIR)/valid_file.c utf8_dfa32.h utf8_valid.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/valid_file.c

valid_file64: $(TEST_DIR)/valid_file.c utf8_dfa64.h utf8_valid.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_DFA_64 -o $@ $(TEST_DIR)/valid_file.c

advance_forward32: $(TEST_DIR)/advance_forward.c utf8_dfa32.h utf8_advance_forward.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/advance_forward.c

advance_forward64: $(TEST_DIR)/advance_forward.c utf8_dfa64.h utf8_advance_forward.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_DFA_64 -o $@ $(TEST_DIR)/advance_forward.c

advance_forward_ascii32: $(TEST_DIR)/advance_forward_ascii.c utf8_dfa32.h utf8_advance_forward.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/advance_forward_ascii.c

advance_forward_ascii64: $(TEST_DIR)/advance_forward_ascii.c utf8_dfa64.h utf8_advance_forward.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_DFA_64 -o $@ $(TEST_DIR)/advance_forward_ascii.c

advance_forward_unsafe: $(TEST_DIR)/advance_forward_unsafe.c utf8_swar.h utf8_advance_forward_unsafe.h utf8_dfa64.h utf8_advance_forward.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_DFA_64 -o $@ $(TEST_DIR)/advance_forward_unsafe.c

advance_backward32: $(TEST_DIR)/advance_backward.c utf8_rdfa32.h utf8_advance_backward.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/advance_backward.c

advance_backward64: $(TEST_DIR)/advance_backward.c utf8_rdfa64.h utf8_advance_backward.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_RDFA_64 -o $@ $(TEST_DIR)/advance_backward.c

advance_backward_ascii32: $(TEST_DIR)/advance_backward_ascii.c utf8_rdfa32.h utf8_advance_backward.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/advance_backward_ascii.c

advance_backward_ascii64: $(TEST_DIR)/advance_backward_ascii.c utf8_rdfa64.h utf8_advance_backward.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_RDFA_64 -o $@ $(TEST_DIR)/advance_backward_ascii.c

advance_backward_unsafe: $(TEST_DIR)/advance_backward_unsafe.c utf8_swar.h utf8_advance_backward_unsafe.h utf8_rdfa64.h utf8_advance_backward.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_RDFA_64 -o $@ $(TEST_DIR)/advance_backward_unsafe.c

distance32: $(TEST_DIR)/distance.c utf8_dfa32.h utf8_distance.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/distance.c

distance64: $(TEST_DIR)/distance.c utf8_dfa64.h utf8_distance.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_DFA_64 -o $@ $(TEST_DIR)/distance.c

distance_unsafe: $(TEST_DIR)/distance_unsafe.c utf8_swar.h utf8_distance_unsafe.h utf8_dfa64.h utf8_distance.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_DFA_64 -o $@ $(TEST_DIR)/distance_unsafe.c

distance_ascii32: $(TEST_DIR)/distance_ascii.c utf8_dfa32.h utf8_distance.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/distance_ascii.c

distance_ascii64: $(TEST_DIR)/distance_ascii.c utf8_dfa64.h utf8_distance.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_DFA_64 -o $@ $(TEST_DIR)/distance_ascii.c

decode_next: $(TEST_DIR)/decode_next.c utf8_dfa64.h utf8_decode_next.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/decode_next.c

decode_next_unsafe: $(TEST_DIR)/decode_next_unsafe.c utf8_dfa64.h utf8_decode_next.h utf8_decode_next_unsafe.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/decode_next_unsafe.c

decode_prev: $(TEST_DIR)/decode_prev.c utf8_rdfa64.h utf8_decode_prev.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/decode_prev.c

decode_prev_unsafe: $(TEST_DIR)/decode_prev_unsafe.c utf8_rdfa64.h utf8_decode_prev.h utf8_decode_prev_unsafe.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/decode_prev_unsafe.c

transcode_utf16: $(TEST_DIR)/transcode_utf16.c utf8_dfa64.h utf8_transcode.h utf8_transcode_common.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/transcode_utf16.c

transcode_utf16_unsafe: $(TEST_DIR)/transcode_utf16_unsafe.c utf8_dfa64.h utf8_transcode.h utf8_transcode_unsafe.h utf8_transcode_common.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/transcode_utf16_unsafe.c

transcode_utf32: $(TEST_DIR)/transcode_utf32.c utf8_dfa64.h utf8_transcode.h utf8_transcode_common.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/transcode_utf32.c

transcode_utf32_unsafe: $(TEST_DIR)/transcode_utf32_unsafe.c utf8_dfa64.h utf8_transcode.h utf8_transcode_unsafe.h utf8_transcode_common.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/transcode_utf32_unsafe.c

dfa_run_dual32: $(TEST_DIR)/dfa_run_dual.c utf8_dfa32.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/dfa_run_dual.c

dfa_run_dual64: $(TEST_DIR)/dfa_run_dual.c utf8_dfa64.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_DFA_64 -o $@ $(TEST_DIR)/dfa_run_dual.c

dfa_run_triple32: $(TEST_DIR)/dfa_run_triple.c utf8_dfa32.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/dfa_run_triple.c

dfa_run_triple64: $(TEST_DIR)/dfa_run_triple.c utf8_dfa64.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_DFA_64 -o $@ $(TEST_DIR)/dfa_run_triple.c

swar: $(TEST_DIR)/swar.c utf8_swar.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/swar.c

simd: $(TEST_DIR)/simd.c utf8_simd.h utf8_swar.h $(TEST_HARNESS)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/simd.c

test: $(TESTS)
	@./dfa_step32
	@./dfa_step64
	@./dfa_step_decode
	@./rdfa_step32
	@./rdfa_step64
	@./rdfa_step_decode
	@./valid32
	@./valid64
	@./valid_stream32
	@./valid_stream64
	@./valid_file32 $(TEST_DATA)
	@./valid_file64 $(TEST_DATA)
	@./advance_forward32
	@./advance_forward64
	@./advance_forward_ascii32
	@./advance_forward_ascii64
	@./advance_forward_unsafe
	@./advance_backward32
	@./advance_backward64
	@./advance_backward_ascii32
	@./advance_backward_ascii64
	@./advance_backward_unsafe
	@./distance32
	@./distance64
	@./distance_ascii32
	@./distance_ascii64
	@./distance_unsafe
	@./decode_next
	@./decode_next_unsafe
	@./decode_prev
	@./decode_prev_unsafe
	@./transcode_utf16
	@./transcode_utf16_unsafe
	@./transcode_utf32
	@./transcode_utf32_unsafe
	@./dfa_run_dual32
	@./dfa_run_dual64
	@./dfa_run_triple32
	@./dfa_run_triple64
	@./swar
	@./simd

bench: $(BENCH_SRC) utf8_valid.h
	$(CC) $(CPPFLAGS) $(BENCH_CFLAGS) -o $(BENCH_BIN) $(BENCH_SRC)

clean:
	rm -f $(TESTS) $(BENCH_BIN)

.PHONY: all test clean bench
