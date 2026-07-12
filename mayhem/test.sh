#!/usr/bin/env bash
#
# mayhem/test.sh — RUN ArkScript's own boost-ext/ut unit-test suite (built by mayhem/build.sh)
# and report results as CTRF. This is the project's ENTIRE upstream unit suite (tests/unittests,
# all Suites/*.cpp: parser, compiler, optimizer, type-checker, VM/lang, stdlib examples, rosetta,
# formatter, bytecode reader, ...). It asserts BEHAVIOUR (golden values / diffs), so neutering the
# program to a no-op makes it FAIL.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

RUNNER="$SRC/build-tests/unittests"
if [ ! -x "$RUNNER" ]; then
  echo "test.sh: test runner $RUNNER missing — build.sh must build the 'unittests' target" >&2
  emit_ctrf "arkscript-unittests" 0 1 0
  exit 1
fi

# boost-ext/ut prints a per-suite summary; run from the source root (ARK_TESTS_ROOT is baked in as
# an absolute path, but resources resolve most predictably from $SRC). Capture stdout+stderr and
# strip ANSI colour codes so the counts parse.
run_log="$(cd "$SRC" && "$RUNNER" 2>&1)"; rc=$?
clean="$(printf '%s\n' "$run_log" | sed -E 's/\x1b\[[0-9;]*m//g')"
printf '%s\n' "$run_log"

# This ut build reports PER SUITE:
#   pass:  "Suite 'X': all tests passed (<A> asserts in <M> tests)"
#   fail:  "Suite X" \n "tests:   <T> | <F> failed" \n "asserts: ..."
# Aggregate across all suites.
passed=0; failed=0; skipped=0
while IFS= read -r m; do passed=$(( passed + m )); done < <(
  printf '%s\n' "$clean" | sed -nE "s/^Suite '[^']+': all tests passed \([0-9]+ asserts in ([0-9]+) tests\).*/\1/p")
while IFS= read -r line; do
  t="$(printf '%s' "$line" | sed -nE 's/^tests:[[:space:]]+([0-9]+) \| [0-9]+ failed.*/\1/p')"
  f="$(printf '%s' "$line" | sed -nE 's/^tests:[[:space:]]+[0-9]+ \| ([0-9]+) failed.*/\1/p')"
  if [ -n "$t" ] && [ -n "$f" ]; then
    failed=$(( failed + f )); passed=$(( passed + t - f ))
  fi
done < <(printf '%s\n' "$clean" | grep -E '^tests:[[:space:]]+[0-9]+ \|')
k="$(printf '%s\n' "$clean" | sed -nE 's/^([0-9]+) tests skipped.*/\1/p' | head -1)"
[ -n "$k" ] && skipped="$k"

# Never let a nonzero runner exit (or an unparseable log) become a silent pass.
if [ "$rc" -ne 0 ] && [ "$failed" -eq 0 ]; then failed=1; fi
if [ "$passed" -eq 0 ] && [ "$failed" -eq 0 ]; then
  echo "test.sh: could not parse unittests summary" >&2; failed=1
fi

emit_ctrf "arkscript-unittests" "$passed" "$failed" "$skipped"
