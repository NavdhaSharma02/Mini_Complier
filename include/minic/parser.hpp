#pragma once
#include "minic/ast.hpp"

namespace minic {

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens) : tokens_(tokens) {}
    Program parse();
private:
    const std::vector<Token>& tokens_;
    std::size_t pos_ = 0;
    const Token& peek() const;
    bool match(TokenKind kind);
    Token expect(TokenKind kind);
    StmtPtr block();
    StmtPtr declaration();
    StmtPtr statement();
    ExprPtr expression();
    ExprPtr binary(int minimumPrecedence);
    ExprPtr unary();
    ExprPtr primary();
};

} // namespace minic
