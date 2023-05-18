// RUN: not %clang -c %s 2>&1 | FileCheck %s

// Check that promoting an invalid variable name gives a decent error.

__attribute__((yk_promote("x"))) void f(char x) {
// CHECK: error: Invalid type (not size_t) for promoted variable: 'x'
}
