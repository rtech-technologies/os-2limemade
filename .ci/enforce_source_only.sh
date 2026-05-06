#!/usr/bin/env bash
set -euo pipefail

echo "==> Source-only CI enforce checks"

# 1) Ensure repo builds from source
echo "==> Building from source..."
if ! make -j"$(nproc)"; then
  echo "ERROR: Build failed" >&2
  exit 1
fi
echo "OK: Build succeeded"

# 2) Reject stub patterns
echo "==> Checking for stub patterns..."
STUB_PATTERNS="TOD""O|FIX""ME|/\\* STUB \\*/|panic\\(\"stub\"\\)|return -ENOSYS"
if git grep -n --extended-regexp "$STUB_PATTERNS" -- ':!vendor/*' ':!third_party/*' ':!.ci/*' >/dev/null 2>&1; then
  echo "ERROR: Stub patterns found. Remove before merge." >&2
  git --no-pager grep -n --extended-regexp "$STUB_PATTERNS" -- ':!vendor/*' ':!third_party/*' ':!.ci/*'
  exit 1
fi
echo "OK: No stub patterns"

# 3) Ensure no precompiled binaries checked in
echo "==> Checking for binary artifacts..."
if git ls-files -z | xargs -0 file 2>/dev/null | grep -vE '\.py|\.sh|\.s|\.c|\.h|\.ld|Makefile' | egrep -i 'executable|archive|ELF|PE32' >/dev/null 2>&1; then
  echo "ERROR: Precompiled binary artifacts found in repo. Remove before merge." >&2
  git ls-files -z | xargs -0 file 2>/dev/null | grep -vE '\.py|\.sh|\.s|\.c|\.h|\.ld|Makefile' | egrep -i 'executable|archive|ELF|PE32' || true
  exit 1
fi
echo "OK: No precompiled binaries found"

echo "==> Source-only CI checks passed"
