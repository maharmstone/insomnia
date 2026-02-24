#!/bin/bash
# run_tests.sh — build the plugin and verify diagnostics against test files
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
PLUGIN="$BUILD_DIR/SleepCheckPlugin.so"
TEST_DIR="$SCRIPT_DIR/test"

# Build if needed
if [ ! -f "$PLUGIN" ]; then
    echo "Building plugin..."
    mkdir -p "$BUILD_DIR"
    (cd "$BUILD_DIR" && cmake .. -DCMAKE_BUILD_TYPE=Release >/dev/null 2>&1 && make -j"$(nproc)" >/dev/null 2>&1)
fi

if [ ! -f "$PLUGIN" ]; then
    echo "FAIL: plugin not found at $PLUGIN"
    exit 1
fi

PASS=0
FAIL=0

run_test() {
    local test_file="$1"
    local test_name
    test_name="$(basename "$test_file")"

    # Run clang with the plugin, capture stderr
    local output
    output=$(clang -cc1 -load "$PLUGIN" -add-plugin sleep-check -fsyntax-only "$test_file" 2>&1 || true)

    local test_pass=1

    # Check EXPECTED-ERROR lines
    while IFS= read -r line; do
        local expected
        expected=$(echo "$line" | sed 's/.*EXPECTED-ERROR: //')
        if ! echo "$output" | grep -qF "$expected"; then
            echo "  MISSING ERROR: $expected"
            test_pass=0
        fi
    done < <(grep 'EXPECTED-ERROR:' "$test_file")

    # Check EXPECTED-WARNING lines
    while IFS= read -r line; do
        local expected
        expected=$(echo "$line" | sed 's/.*EXPECTED-WARNING: //')
        if ! echo "$output" | grep -qF "$expected"; then
            echo "  MISSING WARNING: $expected"
            test_pass=0
        fi
    done < <(grep 'EXPECTED-WARNING:' "$test_file")

    if [ "$test_pass" -eq 1 ]; then
        echo "PASS: $test_name"
        PASS=$((PASS + 1))
    else
        echo "FAIL: $test_name"
        echo "  Actual output:"
        echo "$output" | sed 's/^/    /'
        FAIL=$((FAIL + 1))
    fi
}

echo "=== Sleep-Check Plugin Tests ==="
echo ""

for test_file in "$TEST_DIR"/test_*.c; do
    run_test "$test_file"
done

echo ""
echo "Results: $PASS passed, $FAIL failed"

if [ "$FAIL" -ne 0 ]; then
    exit 1
fi
