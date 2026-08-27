; REQUIRES: x86-registered-target
; RUN: llc -O2 -mtriple=x86_64-unknown-linux-gnu \
; RUN:   -yk-mark-traceable-optnone-after-ir-passes \
; RUN:   -yk-no-overflow-intrinsics \
; RUN:   -stop-after=codegenprepare -o - %s | FileCheck %s

;; Avoid transformations which introduce aggregate-valued overflow intrinsics:
;; those intrinsics are not currently supported by the JIT.

define i8 @codegenprepare_runs(ptr %base, i64 %index, i1 %take) {
; CHECK-LABEL: define i8 @codegenprepare_runs(
entry:
; CHECK:       entry:
; CHECK-NOT:     getelementptr
  %address = getelementptr i8, ptr %base, i64 %index
  br i1 %take, label %load, label %exit

load:
; CHECK:       load:
; CHECK:         %sunkaddr = getelementptr i8, ptr %base, i64 %index
  %value = load i8, ptr %address
  ret i8 %value

exit:
  ret i8 0
}

;; CodeGenPrepare continues to respect optnone when it was present before Yk's
;; automatic marking pass ran.
define i8 @explicit_optnone(ptr %base, i64 %index, i1 %take) optnone noinline {
; CHECK-LABEL: define i8 @explicit_optnone(
entry:
; CHECK:       entry:
; CHECK:         %address = getelementptr i8, ptr %base, i64 %index
  %address = getelementptr i8, ptr %base, i64 %index
  br i1 %take, label %load, label %exit

load:
; CHECK:       load:
; CHECK-NOT:     %sunkaddr
  %value = load i8, ptr %address
  ret i8 %value

exit:
  ret i8 0
}

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

define i8 @switch_phi(i1 %direct, i8 %value, i8 %fallback) {
; CHECK-LABEL: define i8 @switch_phi(
; CHECK:         %0 = zext i8 %value to i32
; CHECK:         switch i32 %0
entry:
  br i1 %direct, label %case, label %switch

switch:
  switch i8 %value, label %default [
    i8 7, label %case
    i8 8, label %case8
    i8 9, label %case9
  ]

case:
; CHECK:       case:
; CHECK:         %result = phi i8 [ %fallback, %entry ], [ 7, %switch ]
  %result = phi i8 [ %fallback, %entry ], [ 7, %switch ]
  ret i8 %result

case8:
  ret i8 8

case9:
  ret i8 9

default:
  ret i8 0
}
