; RUN: llc -stop-after yk-basicblock-tracer-pass --yk-opt-functions-only < %s | FileCheck %s
; RUN: llc -stop-after yk-basicblock-tracer-pass --yk-basicblock-tracer < %s | FileCheck %s --check-prefix=CHECK-TRACE

; Test that with --yk-opt-functions-only, no tracing functions are added

define dso_local i32 @inc(i32 %x) {
entry:
  %add = add nsw i32 %x, 1
  ret i32 %add
}

; With --yk-opt-functions-only we don't expect tracing calls
; CHECK-LABEL: define dso_local i32 @inc(i32 %x)
; CHECK-NEXT: entry:
; CHECK-NOT: __yk_trace_basicblock

; Without --yk-opt-functions-only we expected tracing calls
; CHECK-TRACE-LABEL: define dso_local i32 @inc(i32 %x)
; CHECK-TRACE-NEXT: entry:
; CHECK-TRACE-NEXT: call void @__yk_trace_basicblock
