#!/usr/bin/env bash
# ==============================================================================
# Micro C Transpiler Test Suite Runner
# ==============================================================================

set -e

BIN="./build/mk56c"
TARGET="mk56"
PASS_COUNT=0
FAIL_COUNT=0

COLOR_GREEN="\033[32m"
COLOR_RED="\033[31m"
COLOR_BLUE="\033[34m"
COLOR_RESET="\033[0m"

echo -e "${COLOR_BLUE}=== Running Micro C Transpiler Test Suite ===${COLOR_RESET}"

# Positive Test Cases (Must exit with code 0)
POSITIVE_TESTS=(
    "tests/01_simple_calc.c"
    "tests/02_loop.c"
    "tests/03_many_vars.c"
    "tests/04_interactive_io.c"
    "tests/05_nested_control_flow.c"
    "tests/06_complex_precedence.c"
    "tests/07_edge_cases.c"
    "tests/09_subroutine.c"
    "tests/10_indirect_array.c"
)

for test_file in "${POSITIVE_TESTS[@]}"; do
    test_name=$(basename "$test_file" .c)
    out_file="build/${test_name}.mk56"
    echo -n "Running positive test: ${test_file} ... "

    if $BIN -t "$TARGET" "$test_file" -o "$out_file" > "build/${test_name}.log" 2>&1; then
        echo -e "${COLOR_GREEN}[PASS]${COLOR_RESET}"
        PASS_COUNT=$((PASS_COUNT + 1))
    else
        echo -e "${COLOR_RED}[FAIL]${COLOR_RESET} (Unexpected compilation failure)"
        cat "build/${test_name}.log"
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
done

# Negative Test Cases (Must fail gracefully with code != 0 and diagnostic message)
echo -n "Running negative test: tests/08_register_overflow.c (Must detect >14 live registers) ... "
if $BIN -t "$TARGET" tests/08_register_overflow.c -o build/overflow.mk56 > build/overflow.log 2>&1; then
    echo -e "${COLOR_RED}[FAIL]${COLOR_RESET} (Expected failure due to 16 live variables, but compilation succeeded)"
    FAIL_COUNT=$((FAIL_COUNT + 1))
else
    if grep -q "Register allocation failed for target 'mk56'" build/overflow.log; then
        echo -e "${COLOR_GREEN}[PASS]${COLOR_RESET} (Correctly rejected with diagnostic)"
        PASS_COUNT=$((PASS_COUNT + 1))
    else
        echo -e "${COLOR_RED}[FAIL]${COLOR_RESET} (Failed but did not produce expected register diagnostic)"
        cat build/overflow.log
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
fi

echo -n "Running negative test: tests/11_call_depth_overflow.c (Must detect >5 call stack depth) ... "
if $BIN -t "$TARGET" tests/11_call_depth_overflow.c -o build/depth_overflow.mk56 > build/depth_overflow.log 2>&1; then
    echo -e "${COLOR_RED}[FAIL]${COLOR_RESET} (Expected failure due to >5 call stack levels, but compilation succeeded)"
    FAIL_COUNT=$((FAIL_COUNT + 1))
else
    if grep -q "exceeds MK-56 hardware capacity of 5 levels" build/depth_overflow.log; then
        echo -e "${COLOR_GREEN}[PASS]${COLOR_RESET} (Correctly rejected with call stack diagnostic)"
        PASS_COUNT=$((PASS_COUNT + 1))
    else
        echo -e "${COLOR_RED}[FAIL]${COLOR_RESET} (Failed but did not produce expected depth diagnostic)"
        cat build/depth_overflow.log
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
fi

# Verification: Subroutine call and return emission (ПП / 0x53 and В/О / 0x52)
echo -n "Testing subroutine emission: Call (ПП / 0x53) and Return (В/О / 0x52) ... "
if grep -q "0x53" build/09_subroutine.mk56 && grep -q "0x52" build/09_subroutine.mk56 && \
   grep -q "ПП" build/09_subroutine.mk56 && grep -q "В/О" build/09_subroutine.mk56; then
    echo -e "${COLOR_GREEN}[PASS]${COLOR_RESET} (Emits native subroutine calls and returns)"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${COLOR_RED}[FAIL]${COLOR_RESET} (ПП or В/О not found in assembly)"
    cat build/09_subroutine.mk56
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi

