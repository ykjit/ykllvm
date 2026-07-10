; RUN: llc -stop-after=yk-stackmaps --yk-insert-stackmaps < %s | FileCheck %s

define i32 @callee(i32 %x) {
entry:
  ret i32 %x
}

define i32 @order(i32 %x) {
; CHECK-LABEL: define i32 @order
; CHECK: %r = call i32 @callee(i32 %x)
; CHECK-NEXT: call void (i64, i32, ...) @llvm.experimental.stackmap(i64 1, i32 0, i32 %x, i32 %local, i32 %from_later_block, i32 %r)
entry:
  br label %later

join:
  %local = add i32 %x, 7
  %r = call i32 @callee(i32 %x)
  %use1 = add i32 %local, %from_later_block
  %use2 = add i32 %use1, %r
  ret i32 %use2

later:
  %from_later_block = add i32 %x, 1
  br label %join
}

define i32 @dependency_order(i32 %x) {
; CHECK-LABEL: define i32 @dependency_order
; CHECK: %r = call i32 @callee(i32 %x)
; CHECK-NEXT: call void (i64, i32, ...) @llvm.experimental.stackmap(i64 2, i32 0, i32 %x, i32 %base, i32 %mid, i32 %derived, i32 %r)
entry:
  br label %def

use:
  %mid = add i32 %base, 1
  %derived = add i32 %mid, 1
  %r = call i32 @callee(i32 %x)
  %keep1 = add i32 %derived, %mid
  %keep2 = add i32 %keep1, %base
  %keep3 = add i32 %keep2, %r
  ret i32 %keep3

def:
  %base = add i32 %x, 1
  br label %use
}

define i32 @phi_incoming(i1 %cond, i32 %x) {
; CHECK-LABEL: define i32 @phi_incoming
; CHECK: %incoming = add i32 %x, 1
; CHECK-NEXT: call void (i64, i32, ...) @llvm.experimental.stackmap(i64 3, i32 0, i1 %cond, i32 %x, i32 %incoming)
; CHECK-NEXT: br i1 %cond, label %join, label %exit
entry:
  %incoming = add i32 %x, 1
  br i1 %cond, label %join, label %exit

join:
  %p = phi i32 [ %incoming, %entry ]
  ret i32 %p

exit:
  ret i32 %x
}

define i32 @phi_cycle(i1 %cond, i32 %x) {
; CHECK-LABEL: define i32 @phi_cycle
; CHECK: %r = call i32 @callee(i32 %x)
; CHECK-NEXT: call void (i64, i32, ...) @llvm.experimental.stackmap(i64 4, i32 0, i1 %cond, i32 %x, i32 %a, i32 %b, i32 %r)
entry:
  br label %loop

loop:
  %a = phi i32 [ %x, %entry ], [ %b, %loop ]
  %b = phi i32 [ 0, %entry ], [ %a, %loop ]
  %r = call i32 @callee(i32 %x)
  %keep1 = add i32 %a, %b
  %keep2 = add i32 %keep1, %r
  br i1 %cond, label %loop, label %exit

exit:
  ret i32 %keep2
}
