// RUN: not %clang -c %s 2>&1 | FileCheck %s

// Check that promoting an invalid variable name gives a decent error.

__attribute__((yk_promote("bogus"))) void f(int x, char *y) {
// CHECK: error: Invalid promoted variable: 'bogus'
}
