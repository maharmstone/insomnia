// Test force_nosleep: treated as nosleep by callers, but body is not checked.

__attribute__((might_sleep)) void maybe_func(void) {}
__attribute__((sleeps)) void sleepy_func(void) {}
__attribute__((nosleep)) void safe_func(void) {}

// force_nosleep function calling might_sleep — no error (body not checked)
__attribute__((force_nosleep)) void forced_safe(void) {
    maybe_func(); // no error — force_nosleep suppresses body checking
}

// force_nosleep function calling sleeps — no error (body not checked)
__attribute__((force_nosleep)) void forced_safe2(void) {
    sleepy_func(); // no error
}

// For comparison: nosleep function calling might_sleep IS an error
__attribute__((nosleep)) void real_nosleep(void) {
    // EXPECTED-ERROR: 'nosleep' function 'real_nosleep' calls 'might_sleep' function 'maybe_func'
    maybe_func();
}

// Callers see force_nosleep as nosleep — calling it from nosleep context is fine
__attribute__((nosleep)) void caller_of_forced(void) {
    forced_safe(); // fine — forced_safe is treated as nosleep
}

// nosleep_enter + force_nosleep callee — no error in locked region
__attribute__((nosleep_enter)) __attribute__((nosleep)) void lock(void);
__attribute__((nosleep_exit)) __attribute__((nosleep)) void unlock(void);

__attribute__((might_sleep)) void test_forced_in_locked(void) {
    lock();
    forced_safe(); // fine — force_nosleep is nosleep to callers
    unlock();
}

// force_nosleep on a function pointer typedef — works like nosleep for assignments
typedef void (*forced_fp)(void) __attribute__((force_nosleep));
__attribute__((might_sleep)) void test_forced_fptr(void) {
    forced_fp fp = safe_func; // fine — safe_func is nosleep
}
