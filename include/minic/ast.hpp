#pragma once

#include "minic/token.hpp"
#include <cstdint>
#include <memory>
#include <ostream>

namespace minic {

enum class ExprKind { Number, Variable, Unary, Binary, Assign, Call };
struct Expr {
    ExprKind kind;
    Token token;
    std::int64_t value = 0;
    int symbol = -1;
    std::unique_ptr<Expr> left, right;
    std::vector<std::unique_ptr<Expr>> arguments;
    Expr(ExprKind k, Token t) : kind(k), token(std::move(t)) {}
};
using ExprPtr = std::unique_ptr<Expr>;

enum class StmtKind { Block, Declaration, Expression, Return, If, While, For, Break, Continue, Empty };
struct Stmt {
    StmtKind kind;
    Token token;
    int symbol = -1;
    ExprPtr expression, step;
    std::unique_ptr<Stmt> init, body, otherwise;
    std::vector<std::unique_ptr<Stmt>> statements;
    Stmt(StmtKind k, Token t) : kind(k), token(std::move(t)) {}
};
using StmtPtr = std::unique_ptr<Stmt>;

struct Parameter { Token name; int symbol = -1; };
struct Function {
    Token name;
    std::vector<Parameter> parameters;
    StmtPtr body;
    int symbolCount = 0;
};
struct Program { std::vector<Function> functions; };

void printAST(const Program& program, std::ostream& out);

} // namespace minic
