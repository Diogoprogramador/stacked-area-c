#!/bin/sh
# Integration test for the sa-cli command: runs the real CLI against
# examples/data.csv and checks the generated SVG.
set -e

CLI="$1"
if [ -z "$CLI" ] || [ ! -x "$CLI" ]; then
    echo "usage: test_cli.sh <path-to-sa-cli>" >&2
    exit 2
fi

HERE=$(cd "$(dirname "$0")" && pwd)
DATA="$HERE/../examples/data.csv"
OUT=$(mktemp /tmp/opencode/sa_cli_out.XXXXXX.svg 2>/dev/null || echo "sa_cli_out.svg")
trap 'rm -f "$OUT"' EXIT

"$CLI" render "$DATA" -o "$OUT" --title "CLI test"
[ -s "$OUT" ] || { echo "FAIL: output file is empty" >&2; exit 1; }
grep -q "<svg" "$OUT" || { echo "FAIL: no <svg> in output" >&2; exit 1; }
grep -q "alpha" "$OUT" || { echo "FAIL: series name 'alpha' missing" >&2; exit 1; }
grep -q "beta" "$OUT" || { echo "FAIL: series name 'beta' missing" >&2; exit 1; }

# Missing input file must fail with a non-zero exit status.
if "$CLI" render /nonexistent/sa_missing.csv >/dev/null 2>&1; then
    echo "FAIL: expected non-zero exit for missing input file" >&2
    exit 1
fi

# --help must succeed.
"$CLI" --help >/dev/null || { echo "FAIL: --help exit != 0" >&2; exit 1; }

echo "test_cli: OK"
