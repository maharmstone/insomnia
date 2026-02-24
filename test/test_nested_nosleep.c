// Test nested nosleep blocks and nosleep block inside nosleep function

__attribute__((nosleep)) void safe_func(void) {}
__attribute__((might_sleep)) void maybe_func(void) {}

// Nested nosleep blocks are fine
__attribute__((might_sleep)) void test_nested_blocks(void) {
    __attribute__((nosleep)) {
        safe_func(); // fine

        __attribute__((nosleep)) {
            safe_func(); // fine, nested is ok

            // EXPECTED-ERROR: call to 'might_sleep' function 'maybe_func' in nosleep context
            maybe_func();
        }

        // EXPECTED-ERROR: call to 'might_sleep' function 'maybe_func' in nosleep context
        maybe_func(); // still in outer nosleep block
    }
}

// nosleep block inside nosleep function is redundant but harmless
// (the function-level rule 7 already covers it, block check is skipped)
__attribute__((nosleep)) void test_redundant_block(void) {
    __attribute__((nosleep)) {
        safe_func();
    }
}
