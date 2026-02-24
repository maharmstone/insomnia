// Test rule 6: untagged definition that is implicitly nosleep should warn
// only when there is a separate forward declaration.

// Forward declaration + definition that is implicitly nosleep — should warn
void implicit_nosleep(void);
// EXPECTED-WARNING: function 'implicit_nosleep' makes no sleeping calls
void implicit_nosleep(void) {
    // leaf function, no calls
}

// Definition only, no forward declaration — should NOT warn
void definition_only(void) {
    // leaf function, no calls — but no forward decl, so no warning
}

// Explicitly tagged nosleep — should NOT warn
__attribute__((nosleep)) void explicit_nosleep(void) {
}
