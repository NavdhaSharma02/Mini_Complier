# Mini Compiler

A C++17 compiler for a small C-like language, with recursive-descent parsing,
semantic validation, LLVM IR generation, constant folding, and dead-code elimination.

## Build

Requirements: a C++17 compiler, CMake 3.20+, Python 3 for tests, and Clang 15+ to
compile the generated LLVM IR. The compiler emits LLVM's textual IR directly; it
does not require LLVM development headers or link against libLLVM.

```sh
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake --parallel
ctest --test-dir build/cmake --output-on-failure
```

Build without CMake:

```sh
mkdir -p build
c++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude \
    src/main.cpp src/lexer.cpp src/parser.cpp src/ast.cpp \
    src/semantic.cpp src/optimizer.cpp src/codegen.cpp -o build/minic
python3 tests/test_compiler.py --compiler build/minic --clang clang
```

If CMake was downloaded into this checkout's ignored `build/tools` directory, use
`build/tools/cmake/data/bin/cmake` and `build/tools/cmake/data/bin/ctest` in place
of the corresponding commands above.

## Compile and run

```sh
./build/cmake/minic -O1 examples/factorial.c -o build/factorial.ll
clang build/factorial.ll -o build/factorial
./build/factorial
echo $?  # 120

./build/cmake/minic examples/hello.c -o build/hello.ll
clang build/hello.ll -o build/hello
./build/hello  # Hello
```

Clang may print a harmless target-triple override warning because the IR is emitted
without a platform-specific triple. Use `-Wno-override-module` to suppress it.
Clang verifies/parses the generated IR and handles machine-code generation and linking.

## Commands

```sh
minic --tokens examples/factorial.c       # Tokens with line/column locations
minic --ast examples/factorial.c          # Validated syntax tree
minic --check examples/factorial.c        # Syntax and semantic checks only
minic -O0 examples/optimizations.c        # Unoptimized LLVM IR on stdout
minic -O1 --stats examples/optimizations.c # Optimized IR; pass counts on stderr
minic -O1 --ast examples/optimizations.c  # Inspect the optimized AST
minic --help
```

`--emit-llvm` explicitly selects the default output mode. `-o FILE` saves output;
`-o -` writes to stdout. An input filename of `-` reads from stdin. Validation
failures return status 1, show a source location and caret, and do not write output.

## Implemented language

- Signed 32-bit `int` values and decimal integer literals.
- Functions with integer parameters and return values, recursion, and forward calls.
- One required `int main()` or `int main(void)` entry point.
- Initialized local declarations, assignment expressions, lexical scopes, and shadowing.
- Arithmetic `+ - * / %`, unary `+ - !`, comparisons `== != < <= > >=`.
- Short-circuit `&&` and `||`, producing integer 0 or 1.
- `if`/`else`, `while`, `for`, `break`, `continue`, `return`, and empty statements.
- `//` and `/* ... */` comments and multiline source files.
- Built-in `putchar(int)` for output, linked from the system C library.

This is an explicitly limited educational dialect, not a conforming C implementation.
It excludes pointers, arrays, strings, chars, floating point, globals, preprocessing,
prototypes, variadic calls, `++`/`--`, compound assignments, and multiple declarations
in one statement. See [language rules and grammar](docs/language.md).

## Optimizations

`-O1` runs compiler-owned AST passes before IR generation:

- Fold constant arithmetic, unary, comparison, and logical expressions.
- Select constant `if` branches and remove always-false loops.
- Preserve a false `for` loop's initializer, which still executes.
- Remove statements after unconditional `return`, `break`, or `continue`, including
  blocks and `if` statements whose branches both transfer control.
- Remove expression statements reduced to side-effect-free constants.

Calls and assignments are preserved when reachable. Short-circuit folding preserves
operand evaluation rules. Invalid constant division remains guarded by a runtime trap.
This project does not implement general liveness-based removal of unused variables
or assignments. These passes are independent of any further optimization Clang runs.

## Structure

```text
include/minic/  Tokens, AST, and phase interfaces
src/lexer.cpp  Source characters -> tokens
src/parser.cpp Tokens -> AST
src/semantic.cpp  Scopes, symbol resolution, function and control-flow checks
src/optimizer.cpp Constant folding and dead-code elimination
src/codegen.cpp   Validated AST -> textual LLVM IR
src/ast.cpp    AST printer
src/main.cpp   CLI, file IO, diagnostics, phase orchestration
examples/      Programs with expected results
tests/         Native execution, negative, CLI, and differential tests
docs/          Language specification and implementation notes
```

See [architecture](docs/architecture.md) and [lexer notes](docs/01-lexer.md).

## Verification

The integration suite compiles emitted IR with Clang and executes it in both `-O0`
and `-O1` modes. It checks functions, recursion, scopes, loops, short-circuit side
effects, integer boundaries, runtime division traps, diagnostics, and CLI behavior.
Seeded arithmetic expressions are also compared with programs compiled directly by
Clang. AST and IR checks verify that optimizations actually remove computations.

Enable sanitizers for the compiler itself:

```sh
cmake -S . -B build/sanitize -DCMAKE_BUILD_TYPE=Debug -DMINIC_SANITIZERS=ON
cmake --build build/sanitize --parallel
ctest --test-dir build/sanitize --output-on-failure
```

Tests require Python and Clang; configure with `-DBUILD_TESTING=OFF` to build only
the compiler. GitHub Actions runs sanitizer builds and tests on Linux and macOS.

## References

- [LLVM language reference](https://llvm.org/docs/LangRef.html)
- [LLVM opaque pointers](https://llvm.org/docs/OpaquePointers.html)
