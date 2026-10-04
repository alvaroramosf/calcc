# Micro C Example Programs for Elektronika MK-56 / MK-52

This directory contains real-world example programs written in Micro C and compiled into native Soviet RPN keystrokes and bytecodes.

Every program includes:
- **C Source Code** (`.c`)
- **Annotated Assembly Listing** (`.mk56`)
- **Physical Keyboard Typing Guide** (`.keys`)
- **Emulator Raw Bytecode Image** (`.hex`)

---

## Index of Examples

1. [01. Fibonacci Sequence (`01_fibonacci.c`)](#01-fibonacci-sequence)
2. [02. Circle Area Calculation (`02_circle_area.c`)](#02-circle-area-calculation)
3. [03. Pseudo-Random Dice Simulator (`03_dice_simulator.c`)](#03-pseudo-random-dice-simulator)
4. [04. Prime Number Checker (`04_prime_checker.c`)](#04-prime-number-checker)

---

## 01. Fibonacci Sequence

* **Source File**: [`01_fibonacci.c`](./01_fibonacci.c)
* **Program Size**: 24 / 98 steps (24.5% memory used)
* **Registers Used**: 4 / 14 (`R0`–`R3`)
* **Key Features**: Hardware loop decrement counter (`F L0` / `0x5D`), interactive user prompt (`input()`), display pause (`display()`).

### C Source Code
```c
#include <mk56.h>

int main(void) {
    // Prompt user for number of terms N
    int n = input();
    int a = 0;
    int b = 1;

    // Display initial term F(0) = 0
    display(a);

    // Iteratively compute and display Fibonacci numbers
    while (n > 0) {
        display(b);
        int next = a + b;
        a = b;
        b = next;
        n = n - 1;
    }

    return b;
}
```

### Generated Assembly Listing (`01_fibonacci.mk56`)
```assembly
; ==========================================================
; Target: Elektronika MK-56 / MK-52 (B3-34 Series)
; Step | VFD  | Hex   | Cyrillic | Description
; ----------------------------------------------------------
  00:   50    0x50   С/П       ; Prompt user: Enter value into X and press C/P
  01:   40    0x40   П 0       ; Store X into register
  02:   00    0x00   0         ; Load numeric constant into X
  03:   0E    0x0E   B^        ; Push X to Y (Enter)
  04:   41    0x41   П 1       ; Store X into register
  05:   01    0x01   1         ; Load numeric constant into X
  06:   0E    0x0E   B^        ; Push X to Y (Enter)
  07:   42    0x42   П 2       ; Store X into register
  08:   61    0x61   ИП 1      ; Load register into X
  09:   50    0x50   С/П       ; Display value in X and pause for user
  10:   62    0x62   ИП 2      ; Load register into X
  11:   50    0x50   С/П       ; Display value in X and pause for user
  12:   61    0x61   ИП 1      ; Load register into X
  13:   62    0x62   ИП 2      ; Load register into X
  14:   10    0x10   +         ; X = Y + X
  15:   43    0x43   П 3       ; Store X into register
  16:   62    0x62   ИП 2      ; Load register into X
  17:   41    0x41   П 1       ; Store X into register
  18:   63    0x63   ИП 3      ; Load register into X
  19:   42    0x42   П 2       ; Store X into register
  20:   5Г    0x5D   F L0      ; Hardware loop decrement & branch if != 0
  21:   10    0x0A   10        ; Loop restart address
  22:   62    0x62   ИП 2      ; Load register into X
  23:   50    0x50   С/П       ; Stop program execution
```

### How to Run on Real Hardware / Emulator
1. Press `[ В/О ]` (Reset step pointer to 00), then switch to `[ ПРГ ]` (Program Mode).
2. Enter the keystrokes from [`01_fibonacci.keys`](./01_fibonacci.keys).
3. Switch back to `[ АВТ ]` (Automatic / Run Mode).
4. Press `[ В/О ] [ С/П ]`.
5. The calculator stops at Step 00: type `7` and press `[ С/П ]`.
6. Sequential display outputs on each subsequent `[ С/П ]` press:
   - `0` (F₀)
   - `1` (F₁)
   - `1` (F₂)
   - `2` (F₃)
   - `3` (F₄)
   - `5` (F₅)
   - `8` (F₆)
   - `13` (F₇ — final term!)

---

## 02. Circle Area Calculation

* **Source File**: [`02_circle_area.c`](./02_circle_area.c)
* **Program Size**: 21 / 98 steps (21.4% memory used)
* **Registers Used**: 2 / 14 (`R0`, `R1`)
* **Key Features**: Function modularization with parameters, native Pi instruction (`F π` / `0x20`), subroutine call (`ПП` / `0x53`), subroutine return (`В/О` / `0x52`), entry point jump (`БП` / `0x51`).

### C Source Code
```c
#include <mk56.h>

float circle_area(float r) {
    float pi = 3.14159265;
    return pi * r * r;
}

int main(void) {
    // Prompt user to enter radius 'r'
    float r = input();

    // Compute circle area via subroutine call
    float area = circle_area(r);

    // Display calculated area on VFD display
    display(area);

    return 0;
}
```

### Generated Assembly Listing (`02_circle_area.mk56`)
```assembly
; ==========================================================
; Target: Elektronika MK-56 / MK-52 (B3-34 Series)
; Step | VFD  | Hex   | Cyrillic | Description
; ----------------------------------------------------------
  00:   51    0x51   БП        ; Unconditional Jump
  01:   11    0x0B   11        ; Jump destination address
; --- Subroutine circle_area (Step 02) ---
  02:   20    0x20   F π       ; Load constant Pi into X
  03:   0E    0x0E   B^        ; Push X to Y (Enter)
  04:   40    0x40   П 0       ; Store X into register
  05:   60    0x60   ИП 0      ; Load register into X
  06:   61    0x61   ИП 1      ; Load register into X
  07:   12    0x12   *         ; X = Y * X
  08:   61    0x61   ИП 1      ; Load register into X
  09:   12    0x12   *         ; X = Y * X
  10:   52    0x52   В/О       ; Return from subroutine
; --- Function main() (Step 11) ---
  11:   50    0x50   С/П       ; Prompt user: Enter value into X and press C/P
  12:   41    0x41   П 1       ; Store X into register
  13:   53    0x53   ПП        ; Call subroutine
  14:   02    0x02   02        ; Subroutine address
  15:   40    0x40   П 0       ; Store X into register
  16:   60    0x60   ИП 0      ; Load register into X
  17:   41    0x41   П 1       ; Store X into register
  18:   61    0x61   ИП 1      ; Load register into X
  19:   50    0x50   С/П       ; Display value in X and pause for user
  20:   50    0x50   С/П       ; Stop program execution
```

### How to Run
1. Enter the program in `[ ПРГ ]` mode following [`02_circle_area.keys`](./02_circle_area.keys).
2. Switch to `[ АВТ ]`, press `[ В/О ] [ С/П ]`.
3. The program stops at Step 11: enter radius `5` and press `[ С/П ]`.
4. Output displayed: **`78.54`** ($\pi \times 5^2 \approx 78.5398$).

---

## 03. Pseudo-Random Dice Simulator

* **Source File**: [`03_dice_simulator.c`](./03_dice_simulator.c)
* **Program Size**: 39 / 98 steps (39.8% memory used)
* **Registers Used**: 2 / 14 (`R0`, `R1`)
* **Key Features**: Linear Congruential PRNG ($X_{n+1} = (aX + c) \pmod 6$), modulo reduction via repeated subtraction, reusable rolls.

### C Source Code
```c
#include <mk56.h>

int roll(int seed) {
    // Linear congruential generator: next = (seed * 7 + 5) mod 6
    int val = seed * 7 + 5;
    while (val >= 6) {
        val = val - 6;
    }
    return val + 1; // Produces roll in range [1..6]
}

int main(void) {
    // Prompt user to enter initial integer seed (e.g. 17 or any integer)
    int seed = input();

    // Roll dice and display result
    int dice = roll(seed);
    display(dice);

    return dice;
}
```

### Execution Example
* Enter seed `17` $\rightarrow$ output: `3`
* Enter seed `42` $\rightarrow$ output: `6`
* Enter seed `99` $\rightarrow$ output: `4`

---

## 04. Prime Number Checker

* **Source File**: [`04_prime_checker.c`](./04_prime_checker.c)
* **Program Size**: 67 / 98 steps (68.4% memory used)
* **Registers Used**: 3 / 14 (`R0`–`R2`)
* **Key Features**: Trial division algorithm, nested loops, conditional branching (`x=0`, `x>=0`), boolean result output.

### C Source Code
```c
#include <mk56.h>

int is_prime(int n) {
    if (n <= 1) {
        return 0; // 0 and 1 are not prime
    }

    int d = 2;
    while (d * d <= n) {
        // Compute rem = n % d using repeated subtraction
        int rem = n;
        while (rem >= d) {
            rem = rem - d;
        }

        // If remainder is zero, n is divisible by d (not prime)
        if (rem == 0) {
            return 0;
        }

        d = d + 1;
    }

    return 1; // Prime!
}

int main(void) {
    // Prompt user to enter candidate number N
    int n = input();

    // Check primality and display result (1 = prime, 0 = composite)
    int result = is_prime(n);
    display(result);

    return result;
}
```

### Execution Example
1. Run program (`[ В/О ] [ С/П ]`).
2. Input `17` $\rightarrow$ Press `[ С/П ]` $\rightarrow$ Display: **`1`** (Prime!)
3. Run program again (`[ В/О ] [ С/П ]`).
4. Input `21` $\rightarrow$ Press `[ С/П ]` $\rightarrow$ Display: **`0`** (Composite: $3 \times 7 = 21$).
5. Input `97` $\rightarrow$ Press `[ С/П ]` $\rightarrow$ Display: **`1`** (Prime!).

---

## Compilation

To recompile all examples to assembly, keystroke guides, and hex images:

```bash
for f in examples/*.c; do
    base=$(basename "$f" .c)
    ./build/mk56c --export-all "$f" -o "examples/$base" -v
done
```
