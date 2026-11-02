# Implementation map

The compiler's driver runs this pipeline:

```text
source -> lex -> parse -> validate -> optional optimize -> generateIR
             |        |                              |
          --tokens  --ast (after validation)       .ll output
```

`--check` stops after semantic validation. `--ast -O1` shows the optimized tree.

## Lexical analysis

`lexer.cpp` advances an offset, line, and column through the source. It recognizes
keywords, identifiers, decimal integers, punctuation, and two-character operators.
Comments are consumed before interpreting `/` as division. An explicit end token
lets the parser report missing delimiters at the end of a file.

## Parsing and ownership

`parser.cpp` uses recursive descent for functions, blocks, statements, assignment,
unary expressions, and primary expressions. Binary expressions use precedence
climbing inside recursive descent. Increasing the minimum precedence for a right
operand gives arithmetic its left associativity. Assignment recursively parses a
full right expression to obtain right associativity.

AST nodes own their children through `std::unique_ptr`; the complete tree has a
single owner and no manual deletion. Expressions and statements use explicit kind
tags. Tokens are retained on nodes so later phases can report original locations.

## Semantic validation

`semantic.cpp` first registers function signatures, then validates each body. A stack
of maps implements lexical scopes. Every declaration gets a unique integer symbol
ID within its function. Variable nodes are annotated with the resolved ID; code
generation therefore cannot confuse two variables with the same spelling.

The analyzer rejects unresolved names, duplicate declarations, self-initialization,
invalid calls, out-of-range literals, misplaced loop transfers, and missing returns.
All supported expressions have the same integer type. Non-integer declarations and
operations are excluded by the grammar rather than an elaborate type system.

## Optimization safety

`optimizer.cpp` folds bottom-up using 64-bit host arithmetic and an explicit 32-bit
wrap operation. Two 32-bit integer operands multiply safely in the 64-bit host type.
Division is evaluated only when it is safe. Logical short-circuit folding only
discards an operand whose evaluation is unreachable.

Dead-code elimination selects constant branches, eliminates false loops, removes
constant expression statements, and prunes statements following guaranteed control
transfers. It preserves selected blocks and symbol IDs, so shadowing remains intact.
It retains `for` initializers even when the condition is always false. The statistics
count expression replacements and dead constructs, not individual instructions.

The pass does not fold algebraic identities like `x * 0` or discard calls and stores
based on presumed purity. It does not perform constant propagation or global liveness
analysis. Code generation also stops emitting instructions after a block terminates,
including at `-O0`, because LLVM forbids instructions after terminators.

## IR construction

`codegen.cpp` emits LLVM assembly directly. Named temporaries and block labels have
separate prefixes. Local slots are allocated once in each function's entry block,
including declarations inside loops. The current basic-block label is tracked so
each short-circuit `phi` refers to the actual predecessor, even for nested logical
expressions and division guards.

An `if` joins only when at least one branch falls through. A loop has condition,
body, optional step, and exit blocks. A stack records the innermost loop's break and
continue targets. Runtime guards protect division and remainder from invalid input.

This backend does not wrap LLVM's C++ IRBuilder API or call its in-process verifier.
The tests pass the emitted assembly to Clang, which verifies/parses it and uses LLVM
to generate native executables. Keeping the emission layer standalone makes the
compiler buildable with only a C++17 toolchain.

## Tests

`tests/test_compiler.py` uses temporary directories and argument arrays for every
subprocess, so source or file paths never become shell commands. It checks native
behavior at both optimization levels, malformed programs and diagnostic locations,
AST structure and optimization effects, output-file safety, runtime traps, and
seeded differential examples against Clang's C frontend. Each subprocess has a
timeout to catch hangs.
