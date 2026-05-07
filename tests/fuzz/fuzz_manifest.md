# Fuzz Manifest

`tests/fuzz/fuzz_manifest.json` is the authoritative command-surface coverage contract. This file is the human-readable summary of that manifest.

## Command-Surface Targets

- `fuzz_command_metadata`: every registered key-bearing command is exercised through metadata, arity, and key-extraction oracles.
- `fuzz_bitop_keys`: targeted `BITOP` `getkeys-api` and key-position regression coverage.
- `fuzz_command_dispatch`: Redis-backed `R.BITOP` / `R64.BITOP` command-semantic coverage.
- `fuzz_cluster_routing`: same-slot, cross-slot, and function-routing coverage for clustered `BITOP` execution where supported.
- `fuzz_persistence_sequences`: Redis-backed `BITOP` persistence and replay coverage across RDB/AOF round trips.
- `fuzz_r_vs_r64_parity`: Redis-backed `R.BITOP` / `R64.BITOP` differential coverage in the shared 32-bit-safe range.

## Coverage Notes

- The manifest currently covers every registered key-bearing command.
- All families have metadata coverage.
- The `bitop` family is additionally covered by dispatch, routing, persistence, and parity fuzzers.
- The lower-level helper/API fuzzers (`fuzz_bitmap_api`, `fuzz_bitmap64_api`, `fuzz_bitmap_operations`, `fuzz_bitmap_serialization`) are documented in `docs/fuzzing.md` and are intentionally outside the command manifest.

## Updating Coverage

When the command surface changes:

1. Update `tests/fuzz/fuzz_manifest.json`.
2. Update `tests/fuzz/fuzz_command_manifest.h` if metadata-descriptor coverage changed.
3. Add or update any required seed corpora.
4. Run `python3 ./scripts/validate_fuzz_manifest.py`.
5. Run `./scripts/check_fuzz_coverage.sh`.
