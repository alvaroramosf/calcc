# Micro C Language Specification (Version 1.0)
## Formal Definition for Modular Retro-Calculator Transpilation

### 1. Overview and Design Philosophy
Micro C is a strict, structured subset of the ISO C99 standard designed specifically for transpilation into keystroke sequences, opcodes, and bytecode for resource-constrained programmable calculators.

The primary design goal is **target independence**: source programs written in Micro C remain completely portable across distinct calculator architectures (e.g., Soviet RPN B3-34/MK-56, Hewlett-Packard RPN, Texas Instruments AOS/Algebraic), while the compiler's optimization pipeline guarantees efficient physical resource allocation.

---

### 2. Lexical Structure

#### 2.1 Identifiers
Identifiers follow standard C rules:
- Must begin with a letter (`[a-zA-Z]`) or underscore (`_`).
- Subsequent characters may include digits (`[0-9]`).
- Case-sensitive (`total` and `Total` are distinct).

#### 2.2 Keywords
The following keywords are reserved:
```c
int       float     void      return
if        else      while     for
break     continue  halt
```

#### 2.3 Constants and Literals
- **Integer Literals**: Sequence of decimal digits (e.g., `0`, `42`, `-10`).
- **Floating-Point Literals**: Decimal numbers with fractional dot and optional exponent (e.g., `3.14159`, `1e-3`, `.5`).
*Note: Due to target hardware architectures, all numeric values are fundamentally treated as floating-point / BCD representations.*

#### 2.4 Operators
- **Arithmetic**: `+`, `-`, `*`, `/`
- **Relational**: `==`, `!=`, `<`, `<=`, `>`, `>=`
- **Logical**: `!` (logical NOT), `&&`, `||`
- **Assignment**: `=`

---

### 3. Grammar (EBNF)

```ebnf
Program         ::= ( FunctionDecl | GlobalVarDecl )*

FunctionDecl    ::= Type Identifier "(" ParamList? ")" Block
ParamList       ::= Type Identifier ( "," Type Identifier )*
Type            ::= "int" | "float" | "void"

Block           ::= "{" Statement* "}"

Statement       ::= VarDecl
                  | ExprStmt
                  | IfStmt
                  | WhileStmt
                  | ForStmt
                  | ReturnStmt
                  | HaltStmt
                  | Block

VarDecl         ::= Type Identifier ( "[" IntegerLiteral "]" | ( "=" Expression )? ) ";"
ExprStmt        ::= Expression? ";"

IfStmt          ::= "if" "(" Expression ")" Statement ( "else" Statement )?
WhileStmt       ::= "while" "(" Expression ")" Statement
ForStmt         ::= "for" "(" Expression? ";" Expression? ";" Expression? ")" Statement

ReturnStmt      ::= "return" Expression? ";"
HaltStmt        ::= "halt" ";"

Expression      ::= Assignment
Assignment      ::= ( Identifier | Identifier "[" Expression "]" ) "=" Assignment | LogicalOr
LogicalOr       ::= LogicalAnd ( "||" LogicalAnd )*
LogicalAnd      ::= Equality ( "&&" Equality )*
Equality        ::= Relational ( ( "==" | "!=" ) Relational )*
Relational      ::= Additive ( ( "<" | "<=" | ">" | ">=" ) Additive )*
Additive        ::= Multiplicative ( ( "+" | "-" ) Multiplicative )*
Multiplicative  ::= Unary ( ( "*" | "/" ) Unary )*
Unary           ::= ( "-" | "!" ) Unary | Primary
Primary         ::= Identifier ( "[" Expression "]" )?
                  | Constant
                  | CallExpr
                  | "(" Expression ")"

CallExpr        ::= Identifier "(" ArgList? ")"
ArgList         ::= Expression ( "," Expression )*
Constant        ::= IntegerLiteral | FloatLiteral
```

---

### 4. Memory Model and Variable Lifecycle

#### 4.1 Unbounded Virtual Variables
In Micro C source code, the programmer may declare any number of variables (more than 14). Variables are scoped by function or global declaration.

#### 4.2 Dead Variable Elimination (DVE)
If a variable is declared or assigned but its value is never read in any downstream control-flow path, the compiler prunes the declaration and all dead stores during intermediate optimization. No physical calculator register is assigned.

#### 4.3 Liveness Analysis & Register Sharing
Variables with **disjoint live ranges** share the same physical hardware register.
- A variable is *live* at point $P$ if there is a path from $P$ to a subsequent read of the variable without an intervening reassignment.
- The compiler constructs a **Register Interference Graph (RIG)**.
- If the chromatic number of the graph $\chi(G) \le \text{target.caps.data\_registers\_count}$, physical allocation succeeds without spills.

