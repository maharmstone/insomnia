// Test rule 11: nosleep lexical blocks

__attribute__((nosleep)) void safe_func(void) {}
__attribute__((might_sleep)) void maybe_func(void) {}
__attribute__((sleeps)) void sleepy_func(void) {}

// Calling might_sleep inside a nosleep block is an error
__attribute__((might_sleep)) void test_block_calls_might_sleep(void) {
    maybe_func(); // fine, outside nosleep block

    __attribute__((nosleep)) {
        safe_func(); // fine, nosleep callee

        // EXPECTED-ERROR: call to 'might_sleep' function 'maybe_func' inside 'nosleep' block
        maybe_func();
    }

    maybe_func(); // fine again, outside block
}

// Calling sleeps inside a nosleep block is an error
__attribute__((might_sleep)) void test_block_calls_sleeps(void) {
    __attribute__((nosleep)) {
        // EXPECTED-ERROR: call to 'sleeps' function 'sleepy_func' inside 'nosleep' block
        sleepy_func();
    }
}

// Calling nosleep inside a nosleep block is fine
__attribute__((might_sleep)) void test_block_calls_safe(void) {
    __attribute__((nosleep)) {
        safe_func(); // no error
    }
}

// Untagged function pointer inside nosleep block
typedef void (*generic_fp)(void);
__attribute__((might_sleep)) void test_block_fptr(generic_fp fp) {
    __attribute__((nosleep)) {
        // EXPECTED-ERROR: call through 'might_sleep' function pointer inside 'nosleep' block
        fp();
    }
}
