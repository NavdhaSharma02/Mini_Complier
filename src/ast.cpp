#include "minic/ast.hpp"

namespace minic {
namespace {
void indent(std::ostream& out, int depth) { out << std::string(static_cast<std::size_t>(depth) * 2, ' '); }
void expression(const Expr& expr, std::ostream& out, int depth) {
    indent(out, depth);
    switch (expr.kind) {
        case ExprKind::Number: out << "Integer " << expr.value; break;
        case ExprKind::Variable: out << "Variable " << expr.token.text; break;
        case ExprKind::Unary: out << "Unary " << expr.token.text; break;
        case ExprKind::Binary: out << "Binary " << expr.token.text; break;
        case ExprKind::Assign: out << "Assign"; break;
        case ExprKind::Call: out << "Call " << expr.token.text; break;
    }
    out << '\n';
    if (expr.left) expression(*expr.left, out, depth + 1);
    if (expr.right) expression(*expr.right, out, depth + 1);
    for (const auto& argument : expr.arguments) expression(*argument, out, depth + 1);
}
void statement(const Stmt& stmt, std::ostream& out, int depth) {
    indent(out, depth);
    switch (stmt.kind) {
        case StmtKind::Block: out << "Block"; break;
        case StmtKind::Declaration: out << "Declare " << stmt.token.text; break;
        case StmtKind::Expression: out << "Expression"; break;
        case StmtKind::Return: out << "Return"; break;
        case StmtKind::If: out << "If"; break;
        case StmtKind::While: out << "While"; break;
        case StmtKind::For: out << "For"; break;
        case StmtKind::Break: out << "Break"; break;
        case StmtKind::Continue: out << "Continue"; break;
        case StmtKind::Empty: out << "Empty"; break;
    }
    out << '\n';
    if (stmt.init) { indent(out, depth + 1); out << "Initializer\n"; statement(*stmt.init, out, depth + 2); }
    if (stmt.expression) expression(*stmt.expression, out, depth + 1);
    if (stmt.step) { indent(out, depth + 1); out << "Step\n"; expression(*stmt.step, out, depth + 2); }
    for (const auto& child : stmt.statements) statement(*child, out, depth + 1);
    if (stmt.body) statement(*stmt.body, out, depth + 1);
    if (stmt.otherwise) { indent(out, depth + 1); out << "Else\n"; statement(*stmt.otherwise, out, depth + 2); }
}
}
void printAST(const Program& program, std::ostream& out) {
    out << "Program\n";
    for (const auto& function : program.functions) {
        out << "  Function int " << function.name.text << '\n';
        for (const auto& parameter : function.parameters) out << "    Parameter int " << parameter.name.text << '\n';
        statement(*function.body, out, 2);
    }
}
} // namespace minic
