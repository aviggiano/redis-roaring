# Migrate from redis-roaring to Redis

Redis 8.12 brings Roaring bitmaps into Redis core. If you already use the
`redis-roaring` module, copy your bitmap keys to a Redis 8.12+ server and
update the commands your application uses. [Redis bitmaps](https://redis.io/docs/latest/develop/data-types/strings/bitmaps/)

## Prepare the source and target

- Start with two separate standalone Redis instances. Keep the module loaded on
  the source and use a Redis 8.12+ target. The migrator reads the legacy module
  keys through `R.*` and `R64.*` commands and creates native `bitmap` keys on
  the target. Load legacy Redis snapshots on the source with the module.
- Back up the source and rehearse the migration against a staging target.
  Plan a separate transfer for other Redis key types. The migrator copies only
  `reroaring` and `roaring64` keys.
- Check your key scope. The examples use database 0. Add `--all-dbs` to discover
  populated source databases, repeat `--db` to choose specific databases, or
  use `--pattern` to select keys by name. Use the same scope for dry run and apply.

Use Python 3 to run the tested migrator from the `v1.7.5` release.

```sh
git clone --depth 1 --branch v1.7.5 https://github.com/aviggiano/redis-roaring.git
cd redis-roaring
```

## 1. Inventory the keys with a dry run

The examples use a source on port 6379 and a target on port 6380. Replace the
addresses and ports with your own. The dry run writes a local JSON manifest and
leaves Redis keys untouched.

```sh
python3 tools/redis-bitmap-migrate.py \
  --source-host 127.0.0.1 --source-port 6379 \
  --target-host 127.0.0.1 --target-port 6380 \
  --manifest redis-bitmap-plan.json --keep-going
```

`--keep-going` continues the inventory after a per-key error. The dry run
still exits with a failure status if any key fails. Review the manifest and
resolve failures before applying. The migrator accepts
set-bit offsets through `4,294,967,295` with Redis's default
`proto-max-bulk-len`. A lower target setting can reduce the limit. An `R64.*`
key above the migrator's cap fails by default. Use `--cap-policy skip` only
if you have another plan for those keys. The manifest records each skipped key.

## 2. Apply with source writes frozen

Rehearse an apply against staging to verify target writes. Use fresh manifest
paths for each rehearsal and for the production migration. For the production
copy, stop application writes to the source before running this command and
keep them paused through verification and cutover.
`--assume-frozen` records that you have already stopped writes.

```sh
python3 tools/redis-bitmap-migrate.py \
  --source-host 127.0.0.1 --source-port 6379 \
  --target-host 127.0.0.1 --target-port 6380 \
  --manifest redis-bitmap-apply.json \
  --apply --assume-frozen
```

The migrator validates each temporary bitmap before moving it into place.
If a destination key already exists, the default is to report a collision
and preserve the destination. To retry an interrupted apply, add `--resume`
with the same apply manifest, endpoints, and options. Keep source writes
frozen for the retry.

## 3. Update and verify your application

Use the native Redis commands for migrated keys. Review callers of both
`R.*` and `R64.*` commands.

| Module command family | Redis 8.12+ |
| --- | --- |
| `SETBIT`, `GETBIT`, `BITCOUNT` | Use the unprefixed command. |
| `BITPOS` | Use `BITPOS`. For an existing empty bitmap, a search for `0` returns `-1` in native Redis and `0` in the module. |
| `GETINTARRAY` | Use `BITRANGEINTARRAY key 0 -1` to list the set-bit offsets. |
| `RANGEINTARRAY` | Use `BITRANGEINTARRAY` with bit or byte offsets. See the pagination example below. |
| `BITOP` | Redis returns the result's length in bytes. Use `BITCOUNT destkey` if the application needs the number of set bits. |

Native `BITOP NOT` flips all bits through the end of the source's logical byte
length. The module flips through the highest set bit by default and accepts an
optional last offset. Native `BITOP` also clears any existing destination TTL
when it writes a result. Update callers that rely on the module's flip range
or on the destination retaining its TTL.

The module's range commands page by rank among set bits. With native
`BITRANGEINTARRAY`, start with `BITRANGEINTARRAY key 0 -1 BIT LIMIT 1000`.
If the page contains 1000 offsets, get the next page by setting the start
to the last returned offset plus one. A shorter page ends the scan.
Review module-specific operations such as `JACCARD` and `OPTIMIZE` for
application changes. [BITRANGEINTARRAY reference](https://redis.io/docs/latest/commands/bitrangeintarray/) · [BITOP reference](https://redis.io/docs/latest/commands/bitop/)

Before switching traffic, review the apply manifest for failed or skipped
keys. Spot-check representative keys with `TYPE` (expect `bitmap`),
`OBJECT ENCODING` (expect `bitmap-roaring`), `BITCOUNT`, `BITRANGEINTARRAY`,
and `PTTL`. Verify that the target can reload its persisted data and that the
updated application behaves as expected. Keep the source and its backup
available until you have verified the cutover.

Migrated keys are compressed even when `bitmap-default-roaring` is `no`.
Set `bitmap-default-roaring yes` in the target configuration
if you also want new application-created bitmaps to use Roaring compression.
Native `bitmap` values require bitmap commands. Update callers that access
raw bytes with string commands such as `GET`. [Configuration and compatibility](https://redis.io/docs/latest/develop/data-types/strings/bitmaps/#roaring-compressed-bitmaps)
