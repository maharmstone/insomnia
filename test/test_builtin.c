// Test rule 10: builtins/intrinsics are implicitly nosleep

// EXPECTED-WARNING: function 'uses_builtin' makes no sleeping calls
void uses_builtin(void) {
    // __builtin_expect is a builtin — should be nosleep
    int x = __builtin_expect(1, 1);
    (void)x;
}

// nosleep calling a builtin should be fine
__attribute__((nosleep)) void safe_builtin(void) {
    int y = __builtin_clz(42);
    (void)y;
}
