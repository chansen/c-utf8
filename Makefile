CPPFLAGS  = -I.
CC       ?= cc

BUILD_DIR       = build
TEST_DIR        = test
TEST_DATA       = $(TEST_DIR)/data/utf8tests.txt
TEST_HARNESS    = $(TEST_DIR)/test.h $(TEST_DIR)/test_util.h
TEST_CFLAGS     = -std=c99 -O2 -Wall -fsanitize=address,undefined

BENCH_DIR       = benchmark
BENCH_SRC       = $(BENCH_DIR)/bench.c
BENCH_BIN       = $(BUILD_DIR)/bench
BENCH_OPTFLAGS ?= -O3 -march=native
BENCH_CFLAGS   ?= -std=c99 -Wall $(BENCH_OPTFLAGS)

TESTS = \
	$(BUILD_DIR)/dfa_step32 \
	$(BUILD_DIR)/dfa_step64 \
	$(BUILD_DIR)/dfa_step_decode \
	$(BUILD_DIR)/rdfa_step32 \
	$(BUILD_DIR)/rdfa_step64 \
	$(BUILD_DIR)/rdfa_step_decode \
	$(BUILD_DIR)/valid32 \
	$(BUILD_DIR)/valid64 \
	$(BUILD_DIR)/valid_stream32 \
	$(BUILD_DIR)/valid_stream64 \
	$(BUILD_DIR)/valid_file32 \
	$(BUILD_DIR)/valid_file64 \
	$(BUILD_DIR)/advance_forward32 \
	$(BUILD_DIR)/advance_forward64 \
	$(BUILD_DIR)/advance_forward_ascii32 \
	$(BUILD_DIR)/advance_forward_ascii64 \
	$(BUILD_DIR)/advance_forward_unsafe \
	$(BUILD_DIR)/advance_backward32 \
	$(BUILD_DIR)/advance_backward64 \
	$(BUILD_DIR)/advance_backward_ascii32 \
	$(BUILD_DIR)/advance_backward_ascii64 \
	$(BUILD_DIR)/advance_backward_unsafe \
	$(BUILD_DIR)/distance32 \
	$(BUILD_DIR)/distance64 \
	$(BUILD_DIR)/distance_ascii32 \
	$(BUILD_DIR)/distance_ascii64 \
	$(BUILD_DIR)/distance_unsafe \
	$(BUILD_DIR)/decode_next \
	$(BUILD_DIR)/decode_next_unsafe \
	$(BUILD_DIR)/decode_prev \
	$(BUILD_DIR)/decode_prev_unsafe \
	$(BUILD_DIR)/transcode_utf16 \
	$(BUILD_DIR)/transcode_utf16_unsafe \
	$(BUILD_DIR)/transcode_utf32 \
	$(BUILD_DIR)/transcode_utf32_unsafe \
	$(BUILD_DIR)/dfa_run_dual32 \
	$(BUILD_DIR)/dfa_run_dual64 \
	$(BUILD_DIR)/dfa_run_triple32 \
	$(BUILD_DIR)/dfa_run_triple64 \
	$(BUILD_DIR)/swar \
	$(BUILD_DIR)/simd

all: test

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/dfa_step32: $(TEST_DIR)/dfa_step.c utf8_dfa32.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/dfa_step.c

$(BUILD_DIR)/dfa_step64: $(TEST_DIR)/dfa_step.c utf8_dfa64.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_DFA_64 -o $@ $(TEST_DIR)/dfa_step.c

$(BUILD_DIR)/dfa_step_decode: $(TEST_DIR)/dfa_step_decode.c utf8_dfa64.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/dfa_step_decode.c

$(BUILD_DIR)/rdfa_step32: $(TEST_DIR)/rdfa_step.c utf8_rdfa32.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/rdfa_step.c

$(BUILD_DIR)/rdfa_step64: $(TEST_DIR)/rdfa_step.c utf8_rdfa64.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_RDFA_64 -o $@ $(TEST_DIR)/rdfa_step.c

$(BUILD_DIR)/rdfa_step_decode: $(TEST_DIR)/rdfa_step_decode.c utf8_rdfa64.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/rdfa_step_decode.c

$(BUILD_DIR)/valid32: $(TEST_DIR)/valid.c utf8_dfa32.h utf8_valid.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/valid.c

$(BUILD_DIR)/valid64: $(TEST_DIR)/valid.c utf8_dfa64.h utf8_valid.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_DFA_64 -o $@ $(TEST_DIR)/valid.c

$(BUILD_DIR)/valid_stream32: $(TEST_DIR)/valid_stream.c utf8_dfa32.h utf8_valid_stream.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/valid_stream.c

