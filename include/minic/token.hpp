#pragma once

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace minic {

struct Location { std::size_t offset = 0, line = 1, column = 1; };

class Diagnostic : public std::runtime_error {
public:
    Location location;
    std::string stage;
    Diagnostic(Location at, std::string phase, const std::string& message)
        : std::runtime_error(message), location(at), stage(std::move(phase)) {}
};

enum class TokenKind {
    End, Number, Identifier, Int, Void, Return, If, Else, While, For, Break, Continue,
    Plus, Minus, Star, Slash, Percent, Bang, Assign, Equal, NotEqual,
    Less, LessEqual, Greater, GreaterEqual, And, Or,
    LeftParen, RightParen, LeftBrace, RightBrace, Semicolon, Comma
};

struct Token {
    TokenKind kind;
    std::string text;
    Location location;
};

const char* tokenName(TokenKind kind);
std::vector<Token> lex(const std::string& source);

} // namespace minic
