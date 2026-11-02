#include "minic/optimizer.hpp"
#include <optional>

namespace minic {
namespace {

std::int64_t wrap(std::int64_t value) {
    const auto low = static_cast<std::uint32_t>(value);
    return low <= 2147483647U ? static_cast<std::int64_t>(low) : static_cast<std::int64_t>(low) - 4294967296LL;
}
std::optional<std::int64_t> evaluate(TokenKind op, std::int64_t a, std::int64_t b) {
    switch (op) {
        case TokenKind::Plus: return wrap(a + b);
        case TokenKind::Minus: return wrap(a - b);
        case TokenKind::Star: return wrap(a * b);
        case TokenKind::Slash: case TokenKind::Percent:
            if (b == 0 || (a == -2147483648LL && b == -1)) return std::nullopt;
            return op == TokenKind::Slash ? a / b : a % b;
        case TokenKind::Equal: return a == b;
        case TokenKind::NotEqual: return a != b;
        case TokenKind::Less: return a < b;
        case TokenKind::LessEqual: return a <= b;
        case TokenKind::Greater: return a > b;
        case TokenKind::GreaterEqual: return a >= b;
        case TokenKind::And: return a != 0 && b != 0;
        case TokenKind::Or: return a != 0 || b != 0;
        default: return std::nullopt;
    }
}
void replaceNumber(ExprPtr& expr, std::int64_t value, OptimizationStats& stats) {
    auto replacement = std::make_unique<Expr>(ExprKind::Number, expr->token);
    replacement->value = value;
    expr = std::move(replacement);
    ++stats.folded;
}
void fold(ExprPtr& expr, OptimizationStats& stats) {
    if (!expr) return;
    if (expr->left) fold(expr->left, stats);
    // Discard a short-circuited operand only when the evaluated side is a literal.
    if (expr->kind == ExprKind::Binary && expr->left->kind == ExprKind::Number) {
        const auto op = expr->token.kind;
        const bool truth = expr->left->value != 0;
        if ((op == TokenKind::And && !truth) || (op == TokenKind::Or && truth)) {
            replaceNumber(expr, truth ? 1 : 0, stats); return;
        }
    }
    if (expr->right) fold(expr->right, stats);
    for (auto& argument : expr->arguments) fold(argument, stats);
    if (expr->kind == ExprKind::Unary && expr->left->kind == ExprKind::Number) {
        const auto value = expr->left->value;
        switch (expr->token.kind) {
            case TokenKind::Plus: replaceNumber(expr, value, stats); break;
            case TokenKind::Minus: replaceNumber(expr, wrap(-value), stats); break;
            case TokenKind::Bang: replaceNumber(expr, value == 0, stats); break;
            default: break;
        }
    } else if (expr->kind == ExprKind::Binary && expr->left->kind == ExprKind::Number && expr->right->kind == ExprKind::Number) {
        const auto result = evaluate(expr->token.kind, expr->left->value, expr->right->value);
        if (result) replaceNumber(expr, *result, stats);
    }
}
bool transfers(const Stmt& stmt) {
    if (stmt.kind == StmtKind::Return || stmt.kind == StmtKind::Break || stmt.kind == StmtKind::Continue) return true;
    if (stmt.kind == StmtKind::If) return stmt.otherwise && transfers(*stmt.body) && transfers(*stmt.otherwise);
    if (stmt.kind == StmtKind::Block) {
        for (const auto& child : stmt.statements) if (transfers(*child)) return true;
    }
    return false;
}
void optimizeStatement(StmtPtr& stmt, OptimizationStats& stats) {
    fold(stmt->expression, stats);
    fold(stmt->step, stats);
    if (stmt->init) optimizeStatement(stmt->init, stats);
    if ((stmt->kind == StmtKind::If || stmt->kind == StmtKind::While || stmt->kind == StmtKind::For) &&
        stmt->expression && stmt->expression->kind == ExprKind::Number) {
        const bool truth = stmt->expression->value != 0;
        if (stmt->kind == StmtKind::If) {
            auto selected = truth ? std::move(stmt->body) : std::move(stmt->otherwise);
            if (!selected) selected = std::make_unique<Stmt>(StmtKind::Empty, stmt->token);
            stmt = std::move(selected); ++stats.removed;
            optimizeStatement(stmt, stats); return;
        }
        if (!truth) {
            auto replacement = std::make_unique<Stmt>(StmtKind::Block, stmt->token);
            // A for-loop initializer still runs even when its condition is false.
            if (stmt->init) replacement->statements.push_back(std::move(stmt->init));
            stmt = std::move(replacement); ++stats.removed; return;
        }
    }
    if (stmt->body) optimizeStatement(stmt->body, stats);
    if (stmt->otherwise) optimizeStatement(stmt->otherwise, stats);
    if (stmt->kind == StmtKind::Expression && stmt->expression->kind == ExprKind::Number) {
        stmt = std::make_unique<Stmt>(StmtKind::Empty, stmt->token); ++stats.removed;
    }
    if (stmt->kind == StmtKind::Block) {
        std::vector<StmtPtr> kept;
        bool unreachable = false;
        for (auto& child : stmt->statements) {
            if (unreachable) { ++stats.removed; continue; }
            optimizeStatement(child, stats);
            if (child->kind == StmtKind::Empty) continue;
            unreachable = transfers(*child);
            kept.push_back(std::move(child));
        }
        stmt->statements = std::move(kept);
    }
}
}
OptimizationStats optimize(Program& program) {
    OptimizationStats stats;
    for (auto& function : program.functions) optimizeStatement(function.body, stats);
    return stats;
}
} // namespace minic
