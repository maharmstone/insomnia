// Test rule 5: tag mismatch between declaration and definition

// Case 1: declaration tagged nosleep, definition tagged sleeps
__attribute__((nosleep)) void mismatch1(void);
// EXPECTED-ERROR: sleep annotation mismatch: declaration is 'nosleep' but definition is 'sleeps'
__attribute__((sleeps)) void mismatch1(void) {}

// Case 2: declaration tagged, definition untagged
__attribute__((nosleep)) void mismatch2(void);
// EXPECTED-ERROR: sleep annotation mismatch: declaration is 'nosleep' but definition is 'untagged'
void mismatch2(void) {}

// Consistent tags should be fine (no error)
__attribute__((nosleep)) void consistent(void);
__attribute__((nosleep)) void consistent(void) {}
