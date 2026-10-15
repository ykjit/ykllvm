// Idempotence must be visible even without a definition in this translation
// unit. Redeclarations and a later definition must retain the contract.
// RUN: %clang -S -emit-llvm -O2 %s -o - | FileCheck %s
// RUN: %clang -S -emit-llvm -O0 %s -o - | FileCheck %s

__attribute__((yk_idempotent)) int external(int, ...);
int external(int, ...);

int caller(int x, int validity) {
  return external(x, validity);
}

__attribute__((yk_idempotent)) int defined(int);
int defined(int x) { return x + 1; }

int calls_defined(int x) { return defined(x); }

// CHECK: define {{.*}}i32 @caller(
// CHECK: call {{.*}}i32 (i32, ...) @external(i32 {{.*}}, i32 {{.*}})
// CHECK: declare {{.*}}i32 @external(i32 {{.*}}...){{.*}} #[[EXTERNAL:[0-9]+]]
// CHECK: define {{.*}}i32 @defined({{.*}}){{.*}} #[[DEFINED:[0-9]+]]
// CHECK: define {{.*}}i32 @calls_defined(
// CHECK: call {{.*}}i32 @defined(
// CHECK-DAG: attributes #[[EXTERNAL]] = { {{.*}}noinline{{.*}}"yk_idempotent"{{.*}}"yk_outline"{{.*}} }
// CHECK-DAG: attributes #[[DEFINED]] = { {{.*}}noinline{{.*}}"yk_idempotent"{{.*}}"yk_outline"{{.*}} }
