#!/usr/bin/env bash
set -euo pipefail

# Remove compiled `.bin` artifacts with scope controls: all, test, or main.

usage() {
  cat <<'EOF'
Usage: clear.sh OPTION

Options:
  all   Delete every *.bin file found under the repository root.
  test  Delete *.bin files only under the test directories.
  main  Delete *.bin files located at the repository root (non-recursive).
EOF
}

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"

option="${1:-}"
if [[ -z "$option" ]]; then
  usage
  exit 1
fi

rel_path() {
  local abs_path="$1"
  if [[ "$abs_path" == "$repo_root/"* ]]; then
    printf '%s\n' "${abs_path#$repo_root/}"
  elif [[ "$abs_path" == "$repo_root" ]]; then
    printf '%s\n' "$(basename "$abs_path")"
  else
    printf '%s\n' "$abs_path"
  fi
}

delete_files() {
  local label="$1"
  shift
  local -a files=("$@")

  if (( ${#files[@]} == 0 )); then
    echo "No .bin files found for $label."
    return
  fi

  echo "Deleting .bin files for $label:"
  for file in "${files[@]}"; do
    rm -f "$file"
    echo "  $(rel_path "$file")"
  done
}

delete_all() {
  local -a files=()
  local file
  while IFS= read -r file; do
    files+=("$file")
  done < <(find "$repo_root" -type f -name '*.bin')
  delete_files "all" "${files[@]}"
}

delete_main() {
  shopt -s nullglob
  local -a files=("$repo_root"/*.bin)
  shopt -u nullglob
  delete_files "main" "${files[@]}"
}

delete_test() {
  local -a targets=()
  for candidate in "$repo_root/test" "$repo_root/tests"; do
    if [[ -d "$candidate" ]]; then
      targets+=("$candidate")
    fi
  done

  if (( ${#targets[@]} == 0 )); then
    echo "No test directory found at $repo_root/test or $repo_root/tests."
    return
  fi

  local -a files=()
  local dir file
  for dir in "${targets[@]}"; do
    while IFS= read -r file; do
      files+=("$file")
    done < <(find "$dir" -type f -name '*.bin')
  done

  delete_files "test" "${files[@]}"
}

case "$option" in
  all)
    delete_all
    ;;
  test)
    delete_test
    ;;
  main)
    delete_main
    ;;
  -h|--help)
    usage
    ;;
  *)
    echo "Unknown option: $option" >&2
    usage >&2
    exit 1
    ;;
esac
