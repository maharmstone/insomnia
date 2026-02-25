// Test 1: Explicit annotations survive module import.
// A nosleep function calling mod_maybe (might_sleep) through a module should error.
import sleepmod;

// Calling explicitly-tagged functions from a nosleep function
__attribute__((nosleep)) void test_explicit_tags(void) {
    mod_safe();   // fine — nosleep

    // EXPECTED-ERROR: 'nosleep' function 'test_explicit_tags' calls 'might_sleep' function 'mod_maybe'
    mod_maybe();
}