#### 4.4 Hardware Limit Diagnostic
If concurrent live variables at any program point exceed physical capacity, compilation terminates with an informative diagnostic indicating the offending line and the competing variables:
```text
Error: Register allocation failed for target 'mk56': Exceeded hardware limit of 14 physical registers.
```

#### 4.5 Array Memory Model & Indirect Addressing
- Arrays (`type arr[SIZE];`) are allocated to contiguous physical register blocks.
- Constant element accesses (`arr[0]`, `arr[1]`) are scalarized directly to physical registers `RG_base + 0`, `RG_base + 1`, etc.
- Dynamic element accesses (`arr[i]`) are lowered to indirect load (`IR_LOAD_INDIRECT`) and store (`IR_STORE_INDIRECT`) TAC operations.
- On the Elektronika MK-56 target:
  - Register `RG7` is strictly reserved as the hardware pointer register (`[ K ] [ П→x ] [ 7 ]` and `[ K ] [ x→П ] [ 7 ]`) because registers 0..6 undergo automatic increment/decrement during indirect operations, whereas register 7 remains unmodified.
  - Active array elements are automatically preserved across the entire program lifetime when dynamic indirect addressing is detected, preventing register recycling from clobbering array buffers.

#### 4.6 Subroutines & Call Stack Nesting Limit
- Functions with typed parameters and return values are lowered to modular subroutines.
- Invocations emit `ПП target_step` (Subroutine Call, `0x53`), and function returns emit `В/О` (Return, `0x52`).
- The top-level `main()` function is treated uniquely: it terminates with `С/П` (Halt, `0x50`) with the return value loaded into accumulator $X$.
- If auxiliary functions precede `main()`, step `00` emits an unconditional jump (`БП main_addr`) directly to `main()`.
- **Static Call Depth Verification**: The compiler computes the call graph reachability tree from `main()`. If nested calls exceed the MK-56 hardware limit of 5 return levels, compilation is rejected:
  ```text
  Error: Call stack depth (6) exceeds MK-56 hardware capacity of 5 levels.
  ```
- **Recursion Prohibition**: Recursive calls are detected at compile-time and prohibited because programmable calculators lack dynamic activation records and call frames.

### 5. Target Runtime Library: Elektronika MK-56 (`<mk56.h>` / `<elektronika_mk56.h>`)

For programs targeting the Elektronika MK-56, the primary architecture header is `<mk56.h>` (aliased as `<elektronika_mk56.h>`). It provides hardware limits, register constants, and runtime intrinsics:

| Function Signature | Semantics | MK-56 Physical Mapping | Opcode |
|:---|:---|:---|:---:|
| `double input(void)` | Pauses execution for user numerical input in display | Emits `С/П` (Stop/Run); input is stored in register $X$ | `0x0E` |
| `void display(double val)` | Displays `val` and halts program until user resumes | Loads `val` into $X$ (`ИП reg`), then emits `С/П` | `0x6x` + `0x0E` |
| `void pause(void)` | Temporarily flashes display (~0.5s) without halting | Emits hardware instruction `[F] Инд` (Indication) | `0x54` |
| `void clear_x(void)` | Clears the display register $X$ | Emits `Cx` | `0x01` |
| `void swap_xy(void)` | Swaps contents of registers $X$ and $Y$ | Emits `X<->Y` | `0x0D` |

*(Note: `<retro_calc.h>` is retained as a generic portable wrapper for backwards compatibility across future calculator targets).*

---

### 6. Compilation & Optimization Pipeline

```
[ Micro C Source (.c) ]
           │
           ▼ (Frontend: Flex/Bison)
     [ Typed AST ]
           │
           ├── Static Call Graph Verification (Max 5 Call Levels, Recursion Check)
           ▼ (Core Lowering)
  [ Three-Address Code (TAC) ]
           │
           ├── Peephole Optimizer & Copy Propagation
           ├── RPN Stack Optimization (Transient Accumulator $stack Forwarding)
           ├── Dead Variable Elimination (DVE)
           ├── Liveness Interval Analysis
           └── Graph Coloring Register Allocator
                    │
                    ▼ (Backend Target Interface & Step Measurement)
     [ Target RPN / Opcodes Assembly (.mk56, .keys, .hex) ]
```

#### 6.1 RPN Stack Forwarding Pass
When a temporary virtual register $v_K$ is defined by an arithmetic operation, negation, indirect load, or subroutine return, and is consumed exactly once by the immediately following instruction (`use_count == 1`), the compiler forwards the operand via the transient operational stack accumulator `$stack`. This completely bypasses physical register allocation, saving 2 program steps (`П reg` and `ИП reg`) and 1 physical register per intermediate term.
