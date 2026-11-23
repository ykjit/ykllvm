; RUN: opt -yk-module-clone -yk-no-vectorize -passes='lto<O2>' -force-vector-width=4 -force-vector-interleave=1 -S %s | FileCheck %s
; RUN: opt -yk-module-clone -yk-no-vectorize -passes='lto-pre-link<O2>' -force-vector-width=4 -force-vector-interleave=1 -S %s | FileCheck %s --check-prefix=DISABLED --implicit-check-not='<4 x i32>' --implicit-check-not='@__yk_opt_'
; RUN: opt -yk-no-vectorize -passes='lto<O2>' -force-vector-width=4 -force-vector-interleave=1 -S %s | FileCheck %s --check-prefix=DISABLED --implicit-check-not='<4 x i32>'

; Check that only outlined clones without control points can use vector instructions,.

; CHECK-LABEL: define void @bump(
; CHECK-NOT: <4 x i32>
; CHECK: ret void
; CHECK-LABEL: define void @with_cp(
; CHECK-NOT: <4 x i32>
; CHECK: ret void
; CHECK-LABEL: define void @__yk_opt_bump(
; CHECK: load <4 x i32>
; CHECK: add <4 x i32>
; CHECK: store <4 x i32>
; CHECK-LABEL: define void @__yk_opt_with_cp(
; CHECK-NOT: <4 x i32>
; CHECK: call void @yk_mt_control_point(
; CHECK-NOT: <4 x i32>
; CHECK: ret void

; DISABLED-LABEL: define void @bump(
; DISABLED: ret void
; DISABLED-LABEL: define void @with_cp(
; DISABLED: ret void

target triple = "x86_64-unknown-linux-gnu"

declare void @yk_mt_control_point(ptr, ptr)

define void @bump(ptr noalias %dst, ptr noalias %src, i64 %n) alwaysinline {
entry:
  %empty = icmp eq i64 %n, 0
  br i1 %empty, label %exit, label %loop
loop:
  %i = phi i64 [ 0, %entry ], [ %next, %loop ]
  %p = getelementptr i32, ptr %src, i64 %i
  %v = load i32, ptr %p, align 4
  %r = add i32 %v, 1
  %q = getelementptr i32, ptr %dst, i64 %i
  store i32 %r, ptr %q, align 4
  %next = add nuw i64 %i, 1
  %more = icmp ult i64 %next, %n
  br i1 %more, label %loop, label %exit
exit:
  ret void
}

define void @with_cp(ptr noalias %dst, ptr noalias %src, i64 %n,
                     ptr %mt, ptr %loc) {
entry:
  call void @cp_helper(ptr %mt, ptr %loc, i64 %n)
  call void @bump(ptr %dst, ptr %src, i64 %n)
  ret void
}

; This helper is inlined after cloning, so the clone must be checked for
; control points again before vectorisation.
define void @cp_helper(ptr %mt, ptr %loc, i64 %n) alwaysinline {
entry:
  br label %loop
loop:
  %i = phi i64 [ 0, %entry ], [ %next, %loop ]
  call void @yk_mt_control_point(ptr %mt, ptr %loc)
  %next = add i64 %i, 1
  %more = icmp ult i64 %next, %n
  br i1 %more, label %loop, label %exit
exit:
  ret void
}
