# Fuzzing Gate

`tests/fuzz/fuzz_manifest.md` is the command-surface coverage contract for fuzzing. Every key-bearing command must appear there, and every fuzzer named there must have:

- a source file in `tests/fuzz/`
- a seed corpus directory in `tests/fuzz/corpus/`
- a build entry in `CMakeLists.txt`
- CI coverage and smoke entries in `.github/workflows/ci.yml`

`./scripts/check_fuzz_coverage.sh` enforces those invariants. CI runs it on every push and pull request.

When adding a new key-bearing command:

1. Register the command and its `SetCommandInfo` metadata.
2. Add a row to `tests/fuzz/fuzz_manifest.md`.
3. Extend `tests/fuzz/fuzz_command_manifest.h` if the command surface changes.
4. Add or update seed corpus entries if a new fuzz target is needed.
5. Make sure the target is wired into `CMakeLists.txt` and `.github/workflows/ci.yml`.
