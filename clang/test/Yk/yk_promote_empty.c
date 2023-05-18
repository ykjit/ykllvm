// RUN: %clang -c %s

// Check that an empty promote list is OK.

__attribute__((yk_promote())) void f(void) { }