$(BUILD_DIR)/valid_stream64: $(TEST_DIR)/valid_stream.c utf8_dfa64.h utf8_valid_stream.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_DFA_64 -o $@ $(TEST_DIR)/valid_stream.c

$(BUILD_DIR)/valid_file32: $(TEST_DIR)/valid_file.c utf8_dfa32.h utf8_valid.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/valid_file.c

$(BUILD_DIR)/valid_file64: $(TEST_DIR)/valid_file.c utf8_dfa64.h utf8_valid.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_DFA_64 -o $@ $(TEST_DIR)/valid_file.c

$(BUILD_DIR)/advance_forward32: $(TEST_DIR)/advance_forward.c utf8_dfa32.h utf8_advance_forward.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/advance_forward.c

$(BUILD_DIR)/advance_forward64: $(TEST_DIR)/advance_forward.c utf8_dfa64.h utf8_advance_forward.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_DFA_64 -o $@ $(TEST_DIR)/advance_forward.c

$(BUILD_DIR)/advance_forward_ascii32: $(TEST_DIR)/advance_forward_ascii.c utf8_dfa32.h utf8_advance_forward.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/advance_forward_ascii.c

$(BUILD_DIR)/advance_forward_ascii64: $(TEST_DIR)/advance_forward_ascii.c utf8_dfa64.h utf8_advance_forward.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_DFA_64 -o $@ $(TEST_DIR)/advance_forward_ascii.c

$(BUILD_DIR)/advance_forward_unsafe: $(TEST_DIR)/advance_forward_unsafe.c utf8_swar.h utf8_advance_forward_unsafe.h utf8_dfa64.h utf8_advance_forward.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_DFA_64 -o $@ $(TEST_DIR)/advance_forward_unsafe.c

$(BUILD_DIR)/advance_backward32: $(TEST_DIR)/advance_backward.c utf8_rdfa32.h utf8_advance_backward.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/advance_backward.c

$(BUILD_DIR)/advance_backward64: $(TEST_DIR)/advance_backward.c utf8_rdfa64.h utf8_advance_backward.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_RDFA_64 -o $@ $(TEST_DIR)/advance_backward.c

$(BUILD_DIR)/advance_backward_ascii32: $(TEST_DIR)/advance_backward_ascii.c utf8_rdfa32.h utf8_advance_backward.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/advance_backward_ascii.c

$(BUILD_DIR)/advance_backward_ascii64: $(TEST_DIR)/advance_backward_ascii.c utf8_rdfa64.h utf8_advance_backward.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_RDFA_64 -o $@ $(TEST_DIR)/advance_backward_ascii.c

$(BUILD_DIR)/advance_backward_unsafe: $(TEST_DIR)/advance_backward_unsafe.c utf8_swar.h utf8_advance_backward_unsafe.h utf8_rdfa64.h utf8_advance_backward.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_RDFA_64 -o $@ $(TEST_DIR)/advance_backward_unsafe.c

$(BUILD_DIR)/distance32: $(TEST_DIR)/distance.c utf8_dfa32.h utf8_distance.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/distance.c

$(BUILD_DIR)/distance64: $(TEST_DIR)/distance.c utf8_dfa64.h utf8_distance.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_DFA_64 -o $@ $(TEST_DIR)/distance.c

$(BUILD_DIR)/distance_unsafe: $(TEST_DIR)/distance_unsafe.c utf8_swar.h utf8_distance_unsafe.h utf8_dfa64.h utf8_distance.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_DFA_64 -o $@ $(TEST_DIR)/distance_unsafe.c

$(BUILD_DIR)/distance_ascii32: $(TEST_DIR)/distance_ascii.c utf8_dfa32.h utf8_distance.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/distance_ascii.c

$(BUILD_DIR)/distance_ascii64: $(TEST_DIR)/distance_ascii.c utf8_dfa64.h utf8_distance.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_DFA_64 -o $@ $(TEST_DIR)/distance_ascii.c

$(BUILD_DIR)/decode_next: $(TEST_DIR)/decode_next.c utf8_dfa64.h utf8_decode_next.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/decode_next.c

$(BUILD_DIR)/decode_next_unsafe: $(TEST_DIR)/decode_next_unsafe.c utf8_dfa64.h utf8_decode_next.h utf8_decode_next_unsafe.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/decode_next_unsafe.c

$(BUILD_DIR)/decode_prev: $(TEST_DIR)/decode_prev.c utf8_rdfa64.h utf8_decode_prev.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/decode_prev.c

$(BUILD_DIR)/decode_prev_unsafe: $(TEST_DIR)/decode_prev_unsafe.c utf8_rdfa64.h utf8_decode_prev.h utf8_decode_prev_unsafe.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/decode_prev_unsafe.c

