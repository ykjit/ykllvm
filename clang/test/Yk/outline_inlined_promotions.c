// Checks the compiler borks if a __yk_promote_* gets inlined into a
// yk_outline function.
//
// RUN: not %clang -O2 -mllvm --yk-embed-ir -mllvm --yk-insert-stackmaps %s 2>&1 | FileCheck %s

void *__yk_promote_ptr(void *);

void g(void *p) {
  __yk_promote_ptr(p);
}

// CHECK: error: promotion detected in yk_outline annotated function 'main'
__attribute__((yk_outline))
int main(int argc, char **argv) {
  g(argv);
}
