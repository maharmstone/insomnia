// Test rule 7: nosleep function calling might_sleep or sleeps is an error

__attribute__((might_sleep)) void maybe_sleep(void);
__attribute__((sleeps)) void does_sleep(void) {}

// EXPECTED-ERROR: 'nosleep' function 'bad1' calls 'might_sleep' function 'maybe_sleep'
__attribute__((nosleep)) void bad1(void) {
    maybe_sleep();
}

// EXPECTED-ERROR: 'nosleep' function 'bad2' calls 'sleeps' function 'does_sleep'
__attribute__((nosleep)) void bad2(void) {
    does_sleep();
}

// nosleep calling nosleep should be fine
__attribute__((nosleep)) void safe(void) {}
__attribute__((nosleep)) void good(void) {
    safe();
}
