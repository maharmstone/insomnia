// Test 2: Unannotated leaf function imported from module.
// mod_leaf() has no explicit annotation. In its own TU it computes as nosleep,
// but the importing TU only sees the declaration — rule 4 says untagged
// forward declarations are implicitly might_sleep.
// So calling mod_leaf from a nosleep function should error.
import sleepmod;

__attribute__((nosleep)) void test_implicit_tag(void) {
    // EXPECTED-ERROR: 'nosleep' function 'test_implicit_tag' calls 'might_sleep' function 'mod_leaf'
    mod_leaf();
}
