# Fuzzing Guide for redis-roaring

This document describes the fuzzing infrastructure for redis-roaring and how to use it.

## Overview

redis-roaring includes comprehensive fuzz testing using **libFuzzer** with AddressSanitizer (ASan) and UndefinedBehaviorSanitizer (UBSan) to detect:
- Memory safety issues (buffer overflows, use-after-free, memory leaks)
- Undefined behavior (integer overflows, null pointer dereferences)
- Logic errors and edge cases in bitmap operations

## Prerequisites

Fuzzing requires:
- **Clang compiler** (version 6.0 or later recommended)
- **CMake** 3.5 or later
- Linux or macOS (Windows support may be limited)

## Fuzz Targets

The project includes ten specialized fuzz targets. Five are pure in-process API fuzzers and five exercise the module through a real Redis server.

### 1. `fuzz_bitmap_api` - 32-bit Bitmap API Fuzzer
- **Purpose**: Comprehensive testing of all 32-bit bitmap operations
- **Coverage**:
  - SETBIT/GETBIT operations
  - Integer and bit array conversions
  - All BITOP operations (AND, OR, XOR, NOT, ANDOR, ANDNOT, ORNOT, ONE)
  - Range operations, statistics, optimization
  - Jaccard similarity and intersection modes
- **Corpus**: `tests/fuzz/corpus/bitmap_api/`

### 2. `fuzz_bitmap64_api` - 64-bit Bitmap API Fuzzer
- **Purpose**: Testing 64-bit bitmap operations with large values
- **Coverage**: Same as 32-bit API but with uint64_t offsets
- **Focus**: Edge cases with values beyond 32-bit range
- **Corpus**: `tests/fuzz/corpus/bitmap64_api/`

### 3. `fuzz_bitmap_operations` - Focused Operations Fuzzer
- **Purpose**: Deep testing of complex bitmap operations
- **Coverage**:
  - Operations with 2-10 input bitmaps
  - Various input patterns (empty, sparse, dense, ranges)
  - Invariant checking for operation correctness
- **Corpus**: `tests/fuzz/corpus/bitmap_operations/`

### 4. `fuzz_bitmap_serialization` - Data Parsing Fuzzer
- **Purpose**: Testing data conversion and parsing
- **Coverage**:
  - from_int_array / get_int_array round-trips
  - from_bit_array / get_bit_array with malformed inputs
  - Range operations with edge cases
  - Both 32-bit and 64-bit serialization
- **Corpus**: `tests/fuzz/corpus/bitmap_serialization/`

### 5. `fuzz_bitop_keys` - BITOP Key Discovery Fuzzer
- **Purpose**: Testing module key-extraction metadata used by cluster routing and functions
- **Coverage**:
  - `R.BITOP` / `R64.BITOP` key-position reporting
  - Variadic vs unary BITOP arity handling
  - Destination/source key flag assignments
  - Unsupported-operation and invalid-arity behavior
- **Corpus**: `tests/fuzz/corpus/bitop_keys/`

### 6. `fuzz_command_metadata` - Command Metadata Fuzzer
- **Purpose**: Validates command-family coverage, `COMMAND GETKEYS`, and valid/invalid runtime shapes
- **Coverage**:
  - Manifest-backed command family coverage
  - Metadata and runtime agreement for key extraction and arity handling
  - Introspection-only paths that must not mutate the keyspace
  - Valid runtime dispatch for every manifest family and non-mutation on invalid forms
- **Corpus**: `tests/fuzz/corpus/command_metadata/`

### 7. `fuzz_command_dispatch` - BITOP Command Semantics Fuzzer
- **Purpose**: Exercises Redis-backed `R.BITOP` / `R64.BITOP` semantics directly
- **Coverage**:
  - Reply cardinality vs oracle cardinality
  - Destination/source aliasing
  - Invalid-operation and wrong-arity non-mutation guarantees
- **Corpus**: `tests/fuzz/corpus/command_dispatch/`

### 8. `fuzz_cluster_routing` - Cluster Routing Fuzzer
- **Purpose**: Validates clustered BITOP routing behavior through a Redis server in cluster mode
- **Coverage**:
  - Same-slot success cases for unary and variadic BITOP
  - Cross-slot `CROSSSLOT` failures
  - Function-routing coverage through `FCALL` where the Redis version supports functions
  - Destination and source immutability on routing errors
- **Corpus**: `tests/fuzz/corpus/cluster_routing/`

### 9. `fuzz_persistence_sequences` - Persistence Round-Trip Fuzzer
- **Purpose**: Checks BITOP command sequences across RDB and AOF reloads
- **Coverage**:
  - Multi-step Redis-backed command sequences
  - `SAVE` + restart round-trips
  - `BGREWRITEAOF` + restart round-trips
- **Corpus**: `tests/fuzz/corpus/persistence_sequences/`

