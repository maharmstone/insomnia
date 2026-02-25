#!/bin/bash
# run_module_tests.sh — test plugin behaviour with C++20 modules
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
PLUGIN="$BUILD_DIR/SleepCheckPlugin.so"
MOD_DIR="$SCRIPT_DIR/test/modules"
TMP_DIR=$(mktemp -d)

trap "rm -rf $TMP_DIR" EXIT

# Build plugin if needed
if [ ! -f "$PLUGIN" ]; then
    echo "Building plugin..."
    mkdir -p "$BUILD_DIR"
    (cd "$BUILD_DIR" && cmake .. -DCMAKE_BUILD_TYPE=Release >/dev/null 2>&1 && make -j"$(nproc)" >/dev/null 2>&1)
fi

if [ ! -f "$PLUGIN" ]; then
    echo "FAIL: plugin not found at $PLUGIN"
    exit 1
fi

CLANG=$(which clang++)
XCLANG_LOAD="-Xclang -load -Xclang $PLUGIN -Xclang -add-plugin -Xclang sleep-check"

PASS=0
FAIL=0

echo "=== Module Tests ==="
echo ""

# Step 1: precompile the module interface
echo -n "Precompiling sleepmod.cppm... "
PCM="$TMP_DIR/sleepmod.pcm"
pcm_output=$($CLANG -std=c++20 $XCLANG_LOAD --precompile \
    "$MOD_DIR/sleepmod.cppm" -o "$PCM" 2>&1 || true)

if [ -f "$PCM" ]; then
    echo "ok"
else
    echo "FAILED"
    echo "  $pcm_output"
    exit 1
fi

# Step 2: run each test file that imports the module
run_module_test() {
    local test_file="$1"
    local test_name
    test_name="$(basename "$test_file")"

    local output
    output=$($CLANG -std=c++20 $XCLANG_LOAD \
        -fmodule-file=sleepmod="$PCM" \
        -fsyntax-only "$test_file" 2>&1 || true)

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

for test_file in "$MOD_DIR"/test_import_*.cpp; do
    [ -f "$test_file" ] || continue
    run_module_test "$test_file"
done

echo ""
echo "Results: $PASS passed, $FAIL failed"

if [ "$FAIL" -ne 0 ]; then
    exit 1
fi
