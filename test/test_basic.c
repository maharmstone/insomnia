// Test rules 1-3: explicit tags and implicit nosleep for leaf functions
// and functions calling only nosleep functions.

// Rule 1: explicit tags
__attribute__((nosleep)) void safe_func(void) {}
__attribute__((might_sleep)) void maybe_func(void) {}
__attribute__((sleeps)) void sleepy_func(void) {}

// Rule 2: leaf function (no calls) is implicitly nosleep
// Forward declaration + definition triggers the warning.
void leaf_func(void);
// EXPECTED-WARNING: function 'leaf_func' makes no sleeping calls
void leaf_func(void) {
    int x = 42;
    (void)x;
}

// Rule 3: function calling only nosleep functions is implicitly nosleep
void calls_safe(void);
// EXPECTED-WARNING: function 'calls_safe' makes no sleeping calls
void calls_safe(void) {
    safe_func();
}
