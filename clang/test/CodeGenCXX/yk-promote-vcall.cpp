// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm -o - %s | FileCheck %s

// Check that `yk_promote_vcall` promotes the function pointer loaded from the vtable.

struct S {
  virtual int f(int);
};

// CHECK-LABEL: define{{.*}} i32 @_Z8promotedP1S(
// CHECK: [[FP:%.*]] = load ptr, ptr %vfn
// CHECK: [[PROMOTED:%.*]] = call ptr @__yk_promote_ptr(ptr [[FP]])
// CHECK: call noundef i32 [[PROMOTED]](
int promoted(S *s) {
  int r;
  [[clang::yk_promote_vcall]] r = s->f(1);
  return r;
}

// CHECK-LABEL: define{{.*}} i32 @_Z11notpromotedP1S(
// CHECK-NOT: __yk_promote_ptr
// CHECK: ret i32
int notpromoted(S *s) {
  return s->f(1);
}
