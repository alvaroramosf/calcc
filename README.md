# mk56c: Micro C Compiler for Soviet Programmable Calculators

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)]()
[![Test Suite](https://img.shields.io/badge/tests-18%2F18%20passed-brightgreen.svg)]()
[![Standard](https://img.shields.io/badge/c-c11-blue.svg)]()
[![Target](https://img.shields.io/badge/target-Elektronika%20MK--56%20%2F%20MK--52-red.svg)]()
[![Architecture](https://img.shields.io/badge/arch-Soviet%20RPN-orange.svg)]()

`mk56c` is an optimizing C compiler that translates a structured subset of C (**Micro C**) into native keystrokes, opcodes, and hexadecimal bytecode for Soviet programmable calculators based on the B3-34 architecture, primarily the **Elektronika MK-56** and **MK-52**.

Targeting programmable calculators presents extreme hardware constraints:
* **98 program steps** of volatile RAM (`00` to `97`).
* **14 data memory registers** (`RG0`–`RG9`, `RGa`–`RGd`).
* **4-level operational RPN stack** ($X$, $Y$, $Z$, $T$) plus an $X_1$ shadow register.
* **5-level subroutine return stack**.

`mk56c` bridges the gap between modern procedural programming and vintage Soviet hardware through static analysis, liveness-based register allocation, dead-code elimination, and RPN operational stack optimization.

---

## Table of Contents
1. [Key Features](#key-features)
2. [Hardware Architecture & Limits](#hardware-architecture--limits)
3. [Building and Installation](#building-and-installation)
4. [Command-Line Interface](#command-line-interface)
5. [Language Guide & Supported Features](#language-guide--supported-features)
   - [Basic Arithmetic & Precedence](#basic-arithmetic--precedence)
   - [Functions & Subroutines](#functions--subroutines)
   - [Arrays & Indirect Addressing](#arrays--indirect-addressing)
   - [Hardware Loop Counters](#hardware-loop-counters)
   - [Interactive I/O](#interactive-io)
6. [Compilation Pipeline](#compilation-pipeline)
7. [Multi-Format Exporters](#multi-format-exporters)
8. [Example Programs](#example-programs)
9. [Project Structure](#project-structure)
10. [Running Tests](#running-tests)
11. [License](#license)

---

## Key Features

* **High-Level Micro C Syntax**: Write clean, standard C with typed variables (`int`, `float`), control structures (`if`/`else`, `while`, `for`), subroutines with arguments and return values, and array subscripting (`arr[i]`).
* **Graph-Coloring Register Allocation**: Declare arbitrary variables (more than 14) in source code. The compiler computes variable liveness intervals and reuses physical registers across non-overlapping live ranges.
* **RPN Stack Optimizer (Transient Accumulator Forwarding)**: Identifies intermediate subexpression temporaries and forwards them directly on the hardware operational stack ($X$), eliminating redundant `П reg` (store) and `ИП reg` (load) instructions. This saves up to 15–20% of total memory steps.
* **Hardware Loop Counter Synthesis**: Automatically recognizes loop decrement patterns (`while (n > 0) { ... n = n - 1; }`) and lowers them to native hardware instructions (`F L0`..`F L3` / opcodes `0x5D`, `0x5B`, `0x58`, `0x5A`), consuming only 2 steps per loop cycle.
* **Indirect Memory Addressing**: Supports dynamic 1D array indexing (`arr[i]`). Reserves register `RG7` as an unmodified pointer register (`K П→x 7` and `K x→П 7`), preventing unintended auto-increment/decrement side effects.
* **Static Call Stack Diagnostics**: Verifies the call graph reachability tree from `main()`. Fails at compile-time with a clear diagnostic if subroutine nesting exceeds the 5-level hardware call stack or if recursion is attempted.
* **Multi-Format Output**: Emits annotated Cyrillic assembly (`.mk56`), physical typing keystroke guides (`.keys`), and raw emulator bytecode images (`.hex`).

---

## Hardware Architecture & Limits

The **Elektronika MK-56** (desktop) and **MK-52** (handheld with EEPROM) belong to the third-generation Soviet programmable calculator family (B3-34 architecture):

| Resource | Elektronika MK-56 | Notes |
|:---|:---|:---|
| **Program Memory** | 98 steps (`00`..`97`) | 1 byte per instruction or address byte |
| **Data Registers** | 14 registers (`0`..`9`, `a`..`d`) | 84-bit BCD floating-point numbers |
| **Operational Stack** | 4 registers ($X$, $Y$, $Z$, $T$) | Reverse Polish Notation (RPN) |
| **Shadow Register** | 1 register ($X_1$) | Preserves previous $X$ before operations |
| **Subroutine Call Stack**| 5 levels | Hardware LIFO return register stack |
| **Pointer Registers** | `RG0`–`RG3` auto-decrement<br>`RG4`–`RG6` auto-increment<br>`RG7`–`RGd` unmodified | Used with `K x→П` and `K П→x` |
| **Hardware Loop Counters**| `RG0`–`RG3` | Dedicated `F L0`..`F L3` instructions |

---

## Building and Installation

### Prerequisites
* A C11-compliant compiler (`gcc` or `clang`)
* GNU `make`
* `flex` (Fast Lexical Analyzer Generator)
* `bison` (GNU Parser Generator)

### Compilation
Clone or navigate to the repository directory and run:

```bash
make
```

The compiled binary will be placed at `build/mk56c`.

To perform a clean build:
```bash
make clean && make
```

---

## Command-Line Interface

```
Usage: ./build/mk56c [OPTIONS] <input.c>

Options:
  -t, --target <name>      Target hardware profile (default: mk56)
  -o, --output <file>      Output file path (default: stdout)
  -f, --format <fmt>       Output format: asm, keys, hex, all (default: asm)
      --export-all         Export all formats (.mk56, .keys, .hex)
  -v, --verbose            Enable verbose compilation diagnostics
      --ast-dump           Dump Abstract Syntax Tree to stdout
      --ir-dump            Dump Three-Address Code (TAC) IR to stdout
      --list-targets       Display all supported hardware targets
  -h, --help               Display this help message and exit
```

### Examples

**1. Compile a C program to assembly:**
```bash
./build/mk56c tests/01_simple_calc.c -o calc.mk56
```

**2. Generate physical typing keystroke guide:**
```bash
./build/mk56c -f keys tests/04_interactive_io.c -o guide.keys
```

**3. Generate emulator hex bytecode:**
```bash
./build/mk56c -f hex tests/02_loop.c -o program.hex
```

**4. Export all formats simultaneously:**
```bash
./build/mk56c --export-all tests/09_subroutine.c -o build/subroutine
# Generates: build/subroutine.mk56, build/subroutine.keys, build/subroutine.hex
```

**5. Inspect IR and Register Allocation:**
```bash
./build/mk56c --ir-dump -v tests/06_complex_precedence.c
```

---

## Language Guide & Supported Features

### Basic Arithmetic & Precedence
Micro C supports standard integer and floating-point arithmetic with full C operator precedence:
```c
int main(void) {
    int a = 10;
    int b = 2;
    int c = 5;
    int d = 1;
    
    // Parenthesized subexpressions are evaluated efficiently on the RPN stack
    int result = (a + b) * (c - d);
    return result;
}
```

### Functions & Subroutines
Functions compile to subroutines called via `ПП` (`0x53`) and returning via `В/О` (`0x52`). If subroutines are declared before `main()`, step `00` automatically emits an unconditional jump (`БП`) to `main()`.

```c
int square(int x) {
    return x * x;
}

int add3(int a, int b, int c) {
    return a + b + c;
}

int main(void) {
    int s = square(4);
    int total = add3(s, 5, 6);
    display(total);
    return total;
}
```

> [!NOTE]
> The compiler statically validates the call graph reachability tree from `main()`. If subroutine call nesting exceeds 5 levels or recursion is detected, compilation halts with an error.

### Arrays & Indirect Addressing
Declare 1D arrays and access elements with constant or dynamic index expressions:
```c
int main(void) {
    int a[4];
    a[0] = 10;
    a[1] = 20;
    a[2] = 30;
    a[3] = 40;

    int idx = 2;
    int val = a[idx]; // Lowers to indirect load via RG7: K П→x 7 (0xD7)

    display(val);     // Displays 30
    return val;
}
```

### Hardware Loop Counters
Loops following the pattern `var = var - 1` with condition `var > 0` are automatically recognized as hardware loop candidates and mapped to `F L0`..`F L3`:
```c
int main(void) {
    int count = 5;
    int sum = 0;

    // Emits native [ F ] [ L0 ] (0x5D)
    while (count > 0) {
        sum = sum + count;
        count = count - 1;
    }

    display(sum);
    return sum;
}
```

### Interactive I/O
The runtime library `<mk56.h>` (or `<retro_calc.h>`) provides hardware intrinsics for interactive execution:

```c
#include <mk56.h>

int main(void) {
    // Pauses calculator, prompts user to enter a value, and stores it upon C/P
    int x = input();

    // Momentarily flashes display without halting execution
    pause();

    int result = x * 2;

    // Displays result on the VFD and pauses execution
    display(result);

    return 0;
}
```

---

## Compilation Pipeline

The transpiler follows a multi-stage architecture:

```
[ Micro C Source (.c) ]
           │
           ▼ (Frontend: Flex / Bison)
     [ Typed AST ]
           │
           ├── Static Call Graph Verification (Max 5 Call Levels, Recursion Check)
           ▼ (AST to IR Lowering)
  [ Three-Address Code (TAC) ]
           │
           ├── Pass 1: Peephole Optimizer & Copy Propagation
           ├── Pass 2: RPN Stack Optimization (Transient Accumulator $stack Forwarding)
           ├── Pass 3: Dead Variable Elimination (DVE)
           ├── Pass 4: Liveness Interval Analysis
           └── Pass 5: Graph-Coloring Register Allocator (with Pointer & Array Reservations)
                    │
                    ▼ (Backend Target Code Generator)
     [ Multi-Format Exporters (.mk56, .keys, .hex) ]
```

---

## Multi-Format Exporters

### 1. Assembly Listing (`.mk56`)
Contains line-by-line program steps, display glyphs (VFD representation), hex bytecodes, Cyrillic mnemonics, and explanatory comments:
```assembly
; Step | VFD  | Hex   | Cyrillic | Description
; ----------------------------------------------------------
  00:   10    0x0A   10        ; Load numeric constant into X
  01:   0E    0x0E   B^        ; Push X to Y (Enter)
  02:   40    0x40   П 0       ; Store X into register
  03:   60    0x60   ИП 0      ; Load register into X
  04:   60    0x60   ИП 0      ; Load register into X
  05:   12    0x12   *         ; X = Y * X
  06:   50    0x50   С/П       ; Stop program execution
```

### 2. Physical Keystroke Guide (`.keys`)
Detailed button-by-button typing guide designed for physical entry into real calculator hardware:
```
Step 00: [ 1 ] [ 0 ]             ; Load numeric constant into X
Step 01: [ В↑ ]                  ; Push X to Y (Enter)
Step 02: [ x→П ] [ 0 ]           ; Store X into register
Step 03: [ П→x ] [ 0 ]           ; Load register into X
Step 04: [ П→x ] [ 0 ]           ; Load register into X
Step 05: [ * ]                   ; X = Y * X
Step 06: [ С/П ]                 ; Stop program execution
```

### 3. Emulator Bytecode Hex Image (`.hex`)
Bytecode format ready for direct importation into Soviet programmable calculator emulators:
```hex
; MK-56 Raw Bytecode Image
; Total Bytes: 7 / 98
00 0E 40 60 60 12 50
```

---

## Example Programs

A collection of real-world Micro C programs demonstrating real hardware capabilities is provided in the [`examples/`](examples/README.md) directory:

| Program | Source | Steps | Registers | Description & Highlights |
|:---|:---|:---:|:---:|:---|
| **Fibonacci Sequence** | [`01_fibonacci.c`](examples/01_fibonacci.c) | 24 / 98 | 4 / 14 | Computes $F_n$ using native hardware loop decrement counter (`F L0`). |
| **Circle Area** | [`02_circle_area.c`](examples/02_circle_area.c) | 21 / 98 | 2 / 14 | Calculates $A = \pi \cdot r^2$ using modular subroutines (`ПП` / `В/О`). |
| **Dice Simulator** | [`03_dice_simulator.c`](examples/03_dice_simulator.c) | 39 / 98 | 2 / 14 | Pseudo-random roll ($1$–$6$) via LCG with repeated-subtraction modulo. |
| **Prime Checker** | [`04_prime_checker.c`](examples/04_prime_checker.c) | 67 / 98 | 3 / 14 | Trial division algorithm with nested loops and conditional display. |

Each example includes its precompiled `.mk56` assembly listing, `.keys` physical keystroke typing guide, and `.hex` emulator bytecode. See [examples/README.md](examples/README.md) for full execution walkthroughs and expected outputs.

---

## Project Structure

```
.
├── Makefile                # Build configuration (strict -Wall -Wextra -Wpedantic -O2 -std=c11)
├── README.md               # User manual and project documentation
├── docs/
│   ├── micro_c_spec.md     # Formal specification of the Micro C language
│   └── reference_mk56.pdf  # Technical reference manual for Elektronika MK-56
├── examples/
│   ├── README.md           # Comprehensive walkthrough for all example programs
│   ├── 01_fibonacci.c      # Fibonacci sequence generator
│   ├── 02_circle_area.c    # Circle area with subroutine call
│   ├── 03_dice_simulator.c # LCG pseudo-random dice simulator
│   └── 04_prime_checker.c  # Prime number tester via trial division
├── include/
│   ├── ast.h               # Abstract Syntax Tree nodes and constructors
│   ├── ast_to_ir.h         # AST-to-TAC lowering interface
│   ├── exporter.h          # Multi-format exporter interface (.mk56, .keys, .hex)
│   ├── ir.h                # Three-Address Code data structures and helpers
│   ├── mk56.h              # C runtime header for Elektronika MK-56 targets
│   ├── peephole.h          # Peephole optimizer and RPN stack forwarding
│   ├── regalloc.h          # Liveness analysis and register allocator
│   ├── retro_calc.h        # Generic retro-calculator runtime header
│   ├── target_api.h        # Hardware Abstraction Layer (HAL) interface
│   └── target_registry.h   # Pluggable backend target registry
├── src/
│   ├── main.c              # CLI driver and compiler pipeline coordinator
│   ├── frontend/
│   │   ├── lexer.l         # Flex scanner
│   │   └── parser.y        # Bison LALR(1) grammar
│   ├── core/
│   │   ├── ast.c           # AST node management and dump utilities
│   │   ├── ast_to_ir.c     # TAC code generator and call graph checker
│   │   ├── ir.c            # TAC instruction list implementation
│   │   ├── peephole.c      # Optimizations: copy propagation & RPN stack
│   │   └── regalloc.c      # Liveness intervals and register allocation
│   └── backend/
│       ├── codegen.c       # Two-pass address resolution and code emission
│       ├── exporter.c      # .mk56, .keys, and .hex output formatting
│       ├── target_registry.c # Target lookup and registration
│       └── targets/
│           └── mk56.c      # Elektronika MK-56/52 backend implementation
└── tests/
    ├── run_tests.sh        # Automated test suite runner (18 test suites)
    ├── 01_simple_calc.c    # Basic arithmetic and expressions
    ├── 02_loop.c           # While loop and hardware counter optimization
    ├── 03_many_vars.c      # Liveness analysis with >14 variables
    ├── 04_interactive_io.c # input(), display(), pause() intrinsics
    ├── 05_nested_control_flow.c # Nested if/else and while blocks
    ├── 06_complex_precedence.c # Complex operator precedence and RPN stack
    ├── 07_edge_cases.c     # Zero comparisons, negation, chained assignments
    ├── 08_register_overflow.c # Negative test: >14 concurrent live variables
    ├── 09_subroutine.c     # Function calls (ПП) and returns (В/О)
    ├── 10_indirect_array.c # Arrays and indirect addressing (K x->P, K P->x)
    └── 11_call_depth_overflow.c # Negative test: >5 nested call depth
```

---

## Running Tests

The test suite thoroughly exercises the compiler across positive features, optimizations, negative capacity limits, and multi-format outputs:

```bash
make test
```

### Test Suite Output
```text
=== Running Micro C Transpiler Test Suite ===
Running positive test: tests/01_simple_calc.c ... [PASS]
Running positive test: tests/02_loop.c ... [PASS]
Running positive test: tests/03_many_vars.c ... [PASS]
Running positive test: tests/04_interactive_io.c ... [PASS]
Running positive test: tests/05_nested_control_flow.c ... [PASS]
Running positive test: tests/06_complex_precedence.c ... [PASS]
Running positive test: tests/07_edge_cases.c ... [PASS]
Running positive test: tests/09_subroutine.c ... [PASS]
Running positive test: tests/10_indirect_array.c ... [PASS]
Running negative test: tests/08_register_overflow.c (Must detect >14 live registers) ... [PASS] (Correctly rejected with diagnostic)
Running negative test: tests/11_call_depth_overflow.c (Must detect >5 call stack depth) ... [PASS] (Correctly rejected with call stack diagnostic)
Testing subroutine emission: Call (ПП / 0x53) and Return (В/О / 0x52) ... [PASS] (Emits native subroutine calls and returns)
Testing indirect memory addressing: Load (K П→x 7 / 0xD7) ... [PASS] (Emits native indirect addressing K П→x 7)
Testing RPN stack optimization: Forwarding accumulator across operations ... [PASS] (Intermediate values forwarded via $stack)
Testing optimization: Hardware loop counter (F L0 / 0x5D) emission ... [PASS] (Emits native F L0 hardware decrement & jump)
Testing multi-format exporter: --export-all (.mk56, .keys, .hex) ... [PASS]
Testing keystroke guide exporter: -f keys ... [PASS]
Testing emulator bytecode hex exporter: -f hex ... [PASS]

=== Test Summary ===
Passed: 18
Failed: 0
```

---

## License

This project is licensed under the MIT License. See `LICENSE` for details.
