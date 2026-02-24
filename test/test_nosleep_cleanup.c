// Test __attribute__((cleanup)) integration with nosleep_enter/nosleep_exit

__attribute__((nosleep_enter)) __attribute__((nosleep)) void lock(void);
__attribute__((nosleep_exit)) __attribute__((nosleep)) void cleanup_unlock(int *guard);
__attribute__((nosleep)) void safe_func(void) {}
__attribute__((might_sleep)) void maybe_func(void) {}

// 1. Balanced: lock + cleanup unlock at scope exit — no error
__attribute__((might_sleep)) void test_cleanup_balanced(void) {
    int guard __attribute__((cleanup(cleanup_unlock)));
    lock();
    safe_func();
}

// 2. Sleep while locked via cleanup pattern — error
__attribute__((might_sleep)) void test_cleanup_sleep(void) {
    int guard __attribute__((cleanup(cleanup_unlock)));
    lock();
    // EXPECTED-ERROR: call to 'might_sleep' function 'maybe_func' in nosleep context
    maybe_func();
}

// 3. Cleanup without lock — no error (depth clamped to 0)
__attribute__((might_sleep)) void test_cleanup_no_lock(void) {
    int guard __attribute__((cleanup(cleanup_unlock)));
    maybe_func(); // fine, depth is 0 here
}

// 4. Early return with cleanup — cleanup runs, so no unbalanced error
__attribute__((might_sleep)) void test_cleanup_early_return(int cond) {
    int guard __attribute__((cleanup(cleanup_unlock)));
    lock();
    safe_func();
    if (cond)
        return; // cleanup_unlock runs here, depth goes to 0
    safe_func();
    // cleanup_unlock runs here too
}
