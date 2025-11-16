# CI/CD Fuzzing Integration

## Overview

Fuzzing has been fully integrated into the redis-roaring CI/CD pipeline, running on every push to the `master` branch.

## Configuration

### Master Branch Fuzzing (`.github/workflows/ci.yml`)

**Trigger**: Every push to `master`
**Duration**: ~40 minutes (10 min per fuzzer)
**Purpose**: Thorough testing before production deployment

#### Job Configuration

```yaml
Job: fuzz
Condition: if: github.ref == 'refs/heads/master'
Steps:
  1. Checkout with submodules
  2. Install Clang and CMake
  3. Build all fuzzers
  4. Run 4 fuzzers (10 min each):
     - fuzz_bitmap_api
     - fuzz_bitmap64_api
     - fuzz_bitmap_operations
     - fuzz_bitmap_serialization
  5. Check for crashes
  6. Upload artifacts
```

#### Artifact Retention

- **Crashes**: 90 days (named by commit SHA)
- **Corpus**: 30 days

#### Failure Behavior

- Job fails if crashes are detected
- Blocks deployment to production
- Crashes uploaded as artifacts with commit SHA for tracking

## Benefits

### Continuous Security Testing

✅ Every master push tested for memory safety issues
✅ 40 minutes of thorough fuzzing per deployment
✅ Automatic crash detection and reporting
✅ Growing corpus improves coverage over time

### Developer Experience

✅ PRs not slowed down by fuzzing
✅ Master branch thoroughly tested before deployment
✅ Crashes block deployment automatically
✅ Artifacts available for debugging with 90-day retention

### Coverage

- **AddressSanitizer**: Buffer overflows, use-after-free, memory leaks
- **UndefinedBehaviorSanitizer**: Integer overflows, null dereferences
- **4 Specialized Fuzzers**: Complete API coverage

## Monitoring and Debugging

### Viewing Results

**For Master pushes:**
```
GitHub → Actions → Select workflow run → Fuzz Testing job
```

The fuzzing job only runs on master branch pushes.

### Downloading Artifacts

1. Navigate to workflow run
2. Scroll to "Artifacts" section
3. Download:
   - `fuzzing-crashes-<commit-sha>` (if any crashes found)
   - `fuzzing-corpus` (evolved corpus)

### Reproducing Crashes Locally

```bash
# Download crash file from artifacts
wget <artifact-url>/crash-abc123

# Reproduce
./scripts/reproduce_crash.sh crash-abc123

# Minimize
./scripts/minimize_crash.sh crash-abc123 bitmap_api

# Debug
gdb --args ./build-fuzz/tests/fuzz/fuzz_bitmap_api crash-abc123
```

## Statistics and Reporting

### Master Runs

- Pass/fail status visible in Actions tab
- Crash count if failures occur
- Corpus uploaded for corpus evolution
- Artifacts retained for 90 days (crashes) / 30 days (corpus)

## Maintenance

### Corpus Management

The system automatically manages corpus:
- Each master run uses the previous corpus
- Corpus evolves with each run
- **Retention**: 30 days

To reset corpus (if needed):
1. Delete `fuzzing-corpus` artifact
2. Next run will start from seed corpus

### Adjusting Duration

Edit workflow file to change fuzzing duration:

```yaml
# In ci.yml
timeout 600  # 10 minutes = 600 seconds
```

Change `600` to desired duration in seconds.

## Continuous Improvement

The CI setup enables continuous improvement:

1. **Growing Coverage**: Corpus evolves, finding new code paths
2. **Regression Prevention**: Fixed bugs stay fixed
3. **Early Detection**: Bugs caught before production
4. **Knowledge Base**: Crash artifacts preserved for analysis

## Performance Impact

### Master Branch Checks

- **Additional time**: ~40 minutes
- **Sequential execution**: Runs after tests pass
- **Frequency**: Only on master pushes (not PRs)
- **Impact**: No slowdown for PRs, thorough testing for production

## Cost Optimization

GitHub Actions minutes used:
- **Master pushes**: ~40 min per push
- **Estimated monthly**: ~20-40 hours (depends on push frequency)

For public repositories: Free
For private repositories: Within reasonable limits for production testing

## Future Enhancements

Potential improvements:
1. **Coverage reports**: Integrate llvm-cov for coverage analysis
2. **Differential fuzzing**: Compare 32-bit vs 64-bit results
3. **Dictionaries**: Add structure-aware dictionaries
4. **AFL++ integration**: Optional alternative fuzzer
5. **OSS-Fuzz**: Submit to Google's OSS-Fuzz platform

## References

- [libFuzzer Documentation](https://llvm.org/docs/LibFuzzer.html)
- [GitHub Actions Documentation](https://docs.github.com/en/actions)
- [Trail of Bits Fuzzing Guide](https://appsec.guide/docs/fuzzing/c-cpp/)

## Files Modified

### Modified Files
- `.github/workflows/ci.yml` - Added fuzz job for master branch
- `README.md` - Added fuzzing CI info
- `docs/fuzzing.md` - Added CI integration section

## Summary

**Strategy**: Master-only fuzzing with 10-minute duration per target
**Rationale**:
- Provides thorough testing (40 min total) without slowing PRs
- Blocks deployment if crashes found
- Evolves corpus over time
- Balances thoroughness with CI efficiency

---

**Status**: ✅ Fully integrated and operational
**Date**: 2025-11-16
