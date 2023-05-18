// RUN: %clang -target x86_64-linux-gnu -c -emit-llvm %s -o - | llvm-dis -o - | FileCheck --check-prefix CHECK64 %s
// RUN: %clang -target i386-pc-linux -c -emit-llvm %s -o - | llvm-dis -o - | FileCheck --check-prefix CHECK32 %s

// Check that the C `yk_promote` function attribute becomes the LLVM `yk_promote`
// function attribute.

// Note that clang tests have to work without a libc at all, so we can't use
// `size_t` in this function signature.
__attribute__((noinline, yk_promote("x"))) void f(__SIZE_TYPE__ x) {}

// CHECK64: define dso_local void @f(i64 noundef %x) #0 {
// CHECK64: attributes #0 = {{{.*}}"yk_promote"="0"{{.*}}}

// CHECK32: define dso_local void @f(i32 noundef %x) #0 {
// CHECK32: attributes #0 = {{{.*}}"yk_promote"="0"{{.*}}}
