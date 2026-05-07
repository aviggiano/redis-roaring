# Fuzzing Gate

`tests/fuzz/fuzz_manifest.json` is the authoritative command-surface coverage contract for fuzzing. Every key-bearing command must appear there, the metadata-fuzzer descriptor table must enumerate the same command set, and every manifest target must have:

- a source file in `tests/fuzz/`
- a seed corpus directory in `tests/fuzz/corpus/`
- a build entry in `CMakeLists.txt`
- CI coverage and smoke entries in `.github/workflows/ci.yml`

`python3 ./scripts/validate_fuzz_manifest.py` is the CI gate and runs on every push and pull request. `./scripts/check_fuzz_coverage.sh` is the broader local consistency check that also compares command registration, descriptor coverage, and the CMake/workflow fuzzer inventories.

`tests/fuzz/fuzz_manifest.md` is the human-readable summary of the same command-surface coverage. Low-level API fuzzers that are not part of the command manifest are documented in `docs/fuzzing.md`.

When adding a new key-bearing command:

1. Register the command and its `SetCommandInfo` metadata.
2. Add or update the command family entry in `tests/fuzz/fuzz_manifest.json`.
3. Update `tests/fuzz/fuzz_command_manifest.h` if the command surface changes so the metadata fuzzer keeps covering the full manifest.
4. Add or update seed corpus entries if a new fuzz target is needed.
5. Make sure the target is wired into `CMakeLists.txt` and `.github/workflows/ci.yml`.
6. Run `python3 ./scripts/validate_fuzz_manifest.py` and `./scripts/check_fuzz_coverage.sh`.
