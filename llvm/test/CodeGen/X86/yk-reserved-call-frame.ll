; RUN: llc -mtriple=x86_64-unknown-linux-gnu --yk-stackmap-add-locs %s -o - | FileCheck %s

; yk expects frames to have a fixed stack size without RSP being fiddled with
; by the caller.

declare i64 @callee(i64, i64, i64, i64, i64, i64, i64, i64)
declare void @llvm.experimental.stackmap(i64, i32, ...)

define i64 @with_stackmap() {
; CHECK-LABEL: with_stackmap:
; CHECK: subq $16, %rsp
; CHECK: movq $7, (%rsp)
; CHECK: callq callee
; CHECK: addq $16, %rsp
  %r = call i64 @callee(i64 1, i64 2, i64 3, i64 4, i64 5, i64 6, i64 7, i64 8)
  call void (i64, i32, ...) @llvm.experimental.stackmap(i64 0, i32 0, i64 %r)
  ret i64 %r
}

define i64 @without_stackmap() {
; CHECK-LABEL: without_stackmap:
; CHECK: pushq $7
; CHECK: callq callee
  %r = call i64 @callee(i64 1, i64 2, i64 3, i64 4, i64 5, i64 6, i64 7, i64 8)
  ret i64 %r
}
