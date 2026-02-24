// Test rule 4: untagged forward declarations are implicitly might_sleep

void unknown_extern(void);

// This calls an untagged forward declaration (might_sleep), so it's
// implicitly might_sleep too — no warning expected.
void calls_unknown(void) {
    unknown_extern();
}

// A nosleep function calling an untagged extern should error.
// EXPECTED-ERROR: 'nosleep' function 'bad_caller' calls 'might_sleep' function 'unknown_extern'
__attribute__((nosleep)) void bad_caller(void) {
    unknown_extern();
}
