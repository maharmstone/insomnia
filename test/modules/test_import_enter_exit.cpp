// Test 3: nosleep_enter/nosleep_exit from a module work for CFG analysis.
import sleepmod;

__attribute__((might_sleep)) void test_module_lock(void) {
    mod_lock();

    // EXPECTED-ERROR: call to 'might_sleep' function 'mod_maybe' in nosleep context
    mod_maybe();

    mod_unlock();
}
