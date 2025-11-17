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

The project includes four specialized fuzz targets:

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
- **Corpus**: `tests/fuzz/corpus/operations/`

### 4. `fuzz_bitmap_serialization` - Data Parsing Fuzzer
- **Purpose**: Testing data conversion and parsing
- **Coverage**:
  - from_int_array / get_int_array round-trips
  - from_bit_array / get_bit_array with malformed inputs
  - Range operations with edge cases
  - Both 32-bit and 64-bit serialization
- **Corpus**: `tests/fuzz/corpus/serialization/`

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

This will create four executables in `build-fuzz/tests/fuzz/`:
- `fuzz_bitmap_api`
- `fuzz_bitmap64_api`
- `fuzz_bitmap_operations`
- `fuzz_bitmap_serialization`

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

redis-roaring has **full CI/CD integration** for fuzzing in the main workflow.

### Master Branch Fuzzing (`.github/workflows/ci.yml`)

Runs on every push to `master`:
- **Duration**: 10 minutes per fuzzer (40 minutes total)
- **Purpose**: Thorough testing before production deployment
- **Artifacts**: Uploads crashes and corpus

```yaml
# Automatically runs on master branch
# See .github/workflows/ci.yml for full configuration
```

**What it does:**
- - Builds all 4 fuzzers with Clang + sanitizers
- - Runs each fuzzer for 10 minutes
- - Uploads crash files as artifacts (90 days retention)
- - Uploads evolved corpus (30 days retention)
- - Fails the build if crashes are found

**Why master only?**
- Provides thorough testing without slowing down PRs
- 40-minute fuzzing is substantial for regression detection
- Corpus evolves with each master push
- Crashes block deployment automatically

### Viewing Results

**On Master Push:**
1. Go to the Actions tab
2. Click on the workflow run
3. Check the "Fuzz Testing" job
4. Download artifacts if crashes were found

### Artifact Details

**Crash Artifacts**: `fuzzing-crashes-<commit-sha>`
- Retention: 90 days
- Contains all crash files from the run
- Named by commit SHA for easy tracking

**Corpus Artifacts**: `fuzzing-corpus`
- Retention: 30 days
- Contains evolved test inputs
- Grows over time with interesting cases

## Best Practices

### 1. Regular Fuzzing Runs

- Run fuzzers on every PR (5-10 minutes)
- Long fuzzing campaigns weekly or monthly (24+ hours)
- Maintain and grow corpus over time

### 2. Corpus Maintenance

- Commit interesting corpus inputs to repository
- Periodically merge and minimize corpus
- Share corpus between CI runs using artifacts

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
