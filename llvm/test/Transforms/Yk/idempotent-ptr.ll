; RUN: llc -mtriple=x86_64-- -stop-after=yk-stackmaps --yk-patch-idempotent --yk-insert-stackmaps < %s | FileCheck %s

declare ptr @identity_ptr(ptr) "yk_idempotent"

define ptr @caller(ptr %p) {
; CHECK: %result = call ptr @identity_ptr(ptr %p)
; CHECK-NEXT: %{{[0-9]+}} = call ptr @__yk_idempotent_promote_ptr(ptr %result)
  %result = call ptr @identity_ptr(ptr %p)
  ret ptr %result
}
