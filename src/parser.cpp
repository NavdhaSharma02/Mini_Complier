#include "minic/parser.hpp"
#include <limits>

namespace minic {

const Token& Parser::peek() const { return tokens_.at(pos_); }
bool Parser::match(TokenKind kind) {
    if (peek().kind != kind) return false;
    ++pos_; return true;
}
Token Parser::expect(TokenKind kind) {
    const auto token = peek();
    if (!match(kind)) {
        throw Diagnostic(token.location, "parser", std::string("expected ") + tokenName(kind) +
                         ", found " + tokenName(token.kind));
    }
    return token;
}

Program Parser::parse() {
    Program program;
    while (peek().kind != TokenKind::End) {
        expect(TokenKind::Int);
        Function function;
        function.name = expect(TokenKind::Identifier);
        expect(TokenKind::LeftParen);
        if (match(TokenKind::Void)) {
            expect(TokenKind::RightParen);
        } else if (!match(TokenKind::RightParen)) {
            do {
                expect(TokenKind::Int);
                function.parameters.push_back({expect(TokenKind::Identifier), -1});
            } while (match(TokenKind::Comma));
            expect(TokenKind::RightParen);
        }
        function.body = block();
        program.functions.push_back(std::move(function));
    }
    return program;
}

StmtPtr Parser::block() {
    auto result = std::make_unique<Stmt>(StmtKind::Block, expect(TokenKind::LeftBrace));
    while (peek().kind != TokenKind::RightBrace) {
        if (peek().kind == TokenKind::End) expect(TokenKind::RightBrace);
        result->statements.push_back(peek().kind == TokenKind::Int ? declaration() : statement());
    }
    expect(TokenKind::RightBrace);
    return result;
}

StmtPtr Parser::declaration() {
    expect(TokenKind::Int);
    auto result = std::make_unique<Stmt>(StmtKind::Declaration, expect(TokenKind::Identifier));
    if (!match(TokenKind::Assign)) {
        throw Diagnostic(peek().location, "parser", "local declarations require an initializer, for example 'int x = 0;'");
    }
    result->expression = expression();
    expect(TokenKind::Semicolon);
    return result;
}

StmtPtr Parser::statement() {
    const auto token = peek();
    if (token.kind == TokenKind::LeftBrace) return block();
    if (match(TokenKind::Return)) {
        auto result = std::make_unique<Stmt>(StmtKind::Return, token);
        result->expression = expression();
        expect(TokenKind::Semicolon); return result;
    }
    if (match(TokenKind::If) || match(TokenKind::While)) {
        const bool isIf = token.kind == TokenKind::If;
        auto result = std::make_unique<Stmt>(isIf ? StmtKind::If : StmtKind::While, token);
        expect(TokenKind::LeftParen);
        result->expression = expression();
        expect(TokenKind::RightParen);
        result->body = statement();
        if (isIf && match(TokenKind::Else)) result->otherwise = statement();
        return result;
    }
    if (match(TokenKind::For)) {
        auto result = std::make_unique<Stmt>(StmtKind::For, token);
        expect(TokenKind::LeftParen);
        if (peek().kind == TokenKind::Int) result->init = declaration();
        else {
            result->init = std::make_unique<Stmt>(StmtKind::Empty, peek());
            if (peek().kind != TokenKind::Semicolon) {
                result->init->kind = StmtKind::Expression;
                result->init->expression = expression();
            }
            expect(TokenKind::Semicolon);
        }
        if (peek().kind != TokenKind::Semicolon) result->expression = expression();
        expect(TokenKind::Semicolon);
        if (peek().kind != TokenKind::RightParen) result->step = expression();
        expect(TokenKind::RightParen);
        result->body = statement();
        return result;
    }
    if (match(TokenKind::Break) || match(TokenKind::Continue)) {
        auto result = std::make_unique<Stmt>(token.kind == TokenKind::Break ? StmtKind::Break : StmtKind::Continue, token);
        expect(TokenKind::Semicolon); return result;
    }
    if (match(TokenKind::Semicolon)) return std::make_unique<Stmt>(StmtKind::Empty, token);
    auto result = std::make_unique<Stmt>(StmtKind::Expression, token);
    result->expression = expression();
    expect(TokenKind::Semicolon);
    return result;
}

namespace {
int precedence(TokenKind kind) {
    switch (kind) {
        case TokenKind::Or: return 1;
        case TokenKind::And: return 2;
        case TokenKind::Equal: case TokenKind::NotEqual: return 3;
        case TokenKind::Less: case TokenKind::LessEqual:
        case TokenKind::Greater: case TokenKind::GreaterEqual: return 4;
        case TokenKind::Plus: case TokenKind::Minus: return 5;
        case TokenKind::Star: case TokenKind::Slash: case TokenKind::Percent: return 6;
        default: return 0;
    }
}
}

ExprPtr Parser::expression() {
    auto left = binary(1);
    if (peek().kind == TokenKind::Assign) {
        const auto token = expect(TokenKind::Assign);
        if (left->kind != ExprKind::Variable) throw Diagnostic(token.location, "parser", "assignment requires a variable on the left");
        auto result = std::make_unique<Expr>(ExprKind::Assign, token);
        result->left = std::move(left);
        result->right = expression();
        return result;
    }
    return left;
}

ExprPtr Parser::binary(int minimumPrecedence) {
    auto left = unary();
    while (precedence(peek().kind) >= minimumPrecedence) {
        const auto token = tokens_[pos_++];
        auto result = std::make_unique<Expr>(ExprKind::Binary, token);
        result->left = std::move(left);
        result->right = binary(precedence(token.kind) + 1);
        left = std::move(result);
    }
    return left;
}

ExprPtr Parser::unary() {
    if (peek().kind == TokenKind::Plus || peek().kind == TokenKind::Minus || peek().kind == TokenKind::Bang) {
        const auto token = tokens_[pos_++];
        auto operand = unary();
        // Permit the spelling of INT32_MIN without accepting a positive out-of-range literal.
        if (token.kind == TokenKind::Minus && operand->kind == ExprKind::Number && operand->value == 2147483648LL) {
            operand->value = -2147483648LL; operand->token = token; return operand;
        }
        auto result = std::make_unique<Expr>(ExprKind::Unary, token);
        result->left = std::move(operand); return result;
    }
    return primary();
}

ExprPtr Parser::primary() {
    const auto token = peek();
    if (match(TokenKind::Number)) {
        auto result = std::make_unique<Expr>(ExprKind::Number, token);
        for (char digit : token.text) {
            result->value = result->value * 10 + (digit - '0');
            if (result->value > 2147483648LL) throw Diagnostic(token.location, "parser", "integer literal is outside the signed 32-bit range");
        }
        return result;
    }
    if (match(TokenKind::Identifier)) {
        auto result = std::make_unique<Expr>(ExprKind::Variable, token);
        if (match(TokenKind::LeftParen)) {
            result->kind = ExprKind::Call;
            if (!match(TokenKind::RightParen)) {
                do { result->arguments.push_back(expression()); } while (match(TokenKind::Comma));
                expect(TokenKind::RightParen);
            }
        }
        return result;
    }
    if (match(TokenKind::LeftParen)) {
        auto result = expression(); expect(TokenKind::RightParen); return result;
    }
    throw Diagnostic(token.location, "parser", "expected an expression, found " + std::string(tokenName(token.kind)));
}
} // namespace minic
