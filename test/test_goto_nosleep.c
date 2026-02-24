// Test rule 12: cannot goto into a nosleep block from outside one

__attribute__((nosleep)) void safe_func(void) {}
__attribute__((might_sleep)) void maybe_func(void) {}

// goto from outside nosleep into nosleep block is an error
__attribute__((might_sleep)) void test_goto_into_nosleep(void) {
    // EXPECTED-ERROR: goto jumps into 'nosleep' block from outside a 'nosleep' context
    goto inside;

    __attribute__((nosleep)) {
inside:
        safe_func();
    }
}

// goto within the same nosleep block is fine (both at depth > 0)
__attribute__((might_sleep)) void test_goto_within_nosleep(void) {
    __attribute__((nosleep)) {
        goto next;
next:
        safe_func(); // no error
    }
}

// goto from one nosleep block to another nosleep block is fine
// (goto depth > 0, label depth > 0)
__attribute__((might_sleep)) void test_goto_nosleep_to_nosleep(void) {
    __attribute__((nosleep)) {
        goto other;
    }

    __attribute__((nosleep)) {
other:
        safe_func(); // no error
    }
}

// goto from nosleep block to outside is fine
__attribute__((might_sleep)) void test_goto_out_of_nosleep(void) {
    __attribute__((nosleep)) {
        goto outside;
    }
outside:
    maybe_func(); // fine, outside nosleep
}
