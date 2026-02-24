// Test C++ RAII constructor/destructor integration with nosleep_enter/nosleep_exit

__attribute__((nosleep)) void safe_func(void) {}
__attribute__((might_sleep)) void maybe_func(void) {}

struct Guard {
    __attribute__((nosleep_enter)) Guard() {}
    __attribute__((nosleep_exit)) ~Guard() {}
};

// 1. Balanced RAII — constructor enters, destructor exits — no error
__attribute__((might_sleep)) void test_raii_balanced() {
    Guard g;
    safe_func();
}

// 2. Sleep while RAII locked — error
__attribute__((might_sleep)) void test_raii_sleep() {
    Guard g;
    // EXPECTED-ERROR: call to 'might_sleep' function 'maybe_func' in nosleep context
    maybe_func();
}

// 3. Scoped RAII — nosleep only inside inner scope
__attribute__((might_sleep)) void test_raii_scoped() {
    maybe_func(); // fine, outside scope
    {
        Guard g;
        safe_func(); // fine, nosleep work
    } // ~Guard() exits nosleep
    maybe_func(); // fine, outside scope again
}

// 4. Early return — destructor runs at scope exit, no unbalanced error
__attribute__((might_sleep)) void test_raii_early_return(int cond) {
    Guard g;
    safe_func();
    if (cond)
        return; // ~Guard() runs, depth goes to 0
    safe_func();
}

// 5. Nested RAII — double lock/unlock
__attribute__((might_sleep)) void test_raii_nested() {
    Guard g1;
    Guard g2;
    safe_func();
}
