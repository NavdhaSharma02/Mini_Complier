# Lexical analysis

The lexer implementation is in `src/lexer.cpp`; token definitions are in
`include/minic/token.hpp`.

For this source:

```c
int main() { return 12 + 3; }
```

the lexer emits these token kinds:

```text
Int Identifier LeftParen RightParen LeftBrace Return
Number Plus Number Semicolon RightBrace End
```

Each token stores its original text and starting source location. `12` is one
number token because the scanner consumes all consecutive digits. Whitespace and
comments are skipped; source lines and columns still advance while skipping them.

The scanner reads the longest supported operator: `!=` is one token, rather than
separate `!` and `=` tokens. A lone `&` or `|` produces a diagnostic because the
language supports logical operators but excludes bitwise operators.

Inspect real output with:

```sh
./build/cmake/minic --tokens examples/factorial.c
```

Tokenization does not check expression structure. `12 + * 3` can be tokenized but
fails during parsing. Name resolution belongs to semantic validation: the lexer
recognizes an identifier without knowing whether it has been declared.