### 10. `fuzz_r_vs_r64_parity` - 32/64 Parity Fuzzer
- **Purpose**: Compares every paired `R.*` / `R64.*` command family within the shared 32-bit-safe range
- **Coverage**:
  - Reply parity across the full paired command surface
  - Final bitmap parity for every mirrored key touched by the command
  - Shared-range allowlist handling for intentional differences such as `SETFULL`
- **Corpus**: `tests/fuzz/corpus/r_vs_r64_parity/`

## Manifest Coverage

`tests/fuzz/fuzz_manifest.json` maps every registered command to at least one fuzz family.

- `python3 scripts/validate_fuzz_manifest.py` verifies that the manifest covers every registered command exactly once
- the validator also checks that each family points at real fuzz targets and corpus directories
- paired command families now declare both runtime dispatch and parity coverage through `fuzz_r_vs_r64_parity`
- CI runs the validator before the fuzz jobs, and `scripts/build_fuzzers.sh` runs it locally before building

## Building Fuzzers

### Step 1: Configure with Fuzzing Enabled

```bash
mkdir build-fuzz
cd build-fuzz
CC=clang CXX=clang++ cmake -DENABLE_FUZZING=ON ..
```

### Step 2: Build

```bash
make
```

This will create ten executables in `build-fuzz/tests/fuzz/`:
- `fuzz_bitmap_api`
- `fuzz_bitmap64_api`
- `fuzz_bitmap_operations`
- `fuzz_bitmap_serialization`
- `fuzz_bitop_keys`
- `fuzz_command_metadata`
- `fuzz_command_dispatch`
- `fuzz_cluster_routing`
- `fuzz_persistence_sequences`
- `fuzz_r_vs_r64_parity`

For a one-shot local build, `./scripts/build_fuzzers.sh` also:
- validates the fuzz manifest
- builds vendored Redis and hiredis
- regenerates the CRoaring amalgamation used by the module build
- regenerates the seed corpus

## Running Fuzzers

### Basic Usage

Run a fuzzer for 60 seconds:

```bash
./tests/fuzz/fuzz_bitmap_api -max_total_time=60
```

### Common Options

```bash
# Run for specific duration (seconds)
./tests/fuzz/fuzz_bitmap_api -max_total_time=300

# Limit memory usage (MB)
./tests/fuzz/fuzz_bitmap_api -rss_limit_mb=2048

# Use multiple workers (parallel fuzzing)
./tests/fuzz/fuzz_bitmap_api -workers=4 -jobs=4

# Minimize a crash input
./tests/fuzz/fuzz_bitmap_api -minimize_crash=1 crash-file

# Reproduce a specific input
./tests/fuzz/fuzz_bitmap_api crash-file

# Use custom corpus directory
./tests/fuzz/fuzz_bitmap_api corpus_dir/
```

### Recommended Fuzzing Duration

- **Quick CI check**: 5-10 minutes per fuzzer
- **Thorough testing**: 1+ hour per fuzzer
- **Deep fuzzing campaign**: 24+ hours per fuzzer

## Understanding Output

### Normal Operation

```
#1      INITED cov: 234 ft: 567 corp: 1/23b exec/s: 0 rss: 45Mb
#8      NEW    cov: 245 ft: 589 corp: 2/45b exec/s: 0 rss: 45Mb
#16     NEW    cov: 256 ft: 612 corp: 3/67b exec/s: 0 rss: 46Mb
```

- `cov`: Code coverage (edges covered)
- `ft`: Feature count (unique code paths)
- `corp`: Corpus size (number of interesting inputs / total bytes)
- `exec/s`: Executions per second
- `rss`: Memory usage

### Crash Detection

When a crash is found:

```
==12345==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x...
```

The fuzzer will:
1. Save the crashing input to a file (e.g., `crash-1234567890abcdef`)
2. Print a detailed error report
3. Exit

## Handling Crashes

### Step 1: Reproduce the Crash

```bash
./tests/fuzz/fuzz_bitmap_api crash-1234567890abcdef
```

### Step 2: Minimize the Input

```bash
./tests/fuzz/fuzz_bitmap_api -minimize_crash=1 crash-1234567890abcdef
```

This creates a smaller input that triggers the same crash.

### Step 3: Debug

Use GDB or LLDB with the minimized input:

```bash
gdb --args ./tests/fuzz/fuzz_bitmap_api minimized-crash
(gdb) run
(gdb) backtrace
```

### Step 4: Fix and Verify

1. Fix the bug in the source code
2. Rebuild the fuzzer
3. Verify the fix: `./tests/fuzz/fuzz_bitmap_api crash-file`
4. Run fuzzer again to ensure no regression

## Corpus Management

### Growing the Corpus

Fuzzers automatically discover interesting inputs and add them to the corpus. To save your corpus:

```bash
# Run with corpus directory
./tests/fuzz/fuzz_bitmap_api corpus_dir/

# Fuzzer will add new interesting inputs to corpus_dir/
```

### Merging Corpora

After multiple fuzzing runs:

```bash
mkdir merged_corpus
./tests/fuzz/fuzz_bitmap_api -merge=1 merged_corpus/ corpus1/ corpus2/ corpus3/
```

