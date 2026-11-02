int main() {
    int sum = 0;
    for (int i = 0; i < 10; i = i + 1) {
        if (i == 3) continue;
        if (i == 8) break;
        sum = sum + i;
    }
    int countdown = 3;
    while (countdown > 0) {
        sum = sum + countdown;
        countdown = countdown - 1;
    }
    return sum; // Exit status: 31.
}
