#include "minic/semantic.hpp"
#include <unordered_map>

namespace minic {
namespace {

bool alwaysReturns(const Stmt& stmt) {
    if (stmt.kind == StmtKind::Return) return true;
    if (stmt.kind == StmtKind::If)
        return stmt.otherwise && alwaysReturns(*stmt.body) && alwaysReturns(*stmt.otherwise);
    if (stmt.kind == StmtKind::Block) {
        for (const auto& child : stmt.statements) if (alwaysReturns(*child)) return true;
    }
    return false;
}

class Analyzer {
    std::unordered_map<std::string, std::size_t> functions_{{"putchar", 1}};
    std::vector<std::unordered_map<std::string, int>> scopes_;
    int nextSymbol_ = 0, loopDepth_ = 0, initializing_ = -1;

    int lookup(const std::string& name) const {
        for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
            const auto found = scope->find(name);
            if (found != scope->end()) return found->second;
        }
        return -1;
    }
    int declare(const Token& name) {
        if (scopes_.back().count(name.text))
            throw Diagnostic(name.location, "semantic", "duplicate declaration of '" + name.text + "' in this scope");
        const int id = nextSymbol_++;
        scopes_.back()[name.text] = id;
        return id;
    }
    void expression(Expr& expr) {
        switch (expr.kind) {
            case ExprKind::Number:
                if (expr.value > 2147483647LL || expr.value < -2147483648LL)
                    throw Diagnostic(expr.token.location, "semantic", "integer literal is outside the signed 32-bit range");
                break;
            case ExprKind::Variable:
                expr.symbol = lookup(expr.token.text);
                if (expr.symbol < 0) throw Diagnostic(expr.token.location, "semantic", "undeclared variable '" + expr.token.text + "'");
                if (expr.symbol == initializing_) throw Diagnostic(expr.token.location, "semantic", "variable '" + expr.token.text + "' is used in its own initializer");
                break;
            case ExprKind::Call: {
                if (lookup(expr.token.text) >= 0)
                    throw Diagnostic(expr.token.location, "semantic", "called object '" + expr.token.text + "' is a variable, not a function");
                const auto found = functions_.find(expr.token.text);
                if (found == functions_.end()) throw Diagnostic(expr.token.location, "semantic", "unknown function '" + expr.token.text + "'");
                if (found->second != expr.arguments.size())
                    throw Diagnostic(expr.token.location, "semantic", "function '" + expr.token.text + "' expects " + std::to_string(found->second) + " argument(s), got " + std::to_string(expr.arguments.size()));
                for (auto& argument : expr.arguments) expression(*argument);
                break;
            }
            case ExprKind::Unary: expression(*expr.left); break;
            case ExprKind::Binary: case ExprKind::Assign:
                expression(*expr.left); expression(*expr.right); break;
        }
    }
    void block(Stmt& stmt, bool newScope) {
        if (newScope) scopes_.emplace_back();
        for (auto& child : stmt.statements) statement(*child);
        if (newScope) scopes_.pop_back();
    }
    void statement(Stmt& stmt) {
        switch (stmt.kind) {
            case StmtKind::Block: block(stmt, true); break;
            case StmtKind::Declaration:
                stmt.symbol = declare(stmt.token);
                initializing_ = stmt.symbol;
                expression(*stmt.expression);
                initializing_ = -1;
                break;
            case StmtKind::Expression: case StmtKind::Return:
                expression(*stmt.expression); break;
            case StmtKind::If:
                expression(*stmt.expression);
                statement(*stmt.body);
                if (stmt.otherwise) statement(*stmt.otherwise);
                break;
            case StmtKind::While:
                expression(*stmt.expression);
                ++loopDepth_; statement(*stmt.body); --loopDepth_;
                break;
            case StmtKind::For:
                scopes_.emplace_back();
                statement(*stmt.init);
                if (stmt.expression) expression(*stmt.expression);
                if (stmt.step) expression(*stmt.step);
                ++loopDepth_; statement(*stmt.body); --loopDepth_;
                scopes_.pop_back();
                break;
            case StmtKind::Break: case StmtKind::Continue:
                if (loopDepth_ == 0) throw Diagnostic(stmt.token.location, "semantic", "'" + stmt.token.text + "' is only valid inside a loop");
                break;
            case StmtKind::Empty: break;
        }
    }
public:
    void run(Program& program) {
        for (const auto& function : program.functions) {
            if (!functions_.emplace(function.name.text, function.parameters.size()).second)
                throw Diagnostic(function.name.location, "semantic", "duplicate function '" + function.name.text + "'");
        }
        const auto main = functions_.find("main");
        if (main == functions_.end()) throw Diagnostic({}, "semantic", "program must define 'int main()'");
        if (main->second != 0) throw Diagnostic({}, "semantic", "main must have no parameters");
        for (auto& function : program.functions) {
            scopes_.clear(); scopes_.emplace_back(); nextSymbol_ = 0;
            for (auto& parameter : function.parameters) parameter.symbol = declare(parameter.name);
            block(*function.body, false);
            function.symbolCount = nextSymbol_;
            if (function.name.text != "main" && !alwaysReturns(*function.body))
                throw Diagnostic(function.name.location, "semantic", "function '" + function.name.text + "' must return a value on every path (loops are conservatively assumed to fall through)");
        }
    }
};
}
void validate(Program& program) { Analyzer{}.run(program); }
} // namespace minic
