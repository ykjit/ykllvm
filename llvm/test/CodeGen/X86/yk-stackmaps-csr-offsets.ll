; RUN: llc -mtriple=x86_64-unknown-linux-gnu --yk-stackmap-add-locs %s -o - | FileCheck %s

; Check that stackmaps encode a byte offset, not an index.
declare void @llvm.experimental.stackmap(i64, i32, ...)

define i64 @incoming(i64 %a, i64 %b, i64 %c, i64 %d, i64 %e, i64 %f, i64 %g, i64 %h) "frame-pointer"="all" {
; CHECK-LABEL: incoming:
; CHECK: pushq %rbp
; CHECK: movq %rsp, %rbp
; CHECK: pushq %rbx
  call void asm sideeffect "", "~{rbx}"()
  %sum = add i64 %g, %h
  call void (i64, i32, ...) @llvm.experimental.stackmap(i64 42, i32 0, i64 %sum)
  ret i64 %sum
}

; CHECK-LABEL: __LLVM_StackMaps:
; CHECK: .p2align 3, 0x0
; CHECK-NEXT: .short 0
; CHECK-NEXT: .short 0
; CHECK-NEXT: .p2align 3, 0x0
; CHECK-NEXT: .byte 1
; CHECK-NEXT: .byte 0
; CHECK-NEXT: .long 1
; CHECK-NEXT: .short 3
; CHECK-NEXT: .short 0
; CHECK-NEXT: .long -8
