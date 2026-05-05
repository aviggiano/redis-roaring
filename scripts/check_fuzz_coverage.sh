#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

cd "$ROOT_DIR"

emit_registered_commands() {
  perl -nE 'say $1 if /RegisterCommand\(ctx, "([^"]+)"/' \
    src/r_32.c src/r_64.c src/redis-roaring.c | sort -u
}

emit_metadata_commands() {
  perl -nE 'say $1 if /SetCommandInfo\(ctx, "([^"]+)"/' \
    src/cmd_info/r_info.c src/cmd_info/r64_info.c src/cmd_info/root_info.c | sort -u
}

emit_manifest_commands() {
  perl -nE 'say $1 if /^\|\s*`([^`]+)`\s*\|/' tests/fuzz/fuzz_manifest.md | sort -u
}

emit_manifest_targets() {
  perl -nE 'while(/\b(fuzz_[a-z0-9_]+)\b/g){ say $1 }' tests/fuzz/fuzz_manifest.md | sort -u
}

emit_descriptor_commands() {
  perl -nE '
    if (/FUZZ_PAIR_(?:SINGLE_KEY|TWO_KEY_RANGE)\("([^"]+)"/) {
      say "R.$1";
      say "R64.$1";
    } elsif (/FUZZ_PAIR_DIFF\(\)/) {
      say "R.DIFF";
      say "R64.DIFF";
    } elsif (/FUZZ_PAIR_BITOP\(\)/) {
      say "R.BITOP";
      say "R64.BITOP";
    } elsif (/\{\s*"([^"]+)"\s*,\s*NULL\s*,/) {
      say $1;
    }
  ' tests/fuzz/fuzz_command_manifest.h | sort -u
}

emit_cmake_targets() {
  perl -nE 'say $1 if /^\s*(fuzz_[a-z0-9_]+)\s*$/' CMakeLists.txt | sort -u
}

emit_workflow_targets() {
  perl -nE 'say $1 if /^\s*-\s*(fuzz_[a-z0-9_]+)\s*$/' .github/workflows/ci.yml | sort -u
}

compare_sets() {
  local lhs_label="$1"
  local lhs_file="$2"
  local rhs_label="$3"
  local rhs_file="$4"
  local missing extra

  missing="$(comm -23 "$lhs_file" "$rhs_file" || true)"
  extra="$(comm -13 "$lhs_file" "$rhs_file" || true)"

  if [[ -n "$missing" || -n "$extra" ]]; then
    echo "$lhs_label and $rhs_label diverged" >&2
    if [[ -n "$missing" ]]; then
      echo "Missing from $rhs_label:" >&2
      echo "$missing" >&2
    fi
    if [[ -n "$extra" ]]; then
      echo "Only present in $rhs_label:" >&2
      echo "$extra" >&2
    fi
    exit 1
  fi
}

emit_registered_commands > "$TMP_DIR/registered.txt"
emit_metadata_commands > "$TMP_DIR/metadata.txt"
emit_manifest_commands > "$TMP_DIR/manifest_commands.txt"
emit_manifest_targets > "$TMP_DIR/manifest_targets.txt"
emit_descriptor_commands > "$TMP_DIR/descriptor_commands.txt"
emit_cmake_targets > "$TMP_DIR/cmake_targets.txt"
emit_workflow_targets > "$TMP_DIR/workflow_targets.txt"

compare_sets "registered commands" "$TMP_DIR/registered.txt" "metadata commands" "$TMP_DIR/metadata.txt"
compare_sets "registered commands" "$TMP_DIR/registered.txt" "manifest commands" "$TMP_DIR/manifest_commands.txt"
compare_sets "registered commands" "$TMP_DIR/registered.txt" "metadata fuzz descriptors" "$TMP_DIR/descriptor_commands.txt"
compare_sets "manifest targets" "$TMP_DIR/manifest_targets.txt" "CMake fuzz targets" "$TMP_DIR/cmake_targets.txt"
compare_sets "manifest targets" "$TMP_DIR/manifest_targets.txt" "workflow fuzz targets" "$TMP_DIR/workflow_targets.txt"

while IFS= read -r target; do
  [[ -n "$target" ]] || continue

  source_file="tests/fuzz/${target}.c"
  corpus_dir="tests/fuzz/corpus/${target#fuzz_}"

  if [[ ! -f "$source_file" ]]; then
    echo "Missing fuzzer source: $source_file" >&2
    exit 1
  fi

  if [[ ! -d "$corpus_dir" ]]; then
    echo "Missing corpus directory: $corpus_dir" >&2
    exit 1
  fi

  if ! find "$corpus_dir" -mindepth 1 -type f | grep -q .; then
    echo "Corpus directory has no seeds: $corpus_dir" >&2
    exit 1
  fi
done < "$TMP_DIR/manifest_targets.txt"

echo "Fuzz coverage manifest is consistent with the command registry, build, workflow, and corpora."
