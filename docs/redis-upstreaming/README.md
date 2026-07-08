# Redis native bitmap upstreaming — working documents

These documents track the effort to upstream a native Roaring-compressed
bitmap type into Redis core:

- Upstream issue: https://github.com/redis/redis/issues/15296
- Upstream draft PR: https://github.com/redis/redis/pull/15331

They were originally maintained inside the `docs/` directory of the
[aviggiano/redis](https://github.com/aviggiano/redis) fork and were moved
here so the upstream pull request stays a clean product diff. They are
point-in-time working documents: where they disagree with the code or the PR
discussion, the PR discussion wins.

Contents:

- `redis-roaring-pr-breakdown.md` — the phased implementation plan.
- `redis-roaring-native-bitmap-design.md` — the design document and behavior
  matrix.
- `redis-roaring-native-bitmap-decision-packets.md` — condensed decision
  packets for the open design questions (DD-01..DD-17).
- `redis-roaring-native-bitmap-rdb-format.md` — the RDB payload layout notes.
- `redis-roaring-migration-contract.md` — the contract for migrating data
  from this module to the native Redis bitmap type (see
  `tools/redis-bitmap-migrate.py`).
- `redis-roaring-test-porting.md` — how this module's test suites map to the
  Redis core test coverage.
- `redis-roaring-native-bitmap-benchmark-gate.md` — the benchmark bar the
  native implementation is held to (see `tools/bitmap-bench.py` and the
  `Bitmap Benchmark` workflow).