### Minimizing Corpus

Remove redundant inputs:

```bash
mkdir minimized_corpus
./tests/fuzz/fuzz_bitmap_api -merge=1 minimized_corpus/ corpus_dir/
```

## CI/CD Integration

redis-roaring runs fuzzing in `.github/workflows/ci.yml` on `push`, `pull_request`, and a nightly `schedule`.

### Workflow Coverage

- `fuzz_manifest` validates `tests/fuzz/fuzz_manifest.json`
- `fuzz` builds all ten fuzzers and runs a short ASan/UBSan-backed smoke pass per target on every push and PR
- `fuzz_coverage` restores cached corpora, layers them over the committed seeds, then builds the same matrix with LLVM coverage instrumentation and exports LCOV data
- `fuzz_nightly` reuses cached corpora for longer scheduled campaigns, minimizes the evolved corpus, and uploads crash and minimized-corpus artifacts

### Viewing Results

1. Go to the Actions tab
2. Open the relevant workflow run
3. Inspect the `Fuzz Testing` or `Fuzzing Coverage` matrix jobs
4. Download crash or corpus artifacts from the matching matrix entry when needed

### Artifact Details

- `fuzz-<target>-artifacts` preserves smoke-run crash files and the minimized corpus directory for that target
- `nightly-fuzz-<target>-artifacts` preserves the same data for scheduled long runs
- the workflow cache keeps rolling corpus state between runs, while artifacts provide point-in-time download and triage snapshots

## Best Practices

### 1. Regular Fuzzing Runs

- Run fuzzers on every PR (5-10 minutes)
- Long fuzzing campaigns weekly or monthly (24+ hours)
- Maintain and grow corpus over time

### 2. Corpus Maintenance

- Commit interesting corpus inputs to repository
- Periodically merge and minimize corpus
- Share corpus between CI runs using the workflow cache, and keep artifacts for reproducible snapshots

### 3. Crash Triage

- Reproduce all crashes before fixing
- Minimize crash inputs for easier debugging
- Add regression tests for fixed crashes
- Update corpus with edge cases

### 4. Coverage Analysis

To generate coverage reports:

```bash
# Build with coverage
CC=clang CXX=clang++ cmake -DENABLE_FUZZING=ON \
  -DCMAKE_CXX_FLAGS="-fprofile-instr-generate -fcoverage-mapping" ..
make

# Run fuzzer
LLVM_PROFILE_FILE="fuzzing.profraw" ./tests/fuzz/fuzz_bitmap_api -runs=10000

# Generate report
llvm-profdata merge -sparse fuzzing.profraw -o fuzzing.profdata
llvm-cov show ./tests/fuzz/fuzz_bitmap_api \
  -instr-profile=fuzzing.profdata \
  -format=html -output-dir=coverage_html
```

## Troubleshooting

### Fuzzer Doesn't Find New Inputs

- Ensure corpus has good seed inputs
- Try different fuzzing strategies (add dictionaries)
- Run for longer duration
- Use multiple workers for parallel fuzzing

### Out of Memory Errors

- Reduce `-rss_limit_mb` value
- Limit input size: `-max_len=10000`
- Check for memory leaks in code

### Slow Fuzzing Speed

- Use `-O1` or `-O2` optimization
- Reduce sanitizer overhead (disable UBSan for speed tests)
- Simplify fuzz target if too complex

### False Positives

- Review sanitizer suppressions
- Check if issue is in CRoaring library vs wrapper code
- Verify initialization of all variables

## Advanced Topics

### Custom Dictionaries

Create `bitmap.dict`:

```
# Common bitmap sizes
"1000"
"65536"
"4294967295"

# Operation codes
"\x00\x01\x02\x03"
```

Use with: `./tests/fuzz/fuzz_bitmap_api -dict=bitmap.dict`

### Structure-Aware Fuzzing

The fuzzers use `FuzzedDataProvider` for structure-aware fuzzing:
- Integers are generated as proper types, not random bytes
- Operations are selected from valid ranges
- Array sizes are reasonable

### Differential Fuzzing

Compare outputs between 32-bit and 64-bit operations:

```cpp
// In fuzzer code
Bitmap* b32 = bitmap_from_int_array(size, array32);
Bitmap64* b64 = bitmap64_from_int_array(size, array64);

// Compare results where applicable
```

## Resources

- [libFuzzer Documentation](https://llvm.org/docs/LibFuzzer.html)
- [Trail of Bits Testing Handbook - C/C++ Fuzzing](https://appsec.guide/docs/fuzzing/c-cpp/)
- [FuzzedDataProvider](https://github.com/llvm/llvm-project/blob/main/compiler-rt/include/fuzzer/FuzzedDataProvider.h)
- [AddressSanitizer](https://clang.llvm.org/docs/AddressSanitizer.html)
- [UndefinedBehaviorSanitizer](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html)

## Support

For issues or questions about fuzzing:
- Open an issue on GitHub
- Check existing crash reports
- Review sanitizer output carefully