$(BUILD_DIR)/transcode_utf16: $(TEST_DIR)/transcode_utf16.c utf8_dfa64.h utf8_transcode.h utf8_transcode_common.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/transcode_utf16.c

$(BUILD_DIR)/transcode_utf16_unsafe: $(TEST_DIR)/transcode_utf16_unsafe.c utf8_dfa64.h utf8_transcode.h utf8_transcode_unsafe.h utf8_transcode_common.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/transcode_utf16_unsafe.c

$(BUILD_DIR)/transcode_utf32: $(TEST_DIR)/transcode_utf32.c utf8_dfa64.h utf8_transcode.h utf8_transcode_common.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/transcode_utf32.c

$(BUILD_DIR)/transcode_utf32_unsafe: $(TEST_DIR)/transcode_utf32_unsafe.c utf8_dfa64.h utf8_transcode.h utf8_transcode_unsafe.h utf8_transcode_common.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/transcode_utf32_unsafe.c

$(BUILD_DIR)/dfa_run_dual32: $(TEST_DIR)/dfa_run_dual.c utf8_dfa32.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/dfa_run_dual.c

$(BUILD_DIR)/dfa_run_dual64: $(TEST_DIR)/dfa_run_dual.c utf8_dfa64.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_DFA_64 -o $@ $(TEST_DIR)/dfa_run_dual.c

$(BUILD_DIR)/dfa_run_triple32: $(TEST_DIR)/dfa_run_triple.c utf8_dfa32.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/dfa_run_triple.c

$(BUILD_DIR)/dfa_run_triple64: $(TEST_DIR)/dfa_run_triple.c utf8_dfa64.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -DUTF8_DFA_64 -o $@ $(TEST_DIR)/dfa_run_triple.c

$(BUILD_DIR)/swar: $(TEST_DIR)/swar.c utf8_swar.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/swar.c

$(BUILD_DIR)/simd: $(TEST_DIR)/simd.c utf8_simd.h utf8_swar.h $(TEST_HARNESS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $(TEST_DIR)/simd.c

test: $(TESTS)
	@$(BUILD_DIR)/dfa_step32
	@$(BUILD_DIR)/dfa_step64
	@$(BUILD_DIR)/dfa_step_decode
	@$(BUILD_DIR)/rdfa_step32
	@$(BUILD_DIR)/rdfa_step64
	@$(BUILD_DIR)/rdfa_step_decode
	@$(BUILD_DIR)/valid32
	@$(BUILD_DIR)/valid64
	@$(BUILD_DIR)/valid_stream32
	@$(BUILD_DIR)/valid_stream64
	@$(BUILD_DIR)/valid_file32 $(TEST_DATA)
	@$(BUILD_DIR)/valid_file64 $(TEST_DATA)
	@$(BUILD_DIR)/advance_forward32
	@$(BUILD_DIR)/advance_forward64
	@$(BUILD_DIR)/advance_forward_ascii32
	@$(BUILD_DIR)/advance_forward_ascii64
	@$(BUILD_DIR)/advance_forward_unsafe
	@$(BUILD_DIR)/advance_backward32
	@$(BUILD_DIR)/advance_backward64
	@$(BUILD_DIR)/advance_backward_ascii32
	@$(BUILD_DIR)/advance_backward_ascii64
	@$(BUILD_DIR)/advance_backward_unsafe
	@$(BUILD_DIR)/distance32
	@$(BUILD_DIR)/distance64
	@$(BUILD_DIR)/distance_ascii32
	@$(BUILD_DIR)/distance_ascii64
	@$(BUILD_DIR)/distance_unsafe
	@$(BUILD_DIR)/decode_next
	@$(BUILD_DIR)/decode_next_unsafe
	@$(BUILD_DIR)/decode_prev
	@$(BUILD_DIR)/decode_prev_unsafe
	@$(BUILD_DIR)/transcode_utf16
	@$(BUILD_DIR)/transcode_utf16_unsafe
	@$(BUILD_DIR)/transcode_utf32
	@$(BUILD_DIR)/transcode_utf32_unsafe
	@$(BUILD_DIR)/dfa_run_dual32
	@$(BUILD_DIR)/dfa_run_dual64
	@$(BUILD_DIR)/dfa_run_triple32
	@$(BUILD_DIR)/dfa_run_triple64
	@$(BUILD_DIR)/swar
	@$(BUILD_DIR)/simd

bench: $(BENCH_SRC) utf8_valid.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(BENCH_CFLAGS) -o $(BENCH_BIN) $(BENCH_SRC)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all test clean bench
