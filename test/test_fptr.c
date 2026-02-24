// Test rule 8: untagged function pointers are implicitly might_sleep
// Test rule 9: assigning might_sleep/sleeps to nosleep fptr is an error

__attribute__((sleeps)) void sleepy(void) {}
__attribute__((might_sleep)) void maybe(void) {}
__attribute__((nosleep)) void safe(void) {}

// Rule 8: nosleep function calling through untagged function pointer
typedef void (*generic_fp)(void);

// EXPECTED-ERROR: 'nosleep' function 'call_generic' calls 'might_sleep' function '(function pointer)'
__attribute__((nosleep)) void call_generic(generic_fp fp) {
    fp();
}

// Rule 9: assigning sleeps function to nosleep function pointer
typedef void (*safe_fp)(void) __attribute__((nosleep));

void test_fptr_assign(void);
// EXPECTED-WARNING: function 'test_fptr_assign' makes no sleeping calls
void test_fptr_assign(void) {
    // EXPECTED-ERROR: assigning 'sleeps' function 'sleepy' to 'nosleep' function pointer
    safe_fp p1 = sleepy;

    // EXPECTED-ERROR: assigning 'might_sleep' function 'maybe' to 'nosleep' function pointer
    safe_fp p2 = maybe;

    // This should be fine
    safe_fp p3 = safe;
    (void)p1; (void)p2; (void)p3;
}
