#include "minic/token.hpp"
#include <unordered_map>

namespace minic {

const char* tokenName(TokenKind kind) {
    switch (kind) {
#define NAME(K) case TokenKind::K: return #K
        NAME(End); NAME(Number); NAME(Identifier); NAME(Int); NAME(Void);
        NAME(Return); NAME(If); NAME(Else); NAME(While); NAME(For);
        NAME(Break); NAME(Continue); NAME(Plus); NAME(Minus); NAME(Star);
        NAME(Slash); NAME(Percent); NAME(Bang); NAME(Assign); NAME(Equal);
        NAME(NotEqual); NAME(Less); NAME(LessEqual); NAME(Greater);
        NAME(GreaterEqual); NAME(And); NAME(Or); NAME(LeftParen);
        NAME(RightParen); NAME(LeftBrace); NAME(RightBrace);
        NAME(Semicolon); NAME(Comma);
#undef NAME
    }
    return "Unknown";
}

namespace {
bool digit(char c) { return c >= '0' && c <= '9'; }
bool alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
}

std::vector<Token> lex(const std::string& source) {
    static const std::unordered_map<std::string, TokenKind> keywords = {
        {"int", TokenKind::Int}, {"void", TokenKind::Void}, {"return", TokenKind::Return},
        {"if", TokenKind::If}, {"else", TokenKind::Else}, {"while", TokenKind::While},
        {"for", TokenKind::For}, {"break", TokenKind::Break}, {"continue", TokenKind::Continue}
    };
    std::vector<Token> tokens;
    Location at;
    auto advance = [&]() {
        if (source[at.offset] == '\n') { ++at.line; at.column = 1; }
        else { ++at.column; }
        ++at.offset;
    };
    auto current = [&]() { return at.offset < source.size() ? source[at.offset] : '\0'; };
    auto next = [&]() { return at.offset + 1 < source.size() ? source[at.offset + 1] : '\0'; };
    auto emit = [&](TokenKind kind, Location start) {
        tokens.push_back({kind, source.substr(start.offset, at.offset - start.offset), start});
    };
    while (at.offset < source.size()) {
        const char c = current();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f') {
            advance(); continue;
        }
        const Location start = at;
        if (c == '/' && next() == '/') {
            while (at.offset < source.size() && current() != '\n') advance();
            continue;
        }
        if (c == '/' && next() == '*') {
            advance(); advance();
            while (at.offset < source.size() && !(current() == '*' && next() == '/')) advance();
            if (at.offset == source.size()) throw Diagnostic(start, "lexer", "unterminated block comment");
            advance(); advance(); continue;
        }
        if (digit(c)) {
            while (digit(current())) advance();
            emit(TokenKind::Number, start); continue;
        }
        if (alpha(c)) {
            while (alpha(current()) || digit(current())) advance();
            const auto text = source.substr(start.offset, at.offset - start.offset);
            const auto keyword = keywords.find(text);
            emit(keyword == keywords.end() ? TokenKind::Identifier : keyword->second, start);
            continue;
        }
        advance();
        TokenKind kind;
        switch (c) {
            case '+': kind = TokenKind::Plus; break;
            case '-': kind = TokenKind::Minus; break;
            case '*': kind = TokenKind::Star; break;
            case '/': kind = TokenKind::Slash; break;
            case '%': kind = TokenKind::Percent; break;
            case '(': kind = TokenKind::LeftParen; break;
            case ')': kind = TokenKind::RightParen; break;
            case '{': kind = TokenKind::LeftBrace; break;
            case '}': kind = TokenKind::RightBrace; break;
            case ';': kind = TokenKind::Semicolon; break;
            case ',': kind = TokenKind::Comma; break;
            case '=': kind = current() == '=' ? (advance(), TokenKind::Equal) : TokenKind::Assign; break;
            case '!': kind = current() == '=' ? (advance(), TokenKind::NotEqual) : TokenKind::Bang; break;
            case '<': kind = current() == '=' ? (advance(), TokenKind::LessEqual) : TokenKind::Less; break;
            case '>': kind = current() == '=' ? (advance(), TokenKind::GreaterEqual) : TokenKind::Greater; break;
            case '&':
                if (current() != '&') throw Diagnostic(start, "lexer", "use '&&'; bitwise '&' is not supported");
                advance(); kind = TokenKind::And; break;
            case '|':
                if (current() != '|') throw Diagnostic(start, "lexer", "use '||'; bitwise '|' is not supported");
                advance(); kind = TokenKind::Or; break;
            default: throw Diagnostic(start, "lexer", "unexpected character '" + std::string(1, c) + "'");
        }
        emit(kind, start);
    }
    tokens.push_back({TokenKind::End, "", at});
    return tokens;
}
} // namespace minic
