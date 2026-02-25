// Module interface with explicit sleep annotations on exported functions.
export module sleepmod;

export __attribute__((nosleep)) void mod_safe(void) {}
export __attribute__((might_sleep)) void mod_maybe(void) {}
export __attribute__((sleeps)) void mod_sleepy(void) {}

// Unannotated leaf function — implicitly nosleep via fixed-point,
// but the importing TU won't have the computed status.
export void mod_leaf(void) {}

// nosleep_enter / nosleep_exit
export __attribute__((nosleep_enter)) void mod_lock(void) {}
export __attribute__((nosleep_exit)) void mod_unlock(void) {}
