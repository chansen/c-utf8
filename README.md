# c-utf8

A header-only UTF-8 library in C implementing validation, decoding, and
transcoding conforming to the Unicode and ISO/IEC 10646 specifications.

## Usage

```c
#include "utf8_dfa32.h"   // or utf8_dfa64.h for the 64-bit variant
#include "utf8_valid.h"

// Check if a string is valid UTF-8
bool ok = utf8_valid(src, len);

// Check and find the error position
size_t cursor;
if (!utf8_check(src, len, &cursor)) {
  // cursor is the byte offset of the first ill-formed sequence
}

// Length of the maximal subpart of an ill-formed sequence
size_t n = utf8_maximal_subpart(src, len);
```

## Requirements

- C99 or later

## DFA backends

The safe routines in this library are driven by a Deterministic Finite
Automaton (DFA) that consumes one byte at a time and transitions between a
small set of states. Four backend headers are provided; you choose one or two
based on what the consumer headers you include actually need. The
[unsafe variants](#unsafe-variants) use no DFA and need no backend.

### Forward DFA

The forward DFA scans bytes left-to-right starting from the `ACCEPT` state.
Each byte indexes into a 256-entry lookup table and produces the next state.
Returning to `ACCEPT` marks the end of a complete valid sequence. Reaching
`REJECT` means an ill-formed byte was encountered; that state is a permanent
trap and no further input can leave it.

The 9 forward DFA states are:

| State    | Meaning |
| :---     | :--- |
| `ACCEPT` | Start / valid sequence boundary |
| `REJECT` | Ill-formed input (absorbing trap) |
| `TAIL1`  | Awaiting 1 more continuation byte |
| `TAIL2`  | Awaiting 2 more continuation bytes |
| `TAIL3`  | Awaiting 3 more continuation bytes |
| `E0`     | After `0xE0`; next must be `A0–BF` (reject non-shortest form) |
| `ED`     | After `0xED`; next must be `80–9F` (reject surrogates) |
| `F0`     | After `0xF0`; next must be `90–BF` (reject non-shortest form) |
| `F4`     | After `0xF4`; next must be `80–8F` (reject above U+10FFFF) |

The four special states (`E0`, `ED`, `F0`, `F4`) enforce Unicode constraints
that a naive continuation-byte count cannot express: non-shortest-form
encodings, surrogate halves (U+D800–U+DFFF), and codepoints above U+10FFFF
are all structurally rejected.

### Reverse DFA

The reverse DFA scans bytes right-to-left and is used for backward navigation
and backward decoding and moving a cursor back by N codepoints.

Going forward you see the lead byte first and know immediately how many
continuation bytes follow and what constraints apply to them. Going in
reverse you see continuation bytes before the lead, so the DFA tracks how
many have been seen and remembers the range of the outermost continuation
byte (the one adjacent to the lead), because that range determines which lead
bytes are valid.

The reverse DFA uses 7 states, 2 fewer than the forward DFA:

| State    | Meaning |
| :---     | :--- |
| `ACCEPT` | Start / valid sequence boundary |
| `REJECT` | Ill-formed input (absorbing trap) |
| `TL1`    | Seen 1 continuation byte |
| `TL2LO`  | Seen 2 continuations; outermost was `80–9F` |
| `TL2HI`  | Seen 2 continuations; outermost was `A0–BF` |
| `TL3LO`  | Seen 3 continuations; outermost was `80–8F` |
| `TL3MH`  | Seen 3 continuations; outermost was `90–BF` |

The LO/HI and LO/MH split on the outermost continuation range enforces the
same constraints as the forward DFA's `E0`, `ED`, `F0`, and `F4` states,
applied when the lead byte finally arrives rather than when it is first seen.

### Shift-based DFA encoding

All four backends implement the shift-based DFA approach originally described
by [Per Vognsen](https://gist.github.com/pervognsen/218ea17743e1442e59bb60d29b1aa725).
Each byte maps to a row in a 256-entry table. The row is a packed integer
where each state's transition target is stored at a fixed bit offset. The
current state value is used directly as the shift amount to extract the next
state, avoiding a second lookup. The inner loop is a table load, a shift, and
a mask:

```c
state = (table[byte] >> state) & mask;
```

The error state is fixed at offset 0. Any transition to error contributes
`(0 << 0) = 0` to the row value, which is itself the error state at every
bit offset. Once entered, no byte can leave it.

The 32-bit row packing technique was derived by
[Dougall Johnson](https://gist.github.com/dougallj/166e326de6ad4cf2c94be97a204c025f);
state offsets are chosen by an SMT solver (`tool/smt_solver.py`) to pack all
transitions without collision inside a `uint32_t`.

The 64-bit forward backend uses state offsets at fixed multiples of 6
(6, 12, 18, ..., 48), packing all transitions into 54 bits of a `uint64_t`.
Bits 56–62 store a per-byte payload mask that the decode step uses to
accumulate codepoint bits in the same pass:

```c
*codepoint = (*codepoint << 6) | (byte & (row >> 56));
state      = (row >> state) & 63;
```

ASCII rows carry mask `0x7F`, 2/3/4-byte lead rows carry `0x1F`/`0x0F`/`0x07`,
continuation rows carry `0x3F`, and error rows carry `0x00`.

The 64-bit reverse backend uses the same payload mask scheme. The sequence
depth is encoded in the state value itself, so the bit shift for codepoint
accumulation can be derived without a separate lookup:

```c
shift      = ((state >> 2) & 3) * 6;
*codepoint = *codepoint | (byte & (row >> 56)) << shift;
state      = (row >> state) & 63;
```

### Choosing a backend

#### Forward backends

| Header         | Row type   | Table size | Decoding |
| :---           | :---       | :---       | :---     |
| `utf8_dfa32.h` | `uint32_t` | 1 KB       | No       |
| `utf8_dfa64.h` | `uint64_t` | 2 KB       | Yes      |

Use `utf8_dfa32.h` for validation-only work: `utf8_valid.h`,
`utf8_scanner.h`, `utf8_validator.h`, `utf8_distance.h`,
`utf8_advance_forward.h`.

Use `utf8_dfa64.h` when codepoint decoding or transcoding is needed:
`utf8_decode_next.h`, `utf8_transcode.h`, `utf8_transcode_endian.h`.

#### Reverse backends

| Header          | Row type   | Table size | Decoding |
| :---            | :---       | :---       | :---     |
| `utf8_rdfa32.h` | `uint32_t` | 1 KB       | No       |
| `utf8_rdfa64.h` | `uint64_t` | 2 KB       | Yes      |

Use `utf8_rdfa32.h` for backward navigation: `utf8_advance_backward.h`.

Use `utf8_rdfa64.h` for backward decoding: `utf8_decode_prev.h`.

#### Summary

The 64-bit variants are supersets of the 32-bit variants — include exactly
one forward backend and, if you need backward traversal, exactly one reverse
backend.

| You need                             | Forward              | Reverse               |
| :---                                 | :---                 | :---                  |
| Validate                             | `utf8_dfa{32,64}.h`  | —                     |
| Count codepoints                     | `utf8_dfa{32,64}.h`  | —                     |
| Streaming validation                 | `utf8_dfa{32,64}.h`  | —                     |
| Buffered streaming validation        | `utf8_dfa{32,64}.h`  | —                     |
| Skip forward N codepoints            | `utf8_dfa{32,64}.h`  | —                     |
| Decode forward                       | `utf8_dfa64.h`       | —                     |
| Transcode to UTF-16/32               | `utf8_dfa64.h`       | —                     |
| Transcode to UTF-16/32 LE/BE         | `utf8_dfa64.h`       | —                     |
| Skip backward N codepoints           | —                    | `utf8_rdfa{32,64}.h`  |
| Decode backward                      | —                    | `utf8_rdfa64.h`       |

## API

The library is split into independent header files. The safe API headers each
require exactly one DFA backend to be included first. The [unsafe variants](#unsafe-variants)
require no DFA backend and operate on pre-validated input only.

---

### Validation — `utf8_valid.h`

Requires `utf8_dfa32.h` or `utf8_dfa64.h`.

```c
bool   utf8_valid(const char *src, size_t len);
bool   utf8_check(const char *src, size_t len, size_t *cursor);

bool   utf8_valid_bounded(const char *src, size_t len);
bool   utf8_check_bounded(const char *src, size_t len, size_t *cursor);

bool   utf8_valid_ascii(const char *src, size_t len);
bool   utf8_check_ascii(const char *src, size_t len, size_t *cursor);

bool   utf8_valid_ascii_bounded(const char *src, size_t len);
bool   utf8_check_ascii_bounded(const char *src, size_t len, size_t *cursor);

size_t utf8_maximal_subpart(const char *src, size_t len);
```

**`utf8_valid`** and **`utf8_check`** return `true` if `src[0..len)` is valid
UTF-8. Both use dual-stream validation: the input is split at a sequence
boundary near the midpoint and two independent DFA chains run in a single
interleaved loop, exploiting instruction-level parallelism on wide-issue
cores. The whole input is scanned even when an ill-formed byte occurs early;
use `utf8_valid_bounded` / `utf8_check_bounded` to stop early. On failure, if
`cursor` is non-NULL, `utf8_check` sets `*cursor` to the byte offset of the
first ill-formed sequence (the length of the maximal valid prefix).

**`utf8_valid_bounded`** and **`utf8_check_bounded`** are drop-in replacements
that validate the input in fixed-size blocks. An ill-formed byte terminates
the scan after the current block instead of walking the whole input, and on
failure the cursor is computed from the last accepted block boundary rather
than rescanning from the start. Behaviour is identical to `utf8_valid` and
`utf8_check`. They are slightly slower on well-formed input, so prefer them
when the input may be ill-formed or when bounding worst-case work matters.

**`utf8_valid_ascii`** and **`utf8_check_ascii`** are drop-in replacements
that use a single DFA stream with a 16-byte ASCII fast path. On each
iteration the fast path checks whether the next 16 bytes are all ASCII; if
so, it skips them without entering the DFA. When a non-ASCII byte is
encountered the DFA processes bytes until it returns to the `ACCEPT` state,
at which point the fast path is re-entered. Like `utf8_valid` and
`utf8_check`, they scan the whole input even on early error; use
`utf8_valid_ascii_bounded` / `utf8_check_ascii_bounded` to stop early.
Behaviour is identical to `utf8_valid` and `utf8_check`. Throughput advantage
depends on content mix and microarchitecture; see the
[Performance](#performance) section.

**`utf8_valid_ascii_bounded`** and **`utf8_check_ascii_bounded`** run the
single-stream 16-byte ASCII fast path in the same fixed-size blocks, giving
the ASCII fast path the block-bounded early exit and cursor behaviour. Like
the other bounded variants, they are slightly slower on well-formed input.
Behaviour is identical to `utf8_valid` and `utf8_check`.

**`utf8_maximal_subpart`** returns the length of the maximal subpart of the
ill-formed sequence starting at `src[0..len)`, as defined by Unicode
(see [Error handling and U+FFFD replacement](#error-handling-and-ufffd-replacement)).
The return value is always >= 1. Call this after `utf8_check` reports failure,
with `src` advanced to the cursor position.

---

### Streaming validation — `utf8_scanner.h`

Requires `utf8_dfa32.h` or `utf8_dfa64.h`.

A `utf8_scanner_t` validates a UTF-8 byte stream that arrives in chunks, such
as reads from a file or a socket, where a sequence may be split across two or
more chunks. It answers the same questions as `utf8_check` and
`utf8_maximal_subpart` do for a complete buffer: is the input well-formed,
and if not, where does each ill-formed subpart start and how long is it.

The scanner is the low-level, zero-copy primitive. Between calls it keeps
only the DFA state and a byte count; it never copies or retains input, and it
reports everything as offsets and lengths. A result can therefore refer to
bytes of an earlier chunk (`carried`). The scanner counts those bytes but
does not have them: a caller that needs the bytes themselves must still hold
the earlier chunk, or use `utf8_validator.h`, which keeps them.

|                         | `utf8_valid.h`     | `utf8_scanner.h`           | `utf8_validator.h`               |
| :---                    | :---               | :---                       | :---                             |
| Input                   | one buffer         | chunks                     | chunks                           |
| State between calls     | none               | DFA state and a count      | scanner and a 4-byte buffer      |
| Copies input            | no                 | no                         | only a sequence split by a chunk boundary |
| Ill-formed subpart      | offset and length  | offset and length          | offset, length and bytes         |
| Well-formed bytes       | prefix length      | prefix length per chunk    | pointer and length               |

The life cycle is: initialise once, feed every chunk in stream order, then
finish. A scanner can be reused for another stream after
`utf8_scanner_finish`.

```c
typedef enum {
  UTF8_SCANNER_OK,         // src fully consumed, no errors
  UTF8_SCANNER_PARTIAL,    // src fully consumed, ends mid-sequence
  UTF8_SCANNER_ILLFORMED,  // stopped at an ill-formed subsequence
  UTF8_SCANNER_TRUNCATED,  // stream ended mid-sequence (utf8_scanner_finish only)
} utf8_scanner_status_t;

typedef struct {
  utf8_scanner_status_t status;
  size_t consumed;            // bytes read from src
  size_t pending;             // bytes in an incomplete trailing sequence, else 0
  size_t advance;             // bytes to skip on ILLFORMED, else 0
  size_t carried;             // bytes from a previous chunk that belong to the same subpart
} utf8_scanner_result_t;

typedef struct {
  utf8_dfa_state_t state;
  size_t carried;
  size_t probe_window;        // bulk look-ahead bound (bytes); tuning only
  bool probe_ascii;           // ASCII fast path for the bulk probe; tuning only
} utf8_scanner_t;

void   utf8_scanner_init(utf8_scanner_t *s);
void   utf8_scanner_set_window(utf8_scanner_t *s, size_t window);
void   utf8_scanner_set_ascii(utf8_scanner_t *s, bool ascii);
utf8_scanner_result_t utf8_scanner_feed(utf8_scanner_t *s,
                                        const char *src,
                                        size_t len);
utf8_scanner_result_t utf8_scanner_finish(utf8_scanner_t *s);
```

**`utf8_scanner_init`** initialises a scanner. Call this before the first
`utf8_scanner_feed`. The probe window is set to the default
(`UTF8_SCANNER_PROBE_WINDOW_SIZE`, 256).

**`utf8_scanner_set_window`** sets the probe window on an already
initialised scanner. It may be called at any time, including between chunks.
A value below 64 disables probing (pure byte-by-byte validation).

**`utf8_scanner_set_ascii`** enables or disables the ASCII fast path for
the bulk probe (default off). It may be called at any time, including between
chunks.

**`utf8_scanner_feed`** validates `src[0..len)` as the next chunk of a
UTF-8 byte stream, continuing from the state left by the previous call. The
result depends on how the chunk ends:

| Status      | Meaning                                              | `consumed`                           | `pending`                                   | `advance`                     | `carried`                          |
| :---        | :---                                                 | :---                                 | :---                                        | :---                          | :---                               |
| `OK`        | Well-formed, ends on a sequence boundary             | `len`                                | 0                                           | 0                             | 0                                  |
| `PARTIAL`   | Well-formed so far, ends in the middle of a sequence | bytes before the incomplete sequence | bytes of the incomplete sequence so far     | 0                             | 0                                  |
| `ILLFORMED` | Stopped at an ill-formed sequence                    | well-formed bytes before the subpart | 0                                           | subpart bytes in this chunk   | subpart bytes in earlier chunks    |

On `OK` and `PARTIAL` the scanner has taken the whole chunk; feed the next
one. On `PARTIAL` this includes the incomplete trailing sequence: it is held
in the scanner's state and must not be fed again, even though `consumed`
only counts up to the last sequence boundary. `pending` counts every byte of
that sequence seen so far, including bytes from earlier chunks, so it can
exceed `len - consumed`.

On `ILLFORMED` the scanner stops after the
[maximal subpart](#error-handling-and-ufffd-replacement) and resets its
state to `ACCEPT`. The rest of the chunk has not been examined: continue by
feeding `src + consumed + advance`. When the subpart began in an earlier chunk
(`carried` is non-zero), `consumed` is 0 and `advance` may be 0 as well. In
that case the first byte of the chunk exposed the error without being part
of the subpart, and it is validated again by the next call.

**`utf8_scanner_finish`** signals the end of the stream. Call it after the
final chunk has been passed to `utf8_scanner_feed`. If the stream ended on a
sequence boundary, status is `OK`. If it ended in the middle of a sequence,
status is `TRUNCATED` and `carried` is the number of trailing bytes in that
incomplete sequence. `consumed`, `pending` and `advance` are always 0. The
scanner state resets to `ACCEPT` either way.

For example, the stream `41 F0 9F 98 42` holds a 4-byte sequence that is cut
short by `42`. Fed as two chunks:

| Call               | Status      | `consumed` | `pending` | `advance` | `carried` |                                               |
| :---               | :---        | :---       | :---      | :---      | :---      | :---                                          |
| feed `41 F0 9F`    | `PARTIAL`   | 1          | 2         | 0         | 0         | `41` is complete, `F0 9F` is held             |
| feed `98 42`       | `ILLFORMED` | 0          | 0         | 1         | 2         | subpart is `F0 9F 98`; continue at `42`       |
| feed `42`          | `OK`        | 1          | 0         | 0         | 0         |                                               |
| finish             | `OK`        | 0          | 0         | 0         | 0         |                                               |

Had the stream ended after the first chunk, `utf8_scanner_finish` would have
returned `TRUNCATED` with `carried` 2.

#### Error positions

If `src` is at absolute stream offset `stream_offset`, then on `ILLFORMED`:

- `stream_offset + consumed - carried` is the position of the subpart,
- `carried + advance` is its length,
- `stream_offset + consumed + advance` is where validation continues.

The same holds for `TRUNCATED` from `utf8_scanner_finish`, with
`stream_offset` at the end of the stream: the subpart is the last `carried`
bytes.

The following validates a stream and reports every ill-formed subpart. To
reject the stream at the first error instead, stop at the first `ILLFORMED`.

```c
utf8_scanner_t s;
utf8_scanner_init(&s);

size_t stream_offset = 0;  // absolute offset of src
bool valid = true;

while ((len = read_chunk(buf, sizeof buf)) > 0) {
  const char *src = buf;

  for (;;) {
    utf8_scanner_result_t r = utf8_scanner_feed(&s, src, len);

    if (r.status != UTF8_SCANNER_ILLFORMED) {
      stream_offset += len;  // OK or PARTIAL: the whole chunk was taken
      break;
    }

    size_t error_pos   = stream_offset + r.consumed - r.carried;
    size_t subpart_len = r.carried + r.advance;

    handle_error(error_pos, subpart_len);
    valid = false;

    // continue with the rest of the chunk
    src           += r.consumed + r.advance;
    len           -= r.consumed + r.advance;
    stream_offset += r.consumed + r.advance;
  }
}

utf8_scanner_result_t r = utf8_scanner_finish(&s);
if (r.status == UTF8_SCANNER_TRUNCATED) {
  size_t error_pos   = stream_offset - r.carried;
  size_t subpart_len = r.carried;

  handle_error(error_pos, subpart_len);
  valid = false;
}
```

#### Probe window

After each sequence boundary, the scanner hands the following bytes to a
bulk DFA path instead of stepping one byte at a time. The **probe window**
caps that run at `min(probe_window, remaining - 1)` bytes, trimmed to the last
sequence boundary within it.

The window is a throughput knob only; results are identical for any value.
The bulk path needs at least 64 bytes to engage, so shorter inputs and
trailing bytes are always stepped one at a time. A larger window suits large,
mostly-valid input (disk reads); a smaller one suits small piecemeal chunks
(sockets). A value below 64 disables the bulk path entirely.

By default the bulk path uses a dual-stream DFA pass.
`utf8_scanner_set_ascii(s, true)` switches it to an ASCII-skipping pass
that jumps over all-ASCII 16-byte blocks and steps the rest single-stream.
This is faster on ASCII-leaning streams and slower on multibyte-heavy ones.

On error the bulk path only signals *that* a run is ill-formed; the exact
position is found by re-stepping that run, so the window never affects
reported errors.

Set it per stream with `utf8_scanner_set_window`. The compile-time
default is controlled by `UTF8_SCANNER_PROBE_WINDOW_SIZE` (default 256);
setting it below 64 disables probing by default:

```c
// raise the default window for all streams in this translation unit
#define UTF8_SCANNER_PROBE_WINDOW_SIZE 4096
#include "utf8_scanner.h"
```

---

### Buffered streaming validation — `utf8_validator.h`

Requires `utf8_dfa32.h` or `utf8_dfa64.h`. Includes `utf8_scanner.h`.

```c
typedef enum {
  UTF8_VALIDATOR_OK,         // no errors, data ends on a sequence boundary
  UTF8_VALIDATOR_PARTIAL,    // src fully consumed, incomplete sequence buffered
  UTF8_VALIDATOR_ILLFORMED,  // stopped at an ill-formed subsequence
  UTF8_VALIDATOR_TRUNCATED,  // stream ended mid-sequence (utf8_validator_finish only)
} utf8_validator_status_t;

typedef struct {
  utf8_validator_status_t status;
  const char *data;           // well-formed bytes, in src or in the validator's buffer
  size_t size;                // length of data, may be 0
  size_t consumed;            // bytes read from src
  size_t pending;             // bytes of an incomplete trailing sequence held in the buffer, else 0
  size_t advance;             // bytes to skip on ILLFORMED, else 0
  size_t carried;             // bytes from previous chunks that belong to the same subpart
} utf8_validator_result_t;

typedef struct {
  utf8_scanner_t scanner;
  char buffer[4];             // sequence that spans chunks
  size_t buffered;
} utf8_validator_t;

void   utf8_validator_init(utf8_validator_t *v);
void   utf8_validator_set_window(utf8_validator_t *v, size_t window);
void   utf8_validator_set_ascii(utf8_validator_t *v, bool ascii);
utf8_validator_result_t utf8_validator_feed(utf8_validator_t *v,
                                            const char *src,
                                            size_t len);
utf8_validator_result_t utf8_validator_finish(utf8_validator_t *v);
```

A `utf8_validator_t` wraps a `utf8_scanner_t` and adds a small buffer for
sequences that span chunks. The scanner only reports positions, so a caller
that forwards the validated bytes has to hold back an incomplete trailing
sequence itself. The validator does that internally: every result describes
one contiguous run of well-formed bytes, either in `src` or, for a sequence
that spans chunks, in the validator's buffer. The caller never needs a
previous chunk.

**`utf8_validator_init`** initialises a validator. Call this before the first
`utf8_validator_feed`. **`utf8_validator_set_window`** and
**`utf8_validator_set_ascii`** configure the underlying scanner (see
[Probe window](#probe-window)).

**`utf8_validator_feed`** validates `src[0..len)` as the next chunk of a
UTF-8 byte stream. `data[0..size)` is well-formed, ends on a sequence
boundary, and stays valid until the next call on the validator.

If no error is found, status is `OK`. Normally `data` is `src` and the whole
chunk is consumed. When the validator holds an incomplete sequence from a
previous chunk, the call reads only the bytes that complete it and returns
the complete sequence from its buffer; `consumed` is then less than `len`
unless the chunk ends there. Call again with the rest of the chunk.

If the chunk ends in the middle of a sequence, status is `PARTIAL`. The whole
chunk is consumed, `data` holds the bytes before the incomplete sequence, and
`pending` is the number of bytes of that sequence held in the buffer.

If validation stops at an ill-formed sequence, status is `ILLFORMED`. `data`
holds the well-formed bytes before it, `advance` is the number of bytes in
the current chunk that belong to the subpart, and `carried` is the number of
bytes from previous chunks that belong to the same subpart. When `carried` is
non-zero, `consumed + advance` may be 0.

In all cases, continue at `src[consumed + advance]`.

**`utf8_validator_finish`** signals the end of the stream. Call it after the
final chunk has been passed to `utf8_validator_feed`. If the stream ended on
a sequence boundary, status is `OK`. If it ended in the middle of a sequence,
status is `TRUNCATED` and `carried` is the number of trailing bytes in that
incomplete sequence. `size`, `consumed`, `pending` and `advance` are always 0.

On `ILLFORMED` or `TRUNCATED`, the validator state resets automatically so
the caller can continue without reinitialising. Error positions follow the
scanner: if `src` is at absolute offset `stream_offset`, then
`stream_offset + consumed - carried` is the error position and
`carried + advance` is the subpart length.

The bytes of the ill-formed subpart follow the well-formed data: they start
at `data + size` and are `carried + advance` long, whether the subpart lies
within the chunk or began in a previous one. Like `data`, they stay valid
until the next call on the validator.

```c
utf8_validator_t v;
utf8_validator_init(&v);

// copy a stream, reporting each ill-formed subpart and replacing it
// with U+FFFD
while ((len = read_chunk(buf, sizeof buf)) > 0) {
  const char *src = buf;

  while (len > 0) {
    utf8_validator_result_t r = utf8_validator_feed(&v, src, len);

    write_bytes(r.data, r.size);
    if (r.status == UTF8_VALIDATOR_ILLFORMED) {
      report_illformed(r.data + r.size, r.carried + r.advance);
      write_bytes("\xEF\xBF\xBD", 3);
    }

    src += r.consumed + r.advance;
    len -= r.consumed + r.advance;
  }
}

utf8_validator_result_t r = utf8_validator_finish(&v);
if (r.status == UTF8_VALIDATOR_TRUNCATED) {
  report_illformed(r.data, r.carried);
  write_bytes("\xEF\xBF\xBD", 3);
}
```

To only validate a stream, ignore `data`. Nothing is copied except an
incomplete sequence at a chunk boundary, and every ill-formed subpart can
still be reported with its bytes, also when it began in a previous chunk:

```c
utf8_validator_t v;
utf8_validator_init(&v);

size_t offset = 0;  // absolute offset of src
bool valid = true;

// validate a stream, reporting the position and bytes of each ill-formed
// subpart
while ((len = read_chunk(buf, sizeof buf)) > 0) {
  const char *src = buf;

  while (len > 0) {
    utf8_validator_result_t r = utf8_validator_feed(&v, src, len);

    if (r.status == UTF8_VALIDATOR_ILLFORMED) {
      report_illformed(offset + r.consumed - r.carried,
                       r.data + r.size, r.carried + r.advance);
      valid = false;
    }

    src    += r.consumed + r.advance;
    len    -= r.consumed + r.advance;
    offset += r.consumed + r.advance;
  }
}

utf8_validator_result_t r = utf8_validator_finish(&v);
if (r.status == UTF8_VALIDATOR_TRUNCATED) {
  report_illformed(offset - r.carried, r.data, r.carried);
  valid = false;
}
```

---

### Codepoint count — `utf8_distance.h`

Requires `utf8_dfa32.h` or `utf8_dfa64.h`.

```c
size_t utf8_distance(const char *src, size_t len);
size_t utf8_distance_ascii(const char *src, size_t len);
```

**`utf8_distance`** returns the number of Unicode codepoints in
`src[0..len)`, or `(size_t)-1` if the input contains ill-formed UTF-8.

**`utf8_distance_ascii`** is a drop-in replacement with an 8-byte ASCII
fast path that skips the DFA for chunks containing only ASCII bytes.
Behaviour is identical to `utf8_distance`.

---

### Forward navigation — `utf8_advance_forward.h`

Requires `utf8_dfa32.h` or `utf8_dfa64.h`.

```c
size_t utf8_advance_forward(const char *src,
                            size_t len,
                            size_t distance,
                            size_t *advanced);
size_t utf8_advance_forward_ascii(const char *src,
                                  size_t len,
                                  size_t distance,
                                  size_t *advanced);
```

**`utf8_advance_forward`** returns the byte offset of the codepoint
`distance` positions ahead within `src[0..len)`. Returns `len` if `distance`
exceeds the number of codepoints in the buffer. Returns `(size_t)-1` if the
input contains ill-formed UTF-8.

If `advanced` is non-NULL, sets `*advanced` to the number of codepoints
actually skipped before stopping.

**`utf8_advance_forward_ascii`** is a drop-in replacement with an 8-byte
ASCII fast path that skips the DFA for chunks containing only ASCII bytes.
Behaviour is identical to `utf8_advance_forward`.

---

### Backward navigation — `utf8_advance_backward.h`

Requires `utf8_rdfa32.h` or `utf8_rdfa64.h`.

```c
size_t utf8_advance_backward(const char *src,
                             size_t len,
                             size_t distance,
                             size_t *advanced);
size_t utf8_advance_backward_ascii(const char *src,
                                   size_t len,
                                   size_t distance,
                                   size_t *advanced);
```

**`utf8_advance_backward`** returns the byte offset of the codepoint
`distance` positions before the end of `src[0..len)`. Returns `0` if
`distance` exceeds the number of codepoints in the buffer. Returns
`(size_t)-1` if the input contains ill-formed UTF-8.

If `advanced` is non-NULL, sets `*advanced` to the number of codepoints
actually skipped before stopping.

**`utf8_advance_backward_ascii`** is a drop-in replacement with an 8-byte
ASCII fast path that skips the DFA for chunks containing only ASCII bytes.
Behaviour is identical to `utf8_advance_backward`.

---

### Forward decoding — `utf8_decode_next.h`

Requires `utf8_dfa64.h`.

```c
int utf8_decode_next(const char *src, size_t len, uint32_t *codepoint);
int utf8_decode_next_replace(const char *src, size_t len, uint32_t *codepoint);
```

**`utf8_decode_next`** decodes the codepoint starting at `src[0]`.

- **Success:** returns bytes consumed (1–4) and writes the codepoint to
  `*codepoint`.
- **End of input** (`len == 0`): returns `0`; `*codepoint` is unchanged.
- **Ill-formed sequence:** returns the negated length of the maximal subpart
  (see [Error handling and U+FFFD replacement](#error-handling-and-ufffd-replacement)),
  in the range `-1..-3`. Advance by `-return_value`
  bytes and call again. `*codepoint` is unchanged.

**`utf8_decode_next_replace`** is identical but on error writes U+FFFD to
`*codepoint` and returns the maximal subpart length as a positive value.
Never returns a negative value; returns `0` only when `len` is 0.

```c
uint32_t cp;
while (len > 0) {
  int n = utf8_decode_next_replace(src, len, &cp);
  process(cp); // U+FFFD for any ill-formed sequence
  src += n;
  len -= n;
}
```

---

### Backward decoding — `utf8_decode_prev.h`

Requires `utf8_rdfa64.h`.

```c
int utf8_decode_prev(const char *src, size_t len, uint32_t *codepoint);
int utf8_decode_prev_replace(const char *src, size_t len, uint32_t *codepoint);
```

**`utf8_decode_prev`** decodes the codepoint ending at `src[len-1]`.

- **Success:** returns bytes consumed (1–4) and writes the codepoint to
  `*codepoint`. Step back by the return value: next call uses
  `src[0..len-return_value)`.
- **End of input** (`len == 0`): returns `0`; `*codepoint` is unchanged.
- **Ill-formed sequence:** returns the negated number of bytes to step back,
  in the range `-1..-3`. Step back by `-return_value` bytes and call again.
  `*codepoint` is unchanged.

**`utf8_decode_prev_replace`** is identical but on error writes U+FFFD to
`*codepoint` and returns the step-back distance as a positive value.
Never returns a negative value; returns `0` only when `len` is 0.

The reverse DFA sees continuation bytes before the lead byte. For some
ill-formed inputs (e.g. two lone continuation bytes `\x80\x80`),
`utf8_decode_prev` may produce fewer U+FFFD substitutions than
`utf8_decode_next` over the same bytes, because all bytes consumed before
rejection are reported as one ill-formed unit rather than individually.

---

### Transcoding — `utf8_transcode.h`

Requires `utf8_dfa64.h`.

```c
typedef enum {
  UTF8_TRANSCODE_OK,         // src fully consumed, no errors
  UTF8_TRANSCODE_EXHAUSTED,  // dst full before src was consumed
  UTF8_TRANSCODE_ILLFORMED,  // stopped at an ill-formed sequence
  UTF8_TRANSCODE_TRUNCATED,  // src ends mid-sequence
} utf8_transcode_status_t;

typedef struct {
  utf8_transcode_status_t status;
  size_t consumed;   // bytes read from src
  size_t decoded;    // codepoints decoded from src
  size_t written;    // code units written to dst
  size_t advance;    // bytes to skip on ILLFORMED or TRUNCATED, else 0
} utf8_transcode_result_t;

utf8_transcode_result_t utf8_transcode_utf16(const char *src, size_t src_len,
                                             uint16_t *dst, size_t dst_len);
utf8_transcode_result_t utf8_transcode_utf16_replace(const char *src, size_t src_len,
                                                     uint16_t *dst, size_t dst_len);
utf8_transcode_result_t utf8_transcode_utf32(const char *src, size_t src_len,
                                             uint32_t *dst, size_t dst_len);
utf8_transcode_result_t utf8_transcode_utf32_replace(const char *src, size_t src_len,
                                                     uint32_t *dst, size_t dst_len);
```

**`utf8_transcode_utf16`** and **`utf8_transcode_utf32`** transcode
`src[0..src_len)` into `dst[0..dst_len)`, stopping at the first ill-formed
or truncated sequence. Codepoints above U+FFFF are encoded as surrogate
pairs in UTF-16 and consume two code units.

**`utf8_transcode_utf16_replace`** and **`utf8_transcode_utf32_replace`**
replace each ill-formed sequence with U+FFFD and continue. Status is always
`OK` or `EXHAUSTED`; `advance` is always 0.

On `ILLFORMED` or `TRUNCATED`, `consumed` is the byte offset of the
ill-formed sequence and `advance` is the length of its maximal subpart
(see [Error handling and U+FFFD replacement](#error-handling-and-ufffd-replacement)).
Resume transcoding at `src[consumed + advance]`.

```c
// Replace ill-formed sequences with U+FFFD
uint16_t dst[256];
utf8_transcode_result_t r;
do {
  r = utf8_transcode_utf16_replace(src, src_len, dst, sizeof dst / sizeof *dst);
  flush(dst, r.written);
  src     += r.consumed;
  src_len -= r.consumed;
} while (r.status == UTF8_TRANSCODE_EXHAUSTED);

// Strict: report each ill-formed sequence to the caller
while (src_len > 0) {
  r = utf8_transcode_utf16(src, src_len, dst, sizeof dst / sizeof *dst);
  flush(dst, r.written);
  src     += r.consumed;
  src_len -= r.consumed;

  if (r.status == UTF8_TRANSCODE_ILLFORMED || r.status == UTF8_TRANSCODE_TRUNCATED) {
    handle_error(src, r.advance);
    src     += r.advance;
    src_len -= r.advance;
    if (r.status == UTF8_TRANSCODE_TRUNCATED)
      break;
  }
  else if (r.status == UTF8_TRANSCODE_OK)
    break;
}
```

For UTF-32, `result.written` always equals `result.decoded` since each
codepoint maps to exactly one code unit. For UTF-16, `result.written` may
exceed `result.decoded` due to surrogate pairs for codepoints above U+FFFF.

---

### Transcoding to a byte order — `utf8_transcode_endian.h`

Requires `utf8_dfa64.h`.

```c
utf8_transcode_result_t utf8_transcode_utf16le(const char *src, size_t src_len,
                                               uint8_t *dst, size_t dst_len);
utf8_transcode_result_t utf8_transcode_utf16be(const char *src, size_t src_len,
                                               uint8_t *dst, size_t dst_len);
utf8_transcode_result_t utf8_transcode_utf16le_replace(const char *src, size_t src_len,
                                                       uint8_t *dst, size_t dst_len);
utf8_transcode_result_t utf8_transcode_utf16be_replace(const char *src, size_t src_len,
                                                       uint8_t *dst, size_t dst_len);
utf8_transcode_result_t utf8_transcode_utf32le(const char *src, size_t src_len,
                                               uint8_t *dst, size_t dst_len);
utf8_transcode_result_t utf8_transcode_utf32be(const char *src, size_t src_len,
                                               uint8_t *dst, size_t dst_len);
utf8_transcode_result_t utf8_transcode_utf32le_replace(const char *src, size_t src_len,
                                                       uint8_t *dst, size_t dst_len);
utf8_transcode_result_t utf8_transcode_utf32be_replace(const char *src, size_t src_len,
                                                       uint8_t *dst, size_t dst_len);
```

`utf8_transcode.h` writes native `uint16_t` / `uint32_t` code units, which
have the byte order of the host. These functions write UTF-16LE, UTF-16BE,
UTF-32LE or UTF-32BE as bytes, in the named byte order on any host, as
required by file formats and network protocols.

`dst` is a byte buffer and needs no alignment. `dst_len` and
`result.written` are **byte counts**, not code unit counts. Only whole code
units are written: trailing bytes of `dst` that cannot hold one are left
untouched. No byte order mark is written.

Everything else is as in `utf8_transcode.h`: the same result struct, the
same meaning of `status`, `consumed`, `decoded` and `advance`, and the same
strict and `_replace` behaviour.

```c
// Write a stream as UTF-16LE with a byte order mark, replacing ill-formed
// sequences with U+FFFD
uint8_t dst[512];
utf8_transcode_result_t r;

write_bytes("\xFF\xFE", 2);
do {
  r = utf8_transcode_utf16le_replace(src, src_len, dst, sizeof dst);
  write_bytes(dst, r.written);
  src     += r.consumed;
  src_len -= r.consumed;
} while (r.status == UTF8_TRANSCODE_EXHAUSTED);
```

## Unsafe variants

The following headers provide validation-free implementations for use on
input that has already been validated. They skip the DFA entirely and decode
or count by inspecting lead/continuation byte structure directly. The caller
**must** guarantee that `src` contains well-formed UTF-8; behaviour is
undefined otherwise.

These are not drop-in replacements for the safe API — they never return error
indicators. Use them on the output side of a validation boundary (e.g. after
`utf8_valid` or `utf8_check` has accepted the input).

---

### Unsafe codepoint count — `utf8_distance_unsafe.h`

No DFA backend required.

```c
size_t utf8_distance_unsafe(const char *src, size_t len);
```

**`utf8_distance_unsafe`** returns the number of codepoints in `src[0..len)`.
Uses SIMD (when available) or SWAR to process blocks in bulk. Cannot fail.

---

### Unsafe forward navigation — `utf8_advance_forward_unsafe.h`

No DFA backend required.

```c
size_t utf8_advance_forward_unsafe(const char *src,
                                   size_t len,
                                   size_t distance,
                                   size_t *advanced);
```

**`utf8_advance_forward_unsafe`** returns the byte offset of the codepoint
`distance` positions ahead in `src[0..len)`, or `len` if `distance` exceeds
the number of codepoints in the buffer. If `advanced` is non-NULL, writes the
number of codepoints actually skipped. Uses SIMD/SWAR bulk counting internally.

---

### Unsafe backward navigation — `utf8_advance_backward_unsafe.h`

No DFA backend required.

```c
size_t utf8_advance_backward_unsafe(const char *src,
                                    size_t len,
                                    size_t distance,
                                    size_t *advanced);
```

**`utf8_advance_backward_unsafe`** returns the byte offset of the codepoint
`distance` positions before the end of `src[0..len)`, or `0` if `distance`
exceeds the number of codepoints in the buffer. If `advanced` is non-NULL,
writes the number of codepoints actually skipped. Uses SIMD/SWAR bulk
counting internally.

---

### Unsafe forward decoding — `utf8_decode_next_unsafe.h`

No DFA backend required.

```c
int utf8_decode_next_unsafe(const char *src, size_t len, uint32_t *codepoint);
```

**`utf8_decode_next_unsafe`** decodes the codepoint starting at `src[0]`.
Returns bytes consumed (1–4) and writes the codepoint to `*codepoint`.
Returns `0` when `len` is 0.

---

### Unsafe backward decoding — `utf8_decode_prev_unsafe.h`

No DFA backend required.

```c
int utf8_decode_prev_unsafe(const char *src, size_t len, uint32_t *codepoint);
```

**`utf8_decode_prev_unsafe`** decodes the codepoint ending at `src[len-1]`.
Returns bytes consumed (1–4) and writes the codepoint to `*codepoint`.
Returns `0` when `len` is 0.

---

### Unsafe transcoding — `utf8_transcode_unsafe.h`

No DFA backend required.

```c
utf8_transcode_result_t utf8_transcode_utf32_unsafe(const char *src, size_t src_len,
                                                    uint32_t *dst, size_t dst_len);
utf8_transcode_result_t utf8_transcode_utf16_unsafe(const char *src, size_t src_len,
                                                    uint16_t *dst, size_t dst_len);
```

**`utf8_transcode_utf32_unsafe`** and **`utf8_transcode_utf16_unsafe`**
transcode `src[0..src_len)` into `dst[0..dst_len)`. Status is always `OK` or
`EXHAUSTED`; `advance` is always 0. The result struct is identical to the
safe variants for easy interchangeability.

Both functions process 8 bytes at a time when all 8 are ASCII (widening
without decoding) and batch consecutive sequences of the same length class to
avoid re-entering the lead-byte classifier. For UTF-16, codepoints above
U+FFFF are encoded as surrogate pairs.

---

### Unsafe transcoding to a byte order — `utf8_transcode_endian_unsafe.h`

No DFA backend required.

```c
utf8_transcode_result_t utf8_transcode_utf16le_unsafe(const char *src, size_t src_len,
                                                      uint8_t *dst, size_t dst_len);
utf8_transcode_result_t utf8_transcode_utf16be_unsafe(const char *src, size_t src_len,
                                                      uint8_t *dst, size_t dst_len);
utf8_transcode_result_t utf8_transcode_utf32le_unsafe(const char *src, size_t src_len,
                                                      uint8_t *dst, size_t dst_len);
utf8_transcode_result_t utf8_transcode_utf32be_unsafe(const char *src, size_t src_len,
                                                      uint8_t *dst, size_t dst_len);
```

The unsafe counterparts of `utf8_transcode_endian.h`: they write UTF-16LE,
UTF-16BE, UTF-32LE or UTF-32BE as bytes, with `dst_len` and
`result.written` as byte counts and no byte order mark. Status is always
`OK` or `EXHAUSTED`; `advance` is always 0. They use the same ASCII fast
path and same-length-class batching as `utf8_transcode_unsafe.h`.

---

## Error handling and U+FFFD replacement

Ill-formed input should either be rejected or have each ill-formed part
replaced with U+FFFD (REPLACEMENT CHARACTER). Silently skipping it has
security implications; see
[Unicode Security Considerations](https://www.unicode.org/reports/tr36/tr36-15.html).

How many U+FFFD to emit is fixed by the *maximal subpart* rule, recommended
by [Unicode 17.0 §3.9](https://www.unicode.org/versions/Unicode17.0.0/core-spec/chapter-3/#G66453)
and required by the [WHATWG Encoding Standard](https://encoding.spec.whatwg.org/#utf-8-decoder).
A maximal subpart is the longest prefix of an ill-formed sequence that is
either the start of an otherwise well-formed sequence, or a single byte.
Each one is replaced by exactly one U+FFFD.

| Bytes      | Why ill-formed               | Maximal subparts | U+FFFD |
| :---       | :---                         | :---             | ---:   |
| `F0 90 80` | 4-byte sequence cut short    | `F0 90 80`       | 1      |
| `F0 80 80` | `80` cannot follow `F0`      | `F0`, `80`, `80` | 3      |
| `80 80`    | lone continuation bytes      | `80`, `80`       | 2      |

The `_replace` functions do this for you. The strict functions, the scanner
and the validator instead stop and report each subpart, for callers that
reject input, log the error or apply their own substitution. Without such a
need, prefer the `_replace` variants.

## Security

The guarantees below apply to the safe (DFA-based) API. The `_unsafe`
variants perform no validation and assume the caller has already verified
the input; passing ill-formed UTF-8 to an unsafe function is undefined
behaviour.

**All decoded codepoints are within the Unicode scalar value range.** The DFA
structurally rejects non-shortest-form encodings, surrogate halves
(U+D800–U+DFFF), and codepoints above U+10FFFF. These cannot appear in the
output of any safe decoding or transcoding function.

**No dynamic allocation.** All functions (safe and unsafe) operate on
caller-supplied buffers with no heap allocation, no global mutable state, and
no use of `errno`.

**The DFA step does not branch on the input byte.** A step is a table lookup
and a bitwise shift. `utf8_valid` applies it to every byte and never exits
early, so an ill-formed byte does not shorten the scan.

This is not a constant-time guarantee and it does not extend to the rest of
the API. The table lookup is indexed by the input byte, and most functions
branch on the DFA state or on the decoded value: `utf8_check` rescans to find
the cursor on failure, the `_bounded` variants stop at the first ill-formed
block, the `_ascii` variants branch on whether a chunk contains only ASCII
bytes, and the decoders, transcoders, scanner and validator branch on the
state after each byte or sequence. The `_unsafe` variants branch on lead-byte
classification, since they are intended for performance-critical paths where
the input is already trusted.

## Performance

### Corpus

Documents are retrieved from Wikipedia and converted to plain text, available in `benchmark/corpus/`.

| File   |   Size | Code points | Distribution          | Best `utf8_valid` | Best `utf8_valid_ascii` |
| :---   |   ---: |        ---: | :---                  |      ---: |       ---: |
| ar.txt |  25 KB |         14K | 19% ASCII, 81% 2-byte | 6725 MB/s |  5349 MB/s |
| el.txt | 102 KB |         59K | 23% ASCII, 77% 2-byte | 6637 MB/s |  5254 MB/s |
| en.txt |  80 KB |         82K | 99.9% ASCII           | 6582 MB/s | 41071 MB/s |
| ja.txt | 176 KB |         65K | 11% ASCII, 89% 3-byte | 6584 MB/s |  5478 MB/s |
| lv.txt | 135 KB |        127K | 92% ASCII, 7% 2-byte  | 6600 MB/s |  6445 MB/s |
| ru.txt | 148 KB |         85K | 23% ASCII, 77% 2-byte | 6601 MB/s |  4154 MB/s |
| sv.txt |  94 KB |         93K | 96% ASCII, 4% 2-byte  | 6646 MB/s |  9199 MB/s |

Best numbers from `-O2 -march=x86-64-v3` (Raptor Lake). `utf8_valid` uses
dual-stream validation; see [Observations](#observations).

---

### Raptor Lake (Clang 20, x86-64)

| Flags                   | `utf8_valid` | `utf8_valid_ascii` | Notes                                  |
| :---                    |         ---: |               ---: | :---                                   |
| `-O2`                   |    4107 MB/s |          2986 MB/s | dual-stream ILP effective without BMI2 |
| `-O2 -march=x86-64-v3`  |    6394 MB/s |          4175 MB/s | BMI2 `SHRX` on two ports               |
| `-O3 -march=x86-64-v3`  |    6471 MB/s |          5349 MB/s | fast path not profitable on multibyte  |

Numbers shown for ar.txt (81% 2-byte). On near-pure ASCII (en.txt) `utf8_valid_ascii`
reaches 29–35 GB/s at all optimization levels.

### Raptor Lake (GCC 14, x86-64)

| Flags                   | `utf8_valid` | `utf8_valid_ascii` | Notes                                  |
| :---                    |         ---: |               ---: | :---                                   |
| `-O2`                   |    3714 MB/s |          2774 MB/s | dual-stream ILP effective without BMI2 |
| `-O2 -march=x86-64-v3`  |    6725 MB/s |          4125 MB/s | BMI2 `SHRX` on two ports               |
| `-O3 -march=x86-64-v3`  |    6489 MB/s |          4164 MB/s | fast path not profitable on multibyte  |

Numbers shown for ar.txt (81% 2-byte). On near-pure ASCII (en.txt) `utf8_valid_ascii`
reaches 36–41 GB/s at all optimization levels.

### Haswell (Clang 22, x86-64)

| Flags                   | `utf8_valid` | `utf8_valid_ascii` | Notes                          |
| :---                    |         ---: |               ---: | :---                           |
| `-O2`                   |    2370 MB/s |          1756 MB/s | narrow backend limits ILP gain |
| `-O2 -march=x86-64-v3`  |    3674 MB/s |          2626 MB/s | BMI2 `SHRX`                    |
| `-O3 -march=x86-64-v3`  |    3682 MB/s |          3328 MB/s | gap narrows at `-O3`           |

Numbers shown for ar.txt (81% 2-byte). On near-pure ASCII (en.txt) `utf8_valid_ascii`
reaches 16–20 GB/s at all optimization levels.

### Apple M1 Pro (Clang 21, AArch64)

| Flags                  | `utf8_valid` | `utf8_valid_ascii` | Notes                                |
| :---                   |         ---: |               ---: | :---                                 |
| `-O2`                  |    4498 MB/s |          2877 MB/s | dual-stream ILP on wide Firestorm    |
| `-O2 -mtune=apple-m1`  |    4356 MB/s |          2867 MB/s | mtune negligible                     |
| `-O3`                  |    4445 MB/s |          5231 MB/s | `-O3` unlocks NEON fast path         |
| `-O3 -mtune=apple-m1`  |    4228 MB/s |          4866 MB/s | fast path profitable on all content  |

Numbers shown for ar.txt (81% 2-byte). On near-pure ASCII (en.txt) `utf8_valid_ascii`
reaches ~21 GB/s at `-O3`.

### Apple M1 Pro (GCC 15, AArch64)

| Flags                  | `utf8_valid` | `utf8_valid_ascii` | Notes                                 |
| :---                   |         ---: |               ---: | :---                                  |
| `-O2`                  |    4206 MB/s |          2742 MB/s | dual-stream ILP on wide Firestorm     |
| `-O2 -mtune=apple-m1`  |    4214 MB/s |          2738 MB/s | mtune no effect                       |
| `-O3`                  |    4506 MB/s |          2757 MB/s | `-O3` improves over `-O2`             |
| `-O3 -mtune=apple-m1`  |    4453 MB/s |          2881 MB/s | fast path not profitable on multibyte |

Numbers shown for ar.txt (81% 2-byte). On near-pure ASCII (en.txt) `utf8_valid_ascii`
reaches ~36 GB/s at `-O3`.

### Observations

- `utf8_valid` uses a dual-stream validation strategy: the input is split at
  a UTF-8 sequence boundary near the midpoint and two independent DFA chains
  run in a single interleaved loop. Since the chains have no data dependency,
  the CPU's out-of-order engine can overlap both shift operations on cores
  with multiple shift-capable execution ports.
- On wide-issue cores (Raptor Lake P-cores with BMI2, Apple M1 Firestorm),
  dual-stream reaches approximately 1.4 bytes per clock cycle at peak
  throughput.
- On x86, `-march=x86-64-v3` or `-march=native` enables BMI2 `SHRX`, which
  removes the variable-shift dependency on `CL`.
- `utf8_valid_ascii` is profitable on high-ASCII content across all tested
  platforms. On multibyte-heavy content it is generally slower than
  `utf8_valid`, with the exception of Apple M1 with Clang `-O3` where the
  NEON fast path keeps it competitive.

## Benchmark

The benchmark is in `benchmark/bench.c`. Compile and run with:

```sh
make bench
make bench BENCH_OPTFLAGS="-O3 -march=x86-64-v3"
./bench                        # benchmarks benchmark/corpus/
./bench -d <directory>         # benchmarks all .txt files in directory
./bench -f <file>              # benchmarks single file
./bench -s <MB>                # resize input to <MB> before benchmarking
```

**Benchmark mode** (mutually exclusive):

| Flag        | Description                                                |
| :---        | :---                                                       |
| `-t <secs>` | run each implementation for `<secs>` seconds (default: 20) |
| `-n <reps>` | run each implementation for `<reps>` repetitions           |
| `-b <MB>`   | run each implementation until `<MB>` total data processed  |

**Warmup** (`-w <n>`): before timing, each implementation is run for `n`
iterations to warm up caches and branch predictors. By default the warmup
count is derived from input size, targeting approximately 256 MB of warmup
data, capped between 1 and 100 iterations. Use `-w 0` to disable warmup
entirely.

**Output format:** For each file, the header line shows the filename,
byte size, code point count, and average bytes per code point
(`units/point`). The code point distribution breaks down the input by
Unicode range. Results are sorted slowest to fastest; the multiplier
shows throughput relative to the slowest implementation.

```
sv.txt: 94 KB; 93K code points; 1.04 units/point
  U+0000..U+007F          90K  96.4%
  U+0080..U+07FF           3K   3.5%
  U+0800..U+FFFF          171   0.2%
  hoehrmann                    432 MB/s
  utf8_valid_old              1780 MB/s  (4.12x)
  utf8_valid                  4449 MB/s  (10.29x)
  utf8_valid_ascii            9968 MB/s  (23.05x)
```

`units/point` is a rough content-mix indicator: `1.00` is near-pure ASCII,
~`1.7–1.9` is common for 2-byte-heavy text, and ~`2.7–3.0` for CJK-heavy text.

The benchmark includes two reference implementations: `hoehrmann` a widely used
table-driven DFA implementation and `utf8_valid_old` (previous scalar decoder)
to track regression and quantify gains from the current DFA approach.

## License

MIT License. Copyright (c) 2017–2026 Christian Hansen.

## See Also

- [Flexible and Economical UTF-8 Decoder](https://bjoern.hoehrmann.de/utf-8/decoder/dfa/) by Björn Höhrmann
- [Branchless UTF-8 Decoder](https://nullprogram.com/blog/2017/10/06/) by Chris Wellons
- [simdutf](https://github.com/simdutf/simdutf) Unicode validation and transcoding using SIMD
