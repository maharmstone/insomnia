// Test rule 6: untagged definition that is implicitly nosleep should warn

// EXPECTED-WARNING: function 'implicit_nosleep' makes no sleeping calls
void implicit_nosleep(void) {
    // leaf function, no calls
}

// Explicitly tagged nosleep — should NOT warn
__attribute__((nosleep)) void explicit_nosleep(void) {
}
