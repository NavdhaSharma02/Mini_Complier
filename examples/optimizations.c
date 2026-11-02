int main() {
    int answer = 2 + 3 * 4;
    if (0) {
        putchar(88);
    }
    while (0) {
        answer = 99;
    }
    return answer;
    putchar(89); // Unreachable: removed from the optimized AST.
}
