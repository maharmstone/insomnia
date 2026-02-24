// Test nosleep_enter / nosleep_exit attributes

__attribute__((nosleep_enter)) void lock(void);
__attribute__((nosleep_exit)) void unlock(void);
__attribute__((nosleep)) void safe_func(void) {}
__attribute__((might_sleep)) void maybe_func(void) {}

// 1. Balanced enter/exit with nosleep work between — no error
__attribute__((might_sleep)) void test_balanced(void) {
    lock();
    safe_func();
    unlock();
}

// 2. Sleep while locked — error
__attribute__((might_sleep)) void test_sleep_while_locked(void) {
    lock();
    // EXPECTED-ERROR: call to 'might_sleep' function 'maybe_func' in nosleep context
    maybe_func();
    unlock();
}

// 3. Unbalanced return — error
__attribute__((might_sleep)) void test_unbalanced_return(void) {
    lock();
    safe_func();
    // EXPECTED-ERROR: function 'test_unbalanced_return' may return with nosleep context held (depth 1)
    return;
}

// 4. Nested enter/exit — no error
__attribute__((might_sleep)) void test_nested(void) {
    lock();
    lock();
    safe_func();
    unlock();
    unlock();
}

// 5. Conditional path — lock in one branch only, sleep after merge → error
__attribute__((might_sleep)) void test_conditional(int cond) {
    if (cond)
        lock();
    // EXPECTED-ERROR: call to 'might_sleep' function 'maybe_func' in nosleep context
    maybe_func();
    if (cond)
        unlock();
}

// 6. Unlock without lock — no error (depth clamped to 0)
__attribute__((might_sleep)) void test_unlock_without_lock(void) {
    unlock();
    maybe_func(); // fine, depth stays at 0
}
