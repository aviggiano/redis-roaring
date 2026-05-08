#!/usr/bin/env python3

import json
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MANIFEST_PATH = ROOT / "tests" / "fuzz" / "fuzz_manifest.json"
CMAKE_PATH = ROOT / "CMakeLists.txt"
WORKFLOW_PATH = ROOT / ".github" / "workflows" / "ci.yml"
METADATA_FUZZ_PATH = ROOT / "tests" / "fuzz" / "fuzz_command_metadata.c"
REGISTER_SOURCES = [
    ROOT / "src" / "r_32.c",
    ROOT / "src" / "r_64.c",
    ROOT / "src" / "redis-roaring.c",
]
REGISTER_RE = re.compile(r'RegisterCommand\(ctx,\s*"([^"]+)"')
CMAKE_TARGET_RE = re.compile(r"^\s*(fuzz_[a-z0-9_]+)\s*$", re.MULTILINE)
WORKFLOW_TARGET_RE = re.compile(r"^\s*-\s*(fuzz_[a-z0-9_]+)\s*$", re.MULTILINE)
METADATA_FUZZ_COMMAND_RE = re.compile(r'^\s*\{"([^"]+)",\s*FUZZ_META_[A-Z_]+,', re.MULTILINE)
REQUIRED_SCOPE_KEYS = {"metadata", "dispatch", "routing", "persistence", "parity"}


def fail(message: str) -> None:
    print(f"error: {message}", file=sys.stderr)
    raise SystemExit(1)


def load_manifest() -> dict:
    try:
        return json.loads(MANIFEST_PATH.read_text())
    except FileNotFoundError:
        fail(f"missing manifest: {MANIFEST_PATH}")
    except json.JSONDecodeError as exc:
        fail(f"invalid JSON in {MANIFEST_PATH}: {exc}")


def registered_commands() -> list[str]:
    commands: list[str] = []
    for path in REGISTER_SOURCES:
        matches = REGISTER_RE.findall(path.read_text())
        if not matches:
            fail(f"no commands found in {path}")
        commands.extend(matches)
    return commands


def declared_cmake_targets() -> set[str]:
    return set(CMAKE_TARGET_RE.findall(CMAKE_PATH.read_text()))


def declared_workflow_targets() -> set[str]:
    return set(WORKFLOW_TARGET_RE.findall(WORKFLOW_PATH.read_text()))


def declared_metadata_fuzzer_commands() -> set[str]:
    commands = set(METADATA_FUZZ_COMMAND_RE.findall(METADATA_FUZZ_PATH.read_text()))
    if not commands:
        fail(f"no metadata fuzzer commands found in {METADATA_FUZZ_PATH}")
    return commands


def validate_family(family: dict) -> None:
    for field in ("family", "commands", "targets", "oracles", "seed_corpus", "scope"):
        if field not in family:
            fail(f"family entry is missing required field '{field}'")

    if not family["commands"]:
        fail(f"family '{family['family']}' does not declare any commands")
    if not family["targets"]:
        fail(f"family '{family['family']}' does not declare any fuzz targets")
    if not family["oracles"]:
        fail(f"family '{family['family']}' does not declare any oracles")

    scope_keys = set(family["scope"].keys())
    if scope_keys != REQUIRED_SCOPE_KEYS:
        fail(
            f"family '{family['family']}' has scope keys {sorted(scope_keys)}, "
            f"expected {sorted(REQUIRED_SCOPE_KEYS)}"
        )

    for target in family["targets"]:
        target_path = ROOT / "tests" / "fuzz" / f"{target}.c"
        if not target_path.exists():
            fail(f"family '{family['family']}' references missing target {target_path}")

    for seed_dir in family["seed_corpus"]:
        seed_path = ROOT / seed_dir
        if not seed_path.exists():
            fail(f"family '{family['family']}' references missing seed corpus {seed_path}")
        if not any(seed_path.iterdir()):
            fail(f"family '{family['family']}' references empty seed corpus {seed_path}")


def validate_target_inventory(families: list[dict]) -> None:
    manifest_targets = {target for family in families for target in family["targets"]}
    cmake_targets = declared_cmake_targets()
    workflow_targets = declared_workflow_targets()

    missing_from_cmake = sorted(manifest_targets - cmake_targets)
    if missing_from_cmake:
        fail(
            "manifest targets missing from CMake fuzz target list: "
            + ", ".join(missing_from_cmake)
        )

    missing_from_workflow = sorted(manifest_targets - workflow_targets)
    if missing_from_workflow:
        fail(
            "manifest targets missing from CI workflow target list: "
            + ", ".join(missing_from_workflow)
        )


def main() -> None:
    manifest = load_manifest()
    if manifest.get("schema_version") != 1:
        fail(f"unsupported schema version: {manifest.get('schema_version')}")

    families = manifest.get("families")
    if not isinstance(families, list) or not families:
        fail("manifest must define a non-empty 'families' list")

    registered = registered_commands()
    registered_set = set(registered)
    covered: dict[str, str] = {}

    for family in families:
        validate_family(family)
        family_name = family["family"]
        for command in family["commands"]:
            previous = covered.get(command)
            if previous is not None:
                fail(f"command '{command}' appears in both '{previous}' and '{family_name}'")
            covered[command] = family_name

    validate_target_inventory(families)

    covered_set = set(covered)
    metadata_fuzzer_set = declared_metadata_fuzzer_commands()
    missing = sorted(registered_set - covered_set)
    extra = sorted(covered_set - registered_set)
    metadata_missing = sorted(registered_set - metadata_fuzzer_set)
    metadata_extra = sorted(metadata_fuzzer_set - registered_set)

    if missing:
        fail(f"manifest is missing registered commands: {', '.join(missing)}")
    if extra:
        fail(f"manifest contains unknown commands: {', '.join(extra)}")
    if metadata_missing:
        fail("metadata fuzzer is missing registered commands: " + ", ".join(metadata_missing))
    if metadata_extra:
        fail("metadata fuzzer contains unknown commands: " + ", ".join(metadata_extra))

    print(
        f"validated fuzz manifest: {len(families)} families cover "
        f"{len(covered_set)}/{len(registered_set)} registered commands"
    )


if __name__ == "__main__":
    main()
