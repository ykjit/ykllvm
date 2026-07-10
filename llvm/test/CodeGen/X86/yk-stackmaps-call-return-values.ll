; RUN: llc -stop-after=yk-stackmaps --yk-insert-stackmaps < %s | FileCheck %s

define i32 @callee(i32 %x) {
entry:
  %r = add i32 %x, 1
  ret i32 %r
}

define void @void_callee(i32 %x) {
entry:
  ret void
}

define i32 @zero() {
entry:
  ret i32 0
}

define i32 @direct_used_return(i32 %x) {
; CHECK-LABEL: define i32 @direct_used_return
; CHECK: %r = call i32 @callee(i32 %x)
; CHECK-NEXT: call void (i64, i32, ...) @llvm.experimental.stackmap(i64 {{[0-9]+}}, i32 0, i32 %x, i32 %keep, i32 %r)
entry:
  %keep = add i32 %x, 7
  %r = call i32 @callee(i32 %x)
  %use = add i32 %r, %keep
  ret i32 %use
}

define i32 @only_used_return() {
; CHECK-LABEL: define i32 @only_used_return
; CHECK: %r = call i32 @zero()
; CHECK-NEXT: call void (i64, i32, ...) @llvm.experimental.stackmap(i64 {{[0-9]+}}, i32 0, i32 %r)
entry:
  %r = call i32 @zero()
  ret i32 %r
}

define i32 @direct_unused_return(i32 %x) {
; CHECK-LABEL: define i32 @direct_unused_return
; CHECK: %r = call i32 @callee(i32 %x)
; CHECK-NEXT: call void (i64, i32, ...) @llvm.experimental.stackmap(i64 {{[0-9]+}}, i32 0, i32 %x, i32 %keep, i32 %keep)
entry:
  %keep = add i32 %x, 7
  %r = call i32 @callee(i32 %x)
  %use = add i32 %keep, 1
  ret i32 %use
}

define i32 @void_call(i32 %x) {
; CHECK-LABEL: define i32 @void_call
; CHECK: call void @void_callee(i32 %x)
; CHECK-NEXT: call void (i64, i32, ...) @llvm.experimental.stackmap(i64 {{[0-9]+}}, i32 0, i32 %x, i32 %keep, i32 %keep)
entry:
  %keep = add i32 %x, 7
  call void @void_callee(i32 %x)
  %use = add i32 %keep, 1
  ret i32 %use
}

define i32 @indirect_used_return(ptr %fp, i32 %x) {
; CHECK-LABEL: define i32 @indirect_used_return
; CHECK: %r = call i32 %fp(i32 %x)
; CHECK-NEXT: call void (i64, i32, ...) @llvm.experimental.stackmap(i64 {{[0-9]+}}, i32 0, ptr %fp, i32 %x, i32 %keep, i32 %r)
entry:
  %keep = add i32 %x, 7
  %r = call i32 %fp(i32 %x)
  %use = add i32 %r, %keep
  ret i32 %use
}
