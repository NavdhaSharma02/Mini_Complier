#include "minic/codegen.hpp"
#include "minic/optimizer.hpp"
#include "minic/parser.hpp"
#include "minic/semantic.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>

namespace {
constexpr const char* help = R"(Mini Compiler 1.0
Usage: minic [options] <file.c | ->

  --tokens       Print lexical tokens
  --ast          Print the validated AST (optimized with -O1)
  --check        Validate the source without generating output
  --emit-llvm    Emit textual LLVM IR (the default)
  -O0            Disable AST optimizations (the default)
  -O1            Enable constant folding and dead-code elimination
  --stats        Print optimization counts to stderr
  -o <file>      Write output to a file; '-' means stdout
  --help         Show this message
  --version      Print the version

Example:
  minic -O1 examples/factorial.c -o build/factorial.ll
  clang build/factorial.ll -o build/factorial
)";

void report(const minic::Diagnostic& error, const std::string& path, const std::string& source) {
    std::cerr << path << ':' << error.location.line << ':' << error.location.column
              << ": " << error.stage << " error: " << error.what() << '\n';
    const auto offset = std::min(error.location.offset, source.size());
    const auto previousNewline = offset == 0 ? std::string::npos : source.rfind('\n', offset - 1);
    const auto start = previousNewline == std::string::npos ? 0 : previousNewline + 1;
    const auto end = source.find('\n', offset);
    const auto line = source.substr(start, end == std::string::npos ? std::string::npos : end - start);
    std::cerr << "  " << line << "\n  ";
    for (std::size_t i = start; i < offset; ++i) std::cerr << (source[i] == '\t' ? '\t' : ' ');
    std::cerr << "^\n";
}
}

int main(int argc, char* argv[]) {
    std::string input, output, source;
    bool optimized = false, statsRequested = false;
    enum class Mode { IR, Tokens, AST, Check };
    Mode mode = Mode::IR;
    bool modeSelected = false;
    try {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h") { std::cout << help; return 0; }
            if (arg == "--version") { std::cout << "minic 1.0.0\n"; return 0; }
            if (arg == "-O0" || arg == "-O1") { optimized = arg == "-O1"; continue; }
            if (arg == "--stats") { statsRequested = true; continue; }
            if (arg == "-o") {
                if (++i >= argc) throw std::runtime_error("-o requires an output path");
                output = argv[i]; continue;
            }
            if (arg == "--tokens" || arg == "--ast" || arg == "--check" || arg == "--emit-llvm") {
                if (modeSelected) throw std::runtime_error("choose only one of --tokens, --ast, --check, --emit-llvm");
                modeSelected = true;
                if (arg == "--tokens") mode = Mode::Tokens;
                else if (arg == "--ast") mode = Mode::AST;
                else if (arg == "--check") mode = Mode::Check;
                continue;
            }
            if (arg != "-" && !arg.empty() && arg[0] == '-') throw std::runtime_error("unknown option: " + arg);
            if (!input.empty()) throw std::runtime_error("expected exactly one input file");
            input = arg;
        }
        if (input.empty()) throw std::runtime_error("no input file; use --help for usage");
        if (mode == Mode::Check && !output.empty()) throw std::runtime_error("--check does not produce an output file");
        if (input != "-" && !output.empty() && output != "-") {
            std::error_code error;
            if (std::filesystem::weakly_canonical(input) == std::filesystem::weakly_canonical(output) ||
                std::filesystem::equivalent(input, output, error))
                throw std::runtime_error("input and output must be different files");
        }
        if (input == "-") source.assign(std::istreambuf_iterator<char>(std::cin), {});
        else {
            std::ifstream file(input, std::ios::binary);
            if (!file) throw std::runtime_error("cannot open input file: " + input);
            source.assign(std::istreambuf_iterator<char>(file), {});
            if (file.bad()) throw std::runtime_error("failed to read input file: " + input);
        }

        const auto tokens = minic::lex(source);
        std::ostringstream result;
        minic::OptimizationStats stats;
        if (mode == Mode::Tokens) {
            for (const auto& token : tokens) result << token.location.line << ':' << token.location.column
                << ' ' << minic::tokenName(token.kind) << ' ' << std::quoted(token.text) << '\n';
        } else {
            auto program = minic::Parser(tokens).parse();
            minic::validate(program);
            if (optimized && mode != Mode::Check) stats = minic::optimize(program);
            if (mode == Mode::AST) minic::printAST(program, result);
            else if (mode == Mode::IR) result << minic::generateIR(program);
        }
        if (statsRequested) std::cerr << "optimizations: " << stats.folded << " constant expression(s) folded, "
                                     << stats.removed << " dead construct(s) removed\n";
        if (!output.empty() && output != "-") {
            std::ofstream file(output, std::ios::binary);
            if (!file) throw std::runtime_error("cannot open output file: " + output);
            file << result.str();
            file.close();
            if (!file) throw std::runtime_error("failed to write output file: " + output);
        } else {
            std::cout << result.str();
            if (!std::cout) throw std::runtime_error("failed to write standard output");
        }
        return 0;
    } catch (const minic::Diagnostic& error) {
        report(error, input, source);
    } catch (const std::exception& error) {
        std::cerr << "minic: error: " << error.what() << '\n';
    }
    return 1;
}
