# Fuzz Manifest

This manifest is the command-surface coverage contract for redis-roaring fuzzing. The CI gate checks that every registered key-bearing command appears here and that every declared fuzz target has a source file, corpus, CMake entry, and CI execution path.

## Target Inventory

- `fuzz_bitmap_api`: 32-bit bitmap helper and low-level semantic invariants. Corpus: `bitmap_api`.
- `fuzz_bitmap64_api`: 64-bit bitmap helper and low-level semantic invariants. Corpus: `bitmap64_api`.
- `fuzz_bitmap_operations`: shared BITOP helper semantics. Corpus: `bitmap_operations`.
- `fuzz_bitmap_serialization`: serialization and round-trip helper invariants. Corpus: `bitmap_serialization`.
- `fuzz_bitop_keys`: `BITOP` getkeys and key-position invariants. Corpus: `bitop_keys`.
- `fuzz_command_metadata`: manifest-driven command metadata, arity, and R/R64 parity checks. Corpus: `command_metadata`.

## Command Coverage

| Command | Targets | Seed Corpus | Metadata | Dispatch | Routing | Persistence | Parity | Invariants / Oracles |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `R.SETBIT` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | single-key arity bounds, key-spec flags, 32/64 metadata parity |
| `R.GETBIT` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | single-key read metadata and parity |
| `R.GETBITS` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | variadic offset arity floor and single-key metadata parity |
| `R.CLEARBITS` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | variadic offset arity floor and single-key metadata parity |
| `R.SETINTARRAY` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | variadic value arity floor and insert-key metadata parity |
| `R.GETINTARRAY` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | fixed-arity single-key metadata parity |
| `R.RANGEINTARRAY` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | range command arity and single-key metadata parity |
| `R.APPENDINTARRAY` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | append arity floor and key flag parity |
| `R.DELETEINTARRAY` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | delete arity floor and key flag parity |
| `R.DIFF` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | dest/source key-spec layout, fixed 3-key expansion, 32/64 parity |
| `R.SETFULL` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | overwrite-key metadata parity |
| `R.SETRANGE` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | fixed-arity single-key metadata parity |
| `R.OPTIMIZE` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | optional token arity and mutating key metadata parity |
| `R.SETBITARRAY` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | fixed-arity insert-key metadata parity |
| `R.GETBITARRAY` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | fixed-arity read metadata parity |
| `R.BITOP` | `fuzz_command_metadata`, `fuzz_bitop_keys` | `command_metadata`, `bitop_keys` | yes | no | no | no | yes | generic key-spec envelope, operation-specific getkeys arity, NOT vs variadic source layout, R/R64 parity |
| `R.BITCOUNT` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | fixed-arity single-key metadata parity |
| `R.BITPOS` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | value-token arity and single-key metadata parity |
| `R.MIN` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | fixed-arity single-key metadata parity |
| `R.MAX` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | fixed-arity single-key metadata parity |
| `R.CLEAR` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | delete-key metadata parity |
| `R.CONTAINS` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | two-key range spec, optional mode arity, 32/64 parity |
| `R.JACCARD` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | two-key range spec and 32/64 parity |
| `R64.SETBIT` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | single-key arity bounds, key-spec flags, 32/64 metadata parity |
| `R64.GETBIT` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | single-key read metadata and parity |
| `R64.GETBITS` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | variadic offset arity floor and single-key metadata parity |
| `R64.CLEARBITS` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | variadic offset arity floor and single-key metadata parity |
| `R64.SETINTARRAY` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | variadic value arity floor and insert-key metadata parity |
| `R64.GETINTARRAY` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | fixed-arity single-key metadata parity |
| `R64.RANGEINTARRAY` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | range command arity and single-key metadata parity |
| `R64.APPENDINTARRAY` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | append arity floor and key flag parity |
| `R64.DELETEINTARRAY` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | delete arity floor and key flag parity |
| `R64.DIFF` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | dest/source key-spec layout, fixed 3-key expansion, 32/64 parity |
| `R64.SETFULL` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | overwrite-key metadata parity |
| `R64.SETRANGE` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | fixed-arity single-key metadata parity |
| `R64.OPTIMIZE` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | optional token arity and mutating key metadata parity |
| `R64.SETBITARRAY` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | fixed-arity insert-key metadata parity |
| `R64.GETBITARRAY` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | fixed-arity read metadata parity |
| `R64.BITOP` | `fuzz_command_metadata`, `fuzz_bitop_keys` | `command_metadata`, `bitop_keys` | yes | no | no | no | yes | generic key-spec envelope, operation-specific getkeys arity, NOT vs variadic source layout, R/R64 parity |
| `R64.BITCOUNT` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | fixed-arity single-key metadata parity |
| `R64.BITPOS` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | value-token arity and single-key metadata parity |
| `R64.MIN` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | fixed-arity single-key metadata parity |
| `R64.MAX` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | fixed-arity single-key metadata parity |
| `R64.CLEAR` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | delete-key metadata parity |
| `R64.CONTAINS` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | two-key range spec, optional mode arity, 32/64 parity |
| `R64.JACCARD` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | yes | two-key range spec and 32/64 parity |
| `R.STAT` | `fuzz_command_metadata` | `command_metadata` | yes | no | no | no | no | optional format token, single-key metadata, root command coverage |
