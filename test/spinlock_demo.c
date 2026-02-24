// spinlock_demo.c — fake spinlock API exercising nosleep_enter/nosleep_exit
//
// Build via CMake: the plugin is loaded automatically via -fplugin.
// Expected: one error on the maybe_func() call inside the locked region.

#include <stddef.h>

typedef struct { int locked; } spinlock_t;

__attribute__((nosleep_enter)) void spin_lock(spinlock_t *lock) {
    lock->locked = 1;
}

__attribute__((nosleep_exit)) void spin_unlock(spinlock_t *lock) {
    lock->locked = 0;
}

__attribute__((nosleep)) void do_nosleep_work(void) {
    // some register twiddling
}

__attribute__((might_sleep)) void maybe_func(void) {
    // might block
}

__attribute__((might_sleep)) void process_data(spinlock_t *lock) {
    spin_lock(lock);

    do_nosleep_work();

    // This should trigger: call to 'might_sleep' function in nosleep context
    maybe_func();

    spin_unlock(lock);
}

int main(void) {
    spinlock_t lock = {0};
    process_data(&lock);
    return 0;
}