# Verification: Indirect memory addressing (K П→x 7 / 0xD7)
echo -n "Testing indirect memory addressing: Load (K П→x 7 / 0xD7) ... "
if grep -q "0xD7" build/10_indirect_array.mk56 && grep -q "K П→x 7" build/10_indirect_array.mk56; then
    echo -e "${COLOR_GREEN}[PASS]${COLOR_RESET} (Emits native indirect addressing K П→x 7)"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${COLOR_RED}[FAIL]${COLOR_RESET} (K П→x 7 not found in assembly)"
    cat build/10_indirect_array.mk56
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi

# Verification: RPN Stack Optimization (Transient Accumulator Forwarding)
echo -n "Testing RPN stack optimization: Forwarding accumulator across operations ... "
if $BIN -t "$TARGET" tests/06_complex_precedence.c --ir-dump > build/06_ir.log 2>&1 && \
   grep -q "\\\$stack" build/06_ir.log; then
    echo -e "${COLOR_GREEN}[PASS]${COLOR_RESET} (Intermediate values forwarded via \$stack)"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${COLOR_RED}[FAIL]${COLOR_RESET} (RPN \$stack forwarding not detected)"
    cat build/06_ir.log
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi

# Optimization Test: Verify Hardware Loop Counter (F Lk) generation
echo -n "Testing optimization: Hardware loop counter (F L0 / 0x5D) emission ... "
if grep -q "0x5D" build/02_loop.mk56 && grep -q "F L0" build/02_loop.mk56; then
    echo -e "${COLOR_GREEN}[PASS]${COLOR_RESET} (Emits native F L0 hardware decrement & jump)"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${COLOR_RED}[FAIL]${COLOR_RESET} (Hardware loop F L0 not found in assembly)"
    cat build/02_loop.mk56
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi

# Multi-Format Exporter Tests
echo -n "Testing multi-format exporter: --export-all (.mk56, .keys, .hex) ... "
if $BIN -t "$TARGET" --export-all tests/02_loop.c -o build/export_test > build/export.log 2>&1 && \
   [ -s "build/export_test.mk56" ] && [ -s "build/export_test.keys" ] && [ -s "build/export_test.hex" ]; then
    echo -e "${COLOR_GREEN}[PASS]${COLOR_RESET}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${COLOR_RED}[FAIL]${COLOR_RESET} (--export-all did not generate all files)"
    cat build/export.log
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi

echo -n "Testing keystroke guide exporter: -f keys ... "
if $BIN -t "$TARGET" -f keys tests/04_interactive_io.c -o build/test_keys.keys > build/keys.log 2>&1 && \
   grep -q "ELEKTRONIKA MK-56 PHYSICAL KEYBOARD TYPING GUIDE" build/test_keys.keys && \
   grep -q "\[ С/П \]" build/test_keys.keys; then
    echo -e "${COLOR_GREEN}[PASS]${COLOR_RESET}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${COLOR_RED}[FAIL]${COLOR_RESET} (-f keys export failed)"
    cat build/keys.log
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi

echo -n "Testing emulator bytecode hex exporter: -f hex ... "
if $BIN -t "$TARGET" -f hex tests/01_simple_calc.c -o build/test_hex.hex > build/hex.log 2>&1 && \
   grep -q "MK-56 Raw Bytecode Image" build/test_hex.hex; then
    echo -e "${COLOR_GREEN}[PASS]${COLOR_RESET}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${COLOR_RED}[FAIL]${COLOR_RESET} (-f hex export failed)"
    cat build/hex.log
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi

echo -e "\n${COLOR_BLUE}=== Test Summary ===${COLOR_RESET}"
echo -e "Passed: ${COLOR_GREEN}${PASS_COUNT}${COLOR_RESET}"
echo -e "Failed: ${COLOR_RED}${FAIL_COUNT}${COLOR_RESET}"

if [ "$FAIL_COUNT" -gt 0 ]; then
    exit 1
fi
