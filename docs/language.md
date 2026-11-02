# Language specification

## Values and evaluation

All values and function return types are signed 32-bit integers. Integer literals
are decimal, including spellings with leading zeros; octal and hexadecimal syntax
are not supported. Literal values must fit `0..2147483647`, with the special negative
spelling `-2147483648` accepted. Leading zeros do not change the base.

Addition, subtraction, multiplication, and unary negation wrap modulo 2^32, then
interpret the result as a signed two's-complement integer. This is a deliberate
extension beyond C's undefined signed-overflow behavior.

Division truncates toward zero. Remainder has the dividend's sign. Division or
remainder by zero, and either operation on `-2147483648` and `-1`, trap at runtime
through `llvm.trap`; the compiler does not fold these expressions into a value.

Zero is false; other values are true. Comparisons and logical operators produce
integer 0 or 1. `&&` and `||` short-circuit. Other binary operands and call arguments
evaluate left to right. Assignment evaluates its right side, stores, and produces
that value. These rules make evaluation deterministic where C may leave ordering
unspecified. Assignment is right associative; other binary operators are left
associative.

## Names and scopes

Identifiers use ASCII letters, digits, and underscores and cannot start with a digit.
Each local declaration must have an initializer. A name is visible from its
declaration through the end of its block; any use in its own initializer is rejected.
Duplicate declarations in the same scope are rejected. Inner scopes may shadow
outer names. A function's parameters share a scope with its outermost body block.
`for` initialization declarations are scoped to the whole loop, including its body
and step, and are not visible afterward.

Functions take integer arguments and return an integer. Definitions are collected
before analyzing bodies, permitting recursion, mutual recursion, and forward calls
without prototypes. Calls must have exactly the declared number of arguments.
A local variable shadowing a function makes that name uncallable in its scope.
`putchar` is a reserved built-in function name and cannot be defined by the program.

The program must define exactly one parameterless `main`. Falling off its end
returns zero. Every other function must return on every syntactically evident path.
This check recognizes sequential returns and `if`/`else` with returning branches;
it conservatively assumes every loop may fall through. It does not prove constant
conditions or infinite loops. An explicit return after a loop satisfies the check.

Both reachable and unreachable code must pass semantic validation before any
optimization. `break` and `continue` require an enclosing loop. `continue` in a
`for` executes the step before rechecking the condition; in a `while` it rechecks
the condition. Transfers always refer to the innermost enclosing loop.

## Grammar

Quoted strings below are source spellings. `{ ... }` means repetition and `[ ... ]`
means optional content. `identifier` excludes keywords.

```ebnf
program      = { function } ;
function     = "int", identifier, "(", [ "void" | parameters ], ")", block ;
parameters   = "int", identifier, { ",", "int", identifier } ;
block        = "{", { declaration | statement }, "}" ;
declaration  = "int", identifier, "=", expression, ";" ;
statement    = block
             | "return", expression, ";"
             | "if", "(", expression, ")", statement, [ "else", statement ]
             | "while", "(", expression, ")", statement
             | "for", "(", for-init, [ expression ], ";", [ expression ], ")", statement
             | "break", ";" | "continue", ";"
             | [ expression ], ";" ;
for-init     = declaration | [ expression ], ";" ;
expression   = logical-or, [ "=", expression ] ;  (* left side must be a variable *)
logical-or   = logical-and, { "||", logical-and } ;
logical-and  = equality, { "&&", equality } ;
equality     = comparison, { ( "==" | "!=" ), comparison } ;
comparison   = sum, { ( "<" | "<=" | ">" | ">=" ), sum } ;
sum          = product, { ( "+" | "-" ), product } ;
product      = unary, { ( "*" | "/" | "%" ), unary } ;
unary        = ( "+" | "-" | "!" ), unary | primary ;
primary      = integer | identifier | "(", expression, ")"
             | identifier, "(", [ expression, { ",", expression } ], ")" ;
```

`else` associates with the nearest unmatched `if`. Declarations are block items or
`for` initializers; a bare declaration cannot be the body of an `if` or loop.
Whitespace and comments separate tokens but have no meaning in the AST. Block
comments do not nest. Diagnostics use one-based line and byte-column positions;
tabs count as one source column and are preserved in caret padding.

## LLVM representation

Every function returns `i32`. Variables have entry-block `alloca i32` slots and are
accessed with `load` and `store` through opaque `ptr` operands. Arithmetic has no
`nsw`/`nuw` flags, preserving wraparound. Comparisons produce `i1`, extended to `i32`.
Short-circuit expressions use conditional branches and `phi i1`. Division guards
branch to a trap block or a safe block. All basic blocks end with a terminator.

The emitted IR uses opaque pointers and targets Clang/LLVM 15 or newer. It has no
fixed target triple or data layout; Clang supplies the host target when compiling it.
