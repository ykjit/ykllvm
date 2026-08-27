; REQUIRES: x86-registered-target
; RUN: llc -O2 -mtriple=x86_64-unknown-linux-gnu \
; RUN:   -yk-no-overflow-intrinsics \
; RUN:   -stop-after=codegenprepare -o - %s | FileCheck %s

;; Avoid transformations which introduce aggregate-valued overflow intrinsics:
;; those intrinsics are not currently supported by the JIT.

define i1 @unsigned_add_overflow(i32 %a, i32 %b) {
; CHECK-LABEL: define i1 @unsigned_add_overflow(
; CHECK:         %sum = add i32 %a, %b
; CHECK-NEXT:    %overflow = icmp ult i32 %sum, %a
; CHECK-NOT:     llvm.uadd.with.overflow
  %sum = add i32 %a, %b
  %overflow = icmp ult i32 %sum, %a
  ret i1 %overflow
}

define i1 @unsigned_sub_overflow(i32 %a, i32 %b) {
; CHECK-LABEL: define i1 @unsigned_sub_overflow(
; CHECK:         %difference = sub i32 %a, %b
; CHECK-NEXT:    %overflow = icmp ult i32 %a, %b
; CHECK-NOT:     llvm.usub.with.overflow
  %difference = sub i32 %a, %b
  %overflow = icmp ult i32 %a, %b
  ret i1 %overflow
}
