; RUN: opt -passes='yk-module-clone,always-inline,yk-module-clone' -verify-each -S %s | FileCheck %s
;
; Check that control points are detected when inlined.

; CHECK-LABEL: define void @caller(
; CHECK: call void @yk_mt_control_point(
; CHECK: ret void
; CHECK-LABEL: define void @__yk_opt_caller(
; CHECK: call void @yk_mt_control_point(
; CHECK: ret void
; CHECK-NOT: define

declare void @yk_mt_control_point(ptr, ptr)

define void @caller(ptr %mt, ptr %loc) {
  call void @helper(ptr %mt, ptr %loc)
  ret void
}

define void @helper(ptr %mt, ptr %loc) alwaysinline {
  call void @yk_mt_control_point(ptr %mt, ptr %loc)
  ret void
}
