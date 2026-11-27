; RUN: llc -mtriple=x86_64-- --yk-stackmap-add-locs < %s | FileCheck %s

; X86FixupBWInsts normally widens an 8/16-bit stack reload into a
; zero-extending load, which yk's additional locations analysis doesn't
; recognise as a reload. Under `--yk-stackmap-add-locs` we therefore don't
; widen reloads in functions containing a stackmap or a patchpoint.
;
; This test validates that the stackmap records the spill slot of a reloaded
; 16-bit value as an extra location, and that the reload isn't zero-extended.

; CHECK: movw %ax, [[SLOT:-[0-9]+]](%rbp) # 2-byte Spill
; CHECK: movw [[SLOT]](%rbp), %ax # 2-byte Reload

; `%x` is in `rax`, with the spill slot as its one extra location.
; CHECK-LABEL: __LLVM_StackMaps:
; CHECK:      .long .Ltmp{{[0-9]+}}-f
; CHECK-NEXT: .short 0
; CHECK-NEXT: .short 1
; CHECK-NEXT: .byte 1
; CHECK-NEXT: .byte 1
; CHECK-NEXT: .byte 0
; CHECK-NEXT: .short 2
; CHECK-NEXT: .short 0
; CHECK-NEXT: .short 0
; CHECK-NEXT: .short 1
; CHECK-NEXT: .short [[SLOT]]

declare void @llvm.experimental.stackmap(i64, i32, ...)
declare preserve_nonecc void @g()

define i16 @f(ptr %p) {
  %x = load volatile i16, ptr %p
  ; `preserve_none` clobbers every register, forcing `%x` onto the stack.
  call preserve_nonecc void @g()
  call void (i64, i32, ...) @llvm.experimental.stackmap(i64 1, i32 0, i16 %x)
  ret i16 %x
}
