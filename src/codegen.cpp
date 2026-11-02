#include "minic/codegen.hpp"
#include <sstream>
#include <utility>

namespace minic {
namespace {

class Generator {
    std::ostringstream out_;
    int nextValue_ = 0, nextLabel_ = 0;
    std::string currentBlock_;
    bool terminated_ = false;
    std::vector<std::pair<std::string, std::string>> loops_; // break, continue

    std::string temporary() { return "%t" + std::to_string(nextValue_++); }
    std::string label(const std::string& prefix) { return prefix + "." + std::to_string(nextLabel_++); }
    static std::string slot(int id) { return "%v" + std::to_string(id); }
    void emit(const std::string& instruction) { out_ << "  " << instruction << '\n'; }
    std::string value(const std::string& instruction) {
        const auto result = temporary(); emit(result + " = " + instruction); return result;
    }
    void startBlock(const std::string& name) {
        out_ << name << ":\n"; currentBlock_ = name; terminated_ = false;
    }
    void jump(const std::string& destination) {
        emit("br label %" + destination); terminated_ = true;
    }
    void branch(const std::string& condition, const std::string& yes, const std::string& no) {
        emit("br i1 " + condition + ", label %" + yes + ", label %" + no); terminated_ = true;
    }
    std::string truth(const std::string& number) { return value("icmp ne i32 " + number + ", 0"); }
    std::string logical(const Expr& expr) {
        const bool isAnd = expr.token.kind == TokenKind::And;
        const auto left = truth(expression(*expr.left));
        const auto leftBlock = currentBlock_;
        const auto rightBlock = label("logic.rhs"), endBlock = label("logic.end");
        branch(left, isAnd ? rightBlock : endBlock, isAnd ? endBlock : rightBlock);
        startBlock(rightBlock);
        const auto right = truth(expression(*expr.right));
        const auto rightEnd = currentBlock_;
        jump(endBlock);
        startBlock(endBlock);
        const auto result = value("phi i1 [ " + std::string(isAnd ? "false" : "true") +
                                  ", %" + leftBlock + " ], [ " + right + ", %" + rightEnd + " ]");
        return value("zext i1 " + result + " to i32");
    }
    void checkDivision(const std::string& left, const std::string& right) {
        const auto zero = value("icmp eq i32 " + right + ", 0");
        const auto minimum = value("icmp eq i32 " + left + ", -2147483648");
        const auto negativeOne = value("icmp eq i32 " + right + ", -1");
        const auto overflow = value("and i1 " + minimum + ", " + negativeOne);
        const auto invalid = value("or i1 " + zero + ", " + overflow);
        const auto fail = label("division.trap"), safe = label("division.safe");
        branch(invalid, fail, safe);
        startBlock(fail);
        emit("call void @llvm.trap()"); emit("unreachable"); terminated_ = true;
        startBlock(safe);
    }
    std::string expression(const Expr& expr) {
        switch (expr.kind) {
            case ExprKind::Number: return std::to_string(expr.value);
            case ExprKind::Variable: return value("load i32, ptr " + slot(expr.symbol) + ", align 4");
            case ExprKind::Assign: {
                const auto result = expression(*expr.right);
                emit("store i32 " + result + ", ptr " + slot(expr.left->symbol) + ", align 4");
                return result;
            }
            case ExprKind::Call: {
                std::vector<std::string> arguments;
                for (const auto& argument : expr.arguments) arguments.push_back(expression(*argument));
                std::string call = "call i32 @" + expr.token.text + "(";
                for (std::size_t i = 0; i < arguments.size(); ++i) {
                    if (i) call += ", ";
                    call += "i32 " + arguments[i];
                }
                return value(call + ")");
            }
            case ExprKind::Unary: {
                const auto operand = expression(*expr.left);
                if (expr.token.kind == TokenKind::Plus) return operand;
                if (expr.token.kind == TokenKind::Minus) return value("sub i32 0, " + operand);
                const auto result = value("icmp eq i32 " + operand + ", 0");
                return value("zext i1 " + result + " to i32");
            }
            case ExprKind::Binary: break;
        }
        if (expr.token.kind == TokenKind::And || expr.token.kind == TokenKind::Or) return logical(expr);
        const auto left = expression(*expr.left);
        const auto right = expression(*expr.right);
        std::string opcode;
        bool comparison = false;
        switch (expr.token.kind) {
            case TokenKind::Plus: opcode = "add"; break;
            case TokenKind::Minus: opcode = "sub"; break;
            case TokenKind::Star: opcode = "mul"; break;
            case TokenKind::Slash: opcode = "sdiv"; checkDivision(left, right); break;
            case TokenKind::Percent: opcode = "srem"; checkDivision(left, right); break;
            case TokenKind::Equal: opcode = "icmp eq"; comparison = true; break;
            case TokenKind::NotEqual: opcode = "icmp ne"; comparison = true; break;
            case TokenKind::Less: opcode = "icmp slt"; comparison = true; break;
            case TokenKind::LessEqual: opcode = "icmp sle"; comparison = true; break;
            case TokenKind::Greater: opcode = "icmp sgt"; comparison = true; break;
            case TokenKind::GreaterEqual: opcode = "icmp sge"; comparison = true; break;
            default: throw std::logic_error("unexpected binary operator in code generation");
        }
        const auto result = value(opcode + " i32 " + left + ", " + right);
        return comparison ? value("zext i1 " + result + " to i32") : result;
    }
    void statement(const Stmt& stmt) {
        if (terminated_) return;
        switch (stmt.kind) {
            case StmtKind::Block:
                for (const auto& child : stmt.statements) statement(*child);
                break;
            case StmtKind::Declaration: {
                const auto initial = expression(*stmt.expression);
                emit("store i32 " + initial + ", ptr " + slot(stmt.symbol) + ", align 4");
                break;
            }
            case StmtKind::Expression: expression(*stmt.expression); break;
            case StmtKind::Return: {
                const auto result = expression(*stmt.expression);
                emit("ret i32 " + result); terminated_ = true; break;
            }
            case StmtKind::If: {
                const auto condition = truth(expression(*stmt.expression));
                const auto yes = label("if.then"), no = label("if.else"), end = label("if.end");
                branch(condition, yes, no);
                startBlock(yes); statement(*stmt.body);
                const bool yesFallsThrough = !terminated_;
                if (yesFallsThrough) jump(end);
                startBlock(no);
                if (stmt.otherwise) statement(*stmt.otherwise);
                const bool noFallsThrough = !terminated_;
                if (noFallsThrough) jump(end);
                if (yesFallsThrough || noFallsThrough) startBlock(end);
                break;
            }
            case StmtKind::While: case StmtKind::For: {
                const bool isFor = stmt.kind == StmtKind::For;
                if (stmt.init) statement(*stmt.init);
                const auto head = label("loop.cond"), body = label("loop.body"),
                           step = label("loop.step"), end = label("loop.end");
                jump(head); startBlock(head);
                if (stmt.expression) {
                    const auto condition = truth(expression(*stmt.expression));
                    branch(condition, body, end);
                } else jump(body);
                startBlock(body);
                loops_.push_back({end, isFor ? step : head});
                statement(*stmt.body);
                loops_.pop_back();
                if (!terminated_) jump(isFor ? step : head);
                if (isFor) {
                    startBlock(step);
                    if (stmt.step) expression(*stmt.step);
                    jump(head);
                }
                startBlock(end);
                break;
            }
            case StmtKind::Break: jump(loops_.back().first); break;
            case StmtKind::Continue: jump(loops_.back().second); break;
            case StmtKind::Empty: break;
        }
    }
public:
    std::string run(const Program& program) {
        out_ << "; Generated by Mini Compiler\n"
                "declare i32 @putchar(i32)\n"
                "declare void @llvm.trap()\n\n";
        for (const auto& function : program.functions) {
            nextValue_ = 0; nextLabel_ = 0; loops_.clear();
            out_ << "define i32 @" << function.name.text << '(';
            for (std::size_t i = 0; i < function.parameters.size(); ++i) {
                if (i) out_ << ", ";
                out_ << "i32 %arg" << i;
            }
            out_ << ") {\n";
            startBlock("entry");
            for (int i = 0; i < function.symbolCount; ++i) emit(slot(i) + " = alloca i32, align 4");
            for (std::size_t i = 0; i < function.parameters.size(); ++i)
                emit("store i32 %arg" + std::to_string(i) + ", ptr " + slot(function.parameters[i].symbol) + ", align 4");
            statement(*function.body);
            if (!terminated_) {
                if (function.name.text == "main") emit("ret i32 0");
                else emit("unreachable"); // Semantic analysis already required all paths to return.
            }
            out_ << "}\n\n";
        }
        return out_.str();
    }
};
}
std::string generateIR(const Program& program) { return Generator{}.run(program); }
} // namespace minic
