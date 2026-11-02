#!/usr/bin/env python3
"""End-to-end tests: source -> minic -> LLVM IR -> Clang -> native execution."""
import argparse
from pathlib import Path
import random
import subprocess
import sys
import tempfile
import unittest

ARGS = None
ROOT = Path(__file__).resolve().parents[1]


def run(command, **kwargs):
    return subprocess.run(command, capture_output=True, text=True, timeout=20, **kwargs)


class CompilerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="minic-test-")
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.source = self.directory / "input.c"

    def compile_source(self, source, *options):
        self.source.write_text(source)
        return run([ARGS.compiler, *options, str(self.source)])

    def execute(self, source, optimization, expected=None, stdout=""):
        result = self.compile_source(source, optimization)
        self.assertEqual(result.returncode, 0, result.stderr + "\n" + source)
        ir = self.directory / "output.ll"
        executable = self.directory / "program"
        ir.write_text(result.stdout)
        compiled = run([ARGS.clang, "-Wno-override-module", "-x", "ir", str(ir), "-o", str(executable)])
        self.assertEqual(compiled.returncode, 0, compiled.stderr + "\n" + result.stdout)
        executed = run([str(executable)])
        if expected is not None:
            self.assertEqual(executed.returncode, expected, source + "\n" + executed.stderr)
            self.assertEqual(executed.stdout, stdout, source)
        return executed

    def test_valid_programs(self):
        cases = [
            ("int main(){return 2+3*4;}", 14),
            ("int main(){return (2+3)*4;}", 20),
            ("int main(){return 20-6-3;}", 11),
            ("int main(){return 48/4/3;}", 4),
            ("int main(){return -17%5+10;}", 8),
            ("int main(){return -17/5+10;}", 7),
            ("int main(){return +5 + !0 + !8;}", 6),
            ("int main(){return (1<2)+(2<=2)+(3>2)+(3>=3)+(2==2)+(2!=3);}", 6),
            ("int main(){return (2<1)+(3<=2)+(1>2)+(1>=2)+(2==3)+(2!=2);}", 0),
            ("int main(){return 1||0&&0;}", 1),
            ("int main(){return (0||5)&&(2||0);}", 1),
            ("int main(){return 1<2==1;}", 1),
            ("int main(){int x=0; int y=0; x=y=9; return x+y;}", 18),
            ("int main(){int x=2; {int x=9; x=x+1;} return x;}", 2),
            ("int main(){int x=3; if(x) x=7; else x=9; return x;}", 7),
            ("int main(){int x=0; if(x) return 1; else return 2;}", 2),
            ("int main(){if(1) if(0) return 1; else return 2; return 3;}", 2),
            ("int main(){int i=0; int s=0; while(i<5){i=i+1; s=s+i;} return s;}", 15),
            ("int main(){int i=0; int s=0; while(i<9){i=i+1; if(i==2) continue; if(i==5) break; s=s+i;} return s;}", 8),
            ("int main(){int s=0; for(int i=0;i<5;i=i+1){if(i==2) continue; s=s+i;} return s;}", 8),
            ("int main(){int i=0; for(;;){i=i+1;if(i==4) break;}return i;}", 4),
            ("int main(){int n=0;for(int i=0;i<3;i=i+1){for(int j=0;j<5;j=j+1){if(j==2)break;n=n+1;}}return n;}", 6),
            ("int main(){int i=0;for(i=7;0;i=i+1){return 99;}return i;}", 7),
            ("int main(){int i=0;for(int j=(i=6);0;){i=8;}return i;}", 6),
            ("int main(){int x=3;while(0){x=9;}if(0)x=8;return x;}", 3),
            ("int main(){int x=1;0&&(x=9);1||(x=8);return x;}", 1),
            ("int main(){int x=0;int y=1;(x=2)&&((y=3)||(x=9));return x+y;}", 5),
            ("int main(){int x=0;int y=0;int z=0;return (x||(y&&z))||((x=7)&&(y=4));}", 1),
            ("int main(){return 0&&(1/0);}", 0),
            ("int main(){return 1||(1/0);}", 1),
            ("int main(){return ((2147483647+1)==-2147483648);}", 1),
            ("int main(){return (-(-2147483648)==-2147483648);}", 1),
            ("int main(){return (65536*65536)==0;}", 1),
            ("int main(){return (-2147483648-1)==2147483647;}", 1),
            ("int main(){/* block\ncomment */return 7;// line\n}", 7),
            ("int main(void){}", 0),
            ("int main(){;;;return 3;}", 3),
            ("int choose(int x){if(x)return 5;else return 6;}int main(){return choose(0);}", 6),
            ("int main(){return twice(7);}int twice(int x){return x*2;}", 14),
            ("int odd(int x){if(x==0)return 0;return even(x-1);}int even(int x){if(x==0)return 1;return odd(x-1);}int main(){return even(8);}", 1),
            ("int add(int a,int b){return a+b;}int main(){return add(4,add(3,2));}", 9),
            ("int f(int x){{int x=2;return x;}}int main(){return f(9);}", 2),
            ("int main(){return 3;putchar(88);}", 3),
            ("int main(){int x=2;if(x){return 3;}else{return 4;}putchar(88);}", 3),
            ("int main(){int x=0;while(1){if(x==0){x=1;continue;}else break;putchar(88);}return x;}", 1),
            ("int main(){int n=0;while(n<2){for(int i=0;i<2;i=i+1){continue;}n=n+1;}return n;}", 2),
        ]
        for optimization in ("-O0", "-O1"):
            for source, expected in cases:
                with self.subTest(optimization=optimization, source=source):
                    self.execute(source, optimization, expected)

    def test_examples(self):
        expected = {"factorial": (120, ""), "fibonacci": (55, ""), "loops": (31, ""),
                    "hello": (0, "Hello\n"), "optimizations": (14, "")}
        for name, (status, stdout) in expected.items():
            for optimization in ("-O0", "-O1"):
                with self.subTest(example=name, optimization=optimization):
                    self.execute((ROOT / "examples" / (name + ".c")).read_text(), optimization, status, stdout)

    def test_invalid_programs(self):
        cases = [
            ("", "must define"),
            ("int main(){return @;}", "unexpected character"),
            ("int main(){/*", "unterminated block comment"),
            ("int main(){return 1 & 2;}", "bitwise"),
            ("int main(){return 1 | 2;}", "bitwise"),
            ("int main(){return 2}", "expected Semicolon"),
            ("int main(){return 2;", "expected RightBrace"),
            ("int main(){return 2+*3;}", "expected an expression"),
            ("int main(){int x;return x;}", "require an initializer"),
            ("int main(){return missing;}", "undeclared variable"),
            ("int main(){x=3;return 0;}", "undeclared variable"),
            ("int main(){int x=1;int x=2;return x;}", "duplicate declaration"),
            ("int f(int x){int x=2;return x;}int main(){return 0;}", "duplicate declaration"),
            ("int f(int x,int x){return x;}int main(){return 0;}", "duplicate declaration"),
            ("int main(){int x=x;return x;}", "own initializer"),
            ("int main(){int x=(x=2);return x;}", "own initializer"),
            ("int main(){int x=1;{int x=x;}return 0;}", "own initializer"),
            ("int main(){{int x=1;}return x;}", "undeclared variable"),
            ("int main(){for(int i=0;i<2;i=i+1){}return i;}", "undeclared variable"),
            ("int main(){break;return 0;}", "only valid inside a loop"),
            ("int main(){continue;return 0;}", "only valid inside a loop"),
            ("int main(){return nope(1);}", "unknown function"),
            ("int main(){return putchar();}", "expects 1 argument"),
            ("int main(){int putchar=0;return putchar(1);}", "not a function"),
            ("int main(){return 1;}int main(){return 2;}", "duplicate function"),
            ("int putchar(int x){return x;}int main(){return 0;}", "duplicate function"),
            ("int main(int x){return x;}", "no parameters"),
            ("int f(){int x=1;}int main(){return 0;}", "every path"),
            ("int f(int x){if(x)return 1;}int main(){return 0;}", "every path"),
            ("int main(){return 2147483648;}", "32-bit range"),
            ("int main(){return 999999999999999999999999;}", "32-bit range"),
            ("int main(){1=2;return 0;}", "assignment requires a variable"),
            ("int main(){return;}", "expected an expression"),
            ("int main(){if(0)return undeclared;return 1;}", "undeclared variable"),
            ("int main(){return 1;return undeclared;}", "undeclared variable"),
        ]
        for source, message in cases:
            for optimization in ("-O0", "-O1"):
                with self.subTest(source=source, optimization=optimization):
                    result = self.compile_source(source, optimization)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn(message, result.stderr)
                    self.assertEqual(result.stdout, "")

    def test_tokens_and_locations(self):
        result = self.compile_source("// comment\nint main(){return 12!=3 && 4<=5;}", "--tokens")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('2:1 Int "int"', result.stdout)
        self.assertIn('2:19 Number "12"', result.stdout)
        self.assertIn('NotEqual "!="', result.stdout)
        self.assertIn('And "&&"', result.stdout)
        self.assertIn('LessEqual "<="', result.stdout)
        result = self.compile_source("int main(){\n  return unknown;\n}", "--check")
        self.assertIn(":2:10: semantic error:", result.stderr)
        self.assertIn("  return unknown;", result.stderr)
        self.assertIn("^", result.stderr)

    def test_ast_precedence_and_optimizations(self):
        source = "int main(){if(0)putchar(88);return 2+3*4;putchar(89);}"
        before = self.compile_source(source, "--ast", "-O0")
        after = self.compile_source(source, "--ast", "-O1", "--stats")
        self.assertEqual(before.returncode, 0, before.stderr)
        self.assertEqual(after.returncode, 0, after.stderr)
        self.assertIn("Binary +\n          Integer 2\n          Binary *", before.stdout)
        self.assertIn("Call putchar", before.stdout)
        self.assertIn("Integer 14", after.stdout)
        self.assertNotIn("Binary", after.stdout)
        self.assertNotIn("Call putchar", after.stdout)
        self.assertIn("2 constant expression(s) folded", after.stderr)
        self.assertIn("2 dead construct(s) removed", after.stderr)
        # DCE changes observable IR, not only the AST dump.
        before_ir = self.compile_source("int main(){if(0)putchar(88);return 2+3*4;}", "-O0")
        after_ir = self.compile_source("int main(){if(0)putchar(88);return 2+3*4;}", "-O1")
        self.assertIn("call i32 @putchar", before_ir.stdout)
        self.assertNotIn("call i32 @putchar", after_ir.stdout)
        self.assertIn("ret i32 14", after_ir.stdout)
        self.assertNotIn("mul i32", after_ir.stdout)

    def test_side_effects_preserved(self):
        source = "int main(){int x=0;(x=1)&&0;(x=2)||1;putchar(65);return x;}"
        for optimization in ("-O0", "-O1"):
            self.execute(source, optimization, 2, "A")

    def test_division_traps(self):
        for expression in ("1/0", "1%0", "-2147483648/-1", "-2147483648%-1"):
            for optimization in ("-O0", "-O1"):
                with self.subTest(expression=expression, optimization=optimization):
                    executed = self.execute("int main(){return " + expression + ";}", optimization)
                    self.assertNotEqual(executed.returncode, 0)

    def test_cli(self):
        for option in ("--help", "--version"):
            self.assertEqual(run([ARGS.compiler, option]).returncode, 0)
        for options in ([], ["--bad"], ["-o"], ["missing.c"], ["a.c", "b.c"], ["--ast", "--tokens", "x.c"]):
            self.assertNotEqual(run([ARGS.compiler, *options]).returncode, 0)
        self.source.write_text("int main(){return 8;}")
        protected = run([ARGS.compiler, str(self.source), "-o", str(self.source)])
        self.assertNotEqual(protected.returncode, 0)
        self.assertEqual(self.source.read_text(), "int main(){return 8;}")
        result = run([ARGS.compiler, "--check", str(self.source)])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "")
        result = run([ARGS.compiler, "-"], input="int main(){return 7;}")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("ret i32 7", result.stdout)
        output = self.directory / "saved.ll"
        result = run([ARGS.compiler, str(self.source), "-o", str(output)])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "")
        self.assertIn("ret i32 8", output.read_text())
        # Invalid source must not truncate an existing output file.
        result = self.compile_source("int main(){return missing;}", "-o", str(output))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("ret i32 8", output.read_text())
        result = self.compile_source("int main(){return 0;}", "-o", str(self.directory / "missing" / "out.ll"))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("cannot open output", result.stderr)

    def test_differential_arithmetic(self):
        rng = random.Random(42)
        def expression(depth):
            if depth == 0:
                return str(rng.randrange(1, 10))
            return "(" + expression(depth - 1) + rng.choice(["+", "-", "*", "<", "==", "&&", "||"]) + expression(depth - 1) + ")"
        for index in range(12):
            source = "int main(){return ((" + expression(3) + ")%100+100)%100;}"
            self.source.write_text(source)
            native = self.directory / "reference"
            compiled = run([ARGS.clang, "-x", "c", str(self.source), "-o", str(native)])
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            expected = run([str(native)]).returncode
            for optimization in ("-O0", "-O1"):
                with self.subTest(index=index, optimization=optimization):
                    self.execute(source, optimization, expected)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--clang", default="clang")
    ARGS, remaining = parser.parse_known_args()
    ARGS.compiler = str(Path(ARGS.compiler).resolve())
    unittest.main(argv=[sys.argv[0], *remaining], verbosity=2)
